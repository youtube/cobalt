// Copyright 2018 Google Inc. All Rights Reserved.
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
#include <set>

#include "starboard/common/scoped_ptr.h"
#include "starboard/condition_variable.h"
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/memory.h"
#include "starboard/mutex.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/string.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

// This has to be defined in the global namespace as its instance will be used
// as SbPlayer.
struct SbPlayerPrivate {};

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
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;
using ::testing::ValuesIn;
using video_dmp::VideoDmpReader;

struct TestParam {
  SbPlayerOutputMode output_mode;
  const char* filename;
};

const SbTimeMonotonic kDefaultWaitForNextEventTimeOut = 5 * kSbTimeSecond;

std::string GetTestInputDirectory() {
  const size_t kPathSize = SB_FILE_MAX_PATH + 1;

  char content_path[kPathSize];
  EXPECT_TRUE(SbSystemGetPath(kSbSystemPathContentDirectory, content_path,
                              kPathSize));
  std::string directory_path =
      std::string(content_path) + SB_FILE_SEP_CHAR + "test" +
      SB_FILE_SEP_CHAR + "starboard" + SB_FILE_SEP_CHAR + "shared" +
      SB_FILE_SEP_CHAR + "starboard" + SB_FILE_SEP_CHAR + "player" +
      SB_FILE_SEP_CHAR + "testdata";

  SB_CHECK(SbDirectoryCanOpen(directory_path.c_str())) << directory_path;
  return directory_path;
}

std::string ResolveTestFileName(const char* filename) {
  return GetTestInputDirectory() + SB_FILE_SEP_CHAR + filename;
}

AssertionResult AlmostEqualPts(SbMediaTime pts1, SbMediaTime pts2) {
  const SbMediaTime kEpsilon = kSbMediaTimeSecond / 1000;
  SbMediaTime diff = pts1 - pts2;
  if (-kEpsilon <= diff && diff <= kEpsilon) {
    return AssertionSuccess();
  }
  return AssertionFailure()
         << "pts " << pts1 << " doesn't match with pts " << pts2;
}

class VideoDecoderTest : public ::testing::TestWithParam<TestParam> {
 public:
  VideoDecoderTest()
      : dmp_reader_(ResolveTestFileName(GetParam().filename).c_str()) {}

  void SetUp() override {
    ASSERT_NE(dmp_reader_.video_codec(), kSbMediaVideoCodecNone);
    ASSERT_GT(dmp_reader_.number_of_video_buffers(), 0);
    ASSERT_TRUE(
        dmp_reader_.GetVideoInputBuffer(0)->video_sample_info()->is_key_frame);

    SbPlayerOutputMode output_mode = GetParam().output_mode;
    ASSERT_TRUE(VideoDecoder::OutputModeSupported(
        output_mode, dmp_reader_.video_codec(), kSbDrmSystemInvalid));

    PlayerComponents::VideoParameters video_parameters = {
        &player_,
        dmp_reader_.video_codec(),
        kSbDrmSystemInvalid,
        &job_queue_,
        output_mode,
        fake_graphics_context_provider_.decoder_target_provider()};

    scoped_ptr<PlayerComponents> components = PlayerComponents::Create();
    components->CreateVideoComponents(video_parameters, &video_decoder_,
                                      &video_render_algorithm_,
                                      &video_renderer_sink_);
    ASSERT_TRUE(video_decoder_);

    video_renderer_sink_->SetRenderCB(
        std::bind(&VideoDecoderTest::Render, this, _1));

    video_decoder_->Initialize(
        std::bind(&VideoDecoderTest::OnDecoderStatusUpdate, this, _1, _2),
        std::bind(&VideoDecoderTest::OnError, this));
  }

  void Render(VideoRendererSink::DrawFrameCB draw_frame_cb) {
    SB_UNREFERENCED_PARAMETER(draw_frame_cb);
  }

  void OnDecoderStatusUpdate(VideoDecoder::Status status,
                             const scoped_refptr<VideoFrame>& frame) {
    ScopedLock scoped_lock(mutex_);
    // TODO: Ensure that this is only called during dtor or Reset().
    if (status == VideoDecoder::kReleaseAllFrames) {
      SB_DCHECK(!frame);
      event_queue_.clear();
      decoded_frames_.clear();
      return;
    } else if (status == VideoDecoder::kNeedMoreInput) {
      event_queue_.push_back(Event(kNeedMoreInput, frame));
    } else if (status == VideoDecoder::kBufferFull) {
      event_queue_.push_back(Event(kBufferFull, frame));
    } else {
      event_queue_.push_back(Event(kError, frame));
    }
  }

