// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"

#include <stddef.h>

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/registration_type.mojom-shared.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/trigger_attestation.h"
#include "services/network/public/mojom/attribution.mojom-blink.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/fake_local_frame_host.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

using ::network::mojom::AttributionReportingEligibility;

using blink::url_test_helpers::RegisterMockedErrorURLLoad;
using blink::url_test_helpers::RegisterMockedURLLoad;
using blink::url_test_helpers::ToKURL;

const char kAttributionReportingSupport[] = "Attribution-Reporting-Support";

const char kUrl[] = "https://example1.com/foo.html";

class AttributionSrcLocalFrameClient : public EmptyLocalFrameClient {
 public:
  AttributionSrcLocalFrameClient() = default;

  std::unique_ptr<URLLoader> CreateURLLoaderForTesting() override {
    return URLLoaderMockFactory::GetSingletonInstance()->CreateURLLoader();
  }

  void DispatchWillSendRequest(ResourceRequest& request) override {
    if (request.GetRequestContext() ==
        mojom::blink::RequestContextType::ATTRIBUTION_SRC) {
      request_head_ = request;
    }
  }

  const ResourceRequestHead& request_head() const { return request_head_; }

 private:
  ResourceRequestHead request_head_;
};

class MockDataHost : public mojom::blink::AttributionDataHost {
 public:
  explicit MockDataHost(
      mojo::PendingReceiver<mojom::blink::AttributionDataHost> data_host) {
    receiver_.Bind(std::move(data_host));
    receiver_.set_disconnect_handler(
        WTF::BindOnce(&MockDataHost::OnDisconnect, WTF::Unretained(this)));
  }

  ~MockDataHost() override = default;

  const Vector<attribution_reporting::SourceRegistration>& source_data() const {
    return source_data_;
  }

  const Vector<attribution_reporting::TriggerRegistration>& trigger_data()
      const {
    return trigger_data_;
  }

  const Vector<absl::optional<network::TriggerAttestation>>&
  trigger_attestation() const {
    return trigger_attestation_;
  }

#if BUILDFLAG(IS_ANDROID)
  const Vector<KURL>& os_sources() const { return os_sources_; }
  const Vector<KURL>& os_triggers() const { return os_triggers_; }
#endif

  size_t disconnects() const { return disconnects_; }

  void Flush() { receiver_.FlushForTesting(); }

 private:
  void OnDisconnect() { disconnects_++; }

  // mojom::blink::AttributionDataHost:
  void SourceDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::SourceRegistration data) override {
    source_data_.push_back(std::move(data));
  }

  void TriggerDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::TriggerRegistration data,
      absl::optional<network::TriggerAttestation> attestation) override {
    trigger_data_.push_back(std::move(data));
    trigger_attestation_.push_back(std::move(attestation));
  }

#if BUILDFLAG(IS_ANDROID)
  void OsSourceDataAvailable(const KURL& registration_url) override {
    os_sources_.push_back(registration_url);
  }

  void OsTriggerDataAvailable(const KURL& registration_url) override {
    os_triggers_.push_back(registration_url);
  }
#endif

  Vector<attribution_reporting::SourceRegistration> source_data_;

  Vector<attribution_reporting::TriggerRegistration> trigger_data_;

  Vector<absl::optional<network::TriggerAttestation>> trigger_attestation_;

#if BUILDFLAG(IS_ANDROID)
  Vector<KURL> os_sources_;
  Vector<KURL> os_triggers_;
#endif

  size_t disconnects_ = 0;
  mojo::Receiver<mojom::blink::AttributionDataHost> receiver_{this};
};

class MockAttributionHost : public mojom::blink::AttributionHost {
 public:
  explicit MockAttributionHost(blink::AssociatedInterfaceProvider* provider) {
    provider->OverrideBinderForTesting(
        mojom::blink::AttributionHost::Name_,
        WTF::BindRepeating(&MockAttributionHost::BindReceiver,
                           WTF::Unretained(this)));
  }

  ~MockAttributionHost() override = default;

  void WaitUntilBoundAndFlush() {
    if (receiver_.is_bound())
      return;
    base::RunLoop wait_loop;
    quit_ = wait_loop.QuitClosure();
    wait_loop.Run();
    receiver_.FlushForTesting();
  }

  MockDataHost* mock_data_host() { return mock_data_host_.get(); }

