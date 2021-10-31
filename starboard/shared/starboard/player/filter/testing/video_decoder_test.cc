// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <set>

#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/stub_player_components_factory.h"
#include "starboard/shared/starboard/player/filter/testing/test_util.h"
#include "starboard/shared/starboard/player/filter/testing/video_decoder_test_fixture.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace testing {
namespace {

using ::starboard::testing::FakeGraphicsContextProvider;
using ::std::placeholders::_1;
using ::std::placeholders::_2;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::ValuesIn;

class VideoDecoderTest
    : public ::testing::TestWithParam<std::tuple<VideoTestParam, bool>> {
 public:
  typedef VideoDecoderTestFixture::Event Event;
  typedef VideoDecoderTestFixture::EventCB EventCB;
  typedef VideoDecoderTestFixture::Status Status;

  VideoDecoderTest()
      : fixture_(&job_queue_,
                 &fake_graphics_context_provider_,
                 std::get<0>(std::get<0>(GetParam())),
                 std::get<1>(std::get<0>(GetParam())),
                 std::get<1>(GetParam())) {}

  void SetUp() override { fixture_.Initialize(); }

 protected:
  JobQueue job_queue_;
  FakeGraphicsContextProvider fake_graphics_context_provider_;
  VideoDecoderTestFixture fixture_;
};

TEST_P(VideoDecoderTest, PrerollFrameCount) {
  EXPECT_GT(fixture_.video_decoder()->GetPrerollFrameCount(), 0);
}

TEST_P(VideoDecoderTest, MaxNumberOfCachedFrames) {
  EXPECT_GT(fixture_.video_decoder()->GetMaxNumberOfCachedFrames(), 1);
}

TEST_P(VideoDecoderTest, PrerollTimeout) {
  EXPECT_GE(fixture_.video_decoder()->GetPrerollTimeout(), 0);
}

// Ensure that OutputModeSupported() is callable on all combinations.
TEST_P(VideoDecoderTest, OutputModeSupported) {
  SbPlayerOutputMode kOutputModes[] = {kSbPlayerOutputModeDecodeToTexture,
                                       kSbPlayerOutputModePunchOut};
  SbMediaVideoCodec kVideoCodecs[] = {
    kSbMediaVideoCodecNone,
    kSbMediaVideoCodecH264,
    kSbMediaVideoCodecH265,
    kSbMediaVideoCodecMpeg2,
    kSbMediaVideoCodecTheora,
    kSbMediaVideoCodecVc1,
    kSbMediaVideoCodecAv1,
    kSbMediaVideoCodecVp8,
    kSbMediaVideoCodecVp9
  };
  for (auto output_mode : kOutputModes) {
    for (auto video_codec : kVideoCodecs) {
      VideoDecoder::OutputModeSupported(output_mode, video_codec,
                                        kSbDrmSystemInvalid);
    }
  }
}

#if SB_HAS(GLES2)
TEST_P(VideoDecoderTest, GetCurrentDecodeTargetBeforeWriteInputBuffer) {
  if (fixture_.output_mode() == kSbPlayerOutputModeDecodeToTexture) {
    fixture_.AssertInvalidDecodeTarget();
  }
}
#endif  // SB_HAS(GLES2)

TEST_P(VideoDecoderTest, ThreeMoreDecoders) {
  // Create three more decoders for each supported combinations.
  const int kDecodersToCreate = 3;

  scoped_ptr<PlayerComponents::Factory> factory =
      PlayerComponents::Factory::Create();

  SbPlayerOutputMode kOutputModes[] = {kSbPlayerOutputModeDecodeToTexture,
                                       kSbPlayerOutputModePunchOut};
  SbMediaVideoCodec kVideoCodecs[] = {
    kSbMediaVideoCodecNone,
    kSbMediaVideoCodecH264,
    kSbMediaVideoCodecH265,
    kSbMediaVideoCodecMpeg2,
    kSbMediaVideoCodecTheora,
    kSbMediaVideoCodecVc1,
    kSbMediaVideoCodecAv1,
    kSbMediaVideoCodecVp8,
    kSbMediaVideoCodecVp9
  };

  for (auto output_mode : kOutputModes) {
    for (auto video_codec : kVideoCodecs) {
      if (VideoDecoder::OutputModeSupported(output_mode, video_codec,
                                            kSbDrmSystemInvalid)) {
        SbPlayerPrivate players[kDecodersToCreate];
        scoped_ptr<VideoDecoder> video_decoders[kDecodersToCreate];
        scoped_ptr<VideoRenderAlgorithm>
            video_render_algorithms[kDecodersToCreate];
        scoped_refptr<VideoRendererSink>
            video_renderer_sinks[kDecodersToCreate];

        for (int i = 0; i < kDecodersToCreate; ++i) {
          SbMediaAudioSampleInfo dummy_audio_sample_info = {
            kSbMediaAudioCodecNone
          };
          PlayerComponents::Factory::CreationParameters creation_parameters(
              fixture_.dmp_reader().video_codec(),
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
              CreateVideoSampleInfo(fixture_.dmp_reader().video_codec()),
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
              &players[i], output_mode,
              fake_graphics_context_provider_.decoder_target_provider(),
              nullptr);

          std::string error_message;
          ASSERT_TRUE(factory->CreateSubComponents(
              creation_parameters, nullptr, nullptr, &video_decoders[i],
              &video_render_algorithms[i], &video_renderer_sinks[i],
              &error_message));
          ASSERT_TRUE(video_decoders[i]);

          if (video_renderer_sinks[i]) {
            video_renderer_sinks[i]->SetRenderCB(
                std::bind(&VideoDecoderTestFixture::Render, &fixture_, _1));
          }

          video_decoders[i]->Initialize(
              std::bind(&VideoDecoderTestFixture::OnDecoderStatusUpdate,
                        &fixture_, _1, _2),
              std::bind(&VideoDecoderTestFixture::OnError, &fixture_));

#if SB_HAS(GLES2)
          if (output_mode == kSbPlayerOutputModeDecodeToTexture) {
            fixture_.AssertInvalidDecodeTarget();
          }
#endif  // SB_HAS(GLES2)
        }
        if (fixture_.HasPendingEvents()) {
          bool error_occurred = false;
          ASSERT_NO_FATAL_FAILURE(fixture_.DrainOutputs(&error_occurred));
          ASSERT_FALSE(error_occurred);
        }
      }
    }
  }
}

TEST_P(VideoDecoderTest, SingleInput) {
  fixture_.WriteSingleInput(0);
  fixture_.WriteEndOfStream();

  bool error_occurred = false;
  ASSERT_NO_FATAL_FAILURE(fixture_.DrainOutputs(
      &error_occurred, [=](const Event& event, bool* continue_process) {
        if (event.frame) {
          // TODO: On some platforms, decode texture will be ready only after
          // rendered by renderer, so decode target is not always available
          // at this point. We should provide a mock renderer and then check
          // the decode target here with AssertValidDecodeTargetWhenSupported().
        }
        *continue_process = true;
      }));
  ASSERT_FALSE(error_occurred);
}

TEST_P(VideoDecoderTest, SingleInvalidKeyFrame) {
  fixture_.UseInvalidDataForInput(0, 0xab);

  fixture_.WriteSingleInput(0);
  fixture_.WriteEndOfStream();

  bool error_occurred = true;
  ASSERT_NO_FATAL_FAILURE(fixture_.DrainOutputs(&error_occurred));
  // We don't expect the video decoder can always recover from a bad key frame
  // and to raise an error, but it shouldn't crash or hang.
  fixture_.GetDecodeTargetWhenSupported();
}

TEST_P(VideoDecoderTest, MultipleValidInputsAfterInvalidKeyFrame) {
  const size_t kMaxNumberOfInputToWrite = 10;
  const size_t number_of_input_to_write =
      std::min(kMaxNumberOfInputToWrite,
               fixture_.dmp_reader().number_of_video_buffers());

  fixture_.UseInvalidDataForInput(0, 0xab);

  bool error_occurred = false;
  bool timeout_occurred = false;
  // Write first few frames.  The first one is invalid and the rest are valid.
  fixture_.WriteMultipleInputs(0, number_of_input_to_write,
                               [&](const Event& event, bool* continue_process) {
                                 if (event.status == Status::kTimeout) {
                                   timeout_occurred = true;
                                   *continue_process = false;
                                   return;
                                 }
                                 if (event.status == Status::kError) {
                                   error_occurred = true;
                                   *continue_process = false;
                                   return;
                                 }
                                 *continue_process =
                                     event.status != Status::kBufferFull;
                               });
  ASSERT_FALSE(timeout_occurred);
  if (!error_occurred) {
    fixture_.GetDecodeTargetWhenSupported();
    fixture_.WriteEndOfStream();
    ASSERT_NO_FATAL_FAILURE(fixture_.DrainOutputs(&error_occurred));
  }
  // We don't expect the video decoder can always recover from a bad key frame
  // and to raise an error, but it shouldn't crash or hang.
  fixture_.GetDecodeTargetWhenSupported();
}

TEST_P(VideoDecoderTest, MultipleInvalidInput) {
  const size_t kMaxNumberOfInputToWrite = 128;
  const size_t number_of_input_to_write =
      std::min(kMaxNumberOfInputToWrite,
               fixture_.dmp_reader().number_of_video_buffers());
  // Replace the content of the first few input buffers with invalid data.
  // Every test instance loads its own copy of data so this won't affect other
  // tests.
  for (size_t i = 0; i < number_of_input_to_write; ++i) {
    fixture_.UseInvalidDataForInput(i, static_cast<uint8_t>(0xab + i));
  }

  bool error_occurred = false;
  bool timeout_occurred = false;
  fixture_.WriteMultipleInputs(0, number_of_input_to_write,
                               [&](const Event& event, bool* continue_process) {
                                 if (event.status == Status::kTimeout) {
                                   timeout_occurred = true;
                                   *continue_process = false;
                                   return;
                                 }
                                 if (event.status == Status::kError) {
                                   error_occurred = true;
                                   *continue_process = false;
                                   return;
                                 }

                                 *continue_process =
                                     event.status != Status::kBufferFull;
                               });
  ASSERT_FALSE(timeout_occurred);
  if (!error_occurred) {
    fixture_.GetDecodeTargetWhenSupported();
    fixture_.WriteEndOfStream();
    ASSERT_NO_FATAL_FAILURE(fixture_.DrainOutputs(&error_occurred));
  }
  // We don't expect the video decoder can always recover from a bad key frame
  // and to raise an error, but it shouldn't crash or hang.
  fixture_.GetDecodeTargetWhenSupported();
}

TEST_P(VideoDecoderTest, EndOfStreamWithoutAnyInput) {
  fixture_.WriteEndOfStream();
  ASSERT_NO_FATAL_FAILURE(fixture_.DrainOutputs());
}

TEST_P(VideoDecoderTest, ResetBeforeInput) {
  EXPECT_FALSE(fixture_.HasPendingEvents());
  fixture_.ResetDecoderAndClearPendingEvents();
  EXPECT_FALSE(fixture_.HasPendingEvents());

  fixture_.WriteSingleInput(0);
  fixture_.WriteEndOfStream();
  ASSERT_NO_FATAL_FAILURE(fixture_.DrainOutputs());
}

TEST_P(VideoDecoderTest, ResetAfterInput) {
  const size_t max_inputs_to_write =
      std::min<size_t>(fixture_.dmp_reader().number_of_video_buffers(), 10);
  bool error_occurred = false;
  fixture_.WriteMultipleInputs(
      0, max_inputs_to_write, [&](const Event& event, bool* continue_process) {
        if (event.status == Status::kTimeout ||
            event.status == Status::kError) {
          error_occurred = true;
          *continue_process = false;
          return;
        }
        if (fixture_.GetDecodedFramesCount() >=
            fixture_.video_decoder()->GetMaxNumberOfCachedFrames()) {
          fixture_.PopDecodedFrame();
        }
        *continue_process = event.status != Status::kBufferFull;
      });
  ASSERT_FALSE(error_occurred);
  fixture_.ResetDecoderAndClearPendingEvents();
  EXPECT_FALSE(fixture_.HasPendingEvents());
}

TEST_P(VideoDecoderTest, MultipleResets) {
  const size_t max_inputs_to_write =
      std::min<size_t>(fixture_.dmp_reader().number_of_video_buffers(), 10);
  for (int max_inputs = 1; max_inputs < max_inputs_to_write; ++max_inputs) {
    bool error_occurred = false;
    fixture_.WriteMultipleInputs(
        0, max_inputs, [&](const Event& event, bool* continue_process) {
          if (event.status == Status::kTimeout ||
              event.status == Status::kError) {
            error_occurred = true;
            *continue_process = false;
            return;
          }
          if (fixture_.GetDecodedFramesCount() >=
              fixture_.video_decoder()->GetMaxNumberOfCachedFrames()) {
            fixture_.PopDecodedFrame();
          }
          *continue_process = event.status != Status::kBufferFull;
        });
    ASSERT_FALSE(error_occurred);
    fixture_.ResetDecoderAndClearPendingEvents();
    EXPECT_FALSE(fixture_.HasPendingEvents());
    fixture_.WriteSingleInput(0);
    fixture_.WriteEndOfStream();
    ASSERT_NO_FATAL_FAILURE(fixture_.DrainOutputs());
    fixture_.ResetDecoderAndClearPendingEvents();
    EXPECT_FALSE(fixture_.HasPendingEvents());
  }
}

TEST_P(VideoDecoderTest, MultipleInputs) {
  const size_t kMaxNumberOfExpectedDecodedFrames = 5;
  const size_t number_of_expected_decoded_frames =
      std::min(kMaxNumberOfExpectedDecodedFrames,
               fixture_.dmp_reader().number_of_video_buffers());
  size_t frames_decoded = 0;
  bool error_occurred = false;
  ASSERT_NO_FATAL_FAILURE(fixture_.WriteMultipleInputs(
      0, fixture_.dmp_reader().number_of_video_buffers(),
      [&](const Event& event, bool* continue_process) {
        if (event.status == Status::kTimeout ||
            event.status == Status::kError) {
          error_occurred = true;
          *continue_process = false;
          return;
        }
        frames_decoded += fixture_.GetDecodedFramesCount();
        fixture_.ClearDecodedFrames();
        *continue_process = frames_decoded < number_of_expected_decoded_frames;
      }));
  ASSERT_FALSE(error_occurred);
  if (frames_decoded < number_of_expected_decoded_frames) {
    fixture_.WriteEndOfStream();
    ASSERT_NO_FATAL_FAILURE(fixture_.DrainOutputs());
  }
}

TEST_P(VideoDecoderTest, Preroll) {
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  SbTime preroll_timeout = fixture_.video_decoder()->GetPrerollTimeout();
  bool error_occurred = false;
  ASSERT_NO_FATAL_FAILURE(fixture_.WriteMultipleInputs(
      0, fixture_.dmp_reader().number_of_video_buffers(),
      [&](const Event& event, bool* continue_process) {
        if (event.status == Status::kError) {
          error_occurred = true;
          *continue_process = false;
          return;
        }
        if (fixture_.GetDecodedFramesCount() >=
            fixture_.video_decoder()->GetPrerollFrameCount()) {
          *continue_process = false;
          return;
        }
        if (SbTimeGetMonotonicNow() - start >= preroll_timeout) {
          // After preroll timeout, we should get at least 1 decoded frame.
          ASSERT_GT(fixture_.GetDecodedFramesCount(), 0);
          *continue_process = false;
          return;
        }
        *continue_process = true;
        return;
      }));
  ASSERT_FALSE(error_occurred);
}

TEST_P(VideoDecoderTest, HoldFramesUntilFull) {
  bool error_occurred = false;
  ASSERT_NO_FATAL_FAILURE(fixture_.WriteMultipleInputs(
      0, fixture_.dmp_reader().number_of_video_buffers(),
      [&](const Event& event, bool* continue_process) {
        if (event.status == Status::kTimeout ||
            event.status == Status::kError) {
          error_occurred = true;
          *continue_process = false;
          return;
        }
        *continue_process =
            fixture_.GetDecodedFramesCount() <
            fixture_.video_decoder()->GetMaxNumberOfCachedFrames();
      }));
  ASSERT_FALSE(error_occurred);
  fixture_.WriteEndOfStream();
  if (fixture_.GetDecodedFramesCount() >=
      fixture_.video_decoder()->GetMaxNumberOfCachedFrames()) {
    return;
  }
  ASSERT_NO_FATAL_FAILURE(fixture_.DrainOutputs(
      &error_occurred, [=](const Event& event, bool* continue_process) {
        *continue_process =
            fixture_.GetDecodedFramesCount() <
            fixture_.video_decoder()->GetMaxNumberOfCachedFrames();
      }));
  ASSERT_FALSE(error_occurred);
}

TEST_P(VideoDecoderTest, DecodeFullGOP) {
  int gop_size = 1;
  while (gop_size < fixture_.dmp_reader().number_of_video_buffers()) {
    if (fixture_.GetVideoInputBuffer(gop_size)
            ->video_sample_info()
            .is_key_frame) {
      break;
    }
    ++gop_size;
  }
  bool error_occurred = false;
  ASSERT_NO_FATAL_FAILURE(fixture_.WriteMultipleInputs(
      0, gop_size, [&](const Event& event, bool* continue_process) {
        if (event.status == Status::kTimeout ||
            event.status == Status::kError) {
          error_occurred = true;
          *continue_process = false;
          return;
        }
        // Keep 1 decoded frame, assuming it's used by renderer.
        while (fixture_.GetDecodedFramesCount() > 1) {
          fixture_.PopDecodedFrame();
        }
        *continue_process = true;
      }));
  ASSERT_FALSE(error_occurred);
  fixture_.WriteEndOfStream();

  ASSERT_NO_FATAL_FAILURE(fixture_.DrainOutputs(
      &error_occurred, [=](const Event& event, bool* continue_process) {
        // Keep 1 decoded frame, assuming it's used by renderer.
        while (fixture_.GetDecodedFramesCount() > 1) {
          fixture_.PopDecodedFrame();
        }
        *continue_process = true;
      }));
  ASSERT_FALSE(error_occurred);
}

INSTANTIATE_TEST_CASE_P(VideoDecoderTests,
                        VideoDecoderTest,
                        Combine(ValuesIn(GetSupportedVideoTests()), Bool()));

}  // namespace
}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