  void OnError() {
    ScopedLock scoped_lock(mutex_);
    event_queue_.push_back(Event(kError, NULL));
  }

 protected:
  enum Status {
    kNeedMoreInput = VideoDecoder::kNeedMoreInput,
    kBufferFull = VideoDecoder::kBufferFull,
    kError
  };

  struct Event {
    Status status;
    scoped_refptr<VideoFrame> frame;

    Event() : status(kNeedMoreInput) {}
    Event(Status status, scoped_refptr<VideoFrame> frame)
        : status(status), frame(frame) {}
  };

  // This function is called inside WriteMultipleInputs() whenever an event has
  // been processed.
  // |continue_process| will always be a valid pointer and always contains
  // |true| when calling this callback.  The callback can set it to false to
  // stop further processing.
  typedef std::function<void(const Event&, bool* continue_process)> EventCB;

  void WaitForNextEvent(
      Event* event,
      SbTimeMonotonic timeout = kDefaultWaitForNextEventTimeOut) {
    ASSERT_TRUE(event);

    SbTimeMonotonic start = SbTimeGetMonotonicNow();
    while (SbTimeGetMonotonicNow() - start < timeout) {
      job_queue_.RunUntilIdle();
      {
        ScopedLock scoped_lock(mutex_);
        if (!event_queue_.empty()) {
          *event = event_queue_.front();
          event_queue_.pop_front();
          if (event->status == kNeedMoreInput) {
            need_more_input_ = true;
          } else if (event->status == kBufferFull) {
            if (!end_of_stream_written_) {
              ASSERT_FALSE(need_more_input_);
            }
          }
          return;
        }
      }
      SbThreadSleep(kSbTimeMillisecond);
    }
    event->status = kError;
    FAIL();
  }

  bool HasPendingEvents() {
    const SbTime kDelay = 5 * kSbTimeMillisecond;
    SbThreadSleep(kDelay);
    ScopedLock scoped_lock(mutex_);
    return !event_queue_.empty();
  }

  void AssertValidDecodeTarget() {
    if (GetParam().output_mode == kSbPlayerOutputModeDecodeToTexture) {
      SbDecodeTarget decode_target = video_decoder_->GetCurrentDecodeTarget();
      ASSERT_TRUE(SbDecodeTargetIsValid(decode_target));
      fake_graphics_context_provider_.ReleaseDecodeTarget(decode_target);
    }
  }

  // This has to be called when the decoder is just initialized/reseted or when
  // status is |kNeedMoreInput|.
  void WriteSingleInput(size_t index) {
    ASSERT_TRUE(need_more_input_);
    ASSERT_LT(index, dmp_reader_.number_of_video_buffers());

    auto input_buffer = dmp_reader_.GetVideoInputBuffer(index);
    {
      ScopedLock scoped_lock(mutex_);
      need_more_input_ = false;
      outstanding_inputs_.insert(input_buffer->pts());
    }

    video_decoder_->WriteInputBuffer(input_buffer);
  }

  void WriteEndOfStream() {
    {
      ScopedLock scoped_lock(mutex_);
      end_of_stream_written_ = true;
    }
    video_decoder_->WriteEndOfStream();
  }

  void WriteMultipleInputs(size_t start_index,
                           size_t number_of_inputs_to_write,
                           EventCB event_cb = EventCB()) {
    ASSERT_LE(start_index + number_of_inputs_to_write,
              dmp_reader_.number_of_video_buffers());

    ASSERT_NO_FATAL_FAILURE(WriteSingleInput(start_index));
    ++start_index;
    --number_of_inputs_to_write;

    while (number_of_inputs_to_write > 0) {
      Event event;
      ASSERT_NO_FATAL_FAILURE(WaitForNextEvent(&event));
      if (event.status == kNeedMoreInput) {
        ASSERT_NO_FATAL_FAILURE(WriteSingleInput(start_index));
        ++start_index;
        --number_of_inputs_to_write;
      } else {
        ASSERT_EQ(event.status, kBufferFull);
      }
      if (event.frame) {
        ASSERT_FALSE(event.frame->is_end_of_stream());
        if (!decoded_frames_.empty()) {
          ASSERT_LT(decoded_frames_.back()->pts(), event.frame->pts());
        }
        decoded_frames_.push_back(event.frame);
        ASSERT_TRUE(
            AlmostEqualPts(*outstanding_inputs_.begin(), event.frame->pts()));
        outstanding_inputs_.erase(outstanding_inputs_.begin());
      }
      if (event_cb) {
        bool continue_process = true;
        event_cb(event, &continue_process);
        if (!continue_process) {
          return;
        }
      }
    }
  }

