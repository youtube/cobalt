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
                 "511bbbfd42f5bedc1a11670a5b3299c7");
FFMPEG_TEST_CASE(Cr47761, "content/crbug47761.ogg", PIPELINE_OK, PIPELINE_OK,
                 kNullHash,
                 "f281adf4c7531d1664e92eb05bf10aa8");
FFMPEG_TEST_CASE(Cr50045, "content/crbug50045.mp4", PIPELINE_OK, PIPELINE_OK,
                 "c345e9ef9ebfc6bfbcbe3f0ddc3125ba",
                 "d429bc20b7f1eafd0d8179fd128a94ed");
FFMPEG_TEST_CASE(Cr62127, "content/crbug62127.webm",
                 PIPELINE_OK, PIPELINE_OK,
                 "a064b2776fc5aef3e9cba47967a75db9", kNullHash);
FFMPEG_TEST_CASE(Cr93620, "security/93620.ogg", PIPELINE_OK, PIPELINE_OK,
                 kNullHash,
                 "a12b7a96d02f3ab68532995464275682");
FFMPEG_TEST_CASE(Cr100492, "security/100492.webm", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullHash, kNullHash);
FFMPEG_TEST_CASE(Cr100543, "security/100543.webm", PIPELINE_OK, PIPELINE_OK,
                 "c16691cc9178db3adbf7e562cadcd6e6",
                 "51347b4b4ce86562a833df3ff6af0fff");
FFMPEG_TEST_CASE(Cr101458, "security/101458.webm", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullHash, kNullHash);
FFMPEG_TEST_CASE(Cr108416, "security/108416.webm", PIPELINE_OK, PIPELINE_OK,
                 "5cb3a934795cd552753dec7687928291",
                 "5ad0c25fb915022824dc65e4fe15bffc");
