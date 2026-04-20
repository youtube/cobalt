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

#include <stddef.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/testing/browser_tests/content_browser_test_utils.h"
#include "cobalt/testing/browser_tests/webrtc_content_browsertest_base.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/public/browser/audio_service.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/audio/public/mojom/testing_api.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

static const char kGetUserMediaAndStop[] = "getUserMediaAndStop";
static const char kGetUserMediaAndAnalyseAndStop[] =
    "getUserMediaAndAnalyseAndStop";
static const char kGetUserMediaAndExpectFailure[] =
    "getUserMediaAndExpectFailure";
static const char kRenderSameTrackMediastreamAndStop[] =
    "renderSameTrackMediastreamAndStop";
static const char kRenderClonedMediastreamAndStop[] =
    "renderClonedMediastreamAndStop";
static const char kRenderClonedTrackMediastreamAndStop[] =
    "renderClonedTrackMediastreamAndStop";
static const char kRenderDuplicatedMediastreamAndStop[] =
    "renderDuplicatedMediastreamAndStop";

std::string GenerateGetUserMediaWithMandatorySourceID(
    const std::string& function_name,
    const std::string& audio_source_id,
    const std::string& video_source_id) {
  const std::string audio_constraint =
      "audio: {mandatory: { sourceId:\"" + audio_source_id + "\"}}, ";

  const std::string video_constraint =
      "video: {mandatory: { sourceId:\"" + video_source_id + "\"}}";
  return function_name + "({" + audio_constraint + video_constraint + "});";
}

std::string GenerateGetUserMediaWithOptionalSourceID(
    const std::string& function_name,
    const std::string& audio_source_id,
    const std::string& video_source_id) {
  const std::string audio_constraint =
      "audio: {optional: [{sourceId:\"" + audio_source_id + "\"}]}, ";

  const std::string video_constraint =
      "video: {optional: [{ sourceId:\"" + video_source_id + "\"}]}";
  return function_name + "({" + audio_constraint + video_constraint + "});";
}

// TODO(crbug.com/1327666): Bring back when
// WebRtcGetUserMediaBrowserTest.DisableLocalEchoParameter is fixed.
#if 0
std::string GenerateGetUserMediaWithDisableLocalEcho(
    const std::string& function_name,
    const std::string& disable_local_echo) {
  const std::string audio_constraint =
      "audio:{mandatory: { chromeMediaSource : 'system', disableLocalEcho : " +
      disable_local_echo + " }},";

  const std::string video_constraint =
      "video: { mandatory: { chromeMediaSource:'screen' }}";
  return function_name + "({" + audio_constraint + video_constraint + "});";
}

bool VerifyDisableLocalEcho(bool expect_value,
                            const blink::StreamControls& controls) {
  return expect_value == controls.disable_local_echo;
}
#endif

}  // namespace

namespace content {

class WebRtcGetUserMediaBrowserTest : public WebRtcContentBrowserTestBase {
 public:
  WebRtcGetUserMediaBrowserTest() {
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
    scoped_feature_list_.InitAndEnableFeature(
        features::kUserMediaCaptureOnFocus);
  }
  ~WebRtcGetUserMediaBrowserTest() override {}

  // Runs the JavaScript twoGetUserMedia with |constraints1| and |constraint2|.
  void RunTwoGetTwoGetUserMediaWithDifferentContraints(
      const std::string& constraints1,
      const std::string& constraints2,
      const std::string& expected_result) {
    ASSERT_TRUE(embedded_test_server()->Start());

    GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));

    std::string command =
        "twoGetUserMedia(" + constraints1 + ',' + constraints2 + ')';

