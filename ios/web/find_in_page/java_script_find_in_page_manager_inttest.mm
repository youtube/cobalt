// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/escape.h"
#import "base/test/ios/wait_util.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/find_in_page/find_in_page_java_script_feature.h"
#import "ios/web/public/find_in_page/java_script_find_in_page_manager.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_find_in_page_manager_delegate.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "testing/gtest/include/gtest/gtest.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {
// Page with text "Main frame body" and iframe with src URL equal to the URL
// query string.
const char kFindPageUrl[] = "/iframe?";
// URL of iframe with text contents "iframe iframe text".
const char kFindInPageIFrameUrl[] = "/echo-query?iframe iframe text";
}  // namespace

namespace web {

// Tests the JavaScriptFindInPageManager and verifies that values passed to
// FindInPageManagerDelegate are correct.
class JavaScriptFindInPageManagerTest : public WebTestWithWebState {
 protected:
  void SetUp() override {
    WebTestWithWebState::SetUp();

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/echo-query",
        base::BindRepeating(&testing::HandlePageWithContents)));
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/iframe",
                            base::BindRepeating(&testing::HandleIFrame)));
    ASSERT_TRUE(test_server_.Start());
    GetFindInPageManager()->SetDelegate(&delegate_);
  }

  // Returns the JavaScriptFindInPageManager associated with `web_state()`.
  JavaScriptFindInPageManager* GetFindInPageManager() {
    return web::JavaScriptFindInPageManager::FromWebState(web_state());
  }

  WebFramesManager* GetWebFramesManager() {
    return FindInPageJavaScriptFeature::GetInstance()->GetWebFramesManager(
        web_state());
  }

  // Waits until the delegate receives `index` from
  // DidSelectMatch(). Returns False if delegate never receives `index` within
  // time.
  [[nodiscard]] bool WaitForSelectedMatchAtIndex(int index) {
    return WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
      base::RunLoop().RunUntilIdle();
      return delegate_.state() && delegate_.state()->index == index;
    });
  }

  net::EmbeddedTestServer test_server_;

  FakeFindInPageManagerDelegate delegate_;
};

// Tests that find in page returns a single match for text which exists only in
// the main frame.
TEST_F(JavaScriptFindInPageManagerTest, FindMatchInMainFrame) {
  std::string url_spec =
      kFindPageUrl +
      base::EscapeQueryParamValue(kFindInPageIFrameUrl, /*use_plus=*/true);
  test::LoadUrl(web_state(), test_server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  GetFindInPageManager()->Find(@"Main frame text",
                               FindInPageOptions::FindInPageSearch);

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return delegate_.state();
  }));
  EXPECT_EQ(1, delegate_.state()->match_count);
  EXPECT_EQ(web_state(), delegate_.state()->web_state);
}

// Checks that find in page finds text that exists within the main frame and
// an iframe.
TEST_F(JavaScriptFindInPageManagerTest, FindMatchInMainFrameAndIFrame) {
  std::string url_spec =
      kFindPageUrl +
      base::EscapeQueryParamValue(kFindInPageIFrameUrl, /*use_plus=*/true);
  test::LoadUrl(web_state(), test_server_.GetURL(url_spec));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  GetFindInPageManager()->Find(@"frame", FindInPageOptions::FindInPageSearch);

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return delegate_.state();
  }));
  EXPECT_EQ(3, delegate_.state()->match_count);
  EXPECT_EQ(web_state(), delegate_.state()->web_state);
}

// Checks that find in page returns no matches for text not contained on the
// page.
TEST_F(JavaScriptFindInPageManagerTest, FindNoMatch) {
  std::string url_spec =
      kFindPageUrl +
      base::EscapeQueryParamValue(kFindInPageIFrameUrl, /*use_plus=*/true);
  test::LoadUrl(web_state(), test_server_.GetURL(url_spec));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  GetFindInPageManager()->Find(@"foobar", FindInPageOptions::FindInPageSearch);

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return delegate_.state();
  }));
  EXPECT_EQ(0, delegate_.state()->match_count);
  EXPECT_EQ(web_state(), delegate_.state()->web_state);
}

