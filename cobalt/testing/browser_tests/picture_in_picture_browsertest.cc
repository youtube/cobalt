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

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "cobalt/browser/cobalt_web_contents_observer.h"
#include "cobalt/common/features/starboard_features_initialization.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "cobalt/testing/browser_tests/content_browser_test_content_browser_client.h"
#include "cobalt/testing/browser_tests/content_browser_test_utils.h"
#include "cobalt/testing/browser_tests/gpu/shell_content_gpu_test_client.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

class TestVideoOverlayWindow : public VideoOverlayWindow {
 public:
  TestVideoOverlayWindow() = default;
  ~TestVideoOverlayWindow() override = default;

  bool IsActive() const override { return false; }
  void Close() override { visible_ = false; }
  void ShowInactive() override { visible_ = true; }
  void Hide() override { visible_ = false; }
  bool IsVisible() const override { return visible_; }
  gfx::Rect GetBounds() override { return gfx::Rect(size_); }
  void UpdateNaturalSize(const gfx::Size& natural_size) override {
    size_ = natural_size;
  }
  void SetPlaybackState(PlaybackState playback_state) override {}
  void SetPlayPauseButtonVisibility(bool is_visible) override {}
  void SetSkipAdButtonVisibility(bool is_visible) override {}
  void SetNextTrackButtonVisibility(bool is_visible) override {}
  void SetPreviousTrackButtonVisibility(bool is_visible) override {}
  void SetMicrophoneMuted(bool muted) override {}
  void SetCameraState(bool turned_on) override {}
  void SetToggleMicrophoneButtonVisibility(bool is_visible) override {}
  void SetToggleCameraButtonVisibility(bool is_visible) override {}
  void SetHangUpButtonVisibility(bool is_visible) override {}
  void SetNextSlideButtonVisibility(bool is_visible) override {}
  void SetPreviousSlideButtonVisibility(bool is_visible) override {}
  void SetMediaPosition(const media_session::MediaPosition&) override {}
  void SetSourceTitle(const std::u16string& source_title) override {}
  void SetFaviconImages(
      const std::vector<media_session::MediaImage>& images) override {}
  void SetSurfaceId(const viz::SurfaceId& surface_id) override {}

 private:
  bool visible_ = false;
  gfx::Size size_;
};

class TestContentBrowserClient : public ContentBrowserTestContentBrowserClient {
 public:
  std::unique_ptr<VideoOverlayWindow> CreateWindowForVideoPictureInPicture(
      VideoPictureInPictureWindowController* controller) override {
    return std::make_unique<TestVideoOverlayWindow>();
  }
};

class PictureInPictureBrowserTest : public ContentBrowserTest {
 public:
  PictureInPictureBrowserTest() {
    gpu_client_ = std::make_unique<ShellContentGpuTestClient>();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    // On Cobalt/Android, Picture-in-Picture (decode-to-texture=true) requires
    // querying GPU hardware display capabilities and video geometry services
    // (`VideoGeometrySetterService`). In multi-process mode inside test APKs,
    // out-of-process GPU child processes run in sandboxes that cannot bind
    // Cobalt's custom Mojo host receivers (`ChildThread::Get()` IPC failures).
    // Running in single-process mode keeps browser, renderer, and GPU threads
    // in shared memory so hardware video decoding and geometry updates work.
    command_line->AppendSwitch(switches::kSingleProcess);

    // Initialize Starboard features explicitly because generic test browser
    // delegates do not execute
    // `CobaltContentBrowserClient::PostEarlyInitialization()`.
    cobalt::features::InitializeStarboardFeatures();

    // When `--single-process` is appended inside `SetUpCommandLine()` on
    // Android, `ContentMainRunnerImpl::Initialize()` has already run earlier
    // during `JNI_OnLoad`, leaving `GetContentClient()->gpu()` as nullptr. We
    // attach our GPU test client directly so single-process Starboard video
    // rendering finds `VideoGeometrySetterService` and satisfies `DCHECK`.
    struct ContentClientLayoutHelper {
      virtual ~ContentClientLayoutHelper() = default;
      raw_ptr<ContentBrowserClient, DanglingUntriaged> browser_;
      raw_ptr<ContentGpuClient> gpu_;
      raw_ptr<ContentRendererClient> renderer_;
      raw_ptr<ContentUtilityClient> utility_;
    };
    auto* helper = reinterpret_cast<ContentClientLayoutHelper*>(
        content::GetContentClientForTesting());
    CHECK_EQ(helper->browser_.get(),
             content::GetContentClientForTesting()->browser());
    helper->gpu_ = gpu_client_.get();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    content_browser_client_ = std::make_unique<TestContentBrowserClient>();
    embedded_test_server()->ServeFilesFromSourceDirectory(
        base::FilePath(FILE_PATH_LITERAL("media/test/data")));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    content_browser_client_.reset();
    ContentBrowserTest::TearDownOnMainThread();
  }

 private:
  std::unique_ptr<TestContentBrowserClient> content_browser_client_;
  std::unique_ptr<ShellContentGpuTestClient> gpu_client_;
};

