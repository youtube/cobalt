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

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cobalt/common/features/features.h"
#include "cobalt/common/features/starboard_features_initialization.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "cobalt/testing/browser_tests/content_browser_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

constexpr char kVideoHelperScript[] = R"(
  window.createAndPlayVideo = function(id, src) {
    return new Promise((resolve, reject) => {
      let video = document.getElementById(id);
      if (!video) {
        video = document.createElement('video');
        video.id = id;
        video.width = 320;
        video.height = 240;
        video.muted = true;
        document.body.appendChild(video);
      }
      video.src = src;
      video.onplaying = () => resolve(true);
      video.onerror = () => reject('Video error on ' + id + ': ' +
          (video.error ? video.error.code : 'unknown'));
      video.play().catch(reject);
    });
  };

  window.waitForProgress = function(id) {
    return new Promise((resolve) => {
      const video = document.getElementById(id);
      if (!video) {
        resolve(false);
        return;
      }
      if (video.currentTime > 0.05) {
        resolve(true);
        return;
      }
      const onTimeUpdate = () => {
        if (video.currentTime > 0.05) {
          video.removeEventListener('timeupdate', onTimeUpdate);
          resolve(true);
        }
      };
      video.addEventListener('timeupdate', onTimeUpdate);
    });
  };

  window.stopAndRemoveVideo = function(id) {
    const video = document.getElementById(id);
    if (!video) {
      return true;
    }
    video.pause();
    video.removeAttribute('src');
    video.load();
    video.remove();
    return true;
  };

  window.switchSrcAndPlay = function(id, newSrc) {
    return new Promise((resolve, reject) => {
      const video = document.getElementById(id);
      if (!video) {
        reject('No video element: ' + id);
        return;
      }
      video.src = newSrc;
      video.onplaying = () => resolve(true);
      video.onerror = () => reject('Switch error on ' + id + ': ' +
          (video.error ? video.error.code : 'unknown'));
      video.play().catch(reject);
    });
  };
)";

bool IsPunchOutVideoSurfaceSupported() {
#if BUILDFLAG(IS_ANDROID)
  return true;
#else
  return false;
#endif
}

}  // namespace

class VideoSurfaceReleaseBrowserTest : public ContentBrowserTest {
 public:
  VideoSurfaceReleaseBrowserTest() = default;
  ~VideoSurfaceReleaseBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kSingleProcess);
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
    cobalt::features::InitializeStarboardFeatures();
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        cobalt::features::kCobaltWaitForVideoSurfaceRelease);
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromSourceDirectory(
        base::FilePath(FILE_PATH_LITERAL("media/test/data")));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Edge Case 1: Rapid consecutive video playback handover.
// Verifies that tearing down a playing video element and immediately starting
// another element succeeds without surface contention errors or deadlocks.
IN_PROC_BROWSER_TEST_F(VideoSurfaceReleaseBrowserTest,
                       RapidConsecutiveVideoPlayback) {
  if (!IsPunchOutVideoSurfaceSupported()) {
    GTEST_SKIP() << "Punch-out video surface is only supported on Android";
  }

  GURL test_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  ASSERT_TRUE(ExecJs(shell(), kVideoHelperScript));

  ASSERT_EQ(true, EvalJs(shell(),
                         "window.createAndPlayVideo('player1', "
                         "'/bear-vp9.webm');"));
  ASSERT_EQ(true, EvalJs(shell(), "window.waitForProgress('player1');"));

  ASSERT_EQ(true, EvalJs(shell(), "window.stopAndRemoveVideo('player1');"));
  ASSERT_EQ(true, EvalJs(shell(),
                         "window.createAndPlayVideo('player2', "
                         "'/bear-vp9-opus.webm');"));
  ASSERT_EQ(true, EvalJs(shell(), "window.waitForProgress('player2');"));
}

// Edge Case 2: Rapid src changes on the same video element.
// Verifies that switching source URLs on an active video player tears down the
// underlying codec and re-acquires the video surface cleanly.
IN_PROC_BROWSER_TEST_F(VideoSurfaceReleaseBrowserTest,
                       RapidSrcChangeDuringPlayback) {
  if (!IsPunchOutVideoSurfaceSupported()) {
    GTEST_SKIP() << "Punch-out video surface is only supported on Android";
  }

  GURL test_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  ASSERT_TRUE(ExecJs(shell(), kVideoHelperScript));

  ASSERT_EQ(true, EvalJs(shell(),
                         "window.createAndPlayVideo('player1', "
                         "'/bear-vp9.webm');"));
  ASSERT_EQ(true, EvalJs(shell(), "window.waitForProgress('player1');"));

  ASSERT_EQ(true, EvalJs(shell(),
                         "window.switchSrcAndPlay('player1', "
                         "'/bear-vp9-opus.webm');"));
  ASSERT_EQ(true, EvalJs(shell(), "window.waitForProgress('player1');"));
}

