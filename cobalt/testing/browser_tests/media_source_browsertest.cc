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

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "media/base/supported_types.h"
#include "media/base/test_data_util.h"
#include "media/media_buildflags.h"

// TODO(b/458665232): Restore MediaSourceTest in M138.
#if 0
#include "cobalt/testing/browser_tests/media_browsertest.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_STARBOARD)
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "cobalt/browser/lifecycle/cobalt_lifecycle_manager.h"
#include "cobalt/common/features/starboard_features_initialization.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "cobalt/testing/browser_tests/content_browser_test_utils.h"
#include "cobalt/testing/browser_tests/gpu/shell_content_gpu_test_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "media/starboard/sbplayer_interface.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/ozone/platform/starboard/platform_window_starboard.h"
#endif  // BUILDFLAG(IS_STARBOARD)

namespace content {

// TODO(b/458665232): Restore MediaSourceTest in M138.
#if 0
class MediaSourceTest : public MediaBrowserTest {
 public:
  void TestSimplePlayback(std::string_view media_file,
                          std::string_view media_type,
                          std::string_view expectation) {
    base::StringPairs query_params;
    query_params.emplace_back("mediaFile", media_file);
    query_params.emplace_back("mediaType", media_type);
    RunMediaTestPage("media_source_player.html", query_params,
                     std::string(expectation), true);
  }

  void TestSimplePlayback(std::string_view media_file,
                          std::string_view expectation) {
    TestSimplePlayback(media_file, media::GetMimeTypeForFile(media_file),
                       expectation);
  }

  base::StringPairs GetAudioVideoQueryParams(std::string_view audio_file,
                                             std::string_view video_file) {
    base::StringPairs params;
    params.emplace_back("audioFile", audio_file);
    params.emplace_back("audioFormat", media::GetMimeTypeForFile(audio_file));
    params.emplace_back("videoFile", video_file);
    params.emplace_back("videoFormat", media::GetMimeTypeForFile(video_file));
    return params;
  }
};

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240.webm", media::kEndedTitle);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-video-only.webm", media::kEndedTitle);
}

// TODO(servolk): Android is supposed to support AAC in ADTS container with
// 'audio/aac' mime type, but for some reason playback fails on trybots due to
// some issue in OMX AAC decoder (crbug.com/528361)
#if BUILDFLAG(USE_PROPRIETARY_CODECS) && !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_AAC_ADTS) {
  TestSimplePlayback("sfx.adts", media::kEndedTitle);
}
#endif

// Opus is not supported in Android as of now.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_Opus_WebM) {
  TestSimplePlayback("bear-opus.webm", media::kEndedTitle);
}
#endif

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_WebM) {
  TestSimplePlayback("bear-320x240-audio-only.webm", media::kEndedTitle);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_MP3) {
  TestSimplePlayback("sfx.mp3", media::kEndedTitle);
}

IN_PROC_BROWSER_TEST_F(
    MediaSourceTest,
    Playback_AudioOnly_MP3_With_Codecs_Parameter_Should_Fail) {
  // We override the correct media type for this file with one which erroneously
  // includes a codecs parameter that is valid for progressive but invalid for
  // MSE type support.
  DCHECK_EQ(media::GetMimeTypeForFile("sfx.mp3"), "audio/mpeg");
  TestSimplePlayback("sfx.mp3", "audio/mpeg; codecs=\"mp3\"",
                     media::kFailedTitle);
}

// Test the case where test file and mime type mismatch.
IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_Type_Error) {
  const char kWebMAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
  TestSimplePlayback("bear-320x240-video-only.webm", kWebMAudioOnly,
                     media::kErrorEventTitle);
}

