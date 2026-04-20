// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/testing/browser_tests/webrtc_content_browsertest_base.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test_utils.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/audio_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "media/audio/audio_system.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

void WebRtcContentBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Loopback interface is the non-default local address. They should only be in
  // the candidate list if the media permission is granted.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowLoopbackInPeerConnection);
}

void WebRtcContentBrowserTestBase::SetUp() {
  // We need pixel output when we dig pixels out of video tags for verification.
  EnablePixelOutput();
  ContentBrowserTest::SetUp();
  ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeDeviceForMediaStream));
}

void WebRtcContentBrowserTestBase::TearDown() {
  ContentBrowserTest::TearDown();
}

void WebRtcContentBrowserTestBase::AppendUseFakeUIForMediaStreamFlag() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kUseFakeUIForMediaStream);
}

void WebRtcContentBrowserTestBase::MakeTypicalCall(
    const std::string& javascript,
    const std::string& html_file) {
  if (!embedded_test_server()->Started()) {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL url(embedded_test_server()->GetURL(html_file));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), javascript));
}

std::string WebRtcContentBrowserTestBase::GenerateGetUserMediaCall(
    const char* function_name,
    int min_width,
    int max_width,
    int min_height,
    int max_height,
    int min_frame_rate,
    int max_frame_rate) const {
  return base::StringPrintf(
      "%s({video: {mandatory: {minWidth: %d, maxWidth: %d, "
      "minHeight: %d, maxHeight: %d, minFrameRate: %d, maxFrameRate: %d}, "
      "optional: []}});",
      function_name, min_width, max_width, min_height, max_height,
      min_frame_rate, max_frame_rate);
}

// static
bool WebRtcContentBrowserTestBase::HasAudioOutputDevices() {
  bool has_devices = false;
  base::RunLoop run_loop;
  auto audio_system = CreateAudioSystemForAudioService();
  audio_system->HasOutputDevices(base::BindOnce(
      [](base::OnceClosure finished_callback, bool* result, bool received) {
        *result = received;
        std::move(finished_callback).Run();
      },
      run_loop.QuitClosure(), &has_devices));
  run_loop.Run();
  return has_devices;
}

}  // namespace content
