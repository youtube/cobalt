// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/testing/video_decoder_test_fixture.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

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
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
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
using video_dmp::VideoDmpReader;

}  // namespace

VideoDecoderTestFixture::VideoDecoderTestFixture(
    JobQueue* job_queue,
    FakeGraphicsContextProvider* fake_graphics_context_provider,
    const char* test_filename,
    SbPlayerOutputMode output_mode,
    bool using_stub_decoder)
    : job_queue_(job_queue),
      fake_graphics_context_provider_(fake_graphics_context_provider),
      test_filename_(test_filename),
      output_mode_(output_mode),
      using_stub_decoder_(using_stub_decoder),
      dmp_reader_(ResolveTestFileName(test_filename).c_str(),
                  VideoDmpReader::kEnableReadOnDemand) {
  SB_DCHECK(job_queue_);
  SB_DCHECK(fake_graphics_context_provider_);
  SB_LOG(INFO) << "Testing " << test_filename_ << ", output mode "
               << output_mode_
               << (using_stub_decoder_ ? " with stub video decoder." : ".");
}

void VideoDecoderTestFixture::Initialize() {
  ASSERT_NE(dmp_reader_.video_codec(), kSbMediaVideoCodecNone);
  ASSERT_GT(dmp_reader_.number_of_video_buffers(), 0);
  ASSERT_TRUE(GetVideoInputBuffer(0)->video_sample_info().is_key_frame);

  SbPlayerOutputMode output_mode = output_mode_;
  ASSERT_TRUE(VideoDecoder::OutputModeSupported(
      output_mode, dmp_reader_.video_codec(), kSbDrmSystemInvalid));

  PlayerComponents::Factory::CreationParameters creation_parameters(
      dmp_reader_.video_codec(),
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
      GetVideoInputBuffer(0)->video_sample_info(),
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
      &player_, output_mode,
      fake_graphics_context_provider_->decoder_target_provider(), nullptr);

  scoped_ptr<PlayerComponents::Factory> factory;
  if (using_stub_decoder_) {
    factory = StubPlayerComponentsFactory::Create();
  } else {
    factory = PlayerComponents::Factory::Create();
  }
  std::string error_message;
  ASSERT_TRUE(factory->CreateSubComponents(
      creation_parameters, nullptr, nullptr, &video_decoder_,
      &video_render_algorithm_, &video_renderer_sink_, &error_message));
  ASSERT_TRUE(video_decoder_);

  if (video_renderer_sink_) {
    video_renderer_sink_->SetRenderCB(
        std::bind(&VideoDecoderTestFixture::Render, this, _1));
  }

  video_decoder_->Initialize(
      std::bind(&VideoDecoderTestFixture::OnDecoderStatusUpdate, this, _1, _2),
      std::bind(&VideoDecoderTestFixture::OnError, this));
  if (HasPendingEvents()) {
    bool error_occurred = false;
    ASSERT_NO_FATAL_FAILURE(DrainOutputs(&error_occurred));
    ASSERT_FALSE(error_occurred);
  }
}

void VideoDecoderTestFixture::OnDecoderStatusUpdate(
    VideoDecoder::Status status,
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
    SB_LOG(WARNING) << "OnDecoderStatusUpdate received unknown state.";
  }
}

void VideoDecoderTestFixture::OnError() {
  ScopedLock scoped_lock(mutex_);
  event_queue_.push_back(Event(kError, NULL));
  SB_LOG(WARNING) << "Video decoder received error.";
}

#if SB_HAS(GLES2)
void VideoDecoderTestFixture::AssertInvalidDecodeTarget() {
  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture &&
      !using_stub_decoder_) {
    volatile bool is_decode_target_valid = true;
    fake_graphics_context_provider_->RunOnGlesContextThread([&]() {
      SbDecodeTarget decode_target = video_decoder_->GetCurrentDecodeTarget();
      is_decode_target_valid = SbDecodeTargetIsValid(decode_target);
      SbDecodeTargetRelease(decode_target);
    });
    ASSERT_FALSE(is_decode_target_valid);
  }
}
#endif  // SB_HAS(GLES2)

