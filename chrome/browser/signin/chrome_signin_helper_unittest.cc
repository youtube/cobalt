// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_helper.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "build/buildflag.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#endif

namespace {

#if BUILDFLAG(ENABLE_MIRROR)
const char kMirrorActionAddSession[] = "action=ADDSESSION";
#if BUILDFLAG(IS_ANDROID)
const char kMirrorActionGoIncognito[] =
    "action=INCOGNITO,continue_url=http://example.com";
#endif
#endif

// URLRequestInterceptor adding a account consistency response header to Gaia
// responses.
class TestRequestInterceptor : public net::URLRequestInterceptor {
 public:
  explicit TestRequestInterceptor(const std::string& header_name,
                                  const std::string& header_value)
      : header_name_(header_name), header_value_(header_value) {}
  ~TestRequestInterceptor() override = default;

 private:
  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    std::string response_headers =
        base::StringPrintf("HTTP/1.1 200 OK\n\n%s: %s\n", header_name_.c_str(),
                           header_value_.c_str());
    return std::make_unique<net::URLRequestTestJob>(request, response_headers,
                                                    "", true);
  }

  const std::string header_name_;
  const std::string header_value_;
};

class TestResponseAdapter : public signin::ResponseAdapter,
                            public base::SupportsUserData {
 public:
  TestResponseAdapter(const std::string& header_name,
                      const std::string& header_value,
                      bool is_outermost_main_frame,
                      content::WebContents* web_contents = nullptr)
      : is_outermost_main_frame_(is_outermost_main_frame),
        headers_(new net::HttpResponseHeaders(std::string())),
        web_contents_(web_contents) {
    headers_->SetHeader(header_name, header_value);
  }

  TestResponseAdapter(const TestResponseAdapter&) = delete;
  TestResponseAdapter& operator=(const TestResponseAdapter&) = delete;

  ~TestResponseAdapter() override {}

  content::WebContents::Getter GetWebContentsGetter() const override {
    return base::BindLambdaForTesting(
        [contents = web_contents_]() -> content::WebContents* {
          return contents;
        });
  }
  bool IsOutermostMainFrame() const override {
    return is_outermost_main_frame_;
  }
  GURL GetUrl() const override { return GURL("https://accounts.google.com"); }
  std::optional<url::Origin> GetRequestInitiator() const override {
    // Pretend the request came from the same origin.
    return url::Origin::Create(GetUrl());
  }
  const url::Origin* GetRequestTopFrameOrigin() const override {
    return &request_top_frame_origin_;
  }
  const net::HttpResponseHeaders* GetHeaders() const override {
    return headers_.get();
  }

  void RemoveHeader(const std::string& name) override {
    headers_->RemoveHeader(name);
  }

  base::SupportsUserData::Data* GetUserData(const void* key) const override {
    return base::SupportsUserData::GetUserData(key);
  }

  void SetUserData(
      const void* key,
      std::unique_ptr<base::SupportsUserData::Data> data) override {
    return base::SupportsUserData::SetUserData(key, std::move(data));
  }

 private:
  bool is_outermost_main_frame_;
  const url::Origin request_top_frame_origin_{url::Origin::Create(GetUrl())};
  scoped_refptr<net::HttpResponseHeaders> headers_;
  raw_ptr<content::WebContents> web_contents_;
};

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;
  ~MockWebContentsDelegate() override = default;

  MOCK_METHOD3(OpenURLFromTab,
               content::WebContents*(
                   content::WebContents*,
                   const content::OpenURLParams&,
                   base::OnceCallback<void(content::NavigationHandle&)>));
};

}  // namespace

using ::testing::_;

class ChromeSigninHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  ChromeSigninHelperTest() = default;
  ~ChromeSigninHelperTest() override = default;
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

class TestChromeRequestAdapter : public signin::ChromeRequestAdapter {
 public:
  explicit TestChromeRequestAdapter(const GURL& url)
      : signin::ChromeRequestAdapter(url,
                                     original_headers_,
                                     &modified_headers_,
                                     &headers_to_remove_) {}

  const net::HttpRequestHeaders& modified_headers() const {
    return modified_headers_;
  }

  // ChromeRequestAdapter:
  content::WebContents::Getter GetWebContentsGetter() const override {
    return content::WebContents::Getter();
  }
  network::mojom::RequestDestination GetRequestDestination() const override {
    return network::mojom::RequestDestination::kDocument;
  }
  bool IsOutermostMainFrame() const override { return true; }
  bool IsFetchLikeAPI() const override { return false; }
  GURL GetReferrer() const override { return GURL(); }
  void SetDestructionCallback(base::OnceClosure closure) override {}

 private:
  net::HttpRequestHeaders original_headers_;
  net::HttpRequestHeaders modified_headers_;
  std::vector<std::string> headers_to_remove_;
};

