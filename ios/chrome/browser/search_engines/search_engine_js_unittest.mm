// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/search_engines/search_engine_java_script_feature.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using web::test::TapWebViewElementWithId;
using web::test::SelectWebViewElementWithId;

namespace {
// This is for cases where no message should be sent back from Js.
constexpr base::TimeDelta kWaitForJsNotReturnTimeout = base::Milliseconds(500);

NSString* kSearchableForm =
    @"<html>"
    @"  <form id='f' action='index.html' method='get'>"
    @"    <input type='search' name='q'>"
    @"    <input type='hidden' name='hidden' value='i1'>"
    @"    <input type='hidden' name='disabled' value='i2' disabled>"
    @"    <input id='r1' type='radio' name='radio' value='r1' checked>"
    @"    <input id='r2' type='radio' name='radio' value='r2'>"
    @"    <input id='c1' type='checkbox' name='check' value='c1'>"
    @"    <input id='c2' type='checkbox' name='check' value='c2' checked>"
    @"    <select name='select' name='select'>"
    @"      <option id='op1' value='op1'>op1</option>"
    @"      <option id='op2' value='op2' selected>op2</option>"
    @"      <option id='op3' value='op3'>op3</option>"
    @"    </select>"
    @"    <input id='btn1' type='submit' name='btn1' value='b1'>"
    @"    <button id='btn2' name='btn2' value='b2'>"
    @"  </form>"
    @"  <input type='hidden' form='f' name='outside form' value='i3'>"
    @"</html>";
}

// Test fixture for search_engine.js testing.
class SearchEngineJsTest : public PlatformTest,
                           public SearchEngineJavaScriptFeatureDelegate {
 public:
  SearchEngineJsTest(const SearchEngineJsTest&) = delete;
  SearchEngineJsTest& operator=(const SearchEngineJsTest&) = delete;

 protected:
  SearchEngineJsTest() : web_client_(std::make_unique<ChromeWebClient>()) {
    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }

  // Stores paramaeters passed to `SetSearchableUrl`.
  struct ReceivedSearchableUrl {
    web::WebState* web_state;
    GURL searchable_url;
  };

  // Stores paramaeters passed to `AddTemplateURLByOSDD`.
  struct ReceivedTemplateUrlByOsdd {
    web::WebState* web_state;
    GURL template_page_url;
    GURL osdd_url;
  };

  void SetUp() override {
    PlatformTest::SetUp();

    // Reset the last received states.
    last_received_searchable_url_ = ReceivedSearchableUrl();
    last_received_template_url_by_osdd_ = ReceivedTemplateUrlByOsdd();

    // Load an empty page in order to fully load the WebClient so that the
    // delegate can be overriden.
    web::test::LoadHtml(@"<html></html>", web_state());
    SearchEngineJavaScriptFeature::GetInstance()->SetDelegate(this);
  }

  void SetSearchableUrl(web::WebState* web_state, GURL url) override {
    ReceivedSearchableUrl state;
    state.web_state = web_state;
    state.searchable_url = url;
    last_received_searchable_url_ = state;
  }

  void AddTemplateURLByOSDD(web::WebState* web_state,
                            const GURL& page_url,
                            const GURL& osdd_url) override {
    ReceivedTemplateUrlByOsdd state;
    state.web_state = web_state;
    state.template_page_url = page_url;
    state.osdd_url = osdd_url;
    last_received_template_url_by_osdd_ = state;
  }

  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  // Details about the last received `SetSearchableUrl` call.
  ReceivedSearchableUrl last_received_searchable_url_;
  // Details about the last received `AddTemplateURLByOSDD` call.
  ReceivedTemplateUrlByOsdd last_received_template_url_by_osdd_;
};

// Tests that if a OSDD <link> is found in page, __gCrWeb.searchEngine will
// send a message containing the page's URL and OSDD's URL.
TEST_F(SearchEngineJsTest, TestGetOpenSearchDescriptionDocumentUrlSucceed) {
  web::test::LoadHtml(
      @"<html><link rel='search' type='application/opensearchdescription+xml' "
      @"title='Chromium Code Search' "
      @"href='//cs.chromium.org/codesearch/first_opensearch.xml' />"
      @"<link rel='search' type='application/opensearchdescription+xml' "
      @"title='Chromium Code Search 2' "
      @"href='//cs.chromium.org/codesearch/second_opensearch.xml' />"
      @"<link href='/favicon.ico' rel='shortcut icon' "
      @"type='image/x-icon'></html>",
      GURL("https://cs.chromium.org"), web_state());
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !!last_received_template_url_by_osdd_.web_state;
  }));

  EXPECT_EQ("https://cs.chromium.org/",
            last_received_template_url_by_osdd_.template_page_url.spec());
  EXPECT_EQ("https://cs.chromium.org/codesearch/first_opensearch.xml",
            last_received_template_url_by_osdd_.osdd_url.spec());
}

// Tests that if no OSDD <link> is found in page, __gCrWeb.searchEngine will
// not send a message about OSDD.
TEST_F(SearchEngineJsTest, TestGetOpenSearchDescriptionDocumentUrlFail) {
  web::test::LoadHtml(@"<html><link href='/favicon.ico' rel='shortcut icon' "
                      @"type='image/x-icon'></html>",
                      GURL("https://cs.chromium.org"), web_state());
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !!last_received_template_url_by_osdd_.web_state;
  }));
}