 private:
  void BindReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<mojom::blink::AttributionHost>(
            std::move(handle)));
    if (quit_)
      std::move(quit_).Run();
  }

  void RegisterDataHost(
      mojo::PendingReceiver<mojom::blink::AttributionDataHost> data_host,
      attribution_reporting::mojom::RegistrationType) override {
    mock_data_host_ = std::make_unique<MockDataHost>(std::move(data_host));
  }

  void RegisterNavigationDataHost(
      mojo::PendingReceiver<mojom::blink::AttributionDataHost> data_host,
      const blink::AttributionSrcToken& attribution_src_token) override {}

  mojo::AssociatedReceiver<mojom::blink::AttributionHost> receiver_{this};
  base::OnceClosure quit_;

  std::unique_ptr<MockDataHost> mock_data_host_;
};

class AttributionTestingPlatformSupport : public TestingPlatformSupport {
 public:
  network::mojom::AttributionSupport GetAttributionReportingSupport() override {
    return attribution_support;
  }

  network::mojom::AttributionSupport attribution_support =
      network::mojom::AttributionSupport::kWeb;
};

class AttributionSrcLoaderTest : public PageTestBase {
 public:
  AttributionSrcLoaderTest() = default;

  ~AttributionSrcLoaderTest() override = default;

  void SetUp() override {
    client_ = MakeGarbageCollected<AttributionSrcLocalFrameClient>();
    PageTestBase::SetupPageWithClients(nullptr, client_);

    SecurityContext& security_context =
        GetDocument().GetFrame()->DomWindow()->GetSecurityContext();
    security_context.SetSecurityOriginForTesting(nullptr);
    security_context.SetSecurityOrigin(
        SecurityOrigin::CreateFromString("https://example.com"));

    attribution_src_loader_ =
        MakeGarbageCollected<AttributionSrcLoader>(GetDocument().GetFrame());
  }

  void TearDown() override {
    GetFrame()
        .GetRemoteNavigationAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::blink::AttributionHost::Name_,
            WTF::BindRepeating([](mojo::ScopedInterfaceEndpointHandle) {}));
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
    PageTestBase::TearDown();
  }

 protected:
  ScopedTestingPlatformSupport<AttributionTestingPlatformSupport> platform_;
  Persistent<AttributionSrcLocalFrameClient> client_;
  Persistent<AttributionSrcLoader> attribution_src_loader_;
};

TEST_F(AttributionSrcLoaderTest, RegisterTriggerWithoutEligibleHeader) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request(test_url);
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      R"({"event_trigger_data":[{"trigger_data": "7"}]})");

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response, resource));
  host.WaitUntilBoundAndFlush();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);

  mock_data_host->Flush();
  EXPECT_EQ(mock_data_host->trigger_data().size(), 1u);
  ASSERT_EQ(mock_data_host->trigger_attestation().size(), 1u);
  ASSERT_FALSE(mock_data_host->trigger_attestation().at(0).has_value());
}

// TODO(https://crbug.com/1412566): Improve tests to properly cover the
// different `kAttributionReportingEligible` header values.
TEST_F(AttributionSrcLoaderTest, RegisterTriggerWithTriggerHeader) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request(test_url);
  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kTrigger);
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      R"({"event_trigger_data":[{"trigger_data": "7"}]})");

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  attribution_src_loader_->MaybeRegisterAttributionHeaders(request, response,
                                                           resource);
  host.WaitUntilBoundAndFlush();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);

  mock_data_host->Flush();
  EXPECT_EQ(mock_data_host->trigger_data().size(), 1u);
}

TEST_F(AttributionSrcLoaderTest, RegisterTriggerWithSourceTriggerHeader) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request(test_url);
  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kEventSourceOrTrigger);
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      R"({"event_trigger_data":[{"trigger_data": "7"}]})");

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  attribution_src_loader_->MaybeRegisterAttributionHeaders(request, response,
                                                           resource);
  host.WaitUntilBoundAndFlush();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);

  mock_data_host->Flush();
  EXPECT_EQ(mock_data_host->trigger_data().size(), 1u);
}

TEST_F(AttributionSrcLoaderTest, RegisterTriggerOsHeadersIgnored) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request(test_url);
  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kEventSourceOrTrigger);
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      R"({"event_trigger_data":[{"trigger_data": "7"}]})");

  // These should be ignored because the relevant feature is disabled by
  // default.
  response.SetHttpHeaderField(http_names::kAttributionReportingRegisterOSSource,
                              R"("https://r.test/x")");
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterOSTrigger,
      R"("https://r.test/y")");

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  attribution_src_loader_->MaybeRegisterAttributionHeaders(request, response,
                                                           resource);
  host.WaitUntilBoundAndFlush();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);

  mock_data_host->Flush();
  EXPECT_EQ(mock_data_host->trigger_data().size(), 1u);
}

