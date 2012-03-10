// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Regression tests for FFmpeg.  Security test files can be found in the
// internal media test data directory:
//
//    svn://svn.chromium.org/chrome-internal/trunk/data/media/security/
//
// Simply add the custom_dep below to your gclient and sync:
//
//    "src/media/test/data/security":
//        "svn://chrome-svn/chrome-internal/trunk/data/media/security"
//
// Many of the files here do not cause issues outside of tooling, so you'll need
// to run this test under ASAN, TSAN, and Valgrind to ensure that all issues are
// caught.
//
// Test cases labeled FLAKY may not always pass, but they should never crash or
// cause any kind of warnings or errors under tooling.

#include "media/filters/pipeline_integration_test_base.h"

#include "base/bind.h"
#include "media/base/test_data_util.h"
#include "media/filters/chunk_demuxer_client.h"

namespace media {

struct RegressionTestData {
  RegressionTestData(const char* filename, PipelineStatus init_status,
                     PipelineStatus end_status)
      : filename(filename),
        init_status(init_status),
        end_status(end_status) {
  }

  const char* filename;
  PipelineStatus init_status;
  PipelineStatus end_status;
};

class FFmpegRegressionTest
    : public testing::TestWithParam<RegressionTestData>,
      public PipelineIntegrationTestBase {
};

#define FFMPEG_TEST_CASE(name, fn, init_status, end_status) \
    INSTANTIATE_TEST_CASE_P(name, FFmpegRegressionTest, \
                            testing::Values(RegressionTestData(fn, \
                                                               init_status, \
                                                               end_status)));

// Test cases from issues.
FFMPEG_TEST_CASE(Cr47325, "security/47325.mp4", PIPELINE_OK, PIPELINE_OK);
FFMPEG_TEST_CASE(Cr93620, "security/93620.ogg", PIPELINE_OK, PIPELINE_OK);
FFMPEG_TEST_CASE(Cr100492, "security/100492.webm", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(Cr100543, "security/100543.webm", PIPELINE_OK, PIPELINE_OK);
FFMPEG_TEST_CASE(Cr101458, "security/101458.webm", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(Cr108416, "security/108416.webm", PIPELINE_OK, PIPELINE_OK);
FFMPEG_TEST_CASE(Cr110849, "security/110849.mkv", DEMUXER_ERROR_COULD_NOT_PARSE,
                 DEMUXER_ERROR_COULD_NOT_PARSE);
FFMPEG_TEST_CASE(Cr112384, "security/112384.webm",
                 DEMUXER_ERROR_COULD_NOT_PARSE, DEMUXER_ERROR_COULD_NOT_PARSE);
FFMPEG_TEST_CASE(Cr112670, "security/112670.mp4", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE);
FFMPEG_TEST_CASE(Cr112976, "security/112976.ogg", PIPELINE_OK, PIPELINE_OK);
FFMPEG_TEST_CASE(Cr116927, "security/116927.ogv", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE);

// General MKV test cases.
FFMPEG_TEST_CASE(MKV_0, "security/nested_tags_lang.mka.627.628", PIPELINE_OK,
                 PIPELINE_OK);
FFMPEG_TEST_CASE(MKV_1, "security/nested_tags_lang.mka.667.628", PIPELINE_OK,
                 PIPELINE_OK);

// General MP4 test cases.
FFMPEG_TEST_CASE(MP4_0, "security/aac.10419.mp4", PIPELINE_OK, PIPELINE_OK);
FFMPEG_TEST_CASE(MP4_1, "security/clockh264aac_200021889.mp4",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN);
FFMPEG_TEST_CASE(MP4_2, "security/clockh264aac_200701257.mp4", PIPELINE_OK,
                 PIPELINE_OK);
FFMPEG_TEST_CASE(MP4_4, "security/clockh264aac_301350139.mp4", PIPELINE_OK,
                 PIPELINE_OK);
FFMPEG_TEST_CASE(MP4_5, "security/clockh264aac_3022500.mp4",
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
FFMPEG_TEST_CASE(MP4_6, "security/clockh264aac_344289.mp4", PIPELINE_OK,
                 PIPELINE_OK);
FFMPEG_TEST_CASE(MP4_7, "security/clockh264mp3_187697.mp4", PIPELINE_OK,
                 PIPELINE_OK);
FFMPEG_TEST_CASE(MP4_8, "security/h264.705767.mp4", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE);
FFMPEG_TEST_CASE(MP4_9, "security/smclockmp4aac_1_0.mp4",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN);

// General OGV test cases.
FFMPEG_TEST_CASE(OGV_1, "security/out.163.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(OGV_2, "security/out.391.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(OGV_5, "security/smclocktheora_1_0.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(OGV_7, "security/smclocktheora_1_102.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(OGV_8, "security/smclocktheora_1_104.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(OGV_9, "security/smclocktheora_1_110.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(OGV_10, "security/smclocktheora_1_179.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(OGV_11, "security/smclocktheora_1_20.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(OGV_12, "security/smclocktheora_1_723.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(OGV_14, "security/smclocktheora_2_10405.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(OGV_15, "security/smclocktheora_2_10619.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(OGV_16, "security/smclocktheora_2_1075.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(OGV_17, "security/vorbis.482086.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(OGV_18, "security/wav.711.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED);

// General WebM test cases.
FFMPEG_TEST_CASE(WEBM_1, "security/no-bug.webm", PIPELINE_OK, PIPELINE_OK);
FFMPEG_TEST_CASE(WEBM_2, "security/uninitialize.webm", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE);
FFMPEG_TEST_CASE(WEBM_3, "security/out.webm.139771.2965",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(WEBM_4, "security/out.webm.68798.1929",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED);
FFMPEG_TEST_CASE(WEBM_5, "content/frame_size_change.webm", PIPELINE_OK,
                 PIPELINE_OK);

// Flaky, maybe larger issues.  All eventually fail in the browser.
FFMPEG_TEST_CASE(FLAKY_Cr99652, "security/99652.webm", PIPELINE_OK,
                 PIPELINE_ERROR_DECODE);
FFMPEG_TEST_CASE(FLAKY_Cr100464, "security/100464.webm", PIPELINE_OK,
                 PIPELINE_ERROR_DECODE);
FFMPEG_TEST_CASE(FLAKY_Cr111342, "security/111342.ogm", PIPELINE_OK,
                 PIPELINE_ERROR_DECODE);
FFMPEG_TEST_CASE(FLAKY_MP4_3, "security/clockh264aac_300413969.mp4",
                 PIPELINE_OK, PIPELINE_ERROR_DECODE);
FFMPEG_TEST_CASE(FLAKY_OGV_0, "security/big_dims.ogv", PIPELINE_OK,
                 PIPELINE_ERROR_DECODE);
FFMPEG_TEST_CASE(FLAKY_OGV_3, "security/smclock_1_0.ogv", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE);
FFMPEG_TEST_CASE(FLAKY_OGV_4, "security/smclock.ogv.1.0.ogv",
                 PIPELINE_OK, PIPELINE_ERROR_DECODE);
FFMPEG_TEST_CASE(FLAKY_OGV_6, "security/smclocktheora_1_10000.ogv",
                 PIPELINE_OK, PIPELINE_ERROR_DECODE);
FFMPEG_TEST_CASE(FLAKY_OGV_13, "security/smclocktheora_1_790.ogv",
                 PIPELINE_ERROR_DECODE, PIPELINE_ERROR_DECODE);

// Hangs. http://crbug.com/117038
// FFMPEG_TEST_CASE(WEBM_0, "security/memcpy.webm", PIPELINE_OK, PIPELINE_OK);

TEST_P(FFmpegRegressionTest, BasicPlayback) {
  if (GetParam().init_status == PIPELINE_OK) {
    ASSERT_TRUE(Start(GetTestDataURL(GetParam().filename),
                      GetParam().init_status));
    Play();
    ASSERT_EQ(WaitUntilEndedOrError(), GetParam().end_status);

    // Check for ended if the pipeline is expected to finish okay.
    if (GetParam().end_status == PIPELINE_OK)
      ASSERT_TRUE(ended_);
  } else {
    ASSERT_FALSE(Start(GetTestDataURL(GetParam().filename),
                       GetParam().init_status));
  }
}

}  // namespace media
