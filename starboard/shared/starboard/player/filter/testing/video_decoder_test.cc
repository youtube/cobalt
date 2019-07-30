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
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/stub_player_components_impl.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_HAS(PLAYER_FILTER_TESTS)
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
using ::testing::Bool;
using ::testing::Combine;
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
  EXPECT_TRUE(
      SbSystemGetPath(kSbSystemPathContentDirectory, content_path, kPathSize));
  std::string directory_path =
      std::string(content_path) + SB_FILE_SEP_CHAR + "test" + SB_FILE_SEP_CHAR +
      "starboard" + SB_FILE_SEP_CHAR + "shared" + SB_FILE_SEP_CHAR +
      "starboard" + SB_FILE_SEP_CHAR + "player" + SB_FILE_SEP_CHAR + "testdata";

  SB_CHECK(SbDirectoryCanOpen(directory_path.c_str())) << directory_path;
  return directory_path;
}

void DeallocateSampleFunc(SbPlayer player,
                          void* context,
                          const void* sample_buffer) {
  SB_UNREFERENCED_PARAMETER(player);
  SB_UNREFERENCED_PARAMETER(context);
  SB_UNREFERENCED_PARAMETER(sample_buffer);
}

std::string ResolveTestFileName(const char* filename) {
  return GetTestInputDirectory() + SB_FILE_SEP_CHAR + filename;
}

AssertionResult AlmostEqualTime(SbTime time1, SbTime time2) {
  const SbTime kEpsilon = kSbTimeSecond / 1000;
  SbTime diff = time1 - time2;
  if (-kEpsilon <= diff && diff <= kEpsilon) {
    return AssertionSuccess();
  }
  return AssertionFailure()
         << "time " << time1 << " doesn't match with time " << time2;
}

class VideoDecoderTest
    : public ::testing::TestWithParam<std::tuple<TestParam, bool>> {
 public:
  VideoDecoderTest()
      : test_filename_(std::get<0>(GetParam()).filename),
        output_mode_(std::get<0>(GetParam()).output_mode),
        using_stub_decoder_(std::get<1>(GetParam())),
        dmp_reader_(ResolveTestFileName(test_filename_).c_str()) {
    SB_LOG(INFO) << "Testing " << test_filename_
                 << (using_stub_decoder_ ? " with stub video decoder." : ".");
  }

  ~VideoDecoderTest() { video_decoder_->Reset(); }

  void SetUp() override {
    ASSERT_NE(dmp_reader_.video_codec(), kSbMediaVideoCodecNone);
    ASSERT_GT(dmp_reader_.number_of_video_buffers(), 0);
    ASSERT_TRUE(GetVideoInputBuffer(0)->video_sample_info().is_key_frame);

    SbPlayerOutputMode output_mode = output_mode_;
    ASSERT_TRUE(VideoDecoder::OutputModeSupported(
        output_mode, dmp_reader_.video_codec(), kSbDrmSystemInvalid));

    PlayerComponents::VideoParameters video_parameters = {
        &player_,
        dmp_reader_.video_codec(),
        kSbDrmSystemInvalid,
        output_mode,
        fake_graphics_context_provider_.decoder_target_provider()};

    scoped_ptr<PlayerComponents> components;
    if (using_stub_decoder_) {
      components = make_scoped_ptr<StubPlayerComponentsImpl>(
          new StubPlayerComponentsImpl);
    } else {
      components = PlayerComponents::Create();
    }
    components->CreateVideoComponents(video_parameters, &video_decoder_,
                                      &video_render_algorithm_,
                                      &video_renderer_sink_);
    ASSERT_TRUE(video_decoder_);

    if (video_renderer_sink_) {
      video_renderer_sink_->SetRenderCB(
          std::bind(&VideoDecoderTest::Render, this, _1));
    }

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

#if SB_HAS(GLES2)
  void AssertInvalidDecodeTarget() {
    if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
      auto decode_target = video_decoder_->GetCurrentDecodeTarget();
      ASSERT_FALSE(SbDecodeTargetIsValid(decode_target));
      fake_graphics_context_provider_.ReleaseDecodeTarget(decode_target);
    }
  }
#endif  // SB_HAS(GLES2)

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
  }

  bool HasPendingEvents() {
    const SbTime kDelay = 5 * kSbTimeMillisecond;
    SbThreadSleep(kDelay);
    ScopedLock scoped_lock(mutex_);
    return !event_queue_.empty();
  }

  void GetDecodeTargetWhenSupported() {
#if SB_HAS(GLES2)
    if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
      SbDecodeTarget decode_target = video_decoder_->GetCurrentDecodeTarget();
      fake_graphics_context_provider_.ReleaseDecodeTarget(decode_target);
    }
