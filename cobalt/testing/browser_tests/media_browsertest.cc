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

#include "cobalt/testing/browser_tests/media_browsertest.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/common/shell_switches.h"
#include "cobalt/shell/common/shell_test_switches.h"  // nogncheck
#include "cobalt/testing/browser_tests/content_browser_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/audio/audio_features.h"
#include "media/base/media_switches.h"
#include "media/base/supported_types.h"
#include "media/base/test_data_util.h"
#include "media/media_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/url_util.h"

// Proprietary codecs require acceleration on Android.
#if BUILDFLAG(IS_ANDROID)
#define REQUIRE_ACCELERATION_ON_ANDROID() \
  if (!is_accelerated())                  \
  return
#else
#define REQUIRE_ACCELERATION_ON_ANDROID()
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {

#if BUILDFLAG(IS_ANDROID)
// Title set by android cleaner page after short timeout.
const char16_t kClean[] = u"CLEAN";
#endif

void MediaBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(
      switches::kAutoplayPolicy,
      switches::autoplay::kNoUserGestureRequiredPolicy);
  command_line->AppendSwitch(switches::kExposeInternalsForTesting);

  std::vector<base::test::FeatureRef> enabled_features = {
#if BUILDFLAG(IS_ANDROID)
      features::kLogJsConsoleMessages,
#endif
  };

  std::vector<base::test::FeatureRef> disabled_features = {
      // Disable fallback after decode error to avoid unexpected test pass on
      // the fallback path.
      media::kFallbackAfterDecodeError,

#if BUILDFLAG(IS_LINUX)
      // Disable out of process audio on Linux due to process spawn
      // failures. http://crbug.com/986021
      features::kAudioServiceOutOfProcess,
#endif
  };

  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

void MediaBrowserTest::RunMediaTestPage(const std::string& html_page,
                                        const base::StringPairs& query_params,
                                        const std::string& expected_title,
                                        bool http) {
  GURL gurl;
  std::string query = media::GetURLQueryString(query_params);
  std::unique_ptr<net::EmbeddedTestServer> http_test_server;
  if (http) {
    http_test_server = std::make_unique<net::EmbeddedTestServer>();
    http_test_server->ServeFilesFromSourceDirectory(media::GetTestDataPath());
    CHECK(http_test_server->Start());
    gurl = http_test_server->GetURL("/" + html_page + "?" + query);
  } else {
    gurl = content::GetFileUrlWithQuery(media::GetTestDataFilePath(html_page),
                                        query);
  }
  std::string final_title = RunTest(gurl, expected_title);
  EXPECT_EQ(expected_title, final_title);
}

std::string MediaBrowserTest::RunTest(const GURL& gurl,
                                      const std::string& expected_title) {
  VLOG(0) << "Running test URL: " << gurl;
  TitleWatcher title_watcher(shell()->web_contents(),
                             base::ASCIIToUTF16(expected_title));
  AddTitlesToAwait(&title_watcher);
  EXPECT_TRUE(NavigateToURL(shell(), gurl));
  std::u16string result = title_watcher.WaitAndGetTitle();

  CleanupTest();
  return base::UTF16ToASCII(result);
}

void MediaBrowserTest::CleanupTest() {
#if BUILDFLAG(IS_ANDROID)
  // We only do this cleanup on Android, as a workaround for a test-only OOM
  // bug. See http://crbug.com/727542
  const std::u16string cleaner_title = kClean;
  TitleWatcher clean_title_watcher(shell()->web_contents(), cleaner_title);
  GURL cleaner_url = content::GetFileUrlWithQuery(
      media::GetTestDataFilePath("cleaner.html"), "");
  EXPECT_TRUE(NavigateToURL(shell(), cleaner_url));
  std::u16string cleaner_result = clean_title_watcher.WaitAndGetTitle();
  EXPECT_EQ(cleaner_result, cleaner_title);
#endif
}

std::string MediaBrowserTest::EncodeErrorMessage(
    const std::string& original_message) {
  url::RawCanonOutputT<char> buffer;
  url::EncodeURIComponent(original_message.data(), original_message.size(),
                          &buffer);
  return std::string(buffer.data(), buffer.length());
}

void MediaBrowserTest::AddTitlesToAwait(content::TitleWatcher* title_watcher) {
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(media::kEndedTitle));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(media::kErrorTitle));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(media::kErrorEventTitle));
  title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(media::kFailedTitle));
}