TEST_F(AttributionSrcLoaderTest, RegisterTriggerWithAttestation) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request(test_url);
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      R"({"event_trigger_data":[{"trigger_data": "7"}]})");

  absl::optional<network::TriggerAttestation> trigger_attestation =
      network::TriggerAttestation::Create(
          "token", "08fa6760-8e5c-4ccb-821d-b5d82bef2b37");
  response.SetTriggerAttestation(trigger_attestation);

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response, resource));

  host.WaitUntilBoundAndFlush();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);
  mock_data_host->Flush();

  ASSERT_EQ(mock_data_host->trigger_attestation().size(), 1u);
  ASSERT_TRUE(mock_data_host->trigger_attestation().at(0).has_value());
  EXPECT_EQ(mock_data_host->trigger_attestation().at(0).value().token(),
            "token");
  EXPECT_EQ(mock_data_host->trigger_attestation()
                .at(0)
                .value()
                .aggregatable_report_id()
                .AsLowercaseString(),
            "08fa6760-8e5c-4ccb-821d-b5d82bef2b37");
}

TEST_F(AttributionSrcLoaderTest, AttributionSrcRequestsIgnored) {
  KURL test_url = ToKURL("https://example1.com/foo.html");
  ResourceRequest request(test_url);
  request.SetRequestContext(mojom::blink::RequestContextType::ATTRIBUTION_SRC);

  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      R"({"event_trigger_data":[{"trigger_data": "7"}]})");

  EXPECT_FALSE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response, resource));
}

TEST_F(AttributionSrcLoaderTest, AttributionSrcRequestsInvalidEligibleHeaders) {
  KURL test_url = ToKURL("https://example1.com/foo.html");
  ResourceRequest request(test_url);
  request.SetRequestContext(mojom::blink::RequestContextType::ATTRIBUTION_SRC);

  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);

  const char* header_values[] = {"navigation-source, event-source, trigger",
                                 "!!!", ""};

  for (const char* header : header_values) {
    response.SetHttpHeaderField(
        http_names::kAttributionReportingRegisterTrigger, header);

    EXPECT_FALSE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
        request, response, resource))
        << header;
  }
}

TEST_F(AttributionSrcLoaderTest, AttributionSrcRequestStatusHistogram) {
  base::HistogramTester histograms;

  KURL url1 = ToKURL(kUrl);
  RegisterMockedURLLoad(url1, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(kUrl, /*element=*/nullptr);

  static constexpr char kUrl2[] = "https://example2.com/foo.html";
  KURL url2 = ToKURL(kUrl2);
  RegisterMockedErrorURLLoad(url2);

  attribution_src_loader_->Register(kUrl2, /*element=*/nullptr);

  // kRequested = 0.
  histograms.ExpectUniqueSample("Conversions.AttributionSrcRequestStatus", 0,
                                2);

  url_test_helpers::ServeAsynchronousRequests();

  // kReceived = 1.
  histograms.ExpectBucketCount("Conversions.AttributionSrcRequestStatus", 1, 1);

  // kFailed = 2.
  histograms.ExpectBucketCount("Conversions.AttributionSrcRequestStatus", 2, 1);
}

TEST_F(AttributionSrcLoaderTest, Referrer) {
  KURL url = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(kUrl, /*element=*/nullptr);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetReferrerPolicy(),
            network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin);
  EXPECT_EQ(client_->request_head().ReferrerString(), String());
}

TEST_F(AttributionSrcLoaderTest, EligibleHeader_Register) {
  KURL url = ToKURL(kUrl);
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(kUrl, /*element=*/nullptr);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetAttributionReportingEligibility(),
            AttributionReportingEligibility::kEventSourceOrTrigger);

  EXPECT_TRUE(client_->request_head()
                  .HttpHeaderField(kAttributionReportingSupport)
                  .IsNull());
}

TEST_F(AttributionSrcLoaderTest, EligibleHeader_RegisterNavigation) {
  KURL url = ToKURL(kUrl);
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  std::ignore = attribution_src_loader_->RegisterNavigation(
      /*navigation_url=*/KURL(), /*attribution_src=*/kUrl,
      /*element=*/MakeGarbageCollected<HTMLAnchorElement>(GetDocument()));

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetAttributionReportingEligibility(),
            AttributionReportingEligibility::kNavigationSource);

  EXPECT_TRUE(client_->request_head()
                  .HttpHeaderField(kAttributionReportingSupport)
                  .IsNull());
}

