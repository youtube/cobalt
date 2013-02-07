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
//    Some OGG files leak ~30 bytes of memory, upstream tracking bug:
//    https://ffmpeg.org/trac/ffmpeg/ticket/1244
//
//    Some OGG files leak hundreds of kilobytes of memory, upstream bug:
//    https://ffmpeg.org/trac/ffmpeg/ticket/1931

#include "media/filters/pipeline_integration_test_base.h"

#include "base/bind.h"
#include "media/base/test_data_util.h"

namespace media {

struct RegressionTestData {
  RegressionTestData(const char* filename, PipelineStatus init_status,
                     PipelineStatus end_status, const char* video_md5,
                     const char* audio_md5)
      : video_md5(video_md5),
        audio_md5(audio_md5),
        filename(filename),
        init_status(init_status),
        end_status(end_status) {
  }

  const char* video_md5;
  const char* audio_md5;
  const char* filename;
  PipelineStatus init_status;
  PipelineStatus end_status;
};

// Used for tests which just need to run without crashing or tooling errors, but
// which may have undefined behavior for hashing, etc.
struct FlakyRegressionTestData {
  FlakyRegressionTestData(const char* filename)
      : filename(filename) {
  }

  const char* filename;
};

class FFmpegRegressionTest
    : public testing::TestWithParam<RegressionTestData>,
      public PipelineIntegrationTestBase {
};

class FlakyFFmpegRegressionTest
    : public testing::TestWithParam<FlakyRegressionTestData>,
      public PipelineIntegrationTestBase {
};

#define FFMPEG_TEST_CASE(name, fn, init_status, end_status, video_md5, \
                         audio_md5) \
    INSTANTIATE_TEST_CASE_P(name, FFmpegRegressionTest, \
                            testing::Values(RegressionTestData(fn, \
                                                               init_status, \
                                                               end_status, \
                                                               video_md5, \
                                                               audio_md5)));

#define FLAKY_FFMPEG_TEST_CASE(name, fn) \
    INSTANTIATE_TEST_CASE_P(FLAKY_##name, FlakyFFmpegRegressionTest, \
                            testing::Values(FlakyRegressionTestData(fn)));

// Test cases from issues.
FFMPEG_TEST_CASE(Cr47325, "security/47325.mp4", PIPELINE_OK, PIPELINE_OK,
                 "2a7a938c6b5979621cec998f02d9bbb6",
                 "efbc63a850c9f8f51942f6a6029eb00f");
FFMPEG_TEST_CASE(Cr47761, "content/crbug47761.ogg", PIPELINE_OK, PIPELINE_OK,
                 kNullHash,
                 "f45b9d7556f39dd811700ec72cb71483");
FFMPEG_TEST_CASE(Cr50045, "content/crbug50045.mp4", PIPELINE_OK, PIPELINE_OK,
                 "c345e9ef9ebfc6bfbcbe3f0ddc3125ba",
                 "73d65d9cc6ce25060b7510bd74678c26");
FFMPEG_TEST_CASE(Cr62127, "content/crbug62127.webm", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE, "d41d8cd98f00b204e9800998ecf8427e",
                 kNullHash);
FFMPEG_TEST_CASE(Cr93620, "security/93620.ogg", PIPELINE_OK, PIPELINE_OK,
                 kNullHash,
                 "0cff252cd46867d26c42a96e6a2e2376");
FFMPEG_TEST_CASE(Cr100492, "security/100492.webm", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullHash, kNullHash);
FFMPEG_TEST_CASE(Cr100543, "security/100543.webm", PIPELINE_OK, PIPELINE_OK,
                 "c16691cc9178db3adbf7e562cadcd6e6",
                 "816d9a772a449bc29f65f58244ee04c9");
FFMPEG_TEST_CASE(Cr101458, "security/101458.webm", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullHash, kNullHash);
FFMPEG_TEST_CASE(Cr108416, "security/108416.webm", PIPELINE_OK, PIPELINE_OK,
                 "5cb3a934795cd552753dec7687928291",
                 "3e576c21f83f3c00719dbe62998d71cb");
