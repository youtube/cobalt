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
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/starboard/sbmedia_interface.h"
#include "starboard/media.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace {

// A test implementation of SbMediaInterface used to intercept and record
// MIME types and key systems queried by the media pipeline during browser
// tests. This class is owned by the CustomMimeTypeBrowserTest fixture and
// its lifetime is tied to it. It is thread-safe and can be accessed from
// any thread.
class TestSbMedia : public ::media::SbMediaInterface {
 public:
  TestSbMedia() = default;
  ~TestSbMedia() override = default;

  void SetSupportType(SbMediaSupportType type) {
    base::AutoLock lock(lock_);
    support_type_ = type;
  }

  // Forces CanPlayMimeAndKeySystem() to report kSbMediaSupportTypeNotSupported
  // for this one MIME, overriding SetSupportType(). Needed when a single test
  // has to get one MIME accepted and another rejected.
  void SetUnsupportedMime(std::string mime) {
    base::AutoLock lock(lock_);
    unsupported_mime_ = std::move(mime);
  }

  // Controls the verdict returned by CanChangeType(), which gates
  // SourceBuffer.changeType() via ChunkDemuxer::CanChangeType().
  void SetCanChangeType(bool can_change_type) {
    base::AutoLock lock(lock_);
    can_change_type_ = can_change_type;
  }

  void ClearIntercepted() {
    base::AutoLock lock(lock_);
    intercepted_mimes_.clear();
    intercepted_change_types_.clear();
  }

  std::vector<std::string> GetInterceptedMimes() const {
    base::AutoLock lock(lock_);
    return intercepted_mimes_;
  }

  // Returns the (current_mime, new_mime) pairs passed to CanChangeType() by
  // ChunkDemuxer::CanChangeType().
  std::vector<std::pair<std::string, std::string>> GetInterceptedChangeTypes()
      const {
    base::AutoLock lock(lock_);
    return intercepted_change_types_;
  }

  SbMediaSupportType CanPlayMimeAndKeySystem(
      const char* mime,
      const char* /*key_system*/) const override {
    base::AutoLock lock(lock_);
    if (mime) {
      intercepted_mimes_.push_back(mime);
      if (!unsupported_mime_.empty() && unsupported_mime_ == mime) {
        return kSbMediaSupportTypeNotSupported;
      }
    }
    return support_type_;
  }

  bool CanChangeType(const char* current_mime,
                     const char* new_mime) const override {
    base::AutoLock lock(lock_);
    if (current_mime && new_mime) {
      intercepted_change_types_.emplace_back(current_mime, new_mime);
    }
    return can_change_type_;
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
  SbMediaSupportType support_type_ GUARDED_BY(lock_) =
      kSbMediaSupportTypeNotSupported;
  bool can_change_type_ GUARDED_BY(lock_) = true;
  std::string unsupported_mime_ GUARDED_BY(lock_);
  mutable std::vector<std::string> intercepted_mimes_ GUARDED_BY(lock_);
  mutable std::vector<std::pair<std::string, std::string>>
      intercepted_change_types_ GUARDED_BY(lock_);
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
    // The renderer must share the process with the test so that it sees the
    // SbMediaInterface installed by SetSbMediaInterfaceForTesting().
    command_line->AppendSwitch(switches::kSingleProcess);
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    ::media::SetSbMediaInterfaceForTesting(&test_media_);

    ASSERT_TRUE(embedded_test_server()->Start());
    GURL url = embedded_test_server()->GetURL("/title1.html");
    ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

    test_media_.ClearIntercepted();
  }