// Tests that Dice response headers are removed after being processed.
TEST_F(ChromeSigninHelperTest, RemoveDiceSigninHeader) {
  // Process the header.
  TestResponseAdapter adapter(signin::kDiceResponseHeader, "Foo",
                              /*is_outermost_main_frame=*/false);
  signin::ProcessAccountConsistencyResponseHeaders(&adapter, GURL(),
                                                   false /* is_incognito */);

  // Check that the header has been removed.
  EXPECT_FALSE(adapter.GetHeaders()->HasHeader(signin::kDiceResponseHeader));
}

TEST_F(ChromeSigninHelperTest, FixAccountConsistencyRequestHeader) {
  // Setup the test environment.
  sync_preferences::TestingPrefServiceSyncable prefs;
  content_settings::CookieSettings::RegisterProfilePrefs(prefs.registry());
  HostContentSettingsMap::RegisterProfilePrefs(prefs.registry());
  privacy_sandbox::RegisterProfilePrefs(prefs.registry());
  scoped_refptr<HostContentSettingsMap> settings_map =
      new HostContentSettingsMap(&prefs, /*is_off_the_record=*/false,
                                 /*store_last_modified=*/false,
                                 /*restore_session=*/false,
                                 /*should_record_metrics=*/false);
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      new content_settings::CookieSettings(
          settings_map.get(), &prefs,
          /*tracking_protection_settings=*/nullptr, /*is_incognito=*/false,
          content_settings::CookieSettings::NoFedCmSharingPermissionsCallback(),
          /*tpcd_metadata_manager=*/nullptr);

  {
    // Non-elligible request, no header.
    TestChromeRequestAdapter request(GURL("https://gmail.com"));
    signin::FixAccountConsistencyRequestHeader(
        &request, GURL(), /*is_off_the_record=*/false,
        /*incognito_availability=*/0, signin::AccountConsistencyMethod::kDice,
        "gaia_id", /*is_child_account=*/signin::Tribool::kFalse,
        /*is_sync_enabled=*/true, "device_id", cookie_settings.get());
    EXPECT_EQ(
        request.modified_headers().GetHeader(signin::kChromeConnectedHeader),
        std::nullopt);
  }

  {
    // Google Docs gets the header.
    TestChromeRequestAdapter request(GURL("https://docs.google.com"));
    signin::FixAccountConsistencyRequestHeader(
        &request, GURL(), /*is_off_the_record=*/false,
        /*incognito_availability=*/0, signin::AccountConsistencyMethod::kDice,
        "gaia_id", /*is_child_account=*/signin::Tribool::kFalse,
        /*is_sync_enabled=*/true, "device_id", cookie_settings.get());
    std::string expected_header =
        "source=Chrome,id=gaia_id,mode=0,enable_account_consistency=false,"
        "supervised=false,consistency_enabled_by_default=false";
    EXPECT_THAT(
        request.modified_headers().GetHeader(signin::kChromeConnectedHeader),
        testing::Optional(expected_header));
  }

  // Tear down the test environment.
  settings_map->ShutdownOnUIThread();
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_MIRROR)
// Tests that user data is set on Mirror requests.
TEST_F(ChromeSigninHelperTest, MirrorMainFrame) {
  // Process the header.
  TestResponseAdapter response_adapter(signin::kChromeManageAccountsHeader,
                                       kMirrorActionAddSession,
                                       /*is_outermost_main_frame=*/true);
  signin::ProcessAccountConsistencyResponseHeaders(&response_adapter, GURL(),
                                                   false /* is_incognito */);
  // Check that the header has not been removed.
  EXPECT_TRUE(response_adapter.GetHeaders()->HasHeader(
      signin::kChromeManageAccountsHeader));
  // Request was flagged with the user data.
  EXPECT_TRUE(response_adapter.GetUserData(
      signin::kManageAccountsHeaderReceivedUserDataKey));
}

// Tests that user data is not set on Mirror requests for sub frames.
TEST_F(ChromeSigninHelperTest, MirrorSubFrame) {
  // Process the header.
  TestResponseAdapter response_adapter(signin::kChromeManageAccountsHeader,
                                       kMirrorActionAddSession,
                                       /*is_outermost_main_frame=*/false);
  signin::ProcessAccountConsistencyResponseHeaders(&response_adapter, GURL(),
                                                   false /* is_incognito */);
  // Request was not flagged with the user data.
  EXPECT_FALSE(response_adapter.GetUserData(
      signin::kManageAccountsHeaderReceivedUserDataKey));
}

#if BUILDFLAG(IS_ANDROID)
// Tests that receiving INCOGNITO action within kChromeManageAccountsHeader
// opens the URL in a new tab.
TEST_F(ChromeSigninHelperTest, MirrorGoIncognitoForemostWebContents) {
  MockWebContentsDelegate mock_web_contents_delegate;
  std::unique_ptr<content::WebContents> web_contents(CreateTestWebContents());
  web_contents->SetDelegate(&mock_web_contents_delegate);

  TestTabModel tab_model(profile());
  TabModelList::AddTabModel(&tab_model);
  base::ScopedClosureRunner remover(base::BindOnce(
      TabModelList::RemoveTabModel, base::Unretained(&tab_model)));

  // WebContents should be considered foremost for kChromeManageAccountsHeader
  // to be processed.
  tab_model.SetWebContentsList({web_contents.get()});
  tab_model.SetIsActiveModel(true);

  // Process the header.
  TestResponseAdapter response_adapter(
      signin::kChromeManageAccountsHeader, kMirrorActionGoIncognito,
      /*is_outermost_main_frame=*/true, web_contents.get());
  signin::ProcessAccountConsistencyResponseHeaders(&response_adapter, GURL(),
                                                   /*is_off_the_record=*/false);

  EXPECT_CALL(mock_web_contents_delegate,
              OpenURLFromTab(web_contents.get(), _, _));
  task_environment()->RunUntilIdle();
}