    EXPECT_EQ(expected_result, EvalJs(shell(), command));
  }

  void GetInputDevices(std::vector<std::string>* audio_ids,
                       std::vector<std::string>* video_ids) {
    GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url));

    std::string devices_as_json =
        EvalJs(shell(), "getSources()").ExtractString();
    EXPECT_FALSE(devices_as_json.empty());

    auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
        devices_as_json, base::JSON_ALLOW_TRAILING_COMMAS);

    ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;
    ASSERT_TRUE(parsed_json->is_list());

    for (const auto& entry : parsed_json->GetList()) {
      const base::Value::Dict* dict = entry.GetIfDict();
      ASSERT_TRUE(dict);
      const std::string* kind = dict->FindString("kind");
      const std::string* device_id = dict->FindString("id");
      ASSERT_TRUE(kind);
      ASSERT_TRUE(device_id);
      ASSERT_FALSE(device_id->empty());
      EXPECT_TRUE(*kind == "audio" || *kind == "video");
      if (*kind == "audio") {
        audio_ids->push_back(*device_id);
      } else if (*kind == "video") {
        video_ids->push_back(*device_id);
      }
    }
    ASSERT_FALSE(audio_ids->empty());
    ASSERT_FALSE(video_ids->empty());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// These tests will all make a getUserMedia call with different constraints and
// see that the success callback is called. If the error callback is called or
// none of the callbacks are called the tests will simply time out and fail.

// Test fails under MSan, http://crbug.com/445745
// TODO(b/437427316): Investigate failing test.
#if defined(MEMORY_SANITIZER)
#define MAYBE_GetVideoStreamAndStop DISABLED_GetVideoStreamAndStop
#elif defined(IS_ANDROIDTV)
#define MAYBE_GetVideoStreamAndStop GetVideoStreamAndStop
#else
#define MAYBE_GetVideoStreamAndStop DISABLED_GetVideoStreamAndStop
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetVideoStreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(
      shell(), base::StringPrintf("%s({video: true});", kGetUserMediaAndStop)));
}

// Test fails under MSan, http://crbug.com/445745
#if defined(MEMORY_SANITIZER)
#define MAYBE_RenderSameTrackMediastreamAndStop \
  DISABLED_RenderSameTrackMediastreamAndStop
#else
#define MAYBE_RenderSameTrackMediastreamAndStop \
  RenderSameTrackMediastreamAndStop
#endif
// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_RenderSameTrackMediastreamAndStop \
  RenderSameTrackMediastreamAndStop
#else
#define MAYBE_RenderSameTrackMediastreamAndStop \
  DISABLED_RenderSameTrackMediastreamAndStop
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_RenderSameTrackMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(
      ExecJs(shell(), base::StringPrintf("%s({video: true});",
                                         kRenderSameTrackMediastreamAndStop)));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_RenderClonedMediastreamAndStop RenderClonedMediastreamAndStop
#else
#define MAYBE_RenderClonedMediastreamAndStop \
  DISABLED_RenderClonedMediastreamAndStop
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_RenderClonedMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(
      ExecJs(shell(), base::StringPrintf("%s({video: true});",
                                         kRenderClonedMediastreamAndStop)));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_kRenderClonedTrackMediastreamAndStop \
  kRenderClonedTrackMediastreamAndStop
#else
#define MAYBE_kRenderClonedTrackMediastreamAndStop \
  DISABLED_kRenderClonedTrackMediastreamAndStop
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_kRenderClonedTrackMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(),
                     base::StringPrintf("%s({video: true});",
                                        kRenderClonedTrackMediastreamAndStop)));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_kRenderDuplicatedMediastreamAndStop \
  kRenderDuplicatedMediastreamAndStop
#else
#define MAYBE_kRenderDuplicatedMediastreamAndStop \
  DISABLED_kRenderDuplicatedMediastreamAndStop
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_kRenderDuplicatedMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(
      ExecJs(shell(), base::StringPrintf("%s({video: true});",
                                         kRenderDuplicatedMediastreamAndStop)));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_GetAudioAndVideoStreamAndStop GetAudioAndVideoStreamAndStop