// Flaky test crbug.com/246308
// Test changed to skip checks resulting in flakiness. Proper fix still needed.
// TODO(crbug.com/330132631): Flaky on Fuchsia, deflake and re-enable the test.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_ConfigChangeVideo DISABLED_ConfigChangeVideo
#else
#define MAYBE_ConfigChangeVideo ConfigChangeVideo
#endif
IN_PROC_BROWSER_TEST_F(MediaSourceTest, MAYBE_ConfigChangeVideo) {
  RunMediaTestPage("mse_config_change.html", base::StringPairs(),
                   media::kEndedTitle, true);
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// Flaky test crbug.com/246308
// Test changed to skip checks resulting in flakiness. Proper fix still needed.
// TODO(crbug.com/330132631): Flaky on Fuchsia, deflake and re-enable the test.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_ConfigChangeVideo_MP4 DISABLED_ConfigChangeVideo_MP4
#else
#define MAYBE_ConfigChangeVideo_MP4 ConfigChangeVideo_MP4
#endif
IN_PROC_BROWSER_TEST_F(MediaSourceTest, MAYBE_ConfigChangeVideo_MP4) {
  base::StringPairs params;

  // Start with 1280x720 first to ensure we end up on the hardware decoder.
  constexpr char kAVFile1[] = "bear-1280x720-av_frag.mp4";
  params.emplace_back("avFormat", media::GetMimeTypeForFile(kAVFile1));
  params.emplace_back("avFile1", kAVFile1);
  params.emplace_back("avResolution1", "1280x720");

  constexpr char kAVFile2[] = "bear-640x360-av_frag.mp4";
  params.emplace_back("avFile2", kAVFile2);
  params.emplace_back("avResolution2", "640x360");

  ASSERT_EQ(media::GetMimeTypeForFile(kAVFile1),
            media::GetMimeTypeForFile(kAVFile2));
  RunMediaTestPage("mse_config_change.html", std::move(params),
                   media::kEndedTitle, true);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_Video_MP4_Audio_WEBM) {
  auto query_params = GetAudioVideoQueryParams("bear-320x240-audio-only.webm",
                                               "bear-640x360-v_frag.mp4");
  RunMediaTestPage("mse_different_containers.html", std::move(query_params),
                   media::kEndedTitle, true);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_Video_WEBM_Audio_MP4) {
  auto query_params = GetAudioVideoQueryParams("bear-640x360-a_frag.mp4",
                                               "bear-320x240-video-only.webm");
  RunMediaTestPage("mse_different_containers.html", std::move(query_params),
                   media::kEndedTitle, true);
}

#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_FLAC_MP4) {
  TestSimplePlayback("bear-flac_frag.mp4", media::kEndedTitle);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_XHE_AAC_MP4) {
  if (media::IsDecoderSupportedAudioType(
          {media::AudioCodec::kAAC, media::AudioCodecProfile::kXHE_AAC})) {
    TestSimplePlayback("noise-xhe-aac.mp4", media::kEndedTitle);
  }
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioVideo_Mp2t) {
  TestSimplePlayback("bear-1280x720.ts", media::kEndedTitle);
}
#endif
#endif
#endif

#if BUILDFLAG(IS_STARBOARD)

namespace {

// Wraps DefaultSbPlayerInterface to record exact SbPlayerCreate /
// SbPlayerDestroy ordering relative to SbWindowDestroy and OnConcealCompleted.
class OrderTrackingSbPlayerInterface : public media::SbPlayerInterface {
 public:
  explicit OrderTrackingSbPlayerInterface(
      base::RepeatingCallback<void(const std::string&)> record_event_cb,
      base::TimeDelta destroy_delay = base::TimeDelta())
      : record_event_cb_(std::move(record_event_cb)),
        destroy_delay_(destroy_delay) {}

  SbPlayer Create(
      SbWindow window,
      const SbPlayerCreationParam* creation_param,
      SbPlayerDeallocateSampleFunc sample_deallocate_func,
      SbPlayerDecoderStatusFunc decoder_status_func,
      SbPlayerStatusFunc player_status_func,
      SbPlayerErrorFunc player_error_func,
      void* context,
      SbDecodeTargetGraphicsContextProvider* context_provider) override {
    SbPlayer player = default_interface_.Create(
        window, creation_param, sample_deallocate_func, decoder_status_func,
        player_status_func, player_error_func, context, context_provider);
    if (SbPlayerIsValid(player)) {
      record_event_cb_.Run("SbPlayerCreate");
    }
    return player;
  }

  SbPlayerOutputMode GetPreferredOutputMode(
      const SbPlayerCreationParam* creation_param) override {
    return default_interface_.GetPreferredOutputMode(creation_param);
  }

  void Destroy(SbPlayer player) override {
    if (!destroy_delay_.is_zero()) {
      // Allowed in tests only to reproduce the async Mojo teardown race.
      base::PlatformThread::Sleep(destroy_delay_);
    }
    default_interface_.Destroy(player);
    record_event_cb_.Run("SbPlayerDestroy");
  }

