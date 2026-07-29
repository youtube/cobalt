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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_AV_AUDIO_SAMPLE_BUFFER_BUILDER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_AV_AUDIO_SAMPLE_BUFFER_BUILDER_H_

#import <AVFoundation/AVFoundation.h>

#include <string>

#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {

class AVAudioSampleBufferBuilder {
 public:
  static AVAudioSampleBufferBuilder* CreateBuilder(
      const AudioStreamInfo& audio_stream_info,
      bool is_encrypted);

  virtual ~AVAudioSampleBufferBuilder() {}
  // Note that AVAudioSampleBufferBuilder assumes input buffers are in decoding
  // order. Return false if error occurs.
  virtual bool BuildSampleBuffer(const scoped_refptr<InputBuffer>& input_buffer,
                                 int64_t media_time_offset,
                                 CMSampleBufferRef* sample_buffer,
                                 size_t* frames_in_buffer) = 0;
  virtual void Reset();

  bool BuildSilenceSampleBuffer(int64_t timestamp,
                                CMSampleBufferRef* sample_buffer,
                                size_t* frames_in_buffer);

  bool HasError() const { return error_occurred_; }
  const std::string& GetErrorMessage() const { return error_message_; }

 protected:
  explicit AVAudioSampleBufferBuilder(const AudioStreamInfo& audio_stream_info)
      : audio_stream_info_(audio_stream_info) {}
  AVAudioSampleBufferBuilder(const AVAudioSampleBufferBuilder&) = delete;
  AVAudioSampleBufferBuilder& operator=(const AVAudioSampleBufferBuilder&) =
      delete;

  void RecordOSError(const char* action, OSStatus status);
  void RecordError(const std::string& message);

  starboard::ThreadChecker thread_checker_;

  const AudioStreamInfo audio_stream_info_;
  bool error_occurred_ = false;
  std::string error_message_;
};

}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_MEDIA_AV_AUDIO_SAMPLE_BUFFER_BUILDER_H_
