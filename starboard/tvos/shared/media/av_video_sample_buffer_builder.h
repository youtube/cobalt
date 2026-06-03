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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_AV_VIDEO_SAMPLE_BUFFER_BUILDER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_AV_VIDEO_SAMPLE_BUFFER_BUILDER_H_

#import <AVFoundation/AVFoundation.h>

#include <functional>
#include <memory>

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {

class AVVideoSampleBufferBuilder {
 public:
  class AVSampleBuffer : public RefCountedThreadSafe<AVSampleBuffer> {
   public:
    AVSampleBuffer(CMSampleBufferRef cm_sample_buffer,
                   const scoped_refptr<InputBuffer>& input_buffer)
        : cm_sample_buffer_(cm_sample_buffer),
          input_buffer_(input_buffer),
          presentation_timestamp_(input_buffer->timestamp()) {}
    ~AVSampleBuffer() { CFRelease(cm_sample_buffer_); }

    CMSampleBufferRef cm_sample_buffer() const { return cm_sample_buffer_; }
    const scoped_refptr<InputBuffer>& input_buffer() { return input_buffer_; }
    int64_t presentation_timestamp() const { return presentation_timestamp_; }

   private:
    AVSampleBuffer(const AVSampleBuffer&) = delete;
    AVSampleBuffer& operator=(const AVSampleBuffer&) = delete;

    CMSampleBufferRef cm_sample_buffer_ = nullptr;
    const scoped_refptr<InputBuffer> input_buffer_;
    int64_t presentation_timestamp_ = 0;
  };

  typedef std::function<void(const scoped_refptr<AVSampleBuffer>)>
      AVVideoSampleBufferBuilderOutputCB;
  typedef std::function<void(const std::string& error_message)>
      AVVideoSampleBufferBuilderErrorCB;

  static AVVideoSampleBufferBuilder* CreateBuilder(
      const VideoStreamInfo& video_stream_info);

  virtual ~AVVideoSampleBufferBuilder() {}

  virtual void Initialize(const AVVideoSampleBufferBuilderOutputCB& output_cb,
                          const AVVideoSampleBufferBuilderErrorCB& error_cb);

  virtual void Reset();

  // Note that AVVideoSampleBufferBuilder assumes input buffers are in decoding
  // order. Return false if error occurs.
  virtual void WriteInputBuffer(const scoped_refptr<InputBuffer>& input_buffer,
                                int64_t media_time_offset) = 0;
  virtual void WriteEndOfStream() = 0;
  // Video renderer should not start playback until received more frames than
  // GetPrerollFrameCount() returns.
  virtual size_t GetPrerollFrameCount() const = 0;
  // It returns true if the output sample is already decoded. In that case,
  // we cannot skip some frames during prerolling.
  virtual bool IsSampleAlreadyDecoded() const = 0;
  // It returns the approximately time needed in AVSampleBufferDisplayLayer
  // to decode the frame.
  virtual int64_t DecodingTimeNeededPerFrame() const = 0;
  // Video renderer should not cache more frames than GetMaxNumberOfCachedFrames
  // returns.
  virtual size_t GetMaxNumberOfCachedFrames() const = 0;
  // Functions of cached frames watermark.
  virtual void OnCachedFramesWatermarkLow() {}
  virtual void OnCachedFramesWatermarkHigh() {}

 protected:
  AVVideoSampleBufferBuilder() = default;
  AVVideoSampleBufferBuilder(const AVVideoSampleBufferBuilder&) = delete;
  AVVideoSampleBufferBuilder& operator=(const AVVideoSampleBufferBuilder&) =
      delete;

  void ReportOSError(const char* action, OSStatus status);
  void ReportError(const std::string& message);

  starboard::ThreadChecker thread_checker_;

  AVVideoSampleBufferBuilderOutputCB output_cb_;
  AVVideoSampleBufferBuilderErrorCB error_cb_;

  std::atomic_bool error_occurred_ = {false};
};

}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_MEDIA_AV_VIDEO_SAMPLE_BUFFER_BUILDER_H_