  void DrainOutputs(bool* error_occurred = NULL, EventCB event_cb = EventCB()) {
    if (error_occurred) {
      *error_occurred = false;
    }

    bool end_of_stream_decoded = false;

    while (!end_of_stream_decoded) {
      Event event;
      ASSERT_NO_FATAL_FAILURE(WaitForNextEvent(&event));
      if (event.status == kError) {
        if (error_occurred) {
          *error_occurred = true;
        } else {
          FAIL();
        }
        return;
      }
      if (event.frame) {
        if (event.frame->is_end_of_stream()) {
          end_of_stream_decoded = true;
          ASSERT_TRUE(outstanding_inputs_.empty());
        } else {
          if (!decoded_frames_.empty()) {
            ASSERT_LT(decoded_frames_.back()->pts(), event.frame->pts());
          }
          decoded_frames_.push_back(event.frame);
          ASSERT_TRUE(
              AlmostEqualPts(*outstanding_inputs_.begin(), event.frame->pts()));
          outstanding_inputs_.erase(outstanding_inputs_.begin());
        }
      }
      if (event_cb) {
        bool continue_process = true;
        event_cb(event, &continue_process);
        if (!continue_process) {
          return;
        }
      }
    }
  }

  void ResetDecoderAndClearPendingEvents() {
    video_decoder_->Reset();
    ScopedLock scoped_lock(mutex_);
    event_queue_.clear();
    need_more_input_ = true;
    end_of_stream_written_ = false;
    outstanding_inputs_.clear();
  }

  JobQueue job_queue_;

  Mutex mutex_;
  std::deque<Event> event_queue_;

  FakeGraphicsContextProvider fake_graphics_context_provider_;
  VideoDmpReader dmp_reader_;
  scoped_ptr<VideoDecoder> video_decoder_;

  bool need_more_input_ = true;
  std::set<SbMediaTime> outstanding_inputs_;
  std::deque<scoped_refptr<VideoFrame>> decoded_frames_;

 private:
  SbPlayerPrivate player_;
  scoped_ptr<VideoRenderAlgorithm> video_render_algorithm_;
  scoped_refptr<VideoRendererSink> video_renderer_sink_;

  bool end_of_stream_written_ = false;
};

TEST_P(VideoDecoderTest, PrerollFrameCount) {
  EXPECT_GT(video_decoder_->GetPrerollFrameCount(), 0);
}

TEST_P(VideoDecoderTest, PrerollTimeout) {
  EXPECT_GE(video_decoder_->GetPrerollTimeout(), 0);
}

// Ensure that OutputModeSupported() is callable on all combinations.
TEST_P(VideoDecoderTest, OutputModeSupported) {
  SbPlayerOutputMode kOutputModes[] = {kSbPlayerOutputModeDecodeToTexture,
                                       kSbPlayerOutputModePunchOut};
  SbMediaVideoCodec kVideoCodecs[] = {
      kSbMediaVideoCodecNone,  kSbMediaVideoCodecH264,   kSbMediaVideoCodecH265,
      kSbMediaVideoCodecMpeg2, kSbMediaVideoCodecTheora, kSbMediaVideoCodecVc1,
      kSbMediaVideoCodecVp10,  kSbMediaVideoCodecVp8,    kSbMediaVideoCodecVp9};
  for (auto output_mode : kOutputModes) {
    for (auto video_codec : kVideoCodecs) {
      VideoDecoder::OutputModeSupported(output_mode, video_codec,
                                        kSbDrmSystemInvalid);
    }
  }
}

TEST_P(VideoDecoderTest, GetCurrentDecodeTargetBeforeWriteInputBuffer) {
  if (GetParam().output_mode == kSbPlayerOutputModeDecodeToTexture) {
    SbDecodeTarget decode_target = video_decoder_->GetCurrentDecodeTarget();
    EXPECT_FALSE(SbDecodeTargetIsValid(decode_target));
    fake_graphics_context_provider_.ReleaseDecodeTarget(decode_target);
  }
}

