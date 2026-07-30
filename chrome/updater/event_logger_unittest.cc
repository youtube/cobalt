// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/event_logger.h"

#include <iostream>
#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/prefs_impl.h"
#include "chrome/updater/protos/omaha_usage_stats_event.pb.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/util/util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/network.h"
#include "components/update_client/update_client.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater {

using ::base::test::EqualsProto;
using ::net::test_server::BasicHttpResponse;
using ::net::test_server::EmbeddedTestServer;
using ::net::test_server::EmbeddedTestServerHandle;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;
using ::testing::Pointwise;
using ::updater::proto::Omaha4Metric;
using ::updater::proto::Omaha4UsageStatsExtension;

std::ostream& operator<<(std::ostream& os,
                         const PersistedData::Cookie& cookie) {
  os << "value: " << cookie.value << " expiration: " << cookie.expiration;
  return os;
}

TEST(ExtractEventLoggingCookieTest, ExtractsCookie) {
  EXPECT_EQ(
      ExtractEventLoggingCookie(
          base::Time::Now(),
          "NID=123=foo-bar-baz; expires=Tue, 11-Nov-2025 17:47:59 GMT; path=/; "
          "domain=some.random.domain; HttpOnly"),
      (PersistedData::Cookie{
          .value = "123=foo-bar-baz",
          .expiration = base::Time::FromSecondsSinceUnixEpoch(1762883279),
      }));
}

TEST(ExtractEventLoggingCookieTest, ExtractsExpirationCaseInsensitive) {
  EXPECT_EQ(
      ExtractEventLoggingCookie(
          base::Time::Now(),
          "NID=123=foo-bar-baz; ExPiReS=Tue, 11-Nov-2025 17:47:59 GMT; path=/; "
          "domain=some.random.domain; HttpOnly"),
      (PersistedData::Cookie{
          .value = "123=foo-bar-baz",
          .expiration = base::Time::FromSecondsSinceUnixEpoch(1762883279),
      }));
}

TEST(ExtractEventLoggingCookieTest, IgnoresUnrelatedCookie) {
  EXPECT_EQ(
      ExtractEventLoggingCookie(
          base::Time::Now(),
          "RID=123=foo-bar-baz; expires=Tue, 11-Nov-2025 17:47:59 GMT; path=/; "
          "domain=some.random.domain; HttpOnly"),
      std::nullopt);
}

TEST(ExtractEventLoggingCookieTest, UsesDefaultTtlWhenExpirationNotProvided) {
  const base::Time now = base::Time::Now();
  EXPECT_LE(
      ExtractEventLoggingCookie(
          now,
          "NID=123=foo-bar-baz; path=/; domain=some.random.domain; HttpOnly"),
      (PersistedData::Cookie{
          .value = "123=foo-bar-baz",
          .expiration = now + base::Days(180),
      }));
}

TEST(ExtractEventLoggingCookieTest, UsesDefaultTtlWhenExpirationMalformed) {
  const base::Time now = base::Time::Now();
  EXPECT_LE(ExtractEventLoggingCookie(
                now,
                "NID=123=foo-bar-baz; expires=not a date; path=/; "
                "domain=some.random.domain; HttpOnly"),
            (PersistedData::Cookie{
                .value = "123=foo-bar-baz",
                .expiration = now + base::Days(180),
            }));
}

TEST(ExtractEventLoggingCookieTest, IgnoresInvalidHeaderValue) {
  EXPECT_EQ(ExtractEventLoggingCookie(base::Time::Now(),
                                      "this ; is not a Set-Cookie line NID="),
            std::nullopt);
}

TEST(ExtractEventLoggingCookieTest, IgnoresEmptyHeaderValue) {
  EXPECT_EQ(ExtractEventLoggingCookie(base::Time::Now(), ""), std::nullopt);
}

class EventLoggerTest : public ::testing::Test {
 public:
  void SetUp() override {
    const UpdaterScope scope = GetUpdaterScopeForTesting();
    ClearPrefs();
    auto pref = std::make_unique<TestingPrefServiceSimple>();
    update_client::RegisterPrefs(pref->registry());
    RegisterPersistedDataPrefs(pref->registry());
    configurator_ = base::MakeRefCounted<Configurator>(
        base::MakeRefCounted<UpdaterPrefsImpl>(
            /*prefs_dir=*/base::FilePath(), /*lock=*/nullptr, std::move(pref)),
        CreateExternalConstants(), scope);
    persisted_data_ = configurator_->GetUpdaterPersistedData();
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &EventLoggerTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE((test_server_handle_ = test_server_.StartAndReturnHandle()));
    auto test_clock = std::make_unique<base::SimpleTestClock>();
    test_clock_ = test_clock.get();
    delegate_ = std::make_unique<RemoteLoggingDelegate>(
        scope, test_server_.GetURL("/event-logging"),
        /*is_cloud_managed=*/false, configurator_, std::move(test_clock));
  }