#endif  // SB_HAS(GLES2)
  }

  void AssertValidDecodeTargetWhenSupported() {
#if SB_HAS(GLES2)
    if (output_mode_ == kSbPlayerOutputModeDecodeToTexture &&
        !using_stub_decoder_) {
      SbDecodeTarget decode_target = video_decoder_->GetCurrentDecodeTarget();
      ASSERT_TRUE(SbDecodeTargetIsValid(decode_target));
      fake_graphics_context_provider_.ReleaseDecodeTarget(decode_target);
    }
#endif  // SB_HAS(GLES2)
  }

  // This has to be called when the decoder is just initialized/reseted or when
  // status is |kNeedMoreInput|.
  void WriteSingleInput(size_t index) {
    ASSERT_TRUE(need_more_input_);
    ASSERT_LT(index, dmp_reader_.number_of_video_buffers());

    auto input_buffer = GetVideoInputBuffer(index);
    {
      ScopedLock scoped_lock(mutex_);
      need_more_input_ = false;
      outstanding_inputs_.insert(input_buffer->timestamp());
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
      } else if (event.status == kError) {
        // Assume that the caller does't expect an error when |event_cb| isn't
        // provided.
        ASSERT_TRUE(event_cb);
        bool continue_process = true;
        event_cb(event, &continue_process);
        ASSERT_FALSE(continue_process);
        return;
      } else {
        ASSERT_EQ(event.status, kBufferFull);
      }
      if (event.frame) {
        ASSERT_FALSE(event.frame->is_end_of_stream());
        if (!decoded_frames_.empty()) {
          ASSERT_LT(decoded_frames_.back()->timestamp(),
                    event.frame->timestamp());
        }
        decoded_frames_.push_back(event.frame);
        ASSERT_TRUE(AlmostEqualTime(*outstanding_inputs_.begin(),
                                    event.frame->timestamp()));
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
          if (!outstanding_inputs_.empty()) {
            if (error_occurred) {
              *error_occurred = true;
            } else {
              // |error_occurred| is NULL indicates that the caller doesn't
              // expect an error, use the following redundant ASSERT to trigger
              // a failure.
              ASSERT_TRUE(outstanding_inputs_.empty());
            }
          }
        } else {
          if (!decoded_frames_.empty()) {
            ASSERT_LT(decoded_frames_.back()->timestamp(),
                      event.frame->timestamp());
          }
          decoded_frames_.push_back(event.frame);
          ASSERT_TRUE(AlmostEqualTime(*outstanding_inputs_.begin(),
                                      event.frame->timestamp()));
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
    decoded_frames_.clear();
  }

  scoped_refptr<InputBuffer> GetVideoInputBuffer(size_t index) const {
    auto video_sample_info =
        dmp_reader_.GetPlayerSampleInfo(kSbMediaTypeVideo, index);
#if SB_API_VERSION >= 11
    auto input_buffer =
        new InputBuffer(DeallocateSampleFunc, NULL, NULL, video_sample_info);
#else   // SB_API_VERSION >= 11
    auto input_buffer = new InputBuffer(kSbMediaTypeVideo, DeallocateSampleFunc,
                                        NULL, NULL, video_sample_info, NULL);
#endif  // SB_API_VERSION >= 11
    auto iter = invalid_inputs_.find(index);
    if (iter != invalid_inputs_.end()) {
      std::vector<uint8_t> content(input_buffer->size(), iter->second);
      // Replace the content with invalid data.
      input_buffer->SetDecryptedContent(content.data(),
                                        static_cast<int>(content.size()));
    }
    return input_buffer;
  }

  void UseInvalidDataForInput(size_t index, uint8_t byte_to_fill) {
    invalid_inputs_[index] = byte_to_fill;
  }

  JobQueue job_queue_;

  Mutex mutex_;
  std::deque<Event> event_queue_;

  // Test parameter filename for the VideoDmpReader to load and test with.
  const char* test_filename_;

  // Test parameter for OutputMode.
  SbPlayerOutputMode output_mode_;

  // Test parameter for whether or not to use the StubVideoDecoder, or the
  // platform-specific VideoDecoderImpl.
  bool using_stub_decoder_;

  FakeGraphicsContextProvider fake_graphics_context_provider_;
  VideoDmpReader dmp_reader_;
  scoped_ptr<VideoDecoder> video_decoder_;

  bool need_more_input_ = true;
  std::set<SbTime> outstanding_inputs_;
  std::deque<scoped_refptr<VideoFrame>> decoded_frames_;

 private:
  SbPlayerPrivate player_;
  scoped_ptr<VideoRenderAlgorithm> video_render_algorithm_;
  scoped_refptr<VideoRendererSink> video_renderer_sink_;

  bool end_of_stream_written_ = false;

  std::map<size_t, uint8_t> invalid_inputs_;
};