#else
#define MAYBE_GetAudioAndVideoStreamAndStop \
  DISABLED_GetAudioAndVideoStreamAndStop
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetAudioAndVideoStreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(
      ExecJs(shell(), base::StringPrintf("%s({video: true, audio: true});",
                                         kGetUserMediaAndStop)));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_GetAudioAndVideoStreamAndClone GetAudioAndVideoStreamAndClone
#else
#define MAYBE_GetAudioAndVideoStreamAndClone \
  DISABLED_GetAudioAndVideoStreamAndClone
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetAudioAndVideoStreamAndClone) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), "getUserMediaAndClone();"));
}

// TODO(crbug.com/803516) : Flaky on all platforms.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_RenderVideoTrackInMultipleTagsAndPause \
  RenderVideoTrackInMultipleTagsAndPause
#else
#define MAYBE_RenderVideoTrackInMultipleTagsAndPause \
  DISABLED_RenderVideoTrackInMultipleTagsAndPause
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_RenderVideoTrackInMultipleTagsAndPause) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), "getUserMediaAndRenderInSeveralVideoTags();"));
}

// TODO(crbug.com/571389, crbug.com/1241538): Flaky on TSAN bots.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_GetUserMediaWithMandatorySourceID \
  DISABLED_GetUserMediaWithMandatorySourceID
#else
#define MAYBE_GetUserMediaWithMandatorySourceID \
  GetUserMediaWithMandatorySourceID
#endif
// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_GetUserMediaWithMandatorySourceID \
  GetUserMediaWithMandatorySourceID
#else
#define MAYBE_GetUserMediaWithMandatorySourceID \
  DISABLED_GetUserMediaWithMandatorySourceID
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetUserMediaWithMandatorySourceID) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetInputDevices(&audio_ids, &video_ids);

  // Test all combinations of mandatory sourceID;
  for (std::vector<std::string>::const_iterator video_it = video_ids.begin();
       video_it != video_ids.end(); ++video_it) {
    for (std::vector<std::string>::const_iterator audio_it = audio_ids.begin();
         audio_it != audio_ids.end(); ++audio_it) {
      EXPECT_TRUE(
          ExecJs(shell(), GenerateGetUserMediaWithMandatorySourceID(
                              kGetUserMediaAndStop, *audio_it, *video_it)));
    }
  }
}
#undef MAYBE_GetUserMediaWithMandatorySourceID

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_GetUserMediaWithInvalidMandatorySourceID \
  GetUserMediaWithInvalidMandatorySourceID
#else
#define MAYBE_GetUserMediaWithInvalidMandatorySourceID \
  DISABLED_GetUserMediaWithInvalidMandatorySourceID
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetUserMediaWithInvalidMandatorySourceID) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetInputDevices(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  // Test with invalid mandatory audio sourceID.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ("OverconstrainedError",
            EvalJs(shell(), GenerateGetUserMediaWithMandatorySourceID(
                                kGetUserMediaAndExpectFailure,
                                "something invalid", video_ids[0])));

  // Test with invalid mandatory video sourceID.
  EXPECT_EQ("OverconstrainedError",
            EvalJs(shell(), GenerateGetUserMediaWithMandatorySourceID(
                                kGetUserMediaAndExpectFailure, audio_ids[0],
                                "something invalid")));

  // Test with empty mandatory audio sourceID.
  EXPECT_EQ(
      "OverconstrainedError",
      EvalJs(shell(), GenerateGetUserMediaWithMandatorySourceID(
                          kGetUserMediaAndExpectFailure, "", video_ids[0])));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_GetUserMediaWithInvalidOptionalSourceID \
  GetUserMediaWithInvalidOptionalSourceID
