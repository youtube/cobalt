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

#import "starboard/tvos/shared/media/av_video_sample_buffer_builder.h"

#include <functional>

#import "internal/starboard/tvos/shared/media/vp9_hw_av_video_sample_buffer_builder.h"
#import "starboard/tvos/shared/media/avc_av_video_sample_buffer_builder.h"
#import "starboard/tvos/shared/media/playback_capabilities.h"
#if SB_IS_ARCH_ARM || SB_IS_ARCH_ARM64
#import "starboard/tvos/shared/media/vp9_sw_av_video_sample_buffer_builder.h"  // nogncheck
#endif  // SB_IS_ARCH_ARM || SB_IS_ARCH_ARM64

namespace starboard {

// static
AVVideoSampleBufferBuilder* AVVideoSampleBufferBuilder::CreateBuilder(
    const VideoStreamInfo& video_stream_info) {
  if (video_stream_info.codec == kSbMediaVideoCodecH264) {
    return new AvcAVVideoSampleBufferBuilder();
  } else if (video_stream_info.codec == kSbMediaVideoCodecVp9) {
    if (PlaybackCapabilities::IsHwVp9Supported()) {
      return new Vp9HwAVVideoSampleBufferBuilder(video_stream_info);
    }
#if SB_IS_ARCH_ARM || SB_IS_ARCH_ARM64
    return new Vp9SwAVVideoSampleBufferBuilder(video_stream_info);
#endif  // SB_IS_ARCH_ARM || SB_IS_ARCH_ARM64
  }
  SB_NOTREACHED();
  return nullptr;
}

void AVVideoSampleBufferBuilder::Initialize(
    const AVVideoSampleBufferBuilderOutputCB& output_cb,
    const AVVideoSampleBufferBuilderErrorCB& error_cb) {
  SB_DCHECK(output_cb);
  SB_DCHECK(error_cb);
  SB_DCHECK(!output_cb_);
  SB_DCHECK(!error_cb_);

  output_cb_ = output_cb;
  error_cb_ = error_cb;
}

void AVVideoSampleBufferBuilder::Reset() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  error_occurred_ = false;
}

void AVVideoSampleBufferBuilder::ReportOSError(const char* action,
                                               OSStatus status) {
  SB_DCHECK(status != 0);

  std::stringstream ss;
  ss << action << " failed with status " << status;
  ReportError(ss.str());
}

void AVVideoSampleBufferBuilder::ReportError(const std::string& message) {
  SB_DCHECK(error_cb_);
  error_occurred_ = true;
  error_cb_(message);
}

}  // namespace starboard