TEST_P(VideoDecoderTest, PrerollFrameCount) {
  EXPECT_GT(video_decoder_->GetPrerollFrameCount(), 0);
}

TEST_P(VideoDecoderTest, MaxNumberOfCachedFrames) {
  EXPECT_GT(video_decoder_->GetMaxNumberOfCachedFrames(), 1);
}

TEST_P(VideoDecoderTest, PrerollTimeout) {
  EXPECT_GE(video_decoder_->GetPrerollTimeout(), 0);
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
#if SB_API_VERSION < 11
    kSbMediaVideoCodecVp10,
#else   // SB_API_VERSION < 11
    kSbMediaVideoCodecAv1,
#endif  // SB_API_VERSION < 11
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
  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    AssertInvalidDecodeTarget();
    SbDecodeTarget decode_target = video_decoder_->GetCurrentDecodeTarget();
    EXPECT_FALSE(SbDecodeTargetIsValid(decode_target));
    fake_graphics_context_provider_.ReleaseDecodeTarget(decode_target);
  }
}
#endif  // SB_HAS(GLES2)

TEST_P(VideoDecoderTest, ThreeMoreDecoders) {
  // Create three more decoders for each supported combinations.
  const int kDecodersToCreate = 3;

  scoped_ptr<PlayerComponents> components = PlayerComponents::Create();

  SbPlayerOutputMode kOutputModes[] = {kSbPlayerOutputModeDecodeToTexture,
                                       kSbPlayerOutputModePunchOut};
  SbMediaVideoCodec kVideoCodecs[] = {
    kSbMediaVideoCodecNone,
    kSbMediaVideoCodecH264,
    kSbMediaVideoCodecH265,
    kSbMediaVideoCodecMpeg2,
    kSbMediaVideoCodecTheora,
    kSbMediaVideoCodecVc1,
#if SB_API_VERSION < 11
    kSbMediaVideoCodecVp10,
#else   // SB_API_VERSION < 11
    kSbMediaVideoCodecAv1,
#endif  // SB_API_VERSION < 11
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
          PlayerComponents::VideoParameters video_parameters = {
              &players[i],
              dmp_reader_.video_codec(),
              kSbDrmSystemInvalid,
              output_mode,
              fake_graphics_context_provider_.decoder_target_provider()};

          components->CreateVideoComponents(
              video_parameters, &video_decoders[i], &video_render_algorithms[i],
              &video_renderer_sinks[i]);
          ASSERT_TRUE(video_decoders[i]);

          if (video_renderer_sinks[i]) {
            video_renderer_sinks[i]->SetRenderCB(
                std::bind(&VideoDecoderTest::Render, this, _1));
          }

          video_decoders[i]->Initialize(
              std::bind(&VideoDecoderTest::OnDecoderStatusUpdate, this, _1, _2),
              std::bind(&VideoDecoderTest::OnError, this));

#if SB_HAS(GLES2)
          if (output_mode == kSbPlayerOutputModeDecodeToTexture) {
            AssertInvalidDecodeTarget();
          }
#endif  // SB_HAS(GLES2)
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
          AssertValidDecodeTargetWhenSupported();
        }
        *continue_process = true;
      }));
  ASSERT_FALSE(error_occurred);
}

TEST_P(VideoDecoderTest, SingleInvalidKeyFrame) {
  UseInvalidDataForInput(0, 0xab);

  WriteSingleInput(0);
  WriteEndOfStream();

  bool error_occurred = true;
  ASSERT_NO_FATAL_FAILURE(DrainOutputs(&error_occurred));
  // We don't expect the video decoder can always recover from a bad key frame
  // and to raise an error, but it shouldn't crash or hang.
  GetDecodeTargetWhenSupported();
}

