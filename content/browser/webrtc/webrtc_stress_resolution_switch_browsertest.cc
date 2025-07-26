// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {
#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(MEMORY_SANITIZER)
static const int kTestDurationSecs = 2;
static const int kNumPeerConnections = 3;
#else
static const int kTestDurationSecs = 10;
static const int kNumPeerConnections = 5;
#endif
}  // namespace

#if BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_WebRtcStressResolutionSwitchBrowserTest \
  DISABLED_WebRtcStressResolutionSwitchBrowserTest
#else
#define MAYBE_WebRtcStressResolutionSwitchBrowserTest \
  WebRtcStressResolutionSwitchBrowserTest
#endif

class MAYBE_WebRtcStressResolutionSwitchBrowserTest
    : public WebRtcContentBrowserTestBase {
 public:
  MAYBE_WebRtcStressResolutionSwitchBrowserTest() {}
  ~MAYBE_WebRtcStressResolutionSwitchBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
  }

 protected:
  void MakeTypicalPeerConnectionCall(const std::string& javascript) {
    MakeTypicalCall(javascript, "/media/peerconnection-resolution-switch.html");
  }
};

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcStressResolutionSwitchBrowserTest,
                       MANUAL_SurvivesPeerConnectionResolutionSwitching) {
  // Args: runtimeSeconds, numPeerConnections, iterationDelayMillis.
  MakeTypicalPeerConnectionCall(base::StringPrintf(
      "startTest(%d, %d, 300);", kTestDurationSecs, kNumPeerConnections));
}

}  // namespace content
