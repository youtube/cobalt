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

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "cobalt/browser/metrics/cobalt_stability_metrics_helper.h"
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
// ApplicationExitInfo.REASON_USER_REQUESTED = 10.
constexpr int kAppExitInfoReasonUserRequested = 10;

void CleanUpMetricsDir() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath base_dir;
  if (base::PathService::Get(base::DIR_ANDROID_APP_DATA, &base_dir)) {
    base::DeletePathRecursively(
        base_dir.AppendASCII(kBrowserStabilityMetricsName));
  }
}

}  // namespace

class H5vccSystemAndroidBrowserTest : public content::ContentBrowserTest {
 public:
  H5vccSystemAndroidBrowserTest() = default;
  ~H5vccSystemAndroidBrowserTest() override = default;

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    CleanUpMetricsDir();
    base::StatisticsRecorder::ForgetHistogramForTesting(
        kSystemExitReasonHistogram);
    cobalt::ResetPriorSessionExitReasonsForTesting();
  }

  void TearDownOnMainThread() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_MockProcessExitReasonHelper_resetForTesting(env);
    base::StatisticsRecorder::ForgetHistogramForTesting(
        kSystemExitReasonHistogram);
    cobalt::ResetPriorSessionExitReasonsForTesting();
    CleanUpMetricsDir();
    content::ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  void CreatePriorSessionPma(base::ProcessId pid) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath base_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &base_dir));
    base::FilePath metrics_dir =
        base_dir.AppendASCII(kBrowserStabilityMetricsName);
    ASSERT_TRUE(base::CreateDirectory(metrics_dir));

    base::FilePath pma_path =
        base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
            metrics_dir, kBrowserStabilityMetricsName, base::Time::Now(), pid);
    ASSERT_TRUE(base::WriteFile(pma_path, ""));
  }

  void SimulatePriorSessionExit(base::ProcessId pid, int exit_reason) {
    CreatePriorSessionPma(pid);

    JNIEnv* env = base::android::AttachCurrentThread();
    Java_MockProcessExitReasonHelper_setMockExitReasonForTesting(
        env, static_cast<jint>(pid), exit_reason);

    base::ScopedAllowBlockingForTesting allow_blocking;
    cobalt::RecordPriorSessionExitReasons();
    cobalt::OnPriorSessionExitReasonsRecorded();
  }

  bool QueryWasLowMemoryKilledFromJs() {
    return content::EvalJs(
               shell()->web_contents(),
               "(async () => await window.h5vcc.system.wasLowMemoryKilled())()")
        .ExtractBool();
  }
};

IN_PROC_BROWSER_TEST_F(H5vccSystemAndroidBrowserTest,
                       VerifyNonLmkExitReasonResolvesFalse) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    GTEST_SKIP()
        << "Historical process exit reasons are only available on Android R+.";
  }

  // Simulate a clean initial run where exit reasons are recorded with no prior
  // LMK.
  cobalt::OnPriorSessionExitReasonsRecorded();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  EXPECT_FALSE(QueryWasLowMemoryKilledFromJs());

  // Reset and simulate a prior session terminated due to REASON_EXIT_SELF
  // (reason 1).
  cobalt::ResetPriorSessionExitReasonsForTesting();
  SimulatePriorSessionExit(/*pid=*/9991, kAppExitInfoReasonExitSelf);

  EXPECT_FALSE(QueryWasLowMemoryKilledFromJs());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemAndroidBrowserTest,
                       VerifyLowMemoryKillExitReasonResolvesTrue) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    GTEST_SKIP()
        << "Historical process exit reasons are only available on Android R+.";
  }

  // Simulate a prior session terminated due to REASON_LOW_MEMORY (reason 3).
  SimulatePriorSessionExit(/*pid=*/9992, kAppExitInfoReasonLowMemory);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  EXPECT_TRUE(QueryWasLowMemoryKilledFromJs());

  // Subsequent call resolves to the same true value.
  EXPECT_TRUE(QueryWasLowMemoryKilledFromJs());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemAndroidBrowserTest,
                       VerifyApiRequestDefersUntilExitReasonsRecorded) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    GTEST_SKIP()
        << "Historical process exit reasons are only available on Android R+.";
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // Reset exit reasons recorded state so that the API call is deferred.
  cobalt::ResetPriorSessionExitReasonsForTesting();

  // Call wasLowMemoryKilled from JS without awaiting, tracking resolution
  // state.
  EXPECT_TRUE(
      content::ExecJs(shell()->web_contents(),
                      "window.lmkResolved = false; "
                      "window.lmkValue = null; "
                      "window.h5vcc.system.wasLowMemoryKilled().then(v => { "
                      "  window.lmkResolved = true; "
                      "  window.lmkValue = v; "
                      "});"));

  // Verify that the promise has NOT resolved yet because UMA is not emitted.
  EXPECT_FALSE(content::EvalJs(shell()->web_contents(), "window.lmkResolved")
                   .ExtractBool());

  // Simulate prior session LMK exit reasons recorded.
  SimulatePriorSessionExit(/*pid=*/9993, kAppExitInfoReasonLowMemory);

  // Now the deferred promise should resolve to true.
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(),
                              "(async () => { "
                              "  while (!window.lmkResolved) { "
                              "    await new Promise(r => setTimeout(r, 10)); "
                              "  } "
                              "  return window.lmkValue; "
                              "})()")
                  .ExtractBool());

  // Subsequent call resolves to true.
  EXPECT_TRUE(QueryWasLowMemoryKilledFromJs());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemAndroidBrowserTest,
                       VerifyMultiplePriorSessionsWithOneLmkResolvesTrue) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    GTEST_SKIP()
        << "Historical process exit reasons are only available on Android R+.";
  }

  CreatePriorSessionPma(9994);
  CreatePriorSessionPma(9995);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MockProcessExitReasonHelper_setMockExitReasonForTesting(
      env, 9994, kAppExitInfoReasonExitSelf);
  Java_MockProcessExitReasonHelper_setMockExitReasonForTesting(
      env, 9995, kAppExitInfoReasonLowMemory);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    cobalt::RecordPriorSessionExitReasons();
    cobalt::OnPriorSessionExitReasonsRecorded();
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  EXPECT_TRUE(QueryWasLowMemoryKilledFromJs());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemAndroidBrowserTest,
                       VerifyMultiplePriorSessionsWithoutLmkResolvesFalse) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    GTEST_SKIP()
        << "Historical process exit reasons are only available on Android R+.";
  }

  CreatePriorSessionPma(9996);
  CreatePriorSessionPma(9997);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MockProcessExitReasonHelper_setMockExitReasonForTesting(
      env, 9996, kAppExitInfoReasonExitSelf);
  Java_MockProcessExitReasonHelper_setMockExitReasonForTesting(
      env, 9997, kAppExitInfoReasonUserRequested);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    cobalt::RecordPriorSessionExitReasons();
    cobalt::OnPriorSessionExitReasonsRecorded();
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  EXPECT_FALSE(QueryWasLowMemoryKilledFromJs());
}

}  // namespace cobalt