  void Seek(SbPlayer player,
            base::TimeDelta seek_to_timestamp,
            int ticket) override {
    default_interface_.Seek(player, seek_to_timestamp, ticket);
  }
  void WriteSamples(SbPlayer player,
                    SbMediaType sample_type,
                    const SbPlayerSampleInfo* sample_infos,
                    int number_of_sample_infos) override {
    default_interface_.WriteSamples(player, sample_type, sample_infos,
                                    number_of_sample_infos);
  }
  int GetMaximumNumberOfSamplesPerWrite(SbPlayer player,
                                        SbMediaType sample_type) override {
    return default_interface_.GetMaximumNumberOfSamplesPerWrite(player,
                                                                sample_type);
  }
  void WriteEndOfStream(SbPlayer player, SbMediaType stream_type) override {
    default_interface_.WriteEndOfStream(player, stream_type);
  }
  void SetBounds(SbPlayer player,
                 int z_index,
                 int x,
                 int y,
                 int width,
                 int height) override {
    default_interface_.SetBounds(player, z_index, x, y, width, height);
  }
  bool SetPlaybackRate(SbPlayer player, double playback_rate) override {
    return default_interface_.SetPlaybackRate(player, playback_rate);
  }
  void SetVolume(SbPlayer player, double volume) override {
    default_interface_.SetVolume(player, volume);
  }
  void GetInfo(SbPlayer player, SbPlayerInfo* out_player_info) override {
    default_interface_.GetInfo(player, out_player_info);
  }
  SbDecodeTarget GetCurrentFrame(SbPlayer player) override {
    return default_interface_.GetCurrentFrame(player);
  }
  bool GetAudioConfiguration(
      SbPlayer player,
      int index,
      SbMediaAudioConfiguration* out_audio_configuration) override {
    return default_interface_.GetAudioConfiguration(player, index,
                                                    out_audio_configuration);
  }

 private:
  media::DefaultSbPlayerInterface default_interface_;
  base::RepeatingCallback<void(const std::string&)> record_event_cb_;
  base::TimeDelta destroy_delay_;
};

// Scoped CobaltLifecycleManagerObserver that records and waits for
// OnConcealCompleted during browser tests. Stack-allocated by the test on the
// browser UI thread; registers and unregisters itself with
// CobaltLifecycleManager on construction and destruction.
class ConcealCompletionWaiter : public cobalt::CobaltLifecycleManagerObserver {
 public:
  explicit ConcealCompletionWaiter(
      base::RepeatingCallback<void(const std::string&)> record_event_cb)
      : record_event_cb_(std::move(record_event_cb)) {
    cobalt::CobaltLifecycleManager::GetInstance()->AddObserver(this);
  }
  ~ConcealCompletionWaiter() override {
    cobalt::CobaltLifecycleManager::GetInstance()->RemoveObserver(this);
  }

  void OnAllFramesVisible(content::WebContents* /*web_contents*/) override {}