TEST_P(VideoDecoderTest, ThreeMoreDecoders) {
  // Create three more decoders for each supported combinations.
  const int kDecodersToCreate = 3;

  scoped_ptr<PlayerComponents> components = PlayerComponents::Create();

  SbPlayerOutputMode kOutputModes[] = {kSbPlayerOutputModeDecodeToTexture,
                                       kSbPlayerOutputModePunchOut};
  SbMediaVideoCodec kVideoCodecs[] = {
      kSbMediaVideoCodecNone,  kSbMediaVideoCodecH264,   kSbMediaVideoCodecH265,
      kSbMediaVideoCodecMpeg2, kSbMediaVideoCodecTheora, kSbMediaVideoCodecVc1,
      kSbMediaVideoCodecVp10,  kSbMediaVideoCodecVp8,    kSbMediaVideoCodecVp9};

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
          PlayerComponents::VideoParameters video_parameters = {
              &players[i],
              dmp_reader_.video_codec(),
              kSbDrmSystemInvalid,
              &job_queue_,
              output_mode,
              fake_graphics_context_provider_.decoder_target_provider()};

          components->CreateVideoComponents(
              video_parameters, &video_decoders[i], &video_render_algorithms[i],
              &video_renderer_sinks[i]);
          ASSERT_TRUE(video_decoders[i]);

          video_renderer_sinks[i]->SetRenderCB(
              std::bind(&VideoDecoderTest::Render, this, _1));

          video_decoders[i]->Initialize(
              std::bind(&VideoDecoderTest::OnDecoderStatusUpdate, this, _1, _2),
              std::bind(&VideoDecoderTest::OnError, this));

          if (output_mode == kSbPlayerOutputModeDecodeToTexture) {
            SbDecodeTarget decode_target =
                video_decoders[i]->GetCurrentDecodeTarget();
            EXPECT_FALSE(SbDecodeTargetIsValid(decode_target));
            fake_graphics_context_provider_.ReleaseDecodeTarget(decode_target);
          }
        }
      }
    }
  }
}

TEST_P(VideoDecoderTest, SingleInput) {
  WriteSingleInput(0);
  WriteEndOfStream();

  bool error_occurred = false;
  ASSERT_NO_FATAL_FAILURE(DrainOutputs(
      &error_occurred, [=](const Event& event, bool* continue_process) {
        if (event.frame) {
          AssertValidDecodeTarget();
        }
        *continue_process = true;
      }));
  ASSERT_FALSE(error_occurred);
}

TEST_P(VideoDecoderTest, SingleInvalidInput) {
  need_more_input_ = false;
  auto input_buffer = dmp_reader_.GetVideoInputBuffer(0);
  outstanding_inputs_.insert(input_buffer->pts());
  std::vector<uint8_t> content(input_buffer->size(), 0xab);
  // Replace the content with invalid data.
  input_buffer->SetDecryptedContent(content.data(),
                                    static_cast<int>(content.size()));
  video_decoder_->WriteInputBuffer(input_buffer);

  WriteEndOfStream();

  bool error_occurred = true;
  ASSERT_NO_FATAL_FAILURE(DrainOutputs(&error_occurred));
  if (error_occurred) {
    ASSERT_TRUE(decoded_frames_.empty());
  } else {
    // We don't expect the video decoder to recover from a bad input but some
    // decoders may just return an empty frame.
    ASSERT_FALSE(decoded_frames_.empty());
    AssertValidDecodeTarget();
  }
}

TEST_P(VideoDecoderTest, EndOfStreamWithoutAnyInput) {
  WriteEndOfStream();
  ASSERT_NO_FATAL_FAILURE(DrainOutputs());
}

TEST_P(VideoDecoderTest, ResetBeforeInput) {
  EXPECT_FALSE(HasPendingEvents());
  ResetDecoderAndClearPendingEvents();
  EXPECT_FALSE(HasPendingEvents());

  WriteSingleInput(0);
  WriteEndOfStream();
  ASSERT_NO_FATAL_FAILURE(DrainOutputs());
}

TEST_P(VideoDecoderTest, ResetAfterInput) {
  const size_t kMaxInputToWrite = 10;
  WriteMultipleInputs(0, kMaxInputToWrite,
                      [](const Event& event, bool* continue_process) {
                        *continue_process = event.status != kBufferFull;
                      });

  ResetDecoderAndClearPendingEvents();
  EXPECT_FALSE(HasPendingEvents());
}

TEST_P(VideoDecoderTest, MultipleInputs) {
  const size_t kMaxNumberOfExpectedDecodedFrames = 5;
  const size_t number_of_expected_decoded_frames = std::min(
      kMaxNumberOfExpectedDecodedFrames, dmp_reader_.number_of_video_buffers());
  size_t frames_decoded = 0;
  ASSERT_NO_FATAL_FAILURE(WriteMultipleInputs(
      0, dmp_reader_.number_of_video_buffers(),
      [&](const Event& event, bool* continue_process) {
        SB_UNREFERENCED_PARAMETER(event);
        frames_decoded += decoded_frames_.size();
        decoded_frames_.clear();
        *continue_process = frames_decoded < number_of_expected_decoded_frames;
      }));
  if (frames_decoded < number_of_expected_decoded_frames) {
    WriteEndOfStream();
    ASSERT_NO_FATAL_FAILURE(DrainOutputs());
  }
}