// Tests that Mirror headers are ignored in an inactive tab model. The header
// should be ignored regardless of the selected action, but the test is using
// INCOGNITO action as it is easier to verify in a test.
TEST_F(ChromeSigninHelperTest, MirrorGoIncognitoInactiveModel) {
  MockWebContentsDelegate mock_web_contents_delegate;
  std::unique_ptr<content::WebContents> web_contents(CreateTestWebContents());
  web_contents->SetDelegate(&mock_web_contents_delegate);

  TestTabModel tab_model(profile());
  TabModelList::AddTabModel(&tab_model);
  base::ScopedClosureRunner remover(base::BindOnce(
      TabModelList::RemoveTabModel, base::Unretained(&tab_model)));

  tab_model.SetWebContentsList({web_contents.get()});

  // Process the header.
  TestResponseAdapter response_adapter(
      signin::kChromeManageAccountsHeader, kMirrorActionGoIncognito,
      /*is_outermost_main_frame=*/true, web_contents.get());
  signin::ProcessAccountConsistencyResponseHeaders(&response_adapter, GURL(),
                                                   /*is_off_the_record=*/false);

  // TestTabModel is considered inactive by default, so
  // kChromeManageAccountsHeader should be ignored and the URL shouldn't be
  // opened.
  EXPECT_CALL(mock_web_contents_delegate, OpenURLFromTab(_, _, _)).Times(0);
  task_environment()->RunUntilIdle();
}

// Tests that Mirror headers are ignored in an inactive tab model. The header
// should be ignored regardless of the selected action, but the test is using
// INCOGNITO action as it is easier to verify in a test.
TEST_F(ChromeSigninHelperTest, MirrorGoIncognitoInactiveWebContents) {
  MockWebContentsDelegate mock_web_contents_delegate;
  std::unique_ptr<content::WebContents> web_contents_background(
      CreateTestWebContents());
  web_contents_background->SetDelegate(&mock_web_contents_delegate);

  TestTabModel tab_model(profile());
  TabModelList::AddTabModel(&tab_model);
  base::ScopedClosureRunner remover(base::BindOnce(
      TabModelList::RemoveTabModel, base::Unretained(&tab_model)));

  std::unique_ptr<content::WebContents> web_contents_foreground(
      CreateTestWebContents());
  // TestTabModel::GetActiveIndex always returns 0, so web_contents_foreground
  // will be considered the active one after this call.
  tab_model.SetWebContentsList(
      {web_contents_foreground.get(), web_contents_background.get()});
  tab_model.SetIsActiveModel(true);

  // Process the header.
  TestResponseAdapter response_adapter(
      signin::kChromeManageAccountsHeader, kMirrorActionGoIncognito,
      /*is_outermost_main_frame=*/true, web_contents_background.get());
  signin::ProcessAccountConsistencyResponseHeaders(&response_adapter, GURL(),
                                                   /*is_off_the_record=*/false);

  // web_contents_background is not foremost (web_contents_foreground is), so
  // kChromeManageAccountsHeader should be ignored and the URL shouldn't be
  // opened.
  EXPECT_CALL(mock_web_contents_delegate, OpenURLFromTab(_, _, _)).Times(0);
  task_environment()->RunUntilIdle();
}
#endif  // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(ENABLE_MIRROR)

TEST_F(ChromeSigninHelperTest,
       ParseGaiaIdFromRemoveLocalAccountResponseHeader) {
  EXPECT_EQ("123456",
            signin::ParseGaiaIdFromRemoveLocalAccountResponseHeaderForTesting(
                TestResponseAdapter("Google-Accounts-RemoveLocalAccount",
                                    "obfuscatedid=\"123456\"",
                                    /*is_outermost_main_frame=*/false)
                    .GetHeaders()));
  EXPECT_EQ("123456",
            signin::ParseGaiaIdFromRemoveLocalAccountResponseHeaderForTesting(
                TestResponseAdapter("Google-Accounts-RemoveLocalAccount",
                                    "obfuscatedid=\"123456\",foo=\"bar\"",
                                    /*is_outermost_main_frame=*/false)
                    .GetHeaders()));
  EXPECT_EQ(
      "",
      signin::ParseGaiaIdFromRemoveLocalAccountResponseHeaderForTesting(
          TestResponseAdapter("Google-Accounts-RemoveLocalAccount", "malformed",
                              /*is_outermost_main_frame=*/false)
              .GetHeaders()));
}
