// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
