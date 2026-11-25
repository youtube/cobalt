// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/lifecycle/cobalt_lifecycle_manager.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "cobalt/testing/browser_tests/content_browser_test_utils.h"
#include "cobalt/testing/browser_tests/content_browsertests_jni_headers/FreezeStorageFlushTestHelper_jni.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

namespace {

class TestFreezeAckWaiter : public CobaltLifecycleManagerObserver {
 public:
  explicit TestFreezeAckWaiter(content::WebContents* web_contents)
      : web_contents_(web_contents) {
    CobaltLifecycleManager::GetInstance()->AddObserver(this);
  }

  ~TestFreezeAckWaiter() override {
    CobaltLifecycleManager::GetInstance()->RemoveObserver(this);
  }

  void Wait() {
    if (!frozen_) {
      run_loop_.Run();
    }
  }

  // CobaltLifecycleManagerObserver:
  void OnAllFramesVisible(content::WebContents* web_contents) override {}

  void OnAllFramesFrozen(content::WebContents* web_contents) override {
    if (web_contents == web_contents_) {
      frozen_ = true;
      run_loop_.Quit();
    }
  }

 private:
  raw_ptr<content::WebContents> web_contents_;
  bool frozen_ = false;
  base::RunLoop run_loop_;
};

}  // namespace

class FreezeStorageFlushAndroidBrowserTest
    : public content::ContentBrowserTest {
 public:
  FreezeStorageFlushAndroidBrowserTest() = default;
  ~FreezeStorageFlushAndroidBrowserTest() override = default;

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

class FreezeStorageFlushEnabledAndroidBrowserTest
    : public FreezeStorageFlushAndroidBrowserTest {
 public:
  FreezeStorageFlushEnabledAndroidBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kCobaltFlushStorageOnFreeze);
  }
  ~FreezeStorageFlushEnabledAndroidBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(FreezeStorageFlushAndroidBrowserTest,
                       FeatureDisabledByDefault) {
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(features::kCobaltFlushStorageOnFreeze));
  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_FALSE(
      Java_FreezeStorageFlushTestHelper_isFlushStorageOnFreezeEnabled(env));
}

IN_PROC_BROWSER_TEST_F(FreezeStorageFlushEnabledAndroidBrowserTest,
                       FeatureEnabled_JniReturnsTrue) {
  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_TRUE(
      Java_FreezeStorageFlushTestHelper_isFlushStorageOnFreezeEnabled(env));
}

IN_PROC_BROWSER_TEST_F(FreezeStorageFlushEnabledAndroidBrowserTest,
                       FreezeAndFlush_NullWebContents_DoesNotCrash) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FreezeStorageFlushTestHelper_freezeAndFlushCookiesAndLocalStorage(
      env, nullptr);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(FreezeStorageFlushEnabledAndroidBrowserTest,
                       FreezeAndFlush_VisibleWebContents_DoesNotCrash) {
  ASSERT_TRUE(content::NavigateToURL(
      shell(), embedded_test_server()->GetURL("/title1.html")));
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FreezeStorageFlushTestHelper_freezeAndFlushCookiesAndLocalStorage(
      env, shell()->web_contents()->GetJavaWebContents());
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(FreezeStorageFlushEnabledAndroidBrowserTest,
                       FreezeAndFlush_HiddenWebContents_DoesNotCrash) {
  ASSERT_TRUE(content::NavigateToURL(
      shell(), embedded_test_server()->GetURL("/title1.html")));
  ASSERT_TRUE(content::ExecJs(shell()->web_contents(),
                              "document.cookie = 'grace_cookie=1; path=/';"
                              "localStorage.setItem('grace_key', '1');"
                              "document.addEventListener('freeze', () => {"
                              "  document.cookie = 'freeze_cookie=1; path=/';"
                              "  localStorage.setItem('freeze_key', '1');"
                              "});"));

  shell()->web_contents()->WasHidden();
  TestFreezeAckWaiter waiter(shell()->web_contents());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FreezeStorageFlushTestHelper_freezeAndFlushCookiesAndLocalStorage(
      env, shell()->web_contents()->GetJavaWebContents());
  waiter.Wait();
  base::RunLoop().RunUntilIdle();

  shell()->web_contents()->SetPageFrozen(false);
  shell()->web_contents()->WasShown();

  EXPECT_EQ(true, content::EvalJs(shell()->web_contents(),
                                  "document.cookie.includes('grace_cookie=1')"
                                  " && localStorage.getItem('grace_key') === "
                                  "'1'"));
  EXPECT_EQ(true, content::EvalJs(shell()->web_contents(),
                                  "document.cookie.includes('freeze_cookie=1')"
                                  " && localStorage.getItem('freeze_key') === "
                                  "'1'"));
}

IN_PROC_BROWSER_TEST_F(FreezeStorageFlushEnabledAndroidBrowserTest,
                       FreezeAndFlush_WebContentsDestroyedBeforeAck_NoCrash) {
  content::TestShell* second_shell = CreateBrowser();
  ASSERT_TRUE(content::NavigateToURL(
      second_shell, embedded_test_server()->GetURL("/title1.html")));
  second_shell->web_contents()->WasHidden();

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FreezeStorageFlushTestHelper_freezeAndFlushCookiesAndLocalStorage(
      env, second_shell->web_contents()->GetJavaWebContents());
  second_shell->Close();
  base::RunLoop().RunUntilIdle();
}

}  // namespace cobalt