TEST_P(VideoDecoderTest, Preroll) {
  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  SbTime preroll_timeout = video_decoder_->GetPrerollTimeout();
  ASSERT_NO_FATAL_FAILURE(WriteMultipleInputs(
      0, dmp_reader_.number_of_video_buffers(),
      [=](const Event& event, bool* continue_process) {
        SB_UNREFERENCED_PARAMETER(event);
        if (decoded_frames_.size() >= video_decoder_->GetPrerollFrameCount()) {
          *continue_process = false;
          return;
        }
        if (SbTimeGetMonotonicNow() - start >= preroll_timeout) {
          *continue_process = false;
          return;
        }
        *continue_process = true;
        return;
      }));
}

TEST_P(VideoDecoderTest, HoldFramesUntilFull) {
  ASSERT_NO_FATAL_FAILURE(WriteMultipleInputs(
      0, dmp_reader_.number_of_video_buffers(),
      [=](const Event& event, bool* continue_process) {
        SB_UNREFERENCED_PARAMETER(event);
        *continue_process = decoded_frames_.size() <
                            video_decoder_->GetMaxNumberOfCachedFrames();
      }));
  WriteEndOfStream();
  bool error_occurred = false;
  ASSERT_NO_FATAL_FAILURE(DrainOutputs(
      &error_occurred, [=](const Event& event, bool* continue_process) {
        SB_UNREFERENCED_PARAMETER(event);
        *continue_process = decoded_frames_.size() <
                            video_decoder_->GetMaxNumberOfCachedFrames();
      }));
  ASSERT_FALSE(error_occurred);
}

TEST_P(VideoDecoderTest, DecodeFullGOP) {
  int gop_size = 1;
  while (gop_size < dmp_reader_.number_of_video_buffers()) {
    if (dmp_reader_.GetVideoInputBuffer(gop_size)
            ->video_sample_info()
            ->is_key_frame) {
      break;
    }
    ++gop_size;
  }

  ASSERT_NO_FATAL_FAILURE(WriteMultipleInputs(
      0, gop_size, [=](const Event& event, bool* continue_process) {
        SB_UNREFERENCED_PARAMETER(event);
        while (decoded_frames_.size() >=
               video_decoder_->GetPrerollFrameCount()) {
          decoded_frames_.pop_front();
        }
        *continue_process = true;
      }));
  WriteEndOfStream();

  bool error_occurred = true;
  ASSERT_NO_FATAL_FAILURE(DrainOutputs(
      &error_occurred, [=](const Event& event, bool* continue_process) {
        SB_UNREFERENCED_PARAMETER(event);
        while (decoded_frames_.size() >=
               video_decoder_->GetMaxNumberOfCachedFrames()) {
          decoded_frames_.pop_front();
        }
        *continue_process = true;
      }));
  ASSERT_FALSE(error_occurred);
}

std::vector<TestParam> GetSupportedTests() {
  SbPlayerOutputMode kOutputModes[] = {kSbPlayerOutputModeDecodeToTexture,
                                       kSbPlayerOutputModePunchOut};

  const char* kFilenames[] = {"google_glass_h264_aac.dmp",
                              "google_glass_vp9_opus.dmp"};

  static std::vector<TestParam> test_params;

  if (!test_params.empty()) {
    return test_params;
  }

  for (auto filename : kFilenames) {
    VideoDmpReader dmp_reader(ResolveTestFileName(filename).c_str());
    SB_DCHECK(dmp_reader.number_of_video_buffers() > 0);

    for (auto output_mode : kOutputModes) {
      if (!VideoDecoder::OutputModeSupported(
              output_mode, dmp_reader.video_codec(), kSbDrmSystemInvalid)) {
        continue;
      }

      auto input_buffer = dmp_reader.GetVideoInputBuffer(0);
      const auto& video_sample_info = input_buffer->video_sample_info();
      if (SbMediaIsVideoSupported(
              dmp_reader.video_codec(), video_sample_info->frame_width,
              video_sample_info->frame_height, dmp_reader.video_bitrate(),
              dmp_reader.video_fps())) {
        test_params.push_back({output_mode, filename});
      }
    }
  }

  SB_DCHECK(!test_params.empty());
  return test_params;
}

INSTANTIATE_TEST_CASE_P(VideoDecoderTests,
                        VideoDecoderTest,
                        ValuesIn(GetSupportedTests()));

}  // namespace
}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
