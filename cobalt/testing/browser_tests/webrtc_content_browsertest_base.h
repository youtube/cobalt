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

#ifndef COBALT_TESTING_BROWSER_TESTS_WEBRTC_CONTENT_BROWSERTEST_BASE_H_
#define COBALT_TESTING_BROWSER_TESTS_WEBRTC_CONTENT_BROWSERTEST_BASE_H_

#include "cobalt/testing/browser_tests/content_browser_test.h"

namespace base {
class CommandLine;
}

namespace content {

// Contains stuff WebRTC browsertests have in common.
class WebRtcContentBrowserTestBase : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUp() override;
  void TearDown() override;

 protected:
  // Helper function to append "--use-fake-ui-for-media-stream".
  void AppendUseFakeUIForMediaStreamFlag();

  // Execute a typical javascript call after having started the webserver.
  void MakeTypicalCall(const std::string& javascript,
                       const std::string& html_file);

  // Generates javascript code for a getUserMedia call.
  std::string GenerateGetUserMediaCall(const char* function_name,
                                       int min_width,
                                       int max_width,
                                       int min_height,
                                       int max_height,
                                       int min_frame_rate,
                                       int max_frame_rate) const;

  // Synchronously checks if the system has audio output devices.
  static bool HasAudioOutputDevices();
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_WEBRTC_CONTENT_BROWSERTEST_BASE_H_