#else
#define MAYBE_GetUserMediaWithInvalidOptionalSourceID \
  DISABLED_GetUserMediaWithInvalidOptionalSourceID
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetUserMediaWithInvalidOptionalSourceID) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetInputDevices(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  // Test with invalid optional audio sourceID.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(
      shell(), GenerateGetUserMediaWithOptionalSourceID(
                   kGetUserMediaAndStop, "something invalid", video_ids[0])));

  // Test with invalid optional video sourceID.
  EXPECT_TRUE(ExecJs(
      shell(), GenerateGetUserMediaWithOptionalSourceID(
                   kGetUserMediaAndStop, audio_ids[0], "something invalid")));

  // Test with empty optional audio sourceID.
  EXPECT_TRUE(ExecJs(shell(), GenerateGetUserMediaWithOptionalSourceID(
                                  kGetUserMediaAndStop, "", video_ids[0])));
}

// Sheriff 2021-08-10, test is flaky.
// See https://crbug.com/1238334.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_TwoGetUserMediaAndStop TwoGetUserMediaAndStop
#else
#define MAYBE_TwoGetUserMediaAndStop DISABLED_TwoGetUserMediaAndStop
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_TwoGetUserMediaAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(
      ExecJs(shell(), "twoGetUserMediaAndStop({video: true, audio: true});"));
}

// Flaky. See https://crbug.com/846741.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_TwoGetUserMediaWithEqualConstraints \
  TwoGetUserMediaWithEqualConstraints
#else
#define MAYBE_TwoGetUserMediaWithEqualConstraints \
  DISABLED_TwoGetUserMediaWithEqualConstraints
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_TwoGetUserMediaWithEqualConstraints) {
  std::string constraints1 = "{video: true, audio: true}";
  const std::string& constraints2 = constraints1;
  std::string expected_result = "w=640:h=480-w=640:h=480";

  RunTwoGetTwoGetUserMediaWithDifferentContraints(constraints1, constraints2,
                                                  expected_result);
}

// Flaky. See https://crbug.com/843844.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_TwoGetUserMediaWithSecondVideoCropped \
  TwoGetUserMediaWithSecondVideoCropped
#else
#define MAYBE_TwoGetUserMediaWithSecondVideoCropped \
  DISABLED_TwoGetUserMediaWithSecondVideoCropped
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_TwoGetUserMediaWithSecondVideoCropped) {
  std::string constraints1 = "{video: true}";
  std::string constraints2 =
      "{video: {width: {exact: 640}, height: {exact: 360}}}";
  std::string expected_result = "w=640:h=480-w=640:h=360";
  RunTwoGetTwoGetUserMediaWithDifferentContraints(constraints1, constraints2,
                                                  expected_result);
}

// Test fails under MSan, http://crbug.com/445745.
// Flaky. See https://crbug.com/846960.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_TwoGetUserMediaWithFirstHdSecondVga \
  TwoGetUserMediaWithFirstHdSecondVga
#else
#define MAYBE_TwoGetUserMediaWithFirstHdSecondVga \
  DISABLED_TwoGetUserMediaWithFirstHdSecondVga
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_TwoGetUserMediaWithFirstHdSecondVga) {
  std::string constraints1 =
      "{video: {width : {exact: 1280}, height: {exact: 720}}}";
  std::string constraints2 =
      "{video: {width : {exact: 640}, height: {exact: 480}}}";
  std::string expected_result = "w=1280:h=720-w=640:h=480";
  RunTwoGetTwoGetUserMediaWithDifferentContraints(constraints1, constraints2,
                                                  expected_result);
}

// Timing out on Windows 7 bot: http://crbug.com/443294
// Flaky: http://crbug.com/660656; possible the test is too perf sensitive.
// TODO(b/437427316): Investigate failing test.
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_TwoGetUserMediaWithFirst1080pSecondVga) {
  std::string constraints1 =
      "{video: {mandatory: {maxWidth:1920 , minWidth:1920 , maxHeight: 1080, "
      "minHeight: 1080}}}";
  std::string constraints2 =
      "{video: {mandatory: {maxWidth:640 , maxHeight: 480}}}";
  std::string expected_result = "w=1920:h=1080-w=640:h=480";
  RunTwoGetTwoGetUserMediaWithDifferentContraints(constraints1, constraints2,
                                                  expected_result);
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_GetUserMediaWithTooHighVideoConstraintsValues \
  GetUserMediaWithTooHighVideoConstraintsValues
#else
#define MAYBE_GetUserMediaWithTooHighVideoConstraintsValues \
  DISABLED_GetUserMediaWithTooHighVideoConstraintsValues
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetUserMediaWithTooHighVideoConstraintsValues) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  int large_value = 99999;
  std::string call = GenerateGetUserMediaCall(
      kGetUserMediaAndExpectFailure, large_value, large_value, large_value,
      large_value, large_value, large_value);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ("OverconstrainedError", EvalJs(shell(), call));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_GetUserMediaFailToAccessAudioDevice \
  GetUserMediaFailToAccessAudioDevice