  void TearDownOnMainThread() override {
    if (shell() && shell()->web_contents()) {
      (void)NavigateToURL(shell()->web_contents(), GURL("about:blank"));
      content::RunAllTasksUntilIdle();
    }
    ::media::SetSbMediaInterfaceForTesting(nullptr);
    content::ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  TestSbMedia test_media_;
};

// Cobalt forwards the MIME string to Starboard verbatim rather than
// normalizing it the way upstream Chromium does. Each entry below is a distinct
// slice of the custom parameter vocabulary; they share one test because they
// all exercise the same passthrough, and a browser launch each would buy no
// additional coverage.
IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       MediaSourceIsTypeSupported_ForwardsRawCustomAttributes) {
  test_media_.SetSupportType(kSbMediaSupportTypeProbably);

  const char* const kCustomMimes[] = {
      // Resolution plus playback flags.
      "video/mp4; codecs=\"avc1.64002a\"; width=3840; height=2160; "
      "tunnelmode=true; hdr=hdr10plus",
      // High framerate and bitrate.
      "video/mp4; codecs=\"avc1.64002a\"; width=3840; height=2160; "
      "framerate=60; bitrate=25000000;",
      // Colorimetry and HDR.
      "video/mp4; codecs=\"vp9\"; width=3840; height=2160; hdr=hdr10plus; "
      "eotf=smpte2084; color_primaries=bt2020; matrix=bt2020nc;",
      // Decoder and buffering flags.
      "video/mp4; codecs=\"avc1.64002a\"; tunnelmode=true; "
      "softwaredecoder=false; disablecache=true; "
      "disabledynamicprerollframecount=true; enableflushduringseek=true;",
      // Audio-specific attributes.
      "audio/mp4; codecs=\"mp4a.40.2\"; channels=6; bitrate=384000;",
      // Everything at once.
      "video/mp4; codecs=\"avc1.64002a\"; width=3840; height=2160; "
      "framerate=60; bitrate=20000000; hdr=hdr10plus; eotf=smpte2084; "
      "color_primaries=bt2020; matrix=bt2020nc; tunnelmode=true; "
      "softwaredecoder=false; disablecache=true; "
      "disabledynamicprerollframecount=true; enableflushduringseek=true; "
      "encryptionscheme=cenc;",
  };

  for (const char* mime : kCustomMimes) {
    SCOPED_TRACE(mime);
    test_media_.ClearIntercepted();

    std::string js_query =
        base::StringPrintf("MediaSource.isTypeSupported('%s');", mime);
    EXPECT_TRUE(
        content::EvalJs(shell()->web_contents(), js_query).ExtractBool());

    EXPECT_TRUE(base::Contains(test_media_.GetInterceptedMimes(), mime));
  }

  // Verify the remaining SbMediaSupportType arms in MediaSource.isTypeSupported
  // (support_type != kSbMediaSupportTypeNotSupported): Maybe -> true,
  // NotSupported -> false. Uses a valid H.264 codec so upstream Chromium would
  // return true if it stripped width/height instead of consulting Starboard.
  const char kProbeMime[] =
      "video/mp4; codecs=\"avc1.64002a\"; width=99999; height=99999;";
  std::string probe_query =
      base::StringPrintf("MediaSource.isTypeSupported('%s');", kProbeMime);

  test_media_.ClearIntercepted();
  test_media_.SetSupportType(kSbMediaSupportTypeMaybe);
  EXPECT_TRUE(
      content::EvalJs(shell()->web_contents(), probe_query).ExtractBool());
  EXPECT_TRUE(base::Contains(test_media_.GetInterceptedMimes(), kProbeMime));

  test_media_.ClearIntercepted();
  test_media_.SetSupportType(kSbMediaSupportTypeNotSupported);
  EXPECT_FALSE(
      content::EvalJs(shell()->web_contents(), probe_query).ExtractBool());
  EXPECT_TRUE(base::Contains(test_media_.GetInterceptedMimes(), kProbeMime));
}

