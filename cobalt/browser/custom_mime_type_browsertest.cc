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
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/starboard/sbmedia_interface.h"
#include "media/starboard/mock_sbplayer_interface.h"
#include "media/starboard/sbplayer_interface.h"
#include "starboard/media.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace {

// A test implementation of SbMediaInterface used to intercept and record
// MIME types and key systems queried by the media pipeline during browser
// tests. This class is owned by the CustomMimeTypeBrowserTest fixture and
// its lifetime is tied to it. It is thread-safe and can be accessed from
// any thread.
class TestSbMediaInterface : public media::SbMediaInterface {
 public:
  TestSbMediaInterface() = default;
  ~TestSbMediaInterface() override = default;

  void SetSupportType(SbMediaSupportType type) {
    base::AutoLock lock(lock_);
    support_type_ = type;
  }

  void ClearIntercepted() {
    base::AutoLock lock(lock_);
    intercepted_mimes_.clear();
    intercepted_key_systems_.clear();
  }

  std::vector<std::string> GetInterceptedMimes() const {
    base::AutoLock lock(lock_);
    return intercepted_mimes_;
  }

  std::vector<std::string> GetInterceptedKeySystems() const {
    base::AutoLock lock(lock_);
    return intercepted_key_systems_;
  }

  SbMediaSupportType CanPlayMimeAndKeySystem(
      const char* mime,
      const char* key_system) const override {
    base::AutoLock lock(lock_);
    if (mime) {
      intercepted_mimes_.push_back(mime);
      if (std::string_view(mime).find("unsupported") !=
          std::string_view::npos) {
        return kSbMediaSupportTypeNotSupported;
      }
    }
    if (key_system) {
      intercepted_key_systems_.push_back(key_system);
    }
    return support_type_;
  }

  bool CanChangeType(const char* /*current_mime*/,
                     const char* /*new_mime*/) const override {
    return true;
  }

  int GetAudioOutputCount() const override { return 1; }

  bool GetAudioConfiguration(
      int /*output_index*/,
      SbMediaAudioConfiguration* /*out_configuration*/) const override {
    return false;
  }

  int GetBufferAllocationUnit() const override { return 65536; }

  int GetAudioBufferBudget() const override { return 4 * 1024 * 1024; }

  int64_t GetBufferGarbageCollectionDurationThreshold() const override {
    return 30000000;
  }

  int GetInitialBufferCapacity() const override { return 1024 * 1024; }

  bool IsBufferPoolAllocateOnDemand() const override { return true; }

  int GetVideoBufferBudget(SbMediaVideoCodec /*codec*/,
                           int /*resolution_width*/,
                           int /*resolution_height*/,
                           int /*bits_per_pixel*/) const override {
    return 20 * 1024 * 1024;
  }

 private:
  mutable base::Lock lock_;
  SbMediaSupportType support_type_ = kSbMediaSupportTypeNotSupported;
  mutable std::vector<std::string> intercepted_mimes_;
  mutable std::vector<std::string> intercepted_key_systems_;
};

}  // namespace