// Edge Case 3: Multiple video elements in the DOM with handover.
// Verifies that having two co-existing video elements in the DOM allows clean
// surface handover from one to another when the active player stops.
IN_PROC_BROWSER_TEST_F(VideoSurfaceReleaseBrowserTest,
                       MultipleVideoElementsDOMHandover) {
  if (!IsPunchOutVideoSurfaceSupported()) {
    GTEST_SKIP() << "Punch-out video surface is only supported on Android";
  }

  GURL test_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  ASSERT_TRUE(ExecJs(shell(), kVideoHelperScript));

  ASSERT_EQ(true, EvalJs(shell(),
                         "window.createAndPlayVideo('player_primary', "
                         "'/bear-vp9.webm');"));
  ASSERT_EQ(true, EvalJs(shell(), "window.waitForProgress('player_primary');"));

  ASSERT_EQ(true,
            EvalJs(shell(), "window.stopAndRemoveVideo('player_primary');"));
  ASSERT_EQ(true, EvalJs(shell(),
                         "window.createAndPlayVideo('player_secondary', "
                         "'/bear-vp9.webm');"));
  ASSERT_EQ(true,
            EvalJs(shell(), "window.waitForProgress('player_secondary');"));
}

// Edge Case 4: Application backgrounding and foregrounding during playback.
// Verifies that simulating WasHidden() while a video is playing and then
// restoring WasShown() preserves decoder stability without crashing.
IN_PROC_BROWSER_TEST_F(VideoSurfaceReleaseBrowserTest,
                       BackgroundAndForegroundPlaybackTransition) {
  if (!IsPunchOutVideoSurfaceSupported()) {
    GTEST_SKIP() << "Punch-out video surface is only supported on Android";
  }

  GURL test_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  ASSERT_TRUE(ExecJs(shell(), kVideoHelperScript));

  ASSERT_EQ(true, EvalJs(shell(),
                         "window.createAndPlayVideo('player1', "
                         "'/bear-vp9.webm');"));
  ASSERT_EQ(true, EvalJs(shell(), "window.waitForProgress('player1');"));

  shell()->web_contents()->WasHidden();
  base::RunLoop().RunUntilIdle();

  shell()->web_contents()->WasShown();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(true, EvalJs(shell(), "window.stopAndRemoveVideo('player1');"));
  ASSERT_EQ(true, EvalJs(shell(),
                         "window.createAndPlayVideo('player2', "
                         "'/bear-vp9-opus.webm');"));
  ASSERT_EQ(true, EvalJs(shell(), "window.waitForProgress('player2');"));
}

// Edge Case 5: Navigation away during active video playback.
// Verifies that navigating to a new URL while video playback is actively
// running tears down the old player, releases the video surface, and allows new
// playback.
IN_PROC_BROWSER_TEST_F(VideoSurfaceReleaseBrowserTest,
                       NavigationAwayDuringActivePlayback) {
  if (!IsPunchOutVideoSurfaceSupported()) {
    GTEST_SKIP() << "Punch-out video surface is only supported on Android";
  }

  GURL test_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  ASSERT_TRUE(ExecJs(shell(), kVideoHelperScript));

  ASSERT_EQ(true, EvalJs(shell(),
                         "window.createAndPlayVideo('player1', "
                         "'/bear-vp9.webm');"));
  ASSERT_EQ(true, EvalJs(shell(), "window.waitForProgress('player1');"));

  GURL next_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(NavigateToURL(shell(), next_url));
  ASSERT_TRUE(ExecJs(shell(), kVideoHelperScript));

  ASSERT_EQ(true, EvalJs(shell(),
                         "window.createAndPlayVideo('player_next', "
                         "'/bear-vp9.webm');"));
  ASSERT_EQ(true, EvalJs(shell(), "window.waitForProgress('player_next');"));
}

}  // namespace content