void VideoDecoderTestFixture::WaitForNextEvent(Event* event,
                                               SbTimeMonotonic timeout) {
  ASSERT_TRUE(event);

  SbTimeMonotonic start = SbTimeGetMonotonicNow();
  do {
    job_queue_->RunUntilIdle();
    GetDecodeTargetWhenSupported();
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
  } while (SbTimeGetMonotonicNow() - start < timeout);
  event->status = kTimeout;
  SB_LOG(WARNING) << "WaitForNextEvent() timeout.";
}

bool VideoDecoderTestFixture::HasPendingEvents() {
  const SbTime kDelay = 5 * kSbTimeMillisecond;
  SbThreadSleep(kDelay);
  ScopedLock scoped_lock(mutex_);
  return !event_queue_.empty();
}

void VideoDecoderTestFixture::GetDecodeTargetWhenSupported() {
#if SB_HAS(GLES2)
  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture &&
      !using_stub_decoder_) {
    fake_graphics_context_provider_->RunOnGlesContextThread([&]() {
      SbDecodeTargetRelease(video_decoder_->GetCurrentDecodeTarget());
    });
  }
#endif  // SB_HAS(GLES2)
}

void VideoDecoderTestFixture::AssertValidDecodeTargetWhenSupported() {
#if SB_HAS(GLES2)
  volatile bool is_decode_target_valid = false;
  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture &&
      !using_stub_decoder_) {
    fake_graphics_context_provider_->RunOnGlesContextThread([&]() {
      SbDecodeTarget decode_target = video_decoder_->GetCurrentDecodeTarget();
      is_decode_target_valid = SbDecodeTargetIsValid(decode_target);
      SbDecodeTargetRelease(decode_target);
    });
    ASSERT_TRUE(is_decode_target_valid);
  }
#endif  // SB_HAS(GLES2)
}

// This has to be called when the decoder is just initialized/reset or when
// status is |kNeedMoreInput|.
void VideoDecoderTestFixture::WriteSingleInput(size_t index) {
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

void VideoDecoderTestFixture::WriteEndOfStream() {
  {
    ScopedLock scoped_lock(mutex_);
    end_of_stream_written_ = true;
  }
  video_decoder_->WriteEndOfStream();
}

void VideoDecoderTestFixture::WriteMultipleInputs(
    size_t start_index,
    size_t number_of_inputs_to_write,
    EventCB event_cb) {
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
    } else if (event.status == kError || event.status == kTimeout) {
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

void VideoDecoderTestFixture::DrainOutputs(bool* error_occurred,
                                           EventCB event_cb) {
  if (error_occurred) {
    *error_occurred = false;
  }

  bool end_of_stream_decoded = false;

  while (!end_of_stream_decoded) {
    Event event;
    ASSERT_NO_FATAL_FAILURE(WaitForNextEvent(&event));
    if (event.status == kError || event.status == kTimeout) {
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
          SB_LOG(WARNING) << "|outstanding_inputs_| is not empty.";
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
        SB_DCHECK(!outstanding_inputs_.empty());
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

void VideoDecoderTestFixture::ResetDecoderAndClearPendingEvents() {
  video_decoder_->Reset();
  ScopedLock scoped_lock(mutex_);
  event_queue_.clear();
  need_more_input_ = true;
  end_of_stream_written_ = false;
  outstanding_inputs_.clear();
  decoded_frames_.clear();
}

scoped_refptr<InputBuffer> VideoDecoderTestFixture::GetVideoInputBuffer(
    size_t index) {
  auto video_sample_info =
      dmp_reader_.GetPlayerSampleInfo(kSbMediaTypeVideo, index);
  auto input_buffer =
      new InputBuffer(StubDeallocateSampleFunc, NULL, NULL, video_sample_info);
  auto iter = invalid_inputs_.find(index);
  if (iter != invalid_inputs_.end()) {
    std::vector<uint8_t> content(input_buffer->size(), iter->second);
    // Replace the content with invalid data.
    input_buffer->SetDecryptedContent(content.data(),
                                      static_cast<int>(content.size()));
  }
  return input_buffer;
}

}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