// Browser test fixture for verifying custom MIME type parameter forwarding
// from the web engine to the Starboard media interface. This class is owned
// and managed by the gtest framework, with a lifetime spanning a single test
// case execution. It is thread-affine to the browser main thread.
class CustomMimeTypeBrowserTest : public content::ContentBrowserTest {
 public:
  CustomMimeTypeBrowserTest() = default;
  ~CustomMimeTypeBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kSingleProcess);
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    media::SetSbMediaInterfaceForTesting(&test_media_interface_);
    media::SetSbPlayerInterfaceForTesting(&mock_player_interface_);

    embedded_test_server()->ServeFilesFromSourceDirectory("media/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL url = embedded_test_server()->GetURL("/title1.html");
    ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

    test_media_interface_.ClearIntercepted();
  }

  void TearDownOnMainThread() override {
    if (shell() && shell()->web_contents()) {
      (void)NavigateToURL(shell()->web_contents(), GURL("about:blank"));
      content::RunAllTasksUntilIdle();
    }
    media::SetSbPlayerInterfaceForTesting(nullptr);
    media::SetSbMediaInterfaceForTesting(nullptr);
    content::ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  TestSbMediaInterface test_media_interface_;
  testing::NiceMock<media::MockSbPlayerInterface> mock_player_interface_;
};

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       MediaSourceIsTypeSupported_ForwardsRawCustomAttributes) {
  test_media_interface_.SetSupportType(kSbMediaSupportTypeProbably);

  const char kCustomMime[] =
      "video/mp4; codecs=\"avc1.64002a\"; width=3840; height=2160; "
      "tunnelmode=true; hdr=hdr10plus";

  std::string js_query =
      base::StringPrintf("MediaSource.isTypeSupported('%s');", kCustomMime);
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(), js_query).ExtractBool());

  std::vector<std::string> intercepted =
      test_media_interface_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kCustomMime));
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       MediaSourceIsTypeSupported_NotSupportedReturnsFalse) {
  test_media_interface_.SetSupportType(kSbMediaSupportTypeNotSupported);

  const char kUnsupportedMime[] =
      "video/mp4; codecs=\"unsupported.codec\"; width=99999; height=99999;";

  std::string js_query = base::StringPrintf(
      "MediaSource.isTypeSupported('%s');", kUnsupportedMime);
  EXPECT_FALSE(
      content::EvalJs(shell()->web_contents(), js_query).ExtractBool());

  std::vector<std::string> intercepted =
      test_media_interface_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kUnsupportedMime));
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       MediaSourceIsTypeSupported_HighFramerateAndBitrate) {
  test_media_interface_.SetSupportType(kSbMediaSupportTypeProbably);

  const char kCustomMime[] =
      "video/mp4; codecs=\"avc1.64002a\"; width=3840; height=2160; "
      "framerate=60; bitrate=25000000;";

  std::string js_query =
      base::StringPrintf("MediaSource.isTypeSupported('%s');", kCustomMime);
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(), js_query).ExtractBool());

  std::vector<std::string> intercepted =
      test_media_interface_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kCustomMime));
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       MediaSourceIsTypeSupported_ColorimetryAndHdrAttributes) {
  test_media_interface_.SetSupportType(kSbMediaSupportTypeProbably);

  const char kCustomMime[] =
      "video/mp4; codecs=\"vp9\"; width=3840; height=2160; hdr=hdr10plus; "
      "eotf=smpte2084; color_primaries=bt2020; matrix=bt2020nc;";

  std::string js_query =
      base::StringPrintf("MediaSource.isTypeSupported('%s');", kCustomMime);
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(), js_query).ExtractBool());

  std::vector<std::string> intercepted =
      test_media_interface_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kCustomMime));
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       MediaSourceIsTypeSupported_PlaybackAndDecoderFlags) {
  test_media_interface_.SetSupportType(kSbMediaSupportTypeProbably);

  const char kCustomMime[] =
      "video/mp4; codecs=\"avc1.64002a\"; tunnelmode=true; "
      "softwaredecoder=false; disablecache=true; "
      "disabledynamicprerollframecount=true; enableflushduringseek=true;";

  std::string js_query =
      base::StringPrintf("MediaSource.isTypeSupported('%s');", kCustomMime);
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(), js_query).ExtractBool());

  std::vector<std::string> intercepted =
      test_media_interface_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kCustomMime));
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       MediaSourceIsTypeSupported_AllCustomParametersCombined) {
  test_media_interface_.SetSupportType(kSbMediaSupportTypeProbably);

  const char kCustomMime[] =
      "video/mp4; codecs=\"avc1.64002a\"; width=3840; height=2160; "
      "framerate=60; bitrate=20000000; hdr=hdr10plus; eotf=smpte2084; "
      "color_primaries=bt2020; matrix=bt2020nc; tunnelmode=true; "
      "softwaredecoder=false; disablecache=true; "
      "disabledynamicprerollframecount=true; enableflushduringseek=true; "
      "encryptionscheme=cenc;";

  std::string js_query =
      base::StringPrintf("MediaSource.isTypeSupported('%s');", kCustomMime);
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(), js_query).ExtractBool());

  std::vector<std::string> intercepted =
      test_media_interface_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kCustomMime));
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       CanPlayType_ForwardsRawCustomAttributesMaybe) {
  test_media_interface_.SetSupportType(kSbMediaSupportTypeMaybe);

  const char kCustomMime[] =
      "video/mp4; codecs=\"avc1.64002a\"; width=1920; height=1080; "
      "tunnelmode=true;";

  std::string js_query = base::StringPrintf(
      "document.createElement('video').canPlayType('%s');", kCustomMime);
  EXPECT_EQ("maybe",
            content::EvalJs(shell()->web_contents(), js_query).ExtractString());

  std::vector<std::string> intercepted =
      test_media_interface_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kCustomMime));
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       CanPlayType_ForwardsRawCustomAttributesProbably) {
  test_media_interface_.SetSupportType(kSbMediaSupportTypeProbably);

  const char kCustomMime[] =
      "video/mp4; codecs=\"avc1.64002a\"; width=1920; height=1080; "
      "tunnelmode=true;";

  std::string js_query = base::StringPrintf(
      "document.createElement('video').canPlayType('%s');", kCustomMime);
  EXPECT_EQ("probably",
            content::EvalJs(shell()->web_contents(), js_query).ExtractString());

  std::vector<std::string> intercepted =
      test_media_interface_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kCustomMime));
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       CanPlayType_NotSupportedReturnsEmptyString) {
  test_media_interface_.SetSupportType(kSbMediaSupportTypeNotSupported);

  const char kUnsupportedMime[] =
      "video/mp4; codecs=\"avc1.64002a\"; width=99999; height=99999;";

  std::string js_query = base::StringPrintf(
      "document.createElement('video').canPlayType('%s');", kUnsupportedMime);
  EXPECT_EQ("",
            content::EvalJs(shell()->web_contents(), js_query).ExtractString());

  std::vector<std::string> intercepted =
      test_media_interface_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kUnsupportedMime));
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       AudioCanPlayType_ForwardsRawCustomAttributes) {
  test_media_interface_.SetSupportType(kSbMediaSupportTypeProbably);

  const char kAudioCustomMime[] =
      "audio/mp4; codecs=\"mp4a.40.2\"; channels=6; bitrate=384000;";

  std::string js_query = base::StringPrintf(
      "document.createElement('audio').canPlayType('%s');", kAudioCustomMime);
  EXPECT_EQ("probably",
            content::EvalJs(shell()->web_contents(), js_query).ExtractString());

  std::vector<std::string> intercepted =
      test_media_interface_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kAudioCustomMime));
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       AudioCanPlayType_PassthroughAndResetAttributes) {
  test_media_interface_.SetSupportType(kSbMediaSupportTypeProbably);

  const char kAudioCustomMime[] =
      "audio/mp4; codecs=\"mp4a.40.2\"; channels=8; bitrate=768000; "
      "audiopassthrough=true; enableresetaudiodecoder=true;";

  std::string js_query = base::StringPrintf(
      "document.createElement('audio').canPlayType('%s');", kAudioCustomMime);
  EXPECT_EQ("probably",
            content::EvalJs(shell()->web_contents(), js_query).ExtractString());

  std::vector<std::string> intercepted =
      test_media_interface_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kAudioCustomMime));
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       MediaSourceAddSourceBuffer_ForwardsRawCustomAttributes) {
  test_media_interface_.SetSupportType(kSbMediaSupportTypeProbably);

  const char kCustomMime[] =
      "video/mp4; codecs=\"avc1.4d401f\"; width=1920; height=1080; "
      "tunnelmode=true; hdr=true";

  std::string script = base::StringPrintf(
      R"(
        (async () => {
          const ms = new MediaSource();
          const video = document.createElement('video');
          video.src = URL.createObjectURL(ms);
          await new Promise(resolve => ms.addEventListener('sourceopen', resolve, {once: true}));
          ms.addSourceBuffer('%s');
          return true;
        })()
      )",
      kCustomMime);

  EXPECT_TRUE(content::EvalJs(shell()->web_contents(), script).ExtractBool());

  std::vector<std::string> intercepted =
      test_media_interface_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kCustomMime));
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       SourceBufferChangeType_ForwardsRawCustomAttributes) {
  test_media_interface_.SetSupportType(kSbMediaSupportTypeProbably);

  const char kInitialMime[] = "video/mp4; codecs=\"avc1.4d401f\"";
  const char kChangedMime[] =
      "video/webm; codecs=\"vp9\"; width=3840; height=2160; "
      "framerate=60; bitrate=10000000; eotf=smpte2084";

  std::string script = base::StringPrintf(
      R"(
        (async () => {
          const ms = new MediaSource();
          const video = document.createElement('video');
          video.src = URL.createObjectURL(ms);
          await new Promise(resolve => ms.addEventListener('sourceopen', resolve, {once: true}));
          const sb = ms.addSourceBuffer('%s');
          sb.changeType('%s');
          return true;
        })()
      )",
      kInitialMime, kChangedMime);

  EXPECT_TRUE(content::EvalJs(shell()->web_contents(), script).ExtractBool());

  std::vector<std::string> intercepted =
      test_media_interface_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kInitialMime));
  EXPECT_TRUE(base::Contains(intercepted, kChangedMime));
}