#else
#define MAYBE_GetUserMediaFailToAccessAudioDevice \
  DISABLED_GetUserMediaFailToAccessAudioDevice
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetUserMediaFailToAccessAudioDevice) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Make sure we'll fail creating the audio stream.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kFailAudioStreamCreation);

  const std::string call = base::StringPrintf(
      "%s({video: false, audio: true});", kGetUserMediaAndExpectFailure);
  EXPECT_EQ("NotReadableError", EvalJs(shell(), call));
}

// This test makes two getUserMedia requests, one with impossible constraints
// that should trigger an error, and one with valid constraints. The test
// verifies getUserMedia can succeed after being given impossible constraints.
// Flaky. See https://crbug.com/846984.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_TwoGetUserMediaAndCheckCallbackAfterFailure \
  TwoGetUserMediaAndCheckCallbackAfterFailure
#else
#define MAYBE_TwoGetUserMediaAndCheckCallbackAfterFailure \
  DISABLED_TwoGetUserMediaAndCheckCallbackAfterFailure
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_TwoGetUserMediaAndCheckCallbackAfterFailure) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  int large_value = 99999;
  const std::string gum_with_impossible_constraints = GenerateGetUserMediaCall(
      kGetUserMediaAndExpectFailure, large_value, large_value, large_value,
      large_value, large_value, large_value);
  const std::string gum_with_vga_constraints = GenerateGetUserMediaCall(
      kGetUserMediaAndAnalyseAndStop, 640, 640, 480, 480, 10, 30);

  ASSERT_EQ("OverconstrainedError",
            EvalJs(shell(), gum_with_impossible_constraints));

  ASSERT_EQ("w=640:h=480", EvalJs(shell(), gum_with_vga_constraints));
}

// This test calls getUserMedia and checks for aspect ratio behavior.
// TODO(1337302): Flaky for tsan.
#if defined(THREAD_SANITIZER)
#define MAYBE_TestGetUserMediaAspectRatio4To3 \
  DISABLED_TestGetUserMediaAspectRatio4To3
#else
#define MAYBE_TestGetUserMediaAspectRatio4To3 TestGetUserMediaAspectRatio4To3
#endif
// TODO(b/437427316): Investigate failing test.
// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_TestGetUserMediaAspectRatio4To3 TestGetUserMediaAspectRatio4To3
#else
#define MAYBE_TestGetUserMediaAspectRatio4To3 \
  DISABLED_TestGetUserMediaAspectRatio4To3
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_TestGetUserMediaAspectRatio4To3) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string constraints_4_3 = GenerateGetUserMediaCall(
      kGetUserMediaAndAnalyseAndStop, 640, 640, 480, 480, 10, 30);

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_EQ("w=640:h=480", EvalJs(shell(), constraints_4_3));
}

// This test calls getUserMedia and checks for aspect ratio behavior.
// Flaky: crbug.com/846582.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_TestGetUserMediaAspectRatio16To9 TestGetUserMediaAspectRatio16To9
#else
#define MAYBE_TestGetUserMediaAspectRatio16To9 \
  DISABLED_TestGetUserMediaAspectRatio16To9
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_TestGetUserMediaAspectRatio16To9) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string constraints_16_9 = GenerateGetUserMediaCall(
      kGetUserMediaAndAnalyseAndStop, 640, 640, 360, 360, 10, 30);

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_EQ("w=640:h=360", EvalJs(shell(), constraints_16_9));
}