// Tests playback and seeking of an audio or video file. Test starts with
// playback then, after X seconds or the ended event fires, seeks near end of
// file; see player.html for details. The test completes when either the last
// 'ended' or an 'error' event fires.
class MediaTest : public testing::WithParamInterface<bool>,
                  public MediaBrowserTest {
 public:
  bool is_accelerated() { return GetParam(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (!is_accelerated()) {
      command_line->AppendSwitch(switches::kDisableAcceleratedVideoDecode);
    }
    MediaBrowserTest::SetUpCommandLine(command_line);
  }

  // Play specified audio over http:// or file:// depending on |http| setting.
  void PlayAudio(const std::string& media_file, bool http = true) {
    PlayMedia("audio", media_file, http);
  }

  // Play specified video over http:// or file:// depending on |http| setting.
  void PlayVideo(const std::string& media_file, bool http = true) {
    PlayMedia("video", media_file, http);
  }

  void PlayMedia(const std::string& tag,
                 const std::string& media_file,
                 bool http) {
    base::StringPairs query_params;
    query_params.emplace_back(tag, media_file);
    RunMediaTestPage("player.html", query_params, media::kEndedTitle, http);
  }

  void RunErrorMessageTest(const std::string& tag,
                           const std::string& media_file,
                           const std::string& expected_error_substring) {
    base::StringPairs query_params;
    query_params.emplace_back(tag, media_file);
    query_params.emplace_back("error_substr",
                              EncodeErrorMessage(expected_error_substring));
    RunMediaTestPage("player.html", query_params, media::kErrorEventTitle,
                     true);
  }

  void RunVideoSizeTest(const char* media_file, int width, int height) {
    std::string expected_title = std::string(media::kEndedTitle) + " " +
                                 base::NumberToString(width) + " " +
                                 base::NumberToString(height);
    base::StringPairs query_params;
    query_params.emplace_back("video", media_file);
    query_params.emplace_back("sizetest", "true");
    RunMediaTestPage("player.html", query_params, expected_title, true);
  }
};

// Android doesn't support Theora.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearTheora) {
  PlayVideo("bear.ogv");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearSilentTheora) {
  PlayVideo("bear_silent.ogv");
}
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearWebm) {
  PlayVideo("bear.webm");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearWebm_FileProtocol) {
  PlayVideo("bear.webm", false);
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_AudioBearOpusWebm) {
  PlayAudio("bear-opus.webm");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_AudioBearOpusMp4) {
  PlayAudio("bear-opus.mp4");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_AudioBearOpusOgg) {
  PlayAudio("bear-opus.ogg");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_AudioBearOpusOgg_FileProtocol) {
  PlayAudio("bear-opus.ogg", false);
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearSilentWebm) {
  PlayVideo("bear_silent.webm");
}

// We don't expect android devices to support highbit yet.
#if !BUILDFLAG(IS_ANDROID)

// TODO(https://crbug.com/1373513): DEMUXER_ERROR_NO_SUPPORTED_STREAMS error on
// Fuchsia Arm64.
#if BUILDFLAG(IS_FUCHSIA) && defined(ARCH_CPU_ARM64)
#define MAYBE_VideoBearHighBitDepthVP9 DISABLED_VideoBearHighBitDepthVP9
#else
#define MAYBE_VideoBearHighBitDepthVP9 VideoBearHighBitDepthVP9
#endif
IN_PROC_BROWSER_TEST_P(MediaTest, MAYBE_VideoBearHighBitDepthVP9) {
  PlayVideo("bear-320x180-hi10p-vp9.webm");
}

