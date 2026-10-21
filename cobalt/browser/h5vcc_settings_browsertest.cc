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

#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {

// The H5VCC media settings take effect only with Starboard media.
#if BUILDFLAG(USE_STARBOARD_MEDIA)
namespace {

// Evaluates to true if the page has the audio and video track APIs, which it
// uses to switch between SourceBuffers.
constexpr char kHasTrackApis[] =
    "['AudioTrack', 'AudioTrackList', 'VideoTrack', 'VideoTrackList'].every("
    "    name => name in window) &&"
    "['AudioTrack', 'VideoTrack'].every("
    "    name => 'sourceBuffer' in window[name].prototype) &&"
    "['audioTracks', 'videoTracks'].every("
    "    name => name in HTMLMediaElement.prototype &&"
    "            name in SourceBuffer.prototype)";

}  // namespace

using H5vccSettingsBrowserTest = content::ContentBrowserTest;

IN_PROC_BROWSER_TEST_F(H5vccSettingsBrowserTest,
                       MultiSourceBufferSwitchingExposesTrackApis) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(),
                            embedded_test_server()->GetURL("/title1.html")));
  EXPECT_FALSE(
      content::EvalJs(shell()->web_contents(), kHasTrackApis).ExtractBool());

  // The setting exposes the track APIs on the current page.
  EXPECT_TRUE(content::EvalJs(
                  shell()->web_contents(),
                  base::StrCat({"h5vcc.settings.set("
                                "'Media.EnableMultiSourceBufferSwitching', 1)"
                                ".then(() => ",
                                kHasTrackApis, ")"}))
                  .ExtractBool());

  // And on pages loaded afterwards.
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(),
                            embedded_test_server()->GetURL("/title2.html")));
  EXPECT_TRUE(
      content::EvalJs(shell()->web_contents(), kHasTrackApis).ExtractBool());
}
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

}  // namespace cobalt
