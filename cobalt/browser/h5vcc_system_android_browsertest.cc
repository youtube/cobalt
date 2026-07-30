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

#include "base/android/android_info.h"
#include "base/android/jni_android.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "cobalt/testing/browser_tests/content_browsertests_jni_headers/MockProcessExitReasonHelper_jni.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {

namespace {

// Android ApplicationExitInfo reason constants (from
// android.app.ApplicationExitInfo). ApplicationExitInfo.REASON_EXIT_SELF = 1.
constexpr int kAppExitInfoReasonExitSelf = 1;
// ApplicationExitInfo.REASON_LOW_MEMORY = 3.
constexpr int kAppExitInfoReasonLowMemory = 3;
// ApplicationExitInfo.REASON_CRASH = 4.
constexpr int kAppExitInfoReasonCrash = 4;
// ApplicationExitInfo.REASON_USER_REQUESTED = 10.
constexpr int kAppExitInfoReasonUserRequested = 10;

}  // namespace

class H5vccSystemAndroidBrowserTest : public content::ContentBrowserTest {
 public:
  H5vccSystemAndroidBrowserTest() = default;
  ~H5vccSystemAndroidBrowserTest() override = default;

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_MockProcessExitReasonHelper_resetForTesting(env);
  }

  void TearDownOnMainThread() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_MockProcessExitReasonHelper_resetForTesting(env);
    content::ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  void SimulatePriorSessionExit(int exit_reason) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_MockProcessExitReasonHelper_setMockExitReasonForTesting(env,
                                                                 exit_reason);
  }

  void SimulateEmptyHistoricalExitReasons() {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_MockProcessExitReasonHelper_setMockEmptyExitReasonsForTesting(env);
  }

  void SimulateNullHistoricalExitReasons() {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_MockProcessExitReasonHelper_setMockNullExitReasonsForTesting(env);
  }

  void SimulateExceptionInHistoricalExitReasons() {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_MockProcessExitReasonHelper_setMockThrowsExceptionForTesting(env);
  }

  bool QueryWasLowMemoryKilledFromJs() {
    return content::EvalJs(
               shell()->web_contents(),
               "(async () => await window.h5vcc.system.wasLowMemoryKilled())()")
        .ExtractBool();
  }
};

IN_PROC_BROWSER_TEST_F(H5vccSystemAndroidBrowserTest,
                       VerifyDefaultExitReasonResolvesFalse) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_R) {
    GTEST_SKIP()
        << "Historical process exit reasons are only available on Android R+.";
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // Without any simulated exit reason, wasLowMemoryKilled() should resolve to
  // false.
  EXPECT_FALSE(QueryWasLowMemoryKilledFromJs());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemAndroidBrowserTest,
                       VerifyNonLmkExitReasonResolvesFalse) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_R) {
    GTEST_SKIP()
        << "Historical process exit reasons are only available on Android R+.";
  }

  // Simulate a prior session terminated due to REASON_EXIT_SELF (reason 1).
  SimulatePriorSessionExit(kAppExitInfoReasonExitSelf);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  EXPECT_FALSE(QueryWasLowMemoryKilledFromJs());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemAndroidBrowserTest,
                       VerifyLowMemoryKillExitReasonResolvesTrue) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_R) {
    GTEST_SKIP()
        << "Historical process exit reasons are only available on Android R+.";
  }

  // Simulate a prior session terminated due to REASON_LOW_MEMORY (reason 3).
  SimulatePriorSessionExit(kAppExitInfoReasonLowMemory);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  EXPECT_TRUE(QueryWasLowMemoryKilledFromJs());

  // Subsequent call resolves to the same true value.
  EXPECT_TRUE(QueryWasLowMemoryKilledFromJs());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemAndroidBrowserTest,
                       VerifyUserRequestedExitReasonResolvesFalse) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_R) {
    GTEST_SKIP()
        << "Historical process exit reasons are only available on Android R+.";
  }

  // Simulate a prior session terminated due to REASON_USER_REQUESTED (reason
  // 10).
  SimulatePriorSessionExit(kAppExitInfoReasonUserRequested);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  EXPECT_FALSE(QueryWasLowMemoryKilledFromJs());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemAndroidBrowserTest,
                       VerifyCrashExitReasonResolvesFalse) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_R) {
    GTEST_SKIP()
        << "Historical process exit reasons are only available on Android R+.";
  }

  // Simulate a prior session terminated due to REASON_CRASH (reason 4).
  SimulatePriorSessionExit(kAppExitInfoReasonCrash);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  EXPECT_FALSE(QueryWasLowMemoryKilledFromJs());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemAndroidBrowserTest,
                       VerifyDynamicExitReasonTransition) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_R) {
    GTEST_SKIP()
        << "Historical process exit reasons are only available on Android R+.";
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // Initial state without simulation: false.
  EXPECT_FALSE(QueryWasLowMemoryKilledFromJs());

  // Transition to LMK: true.
  SimulatePriorSessionExit(kAppExitInfoReasonLowMemory);
  EXPECT_TRUE(QueryWasLowMemoryKilledFromJs());

  // Transition to non-LMK (e.g. USER_REQUESTED): false.
  SimulatePriorSessionExit(kAppExitInfoReasonUserRequested);
  EXPECT_FALSE(QueryWasLowMemoryKilledFromJs());

  // Reset: false.
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MockProcessExitReasonHelper_resetForTesting(env);
  EXPECT_FALSE(QueryWasLowMemoryKilledFromJs());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemAndroidBrowserTest,
                       VerifyEmptyHistoricalExitReasonsResolvesFalse) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_R) {
    GTEST_SKIP()
        << "Historical process exit reasons are only available on Android R+.";
  }

  // Simulate empty list returned from ActivityManager.
  SimulateEmptyHistoricalExitReasons();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  EXPECT_FALSE(QueryWasLowMemoryKilledFromJs());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemAndroidBrowserTest,
                       VerifyNullHistoricalExitReasonsResolvesFalse) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_R) {
    GTEST_SKIP()
        << "Historical process exit reasons are only available on Android R+.";
  }

  // Simulate null list returned from ActivityManager (OEM edge case).
  SimulateNullHistoricalExitReasons();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  EXPECT_FALSE(QueryWasLowMemoryKilledFromJs());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemAndroidBrowserTest,
                       VerifyExceptionInHistoricalExitReasonsResolvesFalse) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_R) {
    GTEST_SKIP()
        << "Historical process exit reasons are only available on Android R+.";
  }

  // Simulate SecurityException/Binder error thrown by ActivityManager IPC.
  SimulateExceptionInHistoricalExitReasons();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // Should gracefully handle the exception without crashing and resolve false.
  EXPECT_FALSE(QueryWasLowMemoryKilledFromJs());
}

}  // namespace cobalt