TEST_P(VideoDecoderTest, MultipleValidInputsAfterInvalidKeyFrame) {
  const size_t kMaxNumberOfInputToWrite = 10;
  const size_t number_of_input_to_write =
      std::min(kMaxNumberOfInputToWrite, dmp_reader_.number_of_video_buffers());

  UseInvalidDataForInput(0, 0xab);

  bool error_occurred = false;

  // Write first few frames.  The first one is invalid and the rest are valid.
  WriteMultipleInputs(0, number_of_input_to_write,
                      [&](const Event& event, bool* continue_process) {
                        if (event.status == kError) {
                          error_occurred = true;
                          *continue_process = false;
                          return;
                        }

                        *continue_process = event.status != kBufferFull;
                      });

  if (!error_occurred) {
    GetDecodeTargetWhenSupported();
    WriteEndOfStream();
    ASSERT_NO_FATAL_FAILURE(DrainOutputs(&error_occurred));
  }
  // We don't expect the video decoder can always recover from a bad key frame
  // and to raise an error, but it shouldn't crash or hang.
  GetDecodeTargetWhenSupported();
}

TEST_P(VideoDecoderTest, MultipleInvalidInput) {
  const size_t kMaxNumberOfInputToWrite = 128;
  const size_t number_of_input_to_write =
      std::min(kMaxNumberOfInputToWrite, dmp_reader_.number_of_video_buffers());
  // Replace the content of the first few input buffers with invalid data.
  // Every test instance loads its own copy of data so this won't affect other
  // tests.
  for (size_t i = 0; i < number_of_input_to_write; ++i) {
    UseInvalidDataForInput(i, static_cast<uint8_t>(0xab + i));
  }

  bool error_occurred = false;
  WriteMultipleInputs(0, number_of_input_to_write,
                      [&](const Event& event, bool* continue_process) {
                        if (event.status == kError) {
                          error_occurred = true;
                          *continue_process = false;
                          return;
                        }

                        *continue_process = event.status != kBufferFull;
                      });

  if (!error_occurred) {
    GetDecodeTargetWhenSupported();
    WriteEndOfStream();
    ASSERT_NO_FATAL_FAILURE(DrainOutputs(&error_occurred));
  }
  // We don't expect the video decoder can always recover from a bad key frame
  // and to raise an error, but it shouldn't crash or hang.
  GetDecodeTargetWhenSupported();
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

TEST_P(VideoDecoderTest, MultipleResets) {
  for (int max_inputs = 1; max_inputs < 10; ++max_inputs) {
    WriteMultipleInputs(0, max_inputs,
                        [](const Event& event, bool* continue_process) {
                          *continue_process = event.status != kBufferFull;
                        });
    ResetDecoderAndClearPendingEvents();
    EXPECT_FALSE(HasPendingEvents());
    WriteSingleInput(0);
    WriteEndOfStream();
    ASSERT_NO_FATAL_FAILURE(DrainOutputs());
    ResetDecoderAndClearPendingEvents();
    EXPECT_FALSE(HasPendingEvents());
  }
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
  if (decoded_frames_.size() >= video_decoder_->GetMaxNumberOfCachedFrames()) {
    return;
  }
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
    if (GetVideoInputBuffer(gop_size)->video_sample_info().is_key_frame) {
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

  const char* kFilenames[] = {"beneath_the_canopy_137_avc.dmp",
                              "beneath_the_canopy_248_vp9.dmp"};

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

      const auto& video_sample_info =
          dmp_reader.GetPlayerSampleInfo(kSbMediaTypeVideo, 0)
              .video_sample_info;

      if (SbMediaIsVideoSupported(
              dmp_reader.video_codec(),
#if SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
              -1, -1, 8, kSbMediaPrimaryIdUnspecified,
              kSbMediaTransferIdUnspecified, kSbMediaMatrixIdUnspecified,
#endif  // SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
#if SB_API_VERSION >= 11
              video_sample_info.frame_width, video_sample_info.frame_height,
#else   // SB_API_VERSION >= 11
              video_sample_info->frame_width, video_sample_info->frame_height,
#endif  // SB_API_VERSION >= 11
              dmp_reader.video_bitrate(), dmp_reader.video_fps()
#if SB_API_VERSION >= 10
                                              ,
              false
#endif  // SB_API_VERSION >= 10
              )) {
        test_params.push_back({output_mode, filename});
      }
    }
  }

  SB_DCHECK(!test_params.empty());
  return test_params;
}

INSTANTIATE_TEST_CASE_P(VideoDecoderTests,
                        VideoDecoderTest,
                        Combine(ValuesIn(GetSupportedTests()), Bool()));

}  // namespace
}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
#endif  // SB_HAS(PLAYER_FILTER_TESTS)