  void TearDown() override { ClearPrefs(); }

 protected:
  void SetRequestHandler(
      base::RepeatingCallback<std::unique_ptr<HttpResponse>(const HttpRequest&)>
          request_handler) {
    request_handler_ = request_handler;
  }

  base::test::TaskEnvironment task_environment_;
  EmbeddedTestServer test_server_;
  scoped_refptr<Configurator> configurator_;
  scoped_refptr<PersistedData> persisted_data_;
  EmbeddedTestServerHandle test_server_handle_;
  std::unique_ptr<RemoteLoggingDelegate> delegate_;
  raw_ptr<base::SimpleTestClock> test_clock_;

 private:
  void ClearPrefs() {
    const UpdaterScope updater_scope = GetUpdaterScopeForTesting();
    for (const std::optional<base::FilePath>& path :
         {GetInstallDirectory(updater_scope),
          GetVersionedInstallDirectory(updater_scope)}) {
      ASSERT_TRUE(path);
      ASSERT_TRUE(
          base::DeleteFile(path->Append(FILE_PATH_LITERAL("prefs.json"))));
    }
  }

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    return request_handler_.Run(request);
  }

  base::RepeatingCallback<std::unique_ptr<HttpResponse>(const HttpRequest&)>
      request_handler_;
};

TEST_F(EventLoggerTest, StoreNextAllowedAttemptTime) {
  static constexpr base::Time kExpectedTime =
      base::Time::FromSecondsSinceUnixEpoch(100);

  base::RunLoop run_loop;
  delegate_->StoreNextAllowedAttemptTime(kExpectedTime, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(persisted_data_->GetNextAllowedLoggingAttemptTime(), kExpectedTime);
}

TEST_F(EventLoggerTest, SerializesEvents) {
  Omaha4Metric metric1;
  metric1.mutable_network_event()->set_url("https://example.com");
  metric1.mutable_network_event()->set_bytes_sent(42);
  metric1.mutable_network_event()->set_bytes_received(24);
  Omaha4Metric metric2;
  metric2.mutable_network_event()->set_url("https://google.com");
  metric2.mutable_network_event()->set_bytes_sent(100);
  metric2.mutable_network_event()->set_error_code(404);
  std::vector<Omaha4Metric> metrics = {metric1, metric2};

  std::string serialized_extension =
      delegate_->AggregateAndSerializeEvents(metrics);

  Omaha4UsageStatsExtension extension;
  ASSERT_TRUE(extension.ParseFromString(serialized_extension));
  EXPECT_THAT(extension.metric(), Pointwise(EqualsProto(), metrics));
}

TEST_F(EventLoggerTest, SerializesMetadata) {
  std::string serialized_extension = delegate_->AggregateAndSerializeEvents({});

  Omaha4UsageStatsExtension extension;
  ASSERT_TRUE(extension.ParseFromString(serialized_extension));
  EXPECT_FALSE(extension.metadata().platform().empty());
  EXPECT_FALSE(extension.metadata().cpu_architecture().empty());
  EXPECT_FALSE(extension.metadata().app_version().empty());
}

TEST_F(EventLoggerTest, DoPostRequest) {
  SetRequestHandler(base::BindLambdaForTesting(
      [this](const HttpRequest& request) -> std::unique_ptr<HttpResponse> {
        GURL absolute_url = test_server_.GetURL(request.relative_url);
        if (absolute_url.GetPath() != "/event-logging") {
          return nullptr;
        }

        EXPECT_EQ(request.content, "request body");

        auto http_response = std::make_unique<BasicHttpResponse>();
        http_response->set_code(net::HTTP_OK);
        http_response->set_content("response body");
        return http_response;
      }));

  base::RunLoop run_loop;
  delegate_->DoPostRequest(
      "request body",
      base::BindOnce([](std::optional<int> http_status,
                        std::optional<std::string> response_body) {
        EXPECT_EQ(http_status, net::HTTP_OK);
        EXPECT_EQ(response_body, "response body");
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(EventLoggerTest, AttachesLoggingCookieToPostRequest) {
  const PersistedData::Cookie logging_cookie{
      .value = "logging-cookie",
      .expiration = test_clock_->Now() + base::Hours(1)};
  persisted_data_->SetRemoteLoggingCookie(logging_cookie);
  SetRequestHandler(base::BindLambdaForTesting(
      [&](const HttpRequest& request) -> std::unique_ptr<HttpResponse> {
        EXPECT_NE(
            request.headers.at(update_client::NetworkFetcher::kHeaderCookie)
                .find(logging_cookie.value),
            std::string::npos);

        auto http_response = std::make_unique<BasicHttpResponse>();
        http_response->set_code(net::HTTP_OK);
        return http_response;
      }));

  base::RunLoop run_loop;
  delegate_->DoPostRequest("request body",
                           base::BindOnce([](std::optional<int> http_status,
                                             std::optional<std::string>) {
                             EXPECT_EQ(http_status, net::HTTP_OK);
                           }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(EventLoggerTest, RequestsNewCookieIfNonePersisted) {
  SetRequestHandler(base::BindLambdaForTesting(
      [&](const HttpRequest& request) -> std::unique_ptr<HttpResponse> {
        EXPECT_NE(
            request.headers.at(update_client::NetworkFetcher::kHeaderCookie)
                .find("\"\""),
            std::string::npos);

        auto http_response = std::make_unique<BasicHttpResponse>();
        http_response->set_code(net::HTTP_OK);
        return http_response;
      }));

  base::RunLoop run_loop;
  delegate_->DoPostRequest("request body",
                           base::BindOnce([](std::optional<int> http_status,
                                             std::optional<std::string>) {
                             EXPECT_EQ(http_status, net::HTTP_OK);
                           }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(EventLoggerTest, RequestsNewCookieIfExpired) {
  const PersistedData::Cookie logging_cookie{
      .value = "logging-cookie",
      .expiration = test_clock_->Now() + base::Hours(1)};
  test_clock_->Advance(base::Hours(2));
  persisted_data_->SetRemoteLoggingCookie(logging_cookie);
  SetRequestHandler(base::BindLambdaForTesting(
      [&](const HttpRequest& request) -> std::unique_ptr<HttpResponse> {
        EXPECT_NE(
            request.headers.at(update_client::NetworkFetcher::kHeaderCookie)
                .find("\"\""),
            std::string::npos);

        auto http_response = std::make_unique<BasicHttpResponse>();
        http_response->set_code(net::HTTP_OK);
        return http_response;
      }));

  base::RunLoop run_loop;
  delegate_->DoPostRequest("request body",
                           base::BindOnce([](std::optional<int> http_status,
                                             std::optional<std::string>) {
                             EXPECT_EQ(http_status, net::HTTP_OK);
                           }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(EventLoggerTest, ClearsPersistedCookieIfExpired) {
  const PersistedData::Cookie logging_cookie{
      .value = "logging-cookie",
      .expiration = test_clock_->Now() + base::Hours(1)};
  test_clock_->Advance(base::Hours(2));
  persisted_data_->SetRemoteLoggingCookie(logging_cookie);
  SetRequestHandler(base::BindLambdaForTesting(
      [&](const HttpRequest& request) -> std::unique_ptr<HttpResponse> {
        auto http_response = std::make_unique<BasicHttpResponse>();
        http_response->set_code(net::HTTP_OK);
        return http_response;
      }));

  base::RunLoop run_loop;
  delegate_->DoPostRequest("request body",
                           base::BindOnce([](std::optional<int> http_status,
                                             std::optional<std::string>) {
                             EXPECT_EQ(http_status, net::HTTP_OK);
                           }).Then(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(persisted_data_->GetRemoteLoggingCookie(), std::nullopt);
}

TEST_F(EventLoggerTest, PersistsLoggingCookieFromPostResponse) {
  const PersistedData::Cookie logging_cookie{
      .value = "logging-cookie",
      .expiration = base::Time::FromSecondsSinceUnixEpoch(1762883279)};
  SetRequestHandler(base::BindLambdaForTesting(
      [&](const HttpRequest& request) -> std::unique_ptr<HttpResponse> {
        auto http_response = std::make_unique<BasicHttpResponse>();
        http_response->set_code(net::HTTP_OK);
        http_response->AddCustomHeader(
            update_client::NetworkFetcher::kHeaderSetCookie,
            base::StrCat({"NID=", logging_cookie.value,
                          "; Expires=Tue, 11-Nov-2025 17:47:59 GMT;"}));
        return http_response;
      }));

  base::RunLoop run_loop;
  delegate_->DoPostRequest("request body",
                           base::BindOnce([](std::optional<int> http_status,
                                             std::optional<std::string>) {
                             EXPECT_EQ(http_status, net::HTTP_OK);
                           }).Then(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(persisted_data_->GetRemoteLoggingCookie(), logging_cookie);
}

namespace {

class FakeNetworkFetcher : public update_client::NetworkFetcher {
 public:
  explicit FakeNetworkFetcher(base::OnceClosure on_post_request)
      : on_post_request_(std::move(on_post_request)) {}
  ~FakeNetworkFetcher() override = default;

  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override {
    response_started_callback_ = std::move(response_started_callback);
    post_request_complete_callback_ = std::move(post_request_complete_callback);
    if (on_post_request_) {
      std::move(on_post_request_).Run();
    }
  }

  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback)
      override {
    return base::DoNothing();
  }

  const ResponseStartedCallback& response_started_callback() const {
    return response_started_callback_;
  }

  PostRequestCompleteCallback TakeCompleteCallback() {
    return std::move(post_request_complete_callback_);
  }

 private:
  ResponseStartedCallback response_started_callback_;
  PostRequestCompleteCallback post_request_complete_callback_;
  base::OnceClosure on_post_request_;
};

class FakeNetworkFetcherFactory : public update_client::NetworkFetcherFactory {
 public:
  explicit FakeNetworkFetcherFactory(
      std::unique_ptr<update_client::NetworkFetcher> fetcher)
      : fetcher_(std::move(fetcher)) {}

  std::unique_ptr<update_client::NetworkFetcher> Create() const override {
    return std::move(fetcher_);
  }

 protected:
  ~FakeNetworkFetcherFactory() override = default;

 private:
  mutable std::unique_ptr<update_client::NetworkFetcher> fetcher_;
};

class TestConfigurator : public Configurator {
 public:
  TestConfigurator(scoped_refptr<UpdaterPrefs> prefs,
                   scoped_refptr<ExternalConstants> external_constants,
                   UpdaterScope scope,
                   scoped_refptr<update_client::NetworkFetcherFactory>
                       network_fetcher_factory)
      : Configurator(prefs, external_constants, scope),
        network_fetcher_factory_(network_fetcher_factory) {}

  scoped_refptr<update_client::NetworkFetcherFactory> GetNetworkFetcherFactory()
      override {
    return network_fetcher_factory_;
  }

 protected:
  ~TestConfigurator() override = default;

 private:
  friend class base::RefCountedThreadSafe<TestConfigurator>;
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
};

}  // namespace

TEST(EventLoggerOutOfOrderTest, CallbacksOutOfOrder) {
  base::test::TaskEnvironment task_environment;
  base::RunLoop post_request_run_loop;
  base::RunLoop complete_run_loop;

  auto fetcher =
      std::make_unique<FakeNetworkFetcher>(post_request_run_loop.QuitClosure());
  FakeNetworkFetcher* fetcher_ptr = fetcher.get();

  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto configurator = base::MakeRefCounted<TestConfigurator>(
      base::MakeRefCounted<UpdaterPrefsImpl>(
          /*prefs_dir=*/base::FilePath(), /*lock=*/nullptr, std::move(pref)),
      CreateExternalConstants(), GetUpdaterScopeForTesting(),
      base::MakeRefCounted<FakeNetworkFetcherFactory>(std::move(fetcher)));

  auto delegate = std::make_unique<RemoteLoggingDelegate>(
      GetUpdaterScopeForTesting(), GURL("https://example.com/event-logging"),
      /*is_cloud_managed=*/false, configurator,
      std::make_unique<base::SimpleTestClock>());

  delegate->DoPostRequest(
      "request body",
      base::BindLambdaForTesting([&](std::optional<int> http_status,
                                     std::optional<std::string> response_body) {
        complete_run_loop.Quit();
      }));

  // Wait for PostRequest to be called on sequence.
  post_request_run_loop.Run();

  ASSERT_NE(fetcher_ptr, nullptr);
  auto response_started = fetcher_ptr->response_started_callback();
  auto complete = fetcher_ptr->TakeCompleteCallback();

  ASSERT_FALSE(response_started.is_null());
  ASSERT_FALSE(complete.is_null());

  // Simulate out-of-order execution: complete first, releasing RefCountedData
  // ownership in once callback.
  std::move(complete).Run("response body", 0, "etag", "proof", "cookie", 0);

  // Wait for the completion callback task to execute.
  complete_run_loop.Run();

  // Then start response. Under the old unique_ptr behavior, this would have
  // dereferenced a dangling pointer and crashed.
  response_started.Run(200, 100);
}

}  // namespace updater