  void OnConcealCompleted(content::WebContents* /*web_contents*/) override {
    record_event_cb_.Run("OnConcealCompleted");
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RepeatingCallback<void(const std::string&)> record_event_cb_;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

constexpr char kStartMseVideoScript[] = R"(
  new Promise(resolve => {
    const v = document.createElement('video');
    v.id = 'test_video';
    v.loop = true;
    v.muted = true;
    document.body.appendChild(v);
    const ms = new MediaSource();
    v.src = URL.createObjectURL(ms);
    ms.addEventListener('sourceopen', async () => {
      const sb = ms.addSourceBuffer('video/webm; codecs="vp9"');
      const resp = await fetch('/bear-vp9.webm');
      const buf = await resp.arrayBuffer();
      sb.addEventListener('updateend', async () => {
        ms.endOfStream();
        await v.play();
        if (!v.paused && v.readyState >= 2) {
          resolve(true);
        } else {
          v.addEventListener('playing', () => resolve(true), {once: true});
        }
      }, {once: true});
      sb.appendBuffer(buf);
    }, {once: true});
  })
)";

}  // namespace

class MediaConcealLifecycleBrowserTest : public ContentBrowserTest {
 public:
  MediaConcealLifecycleBrowserTest() {
    gpu_client_ = std::make_unique<ShellContentGpuTestClient>();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
    command_line->AppendSwitch(switches::kSingleProcess);
    cobalt::features::InitializeStarboardFeatures();

    struct ContentClientLayoutHelper {
      virtual ~ContentClientLayoutHelper() = default;
      raw_ptr<ContentBrowserClient, DanglingUntriaged> browser_;
      raw_ptr<ContentGpuClient> gpu_;
      raw_ptr<ContentRendererClient> renderer_;
      raw_ptr<ContentUtilityClient> utility_;
    };
    auto* helper = reinterpret_cast<ContentClientLayoutHelper*>(
        content::GetContentClientForTesting());
    helper->gpu_ = gpu_client_.get();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ui::PlatformWindowStarboard::SetWindowDestroyedCallback(base::BindRepeating(
        &MediaConcealLifecycleBrowserTest::OnWindowDestroyed,
        base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    ui::PlatformWindowStarboard::ClearWindowDestroyedCallback();
    cobalt::CobaltLifecycleManager::GetInstance()->ResetForTesting();
    ContentBrowserTest::TearDownOnMainThread();
  }

  void RecordEvent(const std::string& event) {
    base::AutoLock lock(events_lock_);
    events_.push_back(event);
  }

  std::vector<std::string> GetEvents() {
    base::AutoLock lock(events_lock_);
    return events_;
  }

  void TriggerConcealAndWait() {
    auto* manager = cobalt::CobaltLifecycleManager::GetInstance();
    ConcealCompletionWaiter waiter(
        base::BindRepeating(&MediaConcealLifecycleBrowserTest::RecordEvent,
                            base::Unretained(this)));
    Shell::OnConceal();
    manager->StartWaitingForAck(shell()->web_contents(),
                                cobalt::PendingAck::kConceal);
    waiter.Wait();
  }

  void TriggerReveal() {
    Shell::OnReveal();
    cobalt::CobaltLifecycleManager::GetInstance()->StartWaitingForAck(
        shell()->web_contents(), cobalt::PendingAck::kReveal);
  }

 private:
  void OnWindowDestroyed(SbWindow /*window*/) {
    RecordEvent("SbWindowDestroy");
  }

  base::Lock events_lock_;
  std::vector<std::string> events_;
  std::unique_ptr<ShellContentGpuTestClient> gpu_client_;
};

// Counter-Test 2A.1: Active playback on conceal must destroy SbPlayer before
// SbWindowDestroy and OnConcealCompleted, and auto-resume on reveal.
IN_PROC_BROWSER_TEST_F(
    MediaConcealLifecycleBrowserTest,
    ConcealDuringActivePlaybackDestroysSbPlayerBeforeWindowAndAutoResumes) {
  OrderTrackingSbPlayerInterface tracking_interface(base::BindRepeating(
      &MediaConcealLifecycleBrowserTest::RecordEvent, base::Unretained(this)));
  media::ScopedSbPlayerInterfaceForTesting scoped_interface(
      &tracking_interface);

  embedded_test_server()->ServeFilesFromSourceDirectory(
      media::GetTestDataPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/cleaner.html");
  ASSERT_TRUE(NavigateToURL(shell(), url));
  cobalt::CobaltLifecycleManager::GetInstance()->InitializeTracker(
      shell()->web_contents());
  ASSERT_EQ(true, EvalJs(shell(), kStartMseVideoScript));

  TriggerConcealAndWait();

  const std::vector<std::string> expected = {
      "SbPlayerCreate", "SbPlayerDestroy", "SbWindowDestroy",
      "OnConcealCompleted"};
  EXPECT_EQ(GetEvents(), expected);

  // Reveal and verify auto-resume.
  TriggerReveal();
  EXPECT_EQ(false,
            EvalJs(shell(),
                   "new Promise(r => {"
                   "  const v = document.getElementById('test_video');"
                   "  if (!v.paused) r(false);"
                   "  else v.addEventListener('play', () => r(v.paused), "
                   "{once: true});"
                   "})"));
}

// Counter-Test 2A (MVP): Web app stopping and removing <video> in
// visibilitychange with a 50ms delayed SbPlayerDestroy must still complete
// SbPlayerDestroy before SbWindowDestroy and OnConcealCompleted.
IN_PROC_BROWSER_TEST_F(
    MediaConcealLifecycleBrowserTest,
    ConcealWhenAppRemovesVideoWaitsForAsyncSbPlayerDestroyBeforeWindowDestroy) {
  OrderTrackingSbPlayerInterface tracking_interface(
      base::BindRepeating(&MediaConcealLifecycleBrowserTest::RecordEvent,
                          base::Unretained(this)),
      /*destroy_delay=*/base::Milliseconds(50));
  media::ScopedSbPlayerInterfaceForTesting scoped_interface(
      &tracking_interface);

  embedded_test_server()->ServeFilesFromSourceDirectory(
      media::GetTestDataPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/cleaner.html");
  ASSERT_TRUE(NavigateToURL(shell(), url));
  cobalt::CobaltLifecycleManager::GetInstance()->InitializeTracker(
      shell()->web_contents());
  ASSERT_EQ(true, EvalJs(shell(), kStartMseVideoScript));
  ASSERT_TRUE(ExecJs(shell(),
                     "document.addEventListener('visibilitychange', () => {"
                     "  if (document.hidden) {"
                     "    const v = document.getElementById('test_video');"
                     "    if (v) {"
                     "      v.removeAttribute('src');"
                     "      v.load();"
                     "      v.remove();"
                     "    }"
                     "  }"
                     "}, {once: true});"));

  TriggerConcealAndWait();

  const std::vector<std::string> expected = {
      "SbPlayerCreate", "SbPlayerDestroy", "SbWindowDestroy",
      "OnConcealCompleted"};
  EXPECT_EQ(GetEvents(), expected);
}

#endif  // BUILDFLAG(IS_STARBOARD)

}  // namespace content