FFMPEG_TEST_CASE(Cr110849, "security/110849.mkv",
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS, kNullHash, kNullHash);
FFMPEG_TEST_CASE(Cr112384, "security/112384.webm",
                 DEMUXER_ERROR_COULD_NOT_PARSE, DEMUXER_ERROR_COULD_NOT_PARSE,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(Cr112670, "security/112670.mp4", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE, kNullHash,
                 "59adb24ef3cdbe0297f05b395827453f");
FFMPEG_TEST_CASE(Cr112976, "security/112976.ogg", PIPELINE_OK, PIPELINE_OK,
                 kNullHash,
                 "d23bacec582c94b8a6dc53b0971bf67e");
FFMPEG_TEST_CASE(Cr116927, "security/116927.ogv", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE, kNullHash, kNullHash);
FFMPEG_TEST_CASE(Cr117912, "security/117912.webm", DEMUXER_ERROR_COULD_NOT_OPEN,
                 DEMUXER_ERROR_COULD_NOT_OPEN, kNullHash, kNullHash);
FFMPEG_TEST_CASE(Cr123481, "security/123481.ogv", PIPELINE_OK,
                 PIPELINE_OK, "e6dd853fcbd746c8bb2ab2b8fc376fc7",
                 "da909399f17e8f8ad7f1fcb3c4ccc33a");
FFMPEG_TEST_CASE(Cr132779, "security/132779.webm",
                 DEMUXER_ERROR_COULD_NOT_PARSE, DEMUXER_ERROR_COULD_NOT_PARSE,
                 kNullHash, kNullHash);

// General MKV test cases.
FFMPEG_TEST_CASE(MKV_0, "security/nested_tags_lang.mka.627.628", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "29b9dd707e75eb0a8cf29ec6213fd8c8");
FFMPEG_TEST_CASE(MKV_1, "security/nested_tags_lang.mka.667.628", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "117643fcab3ef06461c769686d05ccfb");

// General MP4 test cases.
FFMPEG_TEST_CASE(MP4_0, "security/aac.10419.mp4", PIPELINE_OK, PIPELINE_OK,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(MP4_1, "security/clockh264aac_200021889.mp4",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(MP4_2, "security/clockh264aac_200701257.mp4", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "d8bee104916f9f10db767d13400296b6");
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
FFMPEG_TEST_CASE(MP4_10, "security/null1.m4a", PIPELINE_OK, PIPELINE_OK,
                 kNullHash, "287b6f06b6b45ac9e2839f4f397036af");
FFMPEG_TEST_CASE(MP4_11, "security/null1.mp4", PIPELINE_OK, PIPELINE_OK,
                 kNullHash, "f20676c5de9b3c7c174c141762afb957");
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
                 PIPELINE_OK, kNullHash, "5e60018ab1b30a82d53dccb759ab25b2");
FFMPEG_TEST_CASE(AUDIO_GAMING_1, "content/gaming/a_220_00_v2.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "6a79b3c593609f3bdfbea3727c658c73");
FFMPEG_TEST_CASE(AUDIO_GAMING_2, "content/gaming/ai_laser1.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "0d1e07b4374f54e3ae18dc15d7497183");
FFMPEG_TEST_CASE(AUDIO_GAMING_3, "content/gaming/ai_laser2.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "46dfe271f7e7b72225e754dd7ab8ad1c");
FFMPEG_TEST_CASE(AUDIO_GAMING_4, "content/gaming/ai_laser3.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "ef018d911c9941b778e13e1af5bec449");
FFMPEG_TEST_CASE(AUDIO_GAMING_5, "content/gaming/ai_laser4.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "14ff475a7355826077ffad191cc09665");
FFMPEG_TEST_CASE(AUDIO_GAMING_6, "content/gaming/ai_laser5.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "351cd543f0a001b2eb5ca5fad6e384b9");
FFMPEG_TEST_CASE(AUDIO_GAMING_7, "content/gaming/footstep1.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "6edde5a895c900cf7dcd68e56abf0b0b");
FFMPEG_TEST_CASE(AUDIO_GAMING_8, "content/gaming/footstep3.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "59c29eb093a3a6b4bbe5b269837914b9");
FFMPEG_TEST_CASE(AUDIO_GAMING_9, "content/gaming/footstep4.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "3bd5c2b4618f189bc49f7cfd5dae91a9");
FFMPEG_TEST_CASE(AUDIO_GAMING_10, "content/gaming/laser1.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "199f2ccd92c278c4203665dba752b6ef");
FFMPEG_TEST_CASE(AUDIO_GAMING_11, "content/gaming/laser2.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "a1cdedcda4edabded9c4197a374b53dd");
FFMPEG_TEST_CASE(AUDIO_GAMING_12, "content/gaming/laser3.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "6ce272b185e3e6309b9878b543d1b83b");
FFMPEG_TEST_CASE(AUDIO_GAMING_13, "content/gaming/leg1.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "7a426c86203cf6073e8c08b23f5ee57d");
FFMPEG_TEST_CASE(AUDIO_GAMING_14, "content/gaming/leg2.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "d2f81558cea012adbc4cc8aafdbbfa55");
FFMPEG_TEST_CASE(AUDIO_GAMING_15, "content/gaming/leg3.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "9665107f7be378111b9e1928f6ac6dce");
FFMPEG_TEST_CASE(AUDIO_GAMING_16, "content/gaming/lock_on.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullHash, "2193dc2c9c811f96950a8615f7bf47e9");
FFMPEG_TEST_CASE(AUDIO_GAMING_17, "content/gaming/enemy_lock_on.ogg",
                 PIPELINE_OK, PIPELINE_OK, kNullHash,
                 "d9d2d8b26335efc8e5589c5c44d2a979");
FFMPEG_TEST_CASE(AUDIO_GAMING_18, "content/gaming/rocket_launcher.mp3",
                 PIPELINE_OK, PIPELINE_OK, kNullHash,
                 "05289a50acd15fa1357fe6234f82c3fe");

// Allocate gigabytes of memory, likely can't be run on 32bit machines.
FFMPEG_TEST_CASE(BIG_MEM_1, "security/bigmem1.mov",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(BIG_MEM_2, "security/looping1.mov",
                 DEMUXER_ERROR_COULD_NOT_PARSE, DEMUXER_ERROR_COULD_NOT_PARSE,
                 kNullHash, kNullHash);
FFMPEG_TEST_CASE(BIG_MEM_5, "security/looping5.mov",
                 DEMUXER_ERROR_COULD_NOT_PARSE, DEMUXER_ERROR_COULD_NOT_PARSE,
                 kNullHash, kNullHash);

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
FLAKY_FFMPEG_TEST_CASE(BIG_MEM_3, "security/looping3.mov");
FLAKY_FFMPEG_TEST_CASE(BIG_MEM_4, "security/looping4.mov");

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
    ASSERT_TRUE(Start(GetTestDataURL(GetParam().filename),
                      GetParam().init_status, true));
    Play();
    ASSERT_EQ(WaitUntilEndedOrError(), GetParam().end_status);
    EXPECT_EQ(GetVideoHash(), GetParam().video_md5);
    EXPECT_EQ(GetAudioHash(), GetParam().audio_md5);

    // Check for ended if the pipeline is expected to finish okay.
    if (GetParam().end_status == PIPELINE_OK) {
      ASSERT_TRUE(ended_);

      // Tack a seek on the end to catch any seeking issues.
      Seek(base::TimeDelta::FromMilliseconds(0));
    }
  } else {
    ASSERT_FALSE(Start(GetTestDataURL(GetParam().filename),
                       GetParam().init_status, true));
    EXPECT_EQ(GetVideoHash(), GetParam().video_md5);
    EXPECT_EQ(GetAudioHash(), GetParam().audio_md5);
  }
}

TEST_P(FlakyFFmpegRegressionTest, BasicPlayback) {
  if (Start(GetTestDataURL(GetParam().filename))) {
    Play();
    WaitUntilEndedOrError();
  }
}

}  // namespace media
