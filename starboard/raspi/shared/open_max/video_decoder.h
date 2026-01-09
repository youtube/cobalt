// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_RASPI_SHARED_OPEN_MAX_VIDEO_DECODER_H_
#define STARBOARD_RASPI_SHARED_OPEN_MAX_VIDEO_DECODER_H_

#include <functional>
#include <limits>
#include <mutex>
#include <optional>
#include <queue>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/queue.h"
#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/raspi/shared/dispmanx_util.h"
#include "starboard/raspi/shared/open_max/dispmanx_resource_pool.h"
#include "starboard/raspi/shared/open_max/open_max_video_decode_component.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/thread.h"

namespace starboard {

class OpenMaxVideoDecoder : public VideoDecoder, private JobQueue::JobOwner {
 public:
  explicit OpenMaxVideoDecoder(SbMediaVideoCodec video_codec);
  ~OpenMaxVideoDecoder() override;

  void Initialize(DecoderStatusCB decoder_status_cb, ErrorCB error_cb) override;
  size_t GetPrerollFrameCount() const override { return 1; }
  int64_t GetPrerollTimeout() const override {
    return std::numeric_limits<int64_t>::max();
  }
  size_t GetMaxNumberOfCachedFrames() const override { return 12; }
  void WriteInputBuffers(const InputBuffers& input_buffers) override;
  void WriteEndOfStream() override;
  void Reset() override;
  SbDecodeTarget GetCurrentDecodeTarget() override {
    return kSbDecodeTargetInvalid;
  }

 private:
  struct Event {
    enum Type { kWriteInputBuffer, kWriteEOS, kReset };
    explicit Event(const Type type) : type(type) {
      SB_DCHECK_NE(type, kWriteInputBuffer);
    }
    explicit Event(const scoped_refptr<InputBuffer>& input_buffer)
        : type(kWriteInputBuffer), input_buffer(input_buffer) {}

    Type type;
    scoped_refptr<InputBuffer> input_buffer;
  };

  bool TryToDeliverOneFrame();
  static void* ThreadEntryPoint(void* context);
  void RunLoop();
  scoped_refptr<VideoFrame> CreateFrame(const OMX_BUFFERHEADERTYPE* buffer);

  void Update();

  // These variables will be initialized inside ctor or Initialize() and will
  // not be changed during the life time of this class.
  scoped_refptr<DispmanxResourcePool> resource_pool_;
  DecoderStatusCB decoder_status_cb_;
  ErrorCB error_cb_;
  bool eos_written_;
  bool first_input_written_ = false;

  std::optional<pthread_t> thread_;
  bool request_thread_termination_;
  Queue<Event*> queue_;

  std::mutex mutex_;
  std::queue<OMX_BUFFERHEADERTYPE*> filled_buffers_;
  std::queue<OMX_BUFFERHEADERTYPE*> freed_buffers_;

  JobQueue::JobToken update_job_token_;
  std::function<void()> update_job_;
};

}  // namespace starboard

#endif  // STARBOARD_RASPI_SHARED_OPEN_MAX_VIDEO_DECODER_H_