// This test calls getUserMedia and checks for aspect ratio behavior.
// TODO(1337302): Flaky for tsan
#if defined(THREAD_SANITIZER)
#define MAYBE_TestGetUserMediaAspectRatio1To1 \
  DISABLED_TestGetUserMediaAspectRatio1To1
#else
#define MAYBE_TestGetUserMediaAspectRatio1To1 TestGetUserMediaAspectRatio1To1
#endif
// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_TestGetUserMediaAspectRatio1To1 TestGetUserMediaAspectRatio1To1
#else
#define MAYBE_TestGetUserMediaAspectRatio1To1 \
  DISABLED_TestGetUserMediaAspectRatio1To1
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_TestGetUserMediaAspectRatio1To1) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string constraints_1_1 = GenerateGetUserMediaCall(
      kGetUserMediaAndAnalyseAndStop, 320, 320, 320, 320, 10, 30);

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_EQ("w=320:h=320", EvalJs(shell(), constraints_1_1));
}

// This test calls getUserMedia in an iframe and immediately close the iframe
// in the scope of the success callback.
// Flaky: crbug.com/727601.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_AudioInIFrameAndCloseInSuccessCb AudioInIFrameAndCloseInSuccessCb
#else
#define MAYBE_AudioInIFrameAndCloseInSuccessCb \
  DISABLED_AudioInIFrameAndCloseInSuccessCb
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_AudioInIFrameAndCloseInSuccessCb) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string call = "getUserMediaInIframeAndCloseInSuccessCb({audio: true});";
  EXPECT_TRUE(ExecJs(shell(), call));
}

// Flaky: crbug.com/807638
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_VideoInIFrameAndCloseInSuccessCb VideoInIFrameAndCloseInSuccessCb
#else
#define MAYBE_VideoInIFrameAndCloseInSuccessCb \
  DISABLED_VideoInIFrameAndCloseInSuccessCb
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_VideoInIFrameAndCloseInSuccessCb) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string call = "getUserMediaInIframeAndCloseInSuccessCb({video: true});";
  EXPECT_TRUE(ExecJs(shell(), call));
}

// This test calls getUserMedia in an iframe and immediately close the iframe
// in the scope of the failure callback.
// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_VideoWithBadConstraintsInIFrameAndCloseInFailureCb \
  VideoWithBadConstraintsInIFrameAndCloseInFailureCb
#else
#define MAYBE_VideoWithBadConstraintsInIFrameAndCloseInFailureCb \
  DISABLED_VideoWithBadConstraintsInIFrameAndCloseInFailureCb
#endif
IN_PROC_BROWSER_TEST_F(
    WebRtcGetUserMediaBrowserTest,
    MAYBE_VideoWithBadConstraintsInIFrameAndCloseInFailureCb) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  int large_value = 99999;
  std::string call = GenerateGetUserMediaCall(
      "getUserMediaInIframeAndCloseInFailureCb", large_value, large_value,
      large_value, large_value, large_value, large_value);
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), call));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_InvalidSourceIdInIFrameAndCloseInFailureCb \
  InvalidSourceIdInIFrameAndCloseInFailureCb
#else
#define MAYBE_InvalidSourceIdInIFrameAndCloseInFailureCb \
  DISABLED_InvalidSourceIdInIFrameAndCloseInFailureCb
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_InvalidSourceIdInIFrameAndCloseInFailureCb) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string call = GenerateGetUserMediaWithMandatorySourceID(
      "getUserMediaInIframeAndCloseInFailureCb", "invalid", "invalid");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), call));
}