const char kPictureInPictureScript[] = R"(
  window.video = document.createElement('video');
  window.video.loop = true;
  document.body.appendChild(window.video);

  window.addPictureInPictureEventListeners = function() {
    window.video.addEventListener('enterpictureinpicture', _ => {
      document.title = 'enterpictureinpicture';
    });
    window.video.addEventListener('leavepictureinpicture', _ => {
      document.title = 'leavepictureinpicture';
    });
  };

  window.mseReadyPromise = new Promise(resolve => {
    const mediaSource = new MediaSource();
    mediaSource.addEventListener('sourceopen', async () => {
      try {
        const sourceBuffer = mediaSource.addSourceBuffer('video/webm; codecs="vp9"; decode-to-texture=true');
        const response = await fetch('/bear-vp9.webm');
        if (!response.ok) {
          throw new Error('Failed to load /bear-vp9.webm: ' + response.status);
        }
        const data = await response.arrayBuffer();
        sourceBuffer.addEventListener('updateend', () => {
          if (!sourceBuffer.updating && mediaSource.readyState === 'open') {
            mediaSource.endOfStream();
            resolve(true);
          }
        }, { once: true });
        sourceBuffer.appendBuffer(data);
      } catch (e) {
        console.error(e);
        resolve(false);
      }
    }, { once: true });
    window.video.src = URL.createObjectURL(mediaSource);
  });

  window.play = async function() {
    await window.mseReadyPromise;
    await window.video.play();
    return true;
  };

  window.enterPictureInPicture = async function() {
    await new Promise(resolve => {
      if (window.video.readyState >= HTMLMediaElement.HAVE_METADATA) {
        resolve();
        return;
      }
      window.video.addEventListener('loadedmetadata', _ => {
        resolve();
      }, { once: true });
    });
    await window.video.requestPictureInPicture();
    return true;
  };

  window.exitPictureInPicture = async function() {
    try {
      console.log('Calling exitPictureInPicture...');
      await document.exitPictureInPicture();
      console.log('exitPictureInPicture resolved successfully.');
      return true;
    } catch (e) {
      console.error('exitPictureInPicture error: ' + e);
      return false;
    }
  };
)";

}  // namespace

IN_PROC_BROWSER_TEST_F(PictureInPictureBrowserTest,
                       EnterAndExitPictureInPicture) {
  GURL test_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  ASSERT_TRUE(ExecJs(shell(), kPictureInPictureScript));
  ASSERT_TRUE(ExecJs(shell(), "addPictureInPictureEventListeners();"));
  ASSERT_EQ(true, EvalJs(shell(), "play();"));

  TitleWatcher enter_watcher(shell()->web_contents(), u"enterpictureinpicture");
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  EXPECT_EQ(u"enterpictureinpicture", enter_watcher.WaitAndGetTitle());

  TitleWatcher exit_watcher(shell()->web_contents(), u"leavepictureinpicture");
  ASSERT_EQ(true, EvalJs(shell(), "exitPictureInPicture();"));
  EXPECT_EQ(u"leavepictureinpicture", exit_watcher.WaitAndGetTitle());

  GURL cleanup_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(NavigateToURL(shell(), cleanup_url));
}

IN_PROC_BROWSER_TEST_F(PictureInPictureBrowserTest,
                       NavigationExitsPictureInPicture) {
  GURL test_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  ASSERT_TRUE(ExecJs(shell(), kPictureInPictureScript));
  ASSERT_TRUE(ExecJs(shell(), "addPictureInPictureEventListeners();"));
  ASSERT_EQ(true, EvalJs(shell(), "play();"));

  TitleWatcher enter_watcher(shell()->web_contents(), u"enterpictureinpicture");
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  EXPECT_EQ(u"enterpictureinpicture", enter_watcher.WaitAndGetTitle());

  GURL next_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(NavigateToURL(shell(), next_url));
}

IN_PROC_BROWSER_TEST_F(PictureInPictureBrowserTest,
                       AppBackgroundExitsPictureInPicture) {
  GURL test_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  // The custom test shell client bypasses the creation of the Cobalt observer.
  // We manually attach it here.
  cobalt::CobaltWebContentsObserver observer(shell()->web_contents());

  ASSERT_TRUE(ExecJs(shell(), kPictureInPictureScript));
  ASSERT_TRUE(ExecJs(shell(), "addPictureInPictureEventListeners();"));
  ASSERT_EQ(true, EvalJs(shell(), "play();"));

  TitleWatcher enter_watcher(shell()->web_contents(), u"enterpictureinpicture");
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  EXPECT_EQ(u"enterpictureinpicture", enter_watcher.WaitAndGetTitle());

  TitleWatcher exit_watcher(shell()->web_contents(), u"leavepictureinpicture");

  // Simulating the app going to the background (e.g. user pressing Home
  // button).
  shell()->web_contents()->WasHidden();
  EXPECT_EQ(u"leavepictureinpicture", exit_watcher.WaitAndGetTitle());

  GURL cleanup_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(NavigateToURL(shell(), cleanup_url));
}

}  // namespace content