// Tests FindInPageNext iteration when matches exist in both the main frame and
// an iframe.
TEST_F(JavaScriptFindInPageManagerTest, FindForwardIterateThroughAllMatches) {
  std::string url_spec =
      kFindPageUrl +
      base::EscapeQueryParamValue(kFindInPageIFrameUrl, /*use_plus=*/true);
  test::LoadUrl(web_state(), test_server_.GetURL(url_spec));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  GetFindInPageManager()->Find(@"frame", FindInPageOptions::FindInPageSearch);

  EXPECT_TRUE(WaitForSelectedMatchAtIndex(0));
  EXPECT_EQ(3, delegate_.state()->match_count);

  GetFindInPageManager()->Find(@"frame", FindInPageOptions::FindInPageNext);

  EXPECT_TRUE(WaitForSelectedMatchAtIndex(1));

  GetFindInPageManager()->Find(@"frame", FindInPageOptions::FindInPageNext);

  EXPECT_TRUE(WaitForSelectedMatchAtIndex(2));

  GetFindInPageManager()->Find(@"frame", FindInPageOptions::FindInPageNext);

  EXPECT_TRUE(WaitForSelectedMatchAtIndex(0));
}

// Tests FindInPagePrevious iteration when matches exist in both the main frame
// and an iframe.
TEST_F(JavaScriptFindInPageManagerTest, FindBackwardsIterateThroughAllMatches) {
  std::string url_spec =
      kFindPageUrl +
      base::EscapeQueryParamValue(kFindInPageIFrameUrl, /*use_plus=*/true);
  test::LoadUrl(web_state(), test_server_.GetURL(url_spec));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  GetFindInPageManager()->Find(@"frame", FindInPageOptions::FindInPageSearch);

  EXPECT_TRUE(WaitForSelectedMatchAtIndex(0));
  EXPECT_EQ(3, delegate_.state()->match_count);

  GetFindInPageManager()->Find(@"frame", FindInPageOptions::FindInPagePrevious);

  EXPECT_TRUE(WaitForSelectedMatchAtIndex(2));

  GetFindInPageManager()->Find(@"frame", FindInPageOptions::FindInPagePrevious);

  EXPECT_TRUE(WaitForSelectedMatchAtIndex(1));

  GetFindInPageManager()->Find(@"frame", FindInPageOptions::FindInPagePrevious);

  EXPECT_TRUE(WaitForSelectedMatchAtIndex(0));
}

// Tests FindInPageNext iteration when matches exist in only an iframe.
TEST_F(JavaScriptFindInPageManagerTest, FindIterateThroughIframeMatches) {
  std::string url_spec =
      kFindPageUrl +
      base::EscapeQueryParamValue(kFindInPageIFrameUrl, /*use_plus=*/true);
  test::LoadUrl(web_state(), test_server_.GetURL(url_spec));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  GetFindInPageManager()->Find(@"iframe", FindInPageOptions::FindInPageSearch);

  EXPECT_TRUE(WaitForSelectedMatchAtIndex(0));
  EXPECT_EQ(2, delegate_.state()->match_count);

  GetFindInPageManager()->Find(@"iframe", FindInPageOptions::FindInPageNext);

  EXPECT_TRUE(WaitForSelectedMatchAtIndex(1));

  GetFindInPageManager()->Find(@"iframe", FindInPageOptions::FindInPageNext);

  EXPECT_TRUE(WaitForSelectedMatchAtIndex(0));
}

// Tests FindInPageNext and FindInPagePrevious iteration while passing null
// query.
TEST_F(JavaScriptFindInPageManagerTest, FindIterationWithNullQuery) {
  std::string url_spec =
      kFindPageUrl +
      base::EscapeQueryParamValue(kFindInPageIFrameUrl, /*use_plus=*/true);
  test::LoadUrl(web_state(), test_server_.GetURL(url_spec));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  GetFindInPageManager()->Find(@"iframe", FindInPageOptions::FindInPageSearch);

  EXPECT_TRUE(WaitForSelectedMatchAtIndex(0));

  GetFindInPageManager()->Find(nil, FindInPageOptions::FindInPageNext);

  EXPECT_TRUE(WaitForSelectedMatchAtIndex(1));
  EXPECT_EQ(@"iframe", delegate_.state()->query);

  GetFindInPageManager()->Find(nil, FindInPageOptions::FindInPagePrevious);

  EXPECT_TRUE(WaitForSelectedMatchAtIndex(0));
  EXPECT_EQ(@"iframe", delegate_.state()->query);
}

}  // namespace web