FFMPEG_TEST_CASE(Cr110849, "security/110849.mkv",
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS, kNullHash, kNullHash);
FFMPEG_TEST_CASE(Cr112384, "security/112384.webm",
                 DEMUXER_ERROR_COULD_NOT_PARSE, DEMUXER_ERROR_COULD_NOT_PARSE,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(Cr112670, "security/112670.mp4", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE, kNullHash,
                 "59adb24ef3cdbe0297f05b395827453f");
FFMPEG_TEST_CASE(Cr112976, "security/112976.ogg", PIPELINE_OK,
                 PIPELINE_ERROR_DECODE, kNullHash,
                 "ef79f7c5805561908805eb0bb7097bb4");
FFMPEG_TEST_CASE(Cr116927, "security/116927.ogv", DEMUXER_ERROR_COULD_NOT_OPEN,
                 DEMUXER_ERROR_COULD_NOT_OPEN, kNullHash, kNullHash);
FFMPEG_TEST_CASE(Cr117912, "security/117912.webm", DEMUXER_ERROR_COULD_NOT_OPEN,
                 DEMUXER_ERROR_COULD_NOT_OPEN, kNullHash, kNullHash);
FFMPEG_TEST_CASE(Cr123481, "security/123481.ogv", PIPELINE_OK,
                 PIPELINE_OK, "e6dd853fcbd746c8bb2ab2b8fc376fc7",
                 "c96a166a09061ca94202903d7824cf04");
FFMPEG_TEST_CASE(Cr132779, "security/132779.webm",
                 DEMUXER_ERROR_COULD_NOT_PARSE, DEMUXER_ERROR_COULD_NOT_PARSE,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(Cr140165, "security/140165.ogg", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE, kNullHash,
                 "bd42757e42bdada18cb9441ee4ef8313");
FFMPEG_TEST_CASE(Cr140647, "security/140647.ogv", DEMUXER_ERROR_COULD_NOT_OPEN,
                 DEMUXER_ERROR_COULD_NOT_OPEN, kNullHash, kNullHash);
FFMPEG_TEST_CASE(Cr142738, "content/crbug142738.ogg", PIPELINE_OK, PIPELINE_OK,
                 kNullHash,
                 "03a9591e5b596eb848feeafd7693f371");
FFMPEG_TEST_CASE(Cr152691, "security/152691.mp3", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE, kNullHash,
                 "59adb24ef3cdbe0297f05b395827453f");
FFMPEG_TEST_CASE(Cr161639, "security/161639.m4a", PIPELINE_OK, PIPELINE_OK,
                 kNullHash, "97ae2fa2a2e9ff3c2cf17be96b08bbe8");

// General MKV test cases.
FFMPEG_TEST_CASE(MKV_0, "security/nested_tags_lang.mka.627.628", PIPELINE_OK,
                 PIPELINE_ERROR_DECODE, kNullHash,
                 "3fc4e8ef212df08c61acce3db34b2d09");
FFMPEG_TEST_CASE(MKV_1, "security/nested_tags_lang.mka.667.628", PIPELINE_OK,
                 PIPELINE_ERROR_DECODE, kNullHash,
                 "2f5ad3e7dd25fa5c0e8f26879953ef0f");

// General MP4 test cases.
FFMPEG_TEST_CASE(MP4_0, "security/aac.10419.mp4", DEMUXER_ERROR_COULD_NOT_OPEN,
                 DEMUXER_ERROR_COULD_NOT_OPEN, kNullHash, kNullHash);
FFMPEG_TEST_CASE(MP4_1, "security/clockh264aac_200021889.mp4",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(MP4_2, "security/clockh264aac_200701257.mp4", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "d41d8cd98f00b204e9800998ecf8427e");
FFMPEG_TEST_CASE(MP4_5, "security/clockh264aac_3022500.mp4",
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS, kNullHash, kNullHash);
FFMPEG_TEST_CASE(MP4_6, "security/clockh264aac_344289.mp4", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, kNullHash);
FFMPEG_TEST_CASE(MP4_7, "security/clockh264mp3_187697.mp4",
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(MP4_8, "security/h264.705767.mp4",
                 DEMUXER_ERROR_COULD_NOT_PARSE, DEMUXER_ERROR_COULD_NOT_PARSE,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(MP4_9, "security/smclockmp4aac_1_0.mp4",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(MP4_11, "security/null1.mp4", PIPELINE_OK, PIPELINE_OK,
                 kNullHash, "7397188f229211987268f39ef5a45b3c");
FFMPEG_TEST_CASE(MP4_16, "security/looping2.mov",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullHash, kNullHash);

// General OGV test cases.
FFMPEG_TEST_CASE(OGV_1, "security/out.163.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_2, "security/out.391.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_5, "security/smclocktheora_1_0.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_7, "security/smclocktheora_1_102.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_8, "security/smclocktheora_1_104.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_9, "security/smclocktheora_1_110.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_10, "security/smclocktheora_1_179.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_11, "security/smclocktheora_1_20.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_12, "security/smclocktheora_1_723.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_14, "security/smclocktheora_2_10405.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_15, "security/smclocktheora_2_10619.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_16, "security/smclocktheora_2_1075.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_17, "security/vorbis.482086.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_18, "security/wav.711.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_19, "security/null1.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_20, "security/null2.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_21, "security/assert1.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullHash, kNullHash);
FFMPEG_TEST_CASE(OGV_22, "security/assert2.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullHash, kNullHash);

// General WebM test cases.
FFMPEG_TEST_CASE(WEBM_1, "security/no-bug.webm", PIPELINE_OK, PIPELINE_OK,
                 "39e92700cbb77478fd63f49db855e7e5", kNullHash);
FFMPEG_TEST_CASE(WEBM_2, "security/uninitialize.webm", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE, kNullHash, kNullHash);
FFMPEG_TEST_CASE(WEBM_3, "security/out.webm.139771.2965",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(WEBM_4, "security/out.webm.68798.1929",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(WEBM_5, "content/frame_size_change.webm", PIPELINE_OK,
                 PIPELINE_OK, "d8fcf2896b7400a2261bac9e9ea930f8", kNullHash);

// Audio Functional Tests
FFMPEG_TEST_CASE(AUDIO_GAMING_0, "content/gaming/a_220_00.mp3", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "3c2e03569e2af83415a8f32065425f8c");
FFMPEG_TEST_CASE(AUDIO_GAMING_1, "content/gaming/a_220_00_v2.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "2fa0e9fca48759a7de1c22418fba7ea0");
FFMPEG_TEST_CASE(AUDIO_GAMING_2, "content/gaming/ai_laser1.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "d4f331b0f7f04e94cd70f037a1091c2b");
FFMPEG_TEST_CASE(AUDIO_GAMING_3, "content/gaming/ai_laser2.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "7b0eccb651e5572711f9c8826cc14c3c");
FFMPEG_TEST_CASE(AUDIO_GAMING_4, "content/gaming/ai_laser3.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "cd977a2dd4fa570f1a7392fc9948f184");
FFMPEG_TEST_CASE(AUDIO_GAMING_5, "content/gaming/ai_laser4.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "155caa85c878abae43428f424cdc8848");
FFMPEG_TEST_CASE(AUDIO_GAMING_6, "content/gaming/ai_laser5.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "c0f7768ac3c72aaf26ac7b6070d2392a");
FFMPEG_TEST_CASE(AUDIO_GAMING_7, "content/gaming/footstep1.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "46fab3db625f0f9b655b9affbb1fff25");
FFMPEG_TEST_CASE(AUDIO_GAMING_8, "content/gaming/footstep3.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "38b84b04eb3f1993eb97b5d46fa2a444");
FFMPEG_TEST_CASE(AUDIO_GAMING_9, "content/gaming/footstep4.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "7a3927c3026fa96562b6c19950df0be0");
FFMPEG_TEST_CASE(AUDIO_GAMING_10, "content/gaming/laser1.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "d2750f18ffce52f3763daba52117b66b");
FFMPEG_TEST_CASE(AUDIO_GAMING_11, "content/gaming/laser2.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "bb398db9b2873e03a06d486d0a6f6d3a");
FFMPEG_TEST_CASE(AUDIO_GAMING_12, "content/gaming/laser3.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "deb996d817e155ecd56766749d856e74");
FFMPEG_TEST_CASE(AUDIO_GAMING_13, "content/gaming/leg1.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "556e339fd0d1bdcb2d98f69063614067");
FFMPEG_TEST_CASE(AUDIO_GAMING_14, "content/gaming/leg2.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "313344cc2c02db5b23e336a9523b0c4a");
FFMPEG_TEST_CASE(AUDIO_GAMING_15, "content/gaming/leg3.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "25730f36ed51ba07eacca9c2b6235e6c");
FFMPEG_TEST_CASE(AUDIO_GAMING_16, "content/gaming/lock_on.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "92a3af2fc3597e7aaf5b06748daf5d6a");
FFMPEG_TEST_CASE(AUDIO_GAMING_17, "content/gaming/enemy_lock_on.ogg",
                 PIPELINE_OK, PIPELINE_OK, kNullHash,
                 "9670d8f5a668cf85f8ae8d6f8e0fdcdc");
FFMPEG_TEST_CASE(AUDIO_GAMING_18, "content/gaming/rocket_launcher.mp3",
                 PIPELINE_OK, PIPELINE_OK, kNullHash,
                 "91354320606584f4404514d914d01ee0");

// Allocate gigabytes of memory, likely can't be run on 32bit machines.
FFMPEG_TEST_CASE(BIG_MEM_1, "security/bigmem1.mov",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(BIG_MEM_2, "security/looping1.mov",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(BIG_MEM_5, "security/looping5.mov",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullHash, kNullHash);
FLAKY_FFMPEG_TEST_CASE(BIG_MEM_3, "security/looping3.mov");
FLAKY_FFMPEG_TEST_CASE(BIG_MEM_4, "security/looping4.mov");

// Flaky under threading or for other reasons.  Per rbultje, most of these will
// never be reliable since FFmpeg does not guarantee consistency in error cases.
// We only really care that these don't cause crashes or errors under tooling.
FLAKY_FFMPEG_TEST_CASE(Cr99652, "security/99652.webm");
FLAKY_FFMPEG_TEST_CASE(Cr100464, "security/100464.webm");
FLAKY_FFMPEG_TEST_CASE(Cr111342, "security/111342.ogm");
FLAKY_FFMPEG_TEST_CASE(OGV_0, "security/big_dims.ogv");
FLAKY_FFMPEG_TEST_CASE(OGV_3, "security/smclock_1_0.ogv");
FLAKY_FFMPEG_TEST_CASE(OGV_4, "security/smclock.ogv.1.0.ogv");
FLAKY_FFMPEG_TEST_CASE(OGV_6, "security/smclocktheora_1_10000.ogv");
FLAKY_FFMPEG_TEST_CASE(OGV_13, "security/smclocktheora_1_790.ogv");
FLAKY_FFMPEG_TEST_CASE(MP4_3, "security/clockh264aac_300413969.mp4");
FLAKY_FFMPEG_TEST_CASE(MP4_4, "security/clockh264aac_301350139.mp4");
FLAKY_FFMPEG_TEST_CASE(MP4_12, "security/assert1.mov");
// Not really flaky, but can't pass the seek test.
FLAKY_FFMPEG_TEST_CASE(MP4_10, "security/null1.m4a");

// Videos with massive gaps between frame timestamps that result in long hangs
// with our pipeline.  Should be uncommented when we support clockless playback.
// FFMPEG_TEST_CASE(WEBM_0, "security/memcpy.webm", PIPELINE_OK, PIPELINE_OK,
//                  kNullHash, kNullHash);
// FFMPEG_TEST_CASE(MP4_17, "security/assert2.mov", PIPELINE_OK, PIPELINE_OK,
//                  kNullHash, kNullHash);
// FFMPEG_TEST_CASE(OGV_23, "security/assert2.ogv", PIPELINE_OK, PIPELINE_OK,
//                  kNullHash, kNullHash);

TEST_P(FFmpegRegressionTest, BasicPlayback) {
  if (GetParam().init_status == PIPELINE_OK) {
    ASSERT_TRUE(Start(GetTestDataFilePath(GetParam().filename),
                      GetParam().init_status, true));
    Play();
    ASSERT_EQ(WaitUntilEndedOrError(), GetParam().end_status);
    EXPECT_EQ(GetParam().video_md5, GetVideoHash());
    EXPECT_EQ(GetParam().audio_md5, GetAudioHash());

    // Check for ended if the pipeline is expected to finish okay.
    if (GetParam().end_status == PIPELINE_OK) {
      ASSERT_TRUE(ended_);

      // Tack a seek on the end to catch any seeking issues.
      Seek(base::TimeDelta::FromMilliseconds(0));
    }
  } else {
    ASSERT_FALSE(Start(GetTestDataFilePath(GetParam().filename),
                       GetParam().init_status, true));
    EXPECT_EQ(GetParam().video_md5, GetVideoHash());
    EXPECT_EQ(GetParam().audio_md5, GetAudioHash());
  }
}

TEST_P(FlakyFFmpegRegressionTest, BasicPlayback) {
  if (Start(GetTestDataFilePath(GetParam().filename))) {
    Play();
    WaitUntilEndedOrError();
  }
}

}  // namespace media
