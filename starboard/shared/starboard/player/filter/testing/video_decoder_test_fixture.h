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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_TESTING_VIDEO_DECODER_TEST_FIXTURE_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_TESTING_VIDEO_DECODER_TEST_FIXTURE_H_

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
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
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

class VideoDecoderTestFixture {
 public:
  static const SbTimeMonotonic kDefaultWaitForNextEventTimeOut =
      5 * kSbTimeSecond;

  enum Status {
    kNeedMoreInput = VideoDecoder::kNeedMoreInput,
    kBufferFull = VideoDecoder::kBufferFull,
    kError,
    kTimeout
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

  VideoDecoderTestFixture(JobQueue* job_queue,
                          ::starboard::testing::FakeGraphicsContextProvider*
                              fake_graphics_context_provider,
                          const char* test_filename,
                          SbPlayerOutputMode output_mode,
                          bool using_stub_decoder);

  ~VideoDecoderTestFixture() {
    if (video_decoder_) {
      video_decoder_->Reset();
    }
  }

  void Initialize();

  void Render(VideoRendererSink::DrawFrameCB draw_frame_cb) {}

  void OnDecoderStatusUpdate(VideoDecoder::Status status,
                             const scoped_refptr<VideoFrame>& frame);

  void OnError();

#if SB_HAS(GLES2)
  void AssertInvalidDecodeTarget();
#endif  // SB_HAS(GLES2)

  void WaitForNextEvent(
      Event* event,
      SbTimeMonotonic timeout = kDefaultWaitForNextEventTimeOut);

  bool HasPendingEvents();

  void GetDecodeTargetWhenSupported();

  void AssertValidDecodeTargetWhenSupported();

  // This has to be called when the decoder is just initialized/reset or when
  // status is |kNeedMoreInput|.
  void WriteSingleInput(size_t index);

  void WriteEndOfStream();

  void WriteMultipleInputs(size_t start_index,
                           size_t number_of_inputs_to_write,
                           EventCB event_cb = EventCB());

  void DrainOutputs(bool* error_occurred = NULL, EventCB event_cb = EventCB());

  void ResetDecoderAndClearPendingEvents();

  scoped_refptr<InputBuffer> GetVideoInputBuffer(size_t index);

  void UseInvalidDataForInput(size_t index, uint8_t byte_to_fill) {
    invalid_inputs_[index] = byte_to_fill;
  }
  const scoped_ptr<VideoDecoder>& video_decoder() const {
    return video_decoder_;
  }
  const video_dmp::VideoDmpReader& dmp_reader() const { return dmp_reader_; }
  SbPlayerOutputMode output_mode() const { return output_mode_; }
  size_t GetDecodedFramesCount() const { return decoded_frames_.size(); }
  void PopDecodedFrame() { decoded_frames_.pop_front(); }
  void ClearDecodedFrames() { decoded_frames_.clear(); }

 protected:
  JobQueue* job_queue_;

  Mutex mutex_;
  std::deque<Event> event_queue_;

  // Test parameter filename for the VideoDmpReader to load and test with.
  const char* test_filename_;

  // Test parameter for OutputMode.
  SbPlayerOutputMode output_mode_;

  // Test parameter for whether or not to use the StubVideoDecoder, or the
  // platform-specific VideoDecoderImpl.
  bool using_stub_decoder_;

  ::starboard::testing::FakeGraphicsContextProvider*
      fake_graphics_context_provider_;
  video_dmp::VideoDmpReader dmp_reader_;
  scoped_ptr<VideoDecoder> video_decoder_;

  bool need_more_input_ = true;
  std::set<SbTime> outstanding_inputs_;
  std::deque<scoped_refptr<VideoFrame>> decoded_frames_;

  SbPlayerPrivate player_;
  scoped_ptr<VideoRenderAlgorithm> video_render_algorithm_;
  scoped_refptr<VideoRendererSink> video_renderer_sink_;

  bool end_of_stream_written_ = false;

  std::map<size_t, uint8_t> invalid_inputs_;
};

}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_TESTING_VIDEO_DECODER_TEST_FIXTURE_H_
