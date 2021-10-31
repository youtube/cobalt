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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_STUB_VIDEO_DECODER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_STUB_VIDEO_DECODER_H_

#include <set>

#include "starboard/common/optional.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class StubVideoDecoder : public VideoDecoder, private JobQueue::JobOwner {
 public:
  StubVideoDecoder() {}
  ~StubVideoDecoder() { Reset(); }

  void Initialize(const DecoderStatusCB& decoder_status_cb,
                  const ErrorCB& error_cb) override;

  size_t GetPrerollFrameCount() const override;
  SbTime GetPrerollTimeout() const override;
  size_t GetMaxNumberOfCachedFrames() const override;

  void WriteInputBuffer(
      const scoped_refptr<InputBuffer>& input_buffer) override;
  void WriteEndOfStream() override;
  void Reset() override;

  SbDecodeTarget GetCurrentDecodeTarget() override;

 private:
  void DecodeOneBuffer(const scoped_refptr<InputBuffer>& input_buffer);
  void DecodeEndOfStream();

  DecoderStatusCB decoder_status_cb_;
  media::VideoSampleInfo video_sample_info_;

  scoped_ptr<starboard::player::JobThread> decoder_thread_;
  // std::set<> keeps frame timestamps sorted in ascending order.
  std::set<SbTime> output_frame_timestamps_;
  // Used to determine when to send kBufferFull in DecodeOneBuffer().
  int total_input_count_ = 0;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_STUB_VIDEO_DECODER_H_