// TODO(crbug.com/1327666): Fix this test. It seems to be broken (no audio /
// video tracks are requested; "uncaught (in promise) undefined)") and was false
// positive before disabling.
#if 0
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DisableLocalEchoParameter) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  MediaStreamManager* manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();

  manager->SetGenerateStreamsCallbackForTesting(
      base::BindOnce(&VerifyDisableLocalEcho, false));
  std::string call = GenerateGetUserMediaWithDisableLocalEcho(
      "getUserMediaAndExpectSuccess", "false");
  EXPECT_TRUE(ExecJs(shell(), call));

  manager->SetGenerateStreamsCallbackForTesting(
      base::BindOnce(&VerifyDisableLocalEcho, true));
  call = GenerateGetUserMediaWithDisableLocalEcho(
      "getUserMediaAndExpectSuccess", "true");
  EXPECT_TRUE(ExecJs(shell(), call));


  manager->SetGenerateStreamsCallbackForTesting(
      MediaStreamManager::GenerateStreamTestCallback());
}
#endif

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_GetAudioSettingsDefault GetAudioSettingsDefault
#else
#define MAYBE_GetAudioSettingsDefault DISABLED_GetAudioSettingsDefault
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetAudioSettingsDefault) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "getAudioSettingsDefault()"));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_GetAudioSettingsNoEchoCancellation \
  GetAudioSettingsNoEchoCancellation
#else
#define MAYBE_GetAudioSettingsNoEchoCancellation \
  DISABLED_GetAudioSettingsNoEchoCancellation
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetAudioSettingsNoEchoCancellation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "getAudioSettingsNoEchoCancellation()"));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_GetAudioSettingsDeviceId GetAudioSettingsDeviceId
#else
#define MAYBE_GetAudioSettingsDeviceId DISABLED_GetAudioSettingsDeviceId
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetAudioSettingsDeviceId) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "getAudioSettingsDeviceId()"));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_SrcObjectAddVideoTrack SrcObjectAddVideoTrack
#else
#define MAYBE_SrcObjectAddVideoTrack DISABLED_SrcObjectAddVideoTrack
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_SrcObjectAddVideoTrack) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "srcObjectAddVideoTrack()"));
}

// TODO(crbug.com/848330) Flaky on all platforms
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_SrcObjectReplaceInactiveTracks SrcObjectReplaceInactiveTracks
#else
#define MAYBE_SrcObjectReplaceInactiveTracks \
  DISABLED_SrcObjectReplaceInactiveTracks
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_SrcObjectReplaceInactiveTracks) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "srcObjectReplaceInactiveTracks()"));
}

// Flaky on all platforms. https://crbug.com/835332
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_SrcObjectRemoveVideoTrack SrcObjectRemoveVideoTrack
#else
#define MAYBE_SrcObjectRemoveVideoTrack DISABLED_SrcObjectRemoveVideoTrack
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_SrcObjectRemoveVideoTrack) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "srcObjectRemoveVideoTrack()"));
}

// Flaky. https://crbug.com/843844
// TODO(b/437427316): Investigate failing test.
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       DISABLED_SrcObjectRemoveFirstOfTwoVideoTracks) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "srcObjectRemoveFirstOfTwoVideoTracks()"));
}

// TODO(guidou): Add SrcObjectAddAudioTrack and SrcObjectRemoveAudioTrack tests
// when a straightforward mechanism to detect the presence/absence of audio in a
// media element with an assigned MediaStream becomes available.

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_SrcObjectReassignSameObject SrcObjectReassignSameObject
#else
#define MAYBE_SrcObjectReassignSameObject DISABLED_SrcObjectReassignSameObject
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_SrcObjectReassignSameObject) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "srcObjectReassignSameObject()"));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_ApplyConstraintsVideo ApplyConstraintsVideo
#else
#define MAYBE_ApplyConstraintsVideo DISABLED_ApplyConstraintsVideo
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_ApplyConstraintsVideo) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "applyConstraintsVideo()"));
}

