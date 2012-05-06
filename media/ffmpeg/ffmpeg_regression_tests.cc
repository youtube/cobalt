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
//
// Frame hashes must be generated with --video-threads=1 for correctness.
//
// Known issues:
//    Cr47325 will generate an UninitValue error under Valgrind inside of the
//    MD5 hashing code.  The error occurs due to some problematic error
//    resilence code for H264 inside of FFmpeg.  See http://crbug.com/119020
//
//    FLAKY_OGV_0 may run out of memory under ASAN on IA32 Linux/Mac.
//
//    Some OGG files leak ~30 bytes of memory, upstream tracking bug:
//    https://ffmpeg.org/trac/ffmpeg/ticket/1244
//

#include "media/filters/pipeline_integration_test_base.h"

#include "base/bind.h"
#include "media/base/test_data_util.h"
#include "media/filters/chunk_demuxer_client.h"

namespace media {

struct RegressionTestData {
  RegressionTestData(const char* filename, PipelineStatus init_status,
                     PipelineStatus end_status, const char *md5)
      : md5(md5),
        filename(filename),
        init_status(init_status),
        end_status(end_status) {
  }

  const char* md5;
  const char* filename;
  PipelineStatus init_status;
  PipelineStatus end_status;
};

class FFmpegRegressionTest
    : public testing::TestWithParam<RegressionTestData>,
      public PipelineIntegrationTestBase {
};

#define FFMPEG_TEST_CASE(name, fn, init_status, end_status, md5) \
    INSTANTIATE_TEST_CASE_P(name, FFmpegRegressionTest, \
                            testing::Values(RegressionTestData(fn, \
                                                               init_status, \
                                                               end_status, \
                                                               md5)));

// Test cases from issues.
FFMPEG_TEST_CASE(Cr47325, "security/47325.mp4", PIPELINE_OK, PIPELINE_OK,
                 "2a7a938c6b5979621cec998f02d9bbb6");
FFMPEG_TEST_CASE(Cr47761, "content/crbug47761.ogg", PIPELINE_OK, PIPELINE_OK,
                 kNullVideoHash);
FFMPEG_TEST_CASE(Cr50045, "content/crbug50045.mp4", PIPELINE_OK, PIPELINE_OK,
                 "c345e9ef9ebfc6bfbcbe3f0ddc3125ba");
FFMPEG_TEST_CASE(Cr62127, "content/crbug62127.webm", PIPELINE_OK, PIPELINE_OK,
                 "a064b2776fc5aef3e9cba47967a75db9");
FFMPEG_TEST_CASE(Cr93620, "security/93620.ogg", PIPELINE_OK, PIPELINE_OK,
                 kNullVideoHash);
FFMPEG_TEST_CASE(Cr100492, "security/100492.webm", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullVideoHash);
FFMPEG_TEST_CASE(Cr100543, "security/100543.webm", PIPELINE_OK, PIPELINE_OK,
                 "c16691cc9178db3adbf7e562cadcd6e6");
FFMPEG_TEST_CASE(Cr101458, "security/101458.webm", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullVideoHash);
FFMPEG_TEST_CASE(Cr108416, "security/108416.webm", PIPELINE_OK, PIPELINE_OK,
                 "5cb3a934795cd552753dec7687928291");
FFMPEG_TEST_CASE(Cr110849, "security/110849.mkv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullVideoHash);
FFMPEG_TEST_CASE(Cr112384, "security/112384.webm",
                 DEMUXER_ERROR_COULD_NOT_PARSE, DEMUXER_ERROR_COULD_NOT_PARSE,
                 kNullVideoHash);
FFMPEG_TEST_CASE(Cr112670, "security/112670.mp4", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE, kNullVideoHash);
FFMPEG_TEST_CASE(Cr112976, "security/112976.ogg", PIPELINE_OK, PIPELINE_OK,
                 kNullVideoHash);
FFMPEG_TEST_CASE(Cr116927, "security/116927.ogv", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE, kNullVideoHash);
FFMPEG_TEST_CASE(Cr123481, "security/123481.ogv", PIPELINE_OK,
                 PIPELINE_OK, "e6dd853fcbd746c8bb2ab2b8fc376fc7");

// General MKV test cases.
FFMPEG_TEST_CASE(MKV_0, "security/nested_tags_lang.mka.627.628", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash);
FFMPEG_TEST_CASE(MKV_1, "security/nested_tags_lang.mka.667.628", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash);

// General MP4 test cases.
FFMPEG_TEST_CASE(MP4_0, "security/aac.10419.mp4", PIPELINE_OK, PIPELINE_OK,
                 kNullVideoHash);
FFMPEG_TEST_CASE(MP4_1, "security/clockh264aac_200021889.mp4",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullVideoHash);
FFMPEG_TEST_CASE(MP4_2, "security/clockh264aac_200701257.mp4", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash);
FFMPEG_TEST_CASE(MP4_5, "security/clockh264aac_3022500.mp4",
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS, kNullVideoHash);
FFMPEG_TEST_CASE(MP4_6, "security/clockh264aac_344289.mp4", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash);
FFMPEG_TEST_CASE(MP4_7, "security/clockh264mp3_187697.mp4",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash);
FFMPEG_TEST_CASE(MP4_8, "security/h264.705767.mp4",
                 DEMUXER_ERROR_COULD_NOT_PARSE, DEMUXER_ERROR_COULD_NOT_PARSE,
                 kNullVideoHash);
FFMPEG_TEST_CASE(MP4_9, "security/smclockmp4aac_1_0.mp4",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullVideoHash);

// General OGV test cases.
FFMPEG_TEST_CASE(OGV_1, "security/out.163.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullVideoHash);
FFMPEG_TEST_CASE(OGV_2, "security/out.391.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullVideoHash);
FFMPEG_TEST_CASE(OGV_5, "security/smclocktheora_1_0.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash);
FFMPEG_TEST_CASE(OGV_7, "security/smclocktheora_1_102.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash);
FFMPEG_TEST_CASE(OGV_8, "security/smclocktheora_1_104.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash);
FFMPEG_TEST_CASE(OGV_9, "security/smclocktheora_1_110.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash);
FFMPEG_TEST_CASE(OGV_10, "security/smclocktheora_1_179.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash);
FFMPEG_TEST_CASE(OGV_11, "security/smclocktheora_1_20.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash);
FFMPEG_TEST_CASE(OGV_12, "security/smclocktheora_1_723.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash);
FFMPEG_TEST_CASE(OGV_14, "security/smclocktheora_2_10405.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash);
FFMPEG_TEST_CASE(OGV_15, "security/smclocktheora_2_10619.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash);
FFMPEG_TEST_CASE(OGV_16, "security/smclocktheora_2_1075.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash);
FFMPEG_TEST_CASE(OGV_17, "security/vorbis.482086.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash);
FFMPEG_TEST_CASE(OGV_18, "security/wav.711.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullVideoHash);

// General WebM test cases.
FFMPEG_TEST_CASE(WEBM_1, "security/no-bug.webm", PIPELINE_OK, PIPELINE_OK,
                 "39e92700cbb77478fd63f49db855e7e5");
FFMPEG_TEST_CASE(WEBM_2, "security/uninitialize.webm", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE, kNullVideoHash);
FFMPEG_TEST_CASE(WEBM_3, "security/out.webm.139771.2965",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash);
FFMPEG_TEST_CASE(WEBM_4, "security/out.webm.68798.1929",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash);
FFMPEG_TEST_CASE(WEBM_5, "content/frame_size_change.webm", PIPELINE_OK,
                 PIPELINE_OK, "d8fcf2896b7400a2261bac9e9ea930f8");
FFMPEG_TEST_CASE(WEBM_6, "security/117912.webm", DEMUXER_ERROR_COULD_NOT_OPEN,
                 DEMUXER_ERROR_COULD_NOT_OPEN, kNullVideoHash);

// Flaky under threading, maybe larger issues.  Values were set with
// --video-threads=1 under the hope that they may one day pass with threading.
FFMPEG_TEST_CASE(FLAKY_Cr99652, "security/99652.webm", PIPELINE_OK,
                 PIPELINE_ERROR_DECODE, "97956dca8897484fbd0dd1169578adbf");
FFMPEG_TEST_CASE(FLAKY_Cr100464, "security/100464.webm", PIPELINE_OK,
                 PIPELINE_OK, "a32ddb4ac5ca89429e1a3c968e876ce8");
FFMPEG_TEST_CASE(FLAKY_OGV_3, "security/smclock_1_0.ogv", PIPELINE_OK,
                 PIPELINE_OK, "2f7aa71ce789e47a1dde98ecdd774679");
FFMPEG_TEST_CASE(FLAKY_OGV_4, "security/smclock.ogv.1.0.ogv",
                 PIPELINE_OK, PIPELINE_OK, "2f7aa71ce789e47a1dde98ecdd774679");
FFMPEG_TEST_CASE(FLAKY_OGV_13, "security/smclocktheora_1_790.ogv",
                 PIPELINE_OK, PIPELINE_OK, "2f7aa71ce789e47a1dde98ecdd774679");
FFMPEG_TEST_CASE(FLAKY_MP4_4, "security/clockh264aac_301350139.mp4",
                 PIPELINE_OK, PIPELINE_OK, "bf1dbeb4ee6edb1b6c78742f4943c162");

// Flaky due to other non-threading related reasons.
FFMPEG_TEST_CASE(FLAKY_Cr111342, "security/111342.ogm", PIPELINE_OK,
                 PIPELINE_ERROR_DECODE, "8a14eb9ce0671194a0b04fee8b0b5368");
FFMPEG_TEST_CASE(FLAKY_OGV_0, "security/big_dims.ogv", PIPELINE_OK,
                 PIPELINE_OK, "c76ac9c5e9f3b8edaa341d9ee74739f4");
FFMPEG_TEST_CASE(FLAKY_OGV_6, "security/smclocktheora_1_10000.ogv",
                 PIPELINE_OK, PIPELINE_OK, "b16734719247275c9d9c828d25ffd4bf");
FFMPEG_TEST_CASE(FLAKY_MP4_3, "security/clockh264aac_300413969.mp4",
                 PIPELINE_OK, PIPELINE_OK, "b7659b127be88829a68d8a9aa625795e");

// Hangs. http://crbug.com/117038
// FFMPEG_TEST_CASE(WEBM_0, "security/memcpy.webm", PIPELINE_OK, PIPELINE_OK);

TEST_P(FFmpegRegressionTest, BasicPlayback) {
  if (GetParam().init_status == PIPELINE_OK) {
    ASSERT_TRUE(Start(GetTestDataURL(GetParam().filename),
                      GetParam().init_status));
    Play();
    ASSERT_EQ(WaitUntilEndedOrError(), GetParam().end_status);
    ASSERT_EQ(GetVideoHash(), GetParam().md5);

    // Check for ended if the pipeline is expected to finish okay.
    if (GetParam().end_status == PIPELINE_OK) {
      ASSERT_TRUE(ended_);

      // Tack a seek on the end to catch any seeking issues.
      Seek(base::TimeDelta::FromMilliseconds(0));
    }
  } else {
    ASSERT_FALSE(Start(GetTestDataURL(GetParam().filename),
                       GetParam().init_status));
    ASSERT_EQ(GetVideoHash(), GetParam().md5);
  }
}

}  // namespace media