// Regression test for crbug.com/1336797, where we didn't eagerly disconnect a
// source-eligible data host even if we knew there is no more data to be
// received on that channel. This test confirms the channel properly
// disconnects in this case.
TEST_F(AttributionSrcLoaderTest, EagerlyClosesRemote) {
  KURL url = ToKURL(kUrl);
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  attribution_src_loader_->Register(kUrl, /*element=*/nullptr);
  host.WaitUntilBoundAndFlush();
  url_test_helpers::ServeAsynchronousRequests();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);
  EXPECT_EQ(mock_data_host->disconnects(), 1u);
}

#if BUILDFLAG(IS_ANDROID)

TEST_F(AttributionSrcLoaderTest, NoneSupported_CannotRegister) {
  platform_->attribution_support = network::mojom::AttributionSupport::kNone;

  KURL test_url = ToKURL("https://example1.com/foo.html");

  EXPECT_FALSE(
      attribution_src_loader_->CanRegister(test_url, /*element=*/nullptr,
                                           /*request_id=*/absl::nullopt));
}

TEST_F(AttributionSrcLoaderTest, WebDisabled_TriggerNotRegistered) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  for (auto attribution_support : {network::mojom::AttributionSupport::kNone,
                                   network::mojom::AttributionSupport::kOs}) {
    platform_->attribution_support = attribution_support;

    ResourceRequest request(test_url);
    auto* resource = MakeGarbageCollected<MockResource>(test_url);
    ResourceResponse response(test_url);
    response.SetHttpStatusCode(200);
    response.SetHttpHeaderField(
        http_names::kAttributionReportingRegisterTrigger,
        R"({"event_trigger_data":[{"trigger_data": "7"}]})");

    MockAttributionHost host(
        GetFrame().GetRemoteNavigationAssociatedInterfaces());
    EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
        request, response, resource));
    host.WaitUntilBoundAndFlush();

    auto* mock_data_host = host.mock_data_host();
    ASSERT_TRUE(mock_data_host);

    mock_data_host->Flush();
    EXPECT_THAT(mock_data_host->trigger_data(), testing::IsEmpty());
  }
}

#endif

class AttributionSrcLoaderCrossAppWebEnabledTest
    : public AttributionSrcLoaderTest {
 public:
  AttributionSrcLoaderCrossAppWebEnabledTest() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      network::features::kAttributionReportingCrossAppWeb};

 protected:
  ScopedTestingPlatformSupport<AttributionTestingPlatformSupport> platform_;
};

TEST_F(AttributionSrcLoaderCrossAppWebEnabledTest, SupportHeader_Register) {
#if BUILDFLAG(IS_ANDROID)
  auto attribution_support = network::mojom::AttributionSupport::kWebAndOs;
#else
  auto attribution_support = network::mojom::AttributionSupport::kWeb;
#endif

  platform_->attribution_support = attribution_support;

  KURL url = ToKURL(kUrl);
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(kUrl, /*element=*/nullptr);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetAttributionReportingSupport(),
            attribution_support);
}

TEST_F(AttributionSrcLoaderCrossAppWebEnabledTest,
       SupportHeader_RegisterNavigation) {
#if BUILDFLAG(IS_ANDROID)
  auto attribution_support = network::mojom::AttributionSupport::kWebAndOs;
#else
  auto attribution_support = network::mojom::AttributionSupport::kWeb;
#endif

  platform_->attribution_support = attribution_support;

  KURL url = ToKURL(kUrl);
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  std::ignore = attribution_src_loader_->RegisterNavigation(
      /*navigation_url=*/KURL(), /*attribution_src=*/kUrl,
      /*element=*/MakeGarbageCollected<HTMLAnchorElement>(GetDocument()));

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetAttributionReportingSupport(),
            attribution_support);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(AttributionSrcLoaderCrossAppWebEnabledTest, RegisterOsTrigger) {
  platform_->attribution_support =
      network::mojom::AttributionSupport::kWebAndOs;

  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request(test_url);
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterOSTrigger,
      R"("https://r.test/x")");

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response, resource));
  host.WaitUntilBoundAndFlush();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);

  mock_data_host->Flush();
  EXPECT_THAT(mock_data_host->os_triggers(),
              ::testing::ElementsAre(KURL("https://r.test/x")));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace blink