// Flaky due to https://crbug.com/1113820
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_ApplyConstraintsVideoTwoStreams ApplyConstraintsVideoTwoStreams
#else
#define MAYBE_ApplyConstraintsVideoTwoStreams \
  DISABLED_ApplyConstraintsVideoTwoStreams
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_ApplyConstraintsVideoTwoStreams) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "applyConstraintsVideoTwoStreams()"));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_ApplyConstraintsVideoOverconstrained \
  ApplyConstraintsVideoOverconstrained
#else
#define MAYBE_ApplyConstraintsVideoOverconstrained \
  DISABLED_ApplyConstraintsVideoOverconstrained
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_ApplyConstraintsVideoOverconstrained) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "applyConstraintsVideoOverconstrained()"));
}

// Flaky on Linux, see https://crbug.com/952381
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ApplyConstraintsNonDevice DISABLED_ApplyConstraintsNonDevice
#else
#define MAYBE_ApplyConstraintsNonDevice ApplyConstraintsNonDevice
#endif
// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_ApplyConstraintsNonDevice ApplyConstraintsNonDevice
#else
#define MAYBE_ApplyConstraintsNonDevice DISABLED_ApplyConstraintsNonDevice
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_ApplyConstraintsNonDevice) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "applyConstraintsNonDevice()"));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_ConcurrentGetUserMediaStop ConcurrentGetUserMediaStop
#else
#define MAYBE_ConcurrentGetUserMediaStop DISABLED_ConcurrentGetUserMediaStop
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_ConcurrentGetUserMediaStop) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "concurrentGetUserMediaStop()"));
}

// TODO(crbug.com/1087081) : Flaky on all platforms.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_GetUserMediaAfterStopElementCapture \
  GetUserMediaAfterStopElementCapture
#else
#define MAYBE_GetUserMediaAfterStopElementCapture \
  DISABLED_GetUserMediaAfterStopElementCapture
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetUserMediaAfterStopElementCapture) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "getUserMediaAfterStopCanvasCapture()"));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_GetUserMediaEchoCancellationOnAndOff \
  GetUserMediaEchoCancellationOnAndOff
#else
#define MAYBE_GetUserMediaEchoCancellationOnAndOff \
  DISABLED_GetUserMediaEchoCancellationOnAndOff
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetUserMediaEchoCancellationOnAndOff) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "getUserMediaEchoCancellationOnAndOff()"));
}

// TODO(crbug.com/1087081) : Flaky on all platforms.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_GetUserMediaEchoCancellationOnAndOffAndVideo \
  GetUserMediaEchoCancellationOnAndOffAndVideo
#else
#define MAYBE_GetUserMediaEchoCancellationOnAndOffAndVideo \
  DISABLED_GetUserMediaEchoCancellationOnAndOffAndVideo
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetUserMediaEchoCancellationOnAndOffAndVideo) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(
      ExecJs(shell(), "getUserMediaEchoCancellationOnAndOffAndVideo()"));
}

// TODO(b/437427316): Investigate failing test.
#if BUILDFLAG(IS_ANDROIDTV)
#define MAYBE_EnumerationAfterSameDocumentNavigation \
  EnumerationAfterSameDocumentNavigation
#else
#define MAYBE_EnumerationAfterSameDocumentNavigation \
  DISABLED_EnumerationAfterSameDocumentNavigation
#endif
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       MAYBE_EnumerationAfterSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ExecJs(shell(), "enumerationAfterSameDocumentNaviagtion()"));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       RecoverFromCrashInAudioService) {
  // This test only makes sense with the audio service running out of process,
  // with or without sandbox.
  if (!base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess)) {
    return;
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_TRUE(ExecJs(shell(), "setUpForAudioServiceCrash()"));

  // Crash the audio service process.
  mojo::Remote<audio::mojom::TestingApi> service_testing_api;
  GetAudioService().BindTestingApi(
      service_testing_api.BindNewPipeAndPassReceiver());
  service_testing_api->Crash();

  EXPECT_TRUE(ExecJs(shell(), "verifyAfterAudioServiceCrash()"));
}

}  // namespace content