// Tests that __gCrWeb.searchEngine generates and sends back a searchable
// URL when <form> is submitted by click on the first button in <form>.
TEST_F(SearchEngineJsTest,
       GenerateSearchableUrlForValidFormSubmittedByFirstButton) {
  web::test::LoadHtml(kSearchableForm, GURL("https://abc.com"), web_state());
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn1"));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !!last_received_searchable_url_.web_state;
  }));
  EXPECT_EQ(
      "https://abc.com/"
      "index.html?q={searchTerms}&hidden=i1&radio=r1&check=c2&select=op2&btn1="
      "b1&outside+form=i3",
      last_received_searchable_url_.searchable_url);
}

// Tests that __gCrWeb.searchEngine generates and sends back a searchable
// URL when <form> is submitted by click on a non-first button in <form>.
TEST_F(SearchEngineJsTest,
       GenerateSearchableUrlForValidFormSubmittedByNonFirstButton) {
  web::test::LoadHtml(kSearchableForm, GURL("https://abc.com"), web_state());
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn2"));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !!last_received_searchable_url_.web_state;
  }));
  EXPECT_EQ(
      "https://abc.com/"
      "index.html?q={searchTerms}&hidden=i1&radio=r1&check=c2&select=op2&btn2="
      "b2&outside+form=i3",
      last_received_searchable_url_.searchable_url);
}

// Tests that __gCrWeb.searchEngine doesn't generate and send back a searchable
// URL for <form> with <textarea>.
TEST_F(SearchEngineJsTest, GenerateSearchableUrlForInvalidFormWithTextArea) {
  web::test::LoadHtml(
      @"<html><form><input type='search' name='q'><textarea "
      @"name='a'></textarea><input id='btn' type='submit'></form></html>",
      web_state());
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !!last_received_searchable_url_.web_state;
  }));
}

// Tests that __gCrWeb.searchEngine doesn't generate and send back a searchable
// URL for <form> with <input type="password">.
TEST_F(SearchEngineJsTest,
       GenerateSearchableUrlForInvalidFormWithInputPassword) {
  web::test::LoadHtml(
      @"<html><form><input type='search' name='q'><input "
      @"type='password' name='a'><input id='btn' type='submit'></form></html>",
      web_state());
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !!last_received_searchable_url_.web_state;
  }));
}

// Tests that __gCrWeb.searchEngine doesn't generate and send back a searchable
// URL for <form> with <input type="file">.
TEST_F(SearchEngineJsTest, GenerateSearchableUrlForInvalidFormWithInputFile) {
  web::test::LoadHtml(
      @"<html><form><input type='search' name='q'><input "
      @"type='file' name='a'><input id='btn' type='submit'</form></html>",
      web_state());
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !!last_received_searchable_url_.web_state;
  }));
}

// Tests that __gCrWeb.searchEngine doesn't generate and send back a searchable
// URL for <form> without <input type="email|search|tel|text|url|number">.
TEST_F(SearchEngineJsTest, GenerateSearchableUrlForInvalidFormWithNoTextInput) {
  web::test::LoadHtml(@"<html><form id='f'><input type='hidden' name='q' "
                      @"value='v'><input id='btn' type='submit'></form></html>",
                      web_state());
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !!last_received_searchable_url_.web_state;
  }));
}

// Tests that __gCrWeb.searchEngine doesn't generate and send back a searchable
// URL for <form> with more than 1 <input
// type="email|search|tel|text|url|number">.
TEST_F(SearchEngineJsTest,
       GenerateSearchableUrlForInvalidFormWithMoreThanOneTextInput) {
  web::test::LoadHtml(
      @"<html><form id='f'><input type='search' name='q'><input "
      @"type='text' name='q2'><input id='btn' type='submit'></form></html>",
      web_state());
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !!last_received_searchable_url_.web_state;
  }));
}

// Tests that __gCrWeb.searchEngine doesn't generate and send back a searchable
// URL for <form> with <input type='radio'> in non-default state.
TEST_F(SearchEngineJsTest,
       GenerateSearchableUrlForInvalidFormWithNonDefaultRadio) {
  web::test::LoadHtml(kSearchableForm, web_state());
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "r2"));
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn1"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !!last_received_searchable_url_.web_state;
  }));
}

// Tests that __gCrWeb.searchEngine doesn't generate and send back a searchable
// URL for <form> with <input type='checkbox'> in non-default state.
TEST_F(SearchEngineJsTest,
       GenerateSearchableUrlForInvalidFormWithNonDefaultCheckbox) {
  web::test::LoadHtml(kSearchableForm, web_state());
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "c1"));
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn1"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !!last_received_searchable_url_.web_state;
  }));
}

// Tests that __gCrWeb.searchEngine.generateSearchableUrl returns undefined
// for <form> with <select> in non-default state.
TEST_F(SearchEngineJsTest,
       GenerateSearchableUrlForInvalidFormWithNonDefaultSelect) {
  web::test::LoadHtml(kSearchableForm, web_state());
  ASSERT_TRUE(SelectWebViewElementWithId(web_state(), "op1"));
  ASSERT_TRUE(TapWebViewElementWithId(web_state(), "btn1"));
  ASSERT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !!last_received_searchable_url_.web_state;
  }));
}
