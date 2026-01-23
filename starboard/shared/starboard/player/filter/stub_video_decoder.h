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

#include <memory>
#include <set>

#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard {

class StubVideoDecoder : public VideoDecoder, private JobQueue::JobOwner {
 public:
  StubVideoDecoder() {}
  ~StubVideoDecoder() { Reset(); }

  void Initialize(DecoderStatusCB decoder_status_cb, ErrorCB error_cb) override;

  size_t GetPrerollFrameCount() const override;
  int64_t GetPrerollTimeout() const override;
  size_t GetMaxNumberOfCachedFrames() const override;

  void WriteInputBuffers(const InputBuffers& input_buffers) override;
  void WriteEndOfStream() override;
  void Reset() override;

  SbDecodeTarget GetCurrentDecodeTarget() override;

 private:
  void DecodeBuffers(const InputBuffers& input_buffers);
  void DecodeEndOfStream();

  scoped_refptr<VideoFrame> CreateOutputFrame(int64_t timestamp) const;

  DecoderStatusCB decoder_status_cb_;
  VideoStreamInfo video_stream_info_;

  std::unique_ptr<JobThread> decoder_thread_;
  // std::set<> keeps frame timestamps sorted in ascending order.
  std::set<int64_t> output_frame_timestamps_;
  // Used to determine when to send kBufferFull in DecodeOneBuffer().
  int total_input_count_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_STUB_VIDEO_DECODER_H_