IN_PROC_BROWSER_TEST_F(
    CustomMimeTypeBrowserTest,
    SourceBufferChangeType_NotSupportedThrowsNotSupportedError) {
  test_media_interface_.SetSupportType(kSbMediaSupportTypeProbably);

  const char kInitialMime[] = "video/mp4; codecs=\"avc1.4d401f\"";
  const char kUnsupportedMime[] =
      "video/webm; codecs=\"vp9\"; width=7680; height=4320; "
      "unsupported_flag=true";

  std::string script = base::StringPrintf(
      R"(
        (async () => {
          const ms = new MediaSource();
          const video = document.createElement('video');
          video.src = URL.createObjectURL(ms);
          await new Promise(resolve => ms.addEventListener('sourceopen', resolve, {once: true}));
          const sb = ms.addSourceBuffer('%s');
          try {
            sb.changeType('%s');
            return false;
          } catch (e) {
            return e.name === 'NotSupportedError';
          }
        })()
      )",
      kInitialMime, kUnsupportedMime);

  EXPECT_TRUE(content::EvalJs(shell()->web_contents(), script).ExtractBool());

  std::vector<std::string> intercepted =
      test_media_interface_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kInitialMime));
  EXPECT_TRUE(base::Contains(intercepted, kUnsupportedMime));
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       EndToEnd_MediaSourceAppendBuffer_ForwardsToSbPlayer) {
  test_media_interface_.SetSupportType(kSbMediaSupportTypeProbably);

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  std::string created_video_mime;
  base::Lock lock;

  EXPECT_CALL(mock_player_interface_, Destroy(testing::_))
      .WillRepeatedly(testing::Invoke([](SbPlayer player) {
        if (player) {
          delete reinterpret_cast<media::MockSbPlayer*>(player);
        }
      }));

  EXPECT_CALL(mock_player_interface_,
              Create(testing::_, testing::_, testing::_, testing::_, testing::_,
                     testing::_, testing::_, testing::_))
      .WillRepeatedly(testing::Invoke(
          [&](SbWindow /*window*/, const SbPlayerCreationParam* creation_param,
              SbPlayerDeallocateSampleFunc /*sample_deallocate_func*/,
              SbPlayerDecoderStatusFunc /*decoder_status_func*/,
              SbPlayerStatusFunc player_status_func,
              SbPlayerErrorFunc /*player_error_func*/, void* context,
              SbDecodeTargetGraphicsContextProvider* /*context_provider*/) {
            auto player = reinterpret_cast<SbPlayer>(new media::MockSbPlayer());
            {
              base::AutoLock auto_lock(lock);
              if (creation_param && creation_param->video_stream_info.mime) {
                created_video_mime = creation_param->video_stream_info.mime;
              }
            }
            if (player_status_func) {
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(player_status_func, player, context,
                                            kSbPlayerStateInitialized,
                                            SB_PLAYER_INITIAL_TICKET));
            }
            quit_closure.Run();
            return player;
          }));

  const char kCustomMime[] =
      "video/webm; codecs=\"vp8\"; width=320; height=240; tunnelmode=true; "
      "hdr=true";

  std::string script = base::StringPrintf(
      R"(
        (async () => {
          const resp = await fetch('/bear-320x240-video-only.webm');
          const buffer = await resp.arrayBuffer();
          const ms = new MediaSource();
          const video = document.createElement('video');
          document.body.appendChild(video);
          video.src = URL.createObjectURL(ms);
          await new Promise(r => ms.addEventListener('sourceopen', r, {once: true}));
          const sb = ms.addSourceBuffer('%s');
          sb.appendBuffer(buffer);
          await new Promise(r => sb.addEventListener('updateend', r, {once: true}));
          video.play();
          return true;
        })()
      )",
      kCustomMime);

  EXPECT_TRUE(content::EvalJs(shell()->web_contents(), script).ExtractBool());
  run_loop.Run();

  std::vector<std::string> intercepted =
      test_media_interface_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kCustomMime));
  {
    base::AutoLock auto_lock(lock);
    EXPECT_EQ(created_video_mime, kCustomMime);
  }
  testing::Mock::VerifyAndClearExpectations(&mock_player_interface_);
  mock_player_interface_.SetupDefaultExpectations();
}

}  // namespace cobalt