// TODO(https://crbug.com/1373513): DEMUXER_ERROR_NO_SUPPORTED_STREAMS error on
// Fuchsia Arm64.
#if BUILDFLAG(IS_FUCHSIA) && defined(ARCH_CPU_ARM64)
#define MAYBE_VideoBear12DepthVP9 DISABLED_VideoBear12DepthVP9
#else
#define MAYBE_VideoBear12DepthVP9 VideoBear12DepthVP9
#endif
IN_PROC_BROWSER_TEST_P(MediaTest, MAYBE_VideoBear12DepthVP9) {
  // Hardware decode on does not reliably support 12-bit.
  if (is_accelerated()) {
    return;
  }
  PlayVideo("bear-320x180-hi12p-vp9.webm");
}
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearMp4Vp9) {
  PlayVideo("bear-320x240-v_frag-vp9.mp4");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_AudioBearFlacMp4) {
  PlayAudio("bear-flac.mp4");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_AudioBearFlac192kHzMp4) {
  PlayAudio("bear-flac-192kHz.mp4");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearMovPcmS16be) {
  PlayAudio("bear_pcm_s16be.mov");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearMovPcmS24be) {
  PlayAudio("bear_pcm_s24be.mov");
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearMp4) {
  REQUIRE_ACCELERATION_ON_ANDROID();
  PlayVideo("bear.mp4");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearSilentMp4) {
  REQUIRE_ACCELERATION_ON_ANDROID();
  PlayVideo("bear_silent.mp4");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearRotated0) {
  REQUIRE_ACCELERATION_ON_ANDROID();
  RunVideoSizeTest("bear_rotate_0.mp4", 1280, 720);
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearRotated90) {
  REQUIRE_ACCELERATION_ON_ANDROID();
  RunVideoSizeTest("bear_rotate_90.mp4", 720, 1280);
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearRotated180) {
  REQUIRE_ACCELERATION_ON_ANDROID();
  RunVideoSizeTest("bear_rotate_180.mp4", 1280, 720);
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearRotated270) {
  REQUIRE_ACCELERATION_ON_ANDROID();
  RunVideoSizeTest("bear_rotate_270.mp4", 720, 1280);
}

#if !BUILDFLAG(IS_ANDROID)
// Android devices usually only support baseline, main and high.
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearHighBitDepthMp4) {
  PlayVideo("bear-320x180-hi10p.mp4");
}

// Android can't reliably load lots of videos on a page.
// See http://crbug.com/749265
// TODO(crbug.com/1222852): Flaky on Mac.
IN_PROC_BROWSER_TEST_P(MediaTest, LoadManyVideos) {
  // Only run this test in one configuration.
  if (is_accelerated()) {
    return;
  }
  base::StringPairs query_params;
  RunMediaTestPage("load_many_videos.html", query_params, media::kEndedTitle,
                   true);
}
#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_AudioBearFlac) {
  PlayAudio("bear.flac");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_AudioBearFlacOgg) {
  PlayAudio("bear-flac.ogg");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearWavAlaw) {
  PlayAudio("bear_alaw.wav");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearWavMulaw) {
  PlayAudio("bear_mulaw.wav");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearWavPcm) {
  PlayAudio("bear_pcm.wav");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearWavPcm3kHz) {
  PlayAudio("bear_3kHz.wav");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoBearWavPcm192kHz) {
  PlayAudio("bear_192kHz.wav");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoTulipWebm) {
  PlayVideo("tulip2.webm");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoErrorMissingResource) {
  RunErrorMessageTest("video", "nonexistent_file.webm",
                      "MEDIA_ELEMENT_ERROR: Format error");
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoErrorEmptySrcAttribute) {
  RunErrorMessageTest("video", "", "MEDIA_ELEMENT_ERROR: Empty src attribute");
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_VideoErrorNoSupportedStreams) {
  RunErrorMessageTest("video", "no_streams.webm",
                      "DEMUXER_ERROR_NO_SUPPORTED_STREAMS: FFmpegDemuxer: no "
                      "supported streams");
}

// Covers tear-down when navigating away as opposed to browser exiting.
// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_Navigate) {
  PlayVideo("bear.webm");
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  EXPECT_FALSE(shell()->web_contents()->IsCrashed());
}

// TODO(b/437420909): Investigate failing test.
IN_PROC_BROWSER_TEST_P(MediaTest, DISABLED_AudioOnly_XHE_AAC_MP4) {
  if (media::IsSupportedAudioType(
          {media::AudioCodec::kAAC, media::AudioCodecProfile::kXHE_AAC})) {
    PlayAudio("noise-xhe-aac.mp4");
  }
}

INSTANTIATE_TEST_SUITE_P(Default, MediaTest, ::testing::Bool());

}  // namespace content