// canPlayType() is implemented once, on HTMLMediaElement, and inherited
// unchanged by both <video> and <audio>, so a single test covers both element
// types. All three SbMediaSupportType values are exercised here because the
// enum-to-string mapping is hand-written and easy to transpose.
IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       CanPlayType_MapsSupportTypeAndForwardsRawMime) {
  const char kVideoMime[] =
      "video/mp4; codecs=\"avc1.64002a\"; width=1920; height=1080; "
      "tunnelmode=true;";
  const char kAudioMime[] =
      "audio/mp4; codecs=\"mp4a.40.2\"; channels=8; bitrate=768000; "
      "audiopassthrough=true; enableresetaudiodecoder=true;";

  const struct {
    SbMediaSupportType support_type;
    const char* element;
    const char* mime;
    const char* expected;
  } kCases[] = {
      {kSbMediaSupportTypeProbably, "video", kVideoMime, "probably"},
      {kSbMediaSupportTypeMaybe, "video", kVideoMime, "maybe"},
      {kSbMediaSupportTypeNotSupported, "video", kVideoMime, ""},
      {kSbMediaSupportTypeProbably, "audio", kAudioMime, "probably"},
  };

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.mime);
    test_media_.ClearIntercepted();
    test_media_.SetSupportType(test_case.support_type);

    std::string js_query =
        base::StringPrintf("document.createElement('%s').canPlayType('%s');",
                           test_case.element, test_case.mime);
    EXPECT_EQ(
        test_case.expected,
        content::EvalJs(shell()->web_contents(), js_query).ExtractString());

    EXPECT_TRUE(
        base::Contains(test_media_.GetInterceptedMimes(), test_case.mime));
  }
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       MediaSourceAddSourceBuffer_ForwardsRawCustomAttributes) {
  test_media_.SetSupportType(kSbMediaSupportTypeProbably);

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

  std::vector<std::string> intercepted = test_media_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kCustomMime));

  // addSourceBuffer also passes the raw MIME through to ChunkDemuxer::AddId().
  // Retention there is verified via current_mime in
  // SourceBufferChangeType_ForwardsRawCustomAttributes. From there, the MIME
  // reaching the decoder configs is covered by SourceBufferStateTest, and the
  // configs reaching SbPlayerCreate()/SbPlayerWriteSamples() by
  // StarboardRendererTest, both in media_unittests.
}

IN_PROC_BROWSER_TEST_F(CustomMimeTypeBrowserTest,
                       SourceBufferChangeType_ForwardsRawCustomAttributes) {
  test_media_.SetSupportType(kSbMediaSupportTypeProbably);

  const char kInitialMime[] =
      "video/mp4; codecs=\"avc1.4d401f\"; width=1920; height=1080; "
      "tunnelmode=true";
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

  std::vector<std::string> intercepted = test_media_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kInitialMime));
  EXPECT_TRUE(base::Contains(intercepted, kChangedMime));

  // changeType() must forward both the initial MIME (stored in
  // SourceBufferState by addSourceBuffer() -> ChunkDemuxer::AddId()) and the
  // target MIME to the Starboard codec transition check via
  // ChunkDemuxer::CanChangeType().
  EXPECT_TRUE(base::Contains(
      test_media_.GetInterceptedChangeTypes(),
      std::make_pair(std::string(kInitialMime), std::string(kChangedMime))));
}

IN_PROC_BROWSER_TEST_F(
    CustomMimeTypeBrowserTest,
    SourceBufferChangeType_NotSupportedThrowsNotSupportedError) {
  const char kInitialMime[] = "video/mp4; codecs=\"avc1.4d401f\"";
  const char kUnsupportedMime[] =
      "video/webm; codecs=\"vp9\"; width=7680; height=4320; "
      "unsupported_flag=true";

  // addSourceBuffer() must succeed while changeType() is rejected, so the
  // rejection is scoped to a single MIME rather than a global support type.
  test_media_.SetSupportType(kSbMediaSupportTypeProbably);
  test_media_.SetUnsupportedMime(kUnsupportedMime);

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

  std::vector<std::string> intercepted = test_media_.GetInterceptedMimes();
  EXPECT_TRUE(base::Contains(intercepted, kInitialMime));
  EXPECT_TRUE(base::Contains(intercepted, kUnsupportedMime));
}

// Unlike the test above, which is rejected by the isTypeSupported() probe, this
// verifies the second gate: the target MIME is supported, but the Starboard
// codec transition check rejects the switch.
IN_PROC_BROWSER_TEST_F(
    CustomMimeTypeBrowserTest,
    SourceBufferChangeType_CodecTransitionRejectedThrowsNotSupportedError) {
  test_media_.SetSupportType(kSbMediaSupportTypeProbably);
  test_media_.SetCanChangeType(false);

  const char kInitialMime[] =
      "video/mp4; codecs=\"avc1.4d401f\"; width=1920; height=1080";
  const char kChangedMime[] =
      "video/webm; codecs=\"vp9\"; width=3840; height=2160; tunnelmode=true";

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
      kInitialMime, kChangedMime);

  EXPECT_TRUE(content::EvalJs(shell()->web_contents(), script).ExtractBool());

  // Both the current and target raw MIMEs must still reach the codec transition
  // check unmodified.
  EXPECT_TRUE(base::Contains(
      test_media_.GetInterceptedChangeTypes(),
      std::make_pair(std::string(kInitialMime), std::string(kChangedMime))));
}

}  // namespace cobalt
