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

#import "starboard/tvos/shared/media/avc_av_video_sample_buffer_builder.h"

#import "starboard/tvos/shared/media/playback_capabilities.h"

namespace starboard {

AvcAVVideoSampleBufferBuilder::~AvcAVVideoSampleBufferBuilder() {
  Reset();
}

int64_t AvcAVVideoSampleBufferBuilder::DecodingTimeNeededPerFrame() const {
  if (PlaybackCapabilities::IsAppleTVHD()) {
    return 10000;  // 10ms
  }
  return 5000;  // 5ms
}

void AvcAVVideoSampleBufferBuilder::Reset() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  AVVideoSampleBufferBuilder::Reset();
  video_config_ = std::nullopt;
  if (format_description_) {
    CFRelease(format_description_);
    format_description_ = nullptr;
  }
}

void AvcAVVideoSampleBufferBuilder::WriteInputBuffer(
    const scoped_refptr<InputBuffer>& input_buffer,
    int64_t media_time_offset) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(input_buffer);
  SB_DCHECK(!error_occurred_);
  SB_DCHECK(output_cb_);
  SB_DCHECK(input_buffer->video_stream_info().codec == kSbMediaVideoCodecH264);

  const auto& sample_info = input_buffer->video_sample_info();

  if (frame_counter_ == 0 && !sample_info.is_key_frame) {
    ReportError("The first frame should be key frame.");
    return;
  }

  const uint8_t* source_data = input_buffer->data();
  size_t data_size = input_buffer->size();
  if (sample_info.is_key_frame) {
    VideoConfig new_config(input_buffer->video_stream_info(),
                           input_buffer->data(), input_buffer->size());
    if (!new_config.is_valid()) {
      ReportError("Failed to parse video config.");
      return;
    }
    const auto& parameter_sets = new_config.avc_parameter_sets();
    if (!video_config_ || video_config_.value() != new_config) {
      video_config_ = new_config;
      if (!RefreshAVCFormatDescription(parameter_sets)) {
        return;
      }
    }
    SB_DCHECK(parameter_sets.format() == AvcParameterSets::kAnnexB);
    size_t bytes_to_skip =
        parameter_sets.combined_size_in_bytes_with_optionals();
    if (bytes_to_skip > data_size) {
      ReportError("Invalid parameter set size exceeds buffer size.");
      return;
    }
    source_data += bytes_to_skip;
    data_size -= bytes_to_skip;
  }

  CMBlockBufferRef block;
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      NULL, NULL, data_size, NULL, NULL, 0, data_size,
      kCMBlockBufferAssureMemoryNowFlag, &block);
  if (status != 0) {
    ReportOSError("BlockBufferCreate", status);
    return;
  }

  char* block_data;
  status = CMBlockBufferGetDataPointer(block, 0, &data_size, &data_size,
                                       &block_data);
  if (status != 0) {
    ReportOSError("BlockGetDataPointer", status);
    CFRelease(block);
    return;
  }

  if (!ConvertAnnexBToAvcc(source_data, data_size,
                           reinterpret_cast<uint8_t*>(block_data))) {
    ReportError("Failed to convert input data into avcc format");
    CFRelease(block);
    return;
  }

  CMSampleTimingInfo timing_info;
  timing_info.decodeTimeStamp = CMTimeMake(frame_counter_++, 1);
  timing_info.presentationTimeStamp =
      CMTimeMake(input_buffer->timestamp() + media_time_offset, 1000000);
  timing_info.duration = kCMTimeInvalid;

  CMSampleBufferRef cm_sample_buffer;
  status = CMSampleBufferCreateReady(kCFAllocatorDefault, block,
                                     format_description_, 1, 1, &timing_info, 1,
                                     &data_size, &cm_sample_buffer);
  CFRelease(block);
  if (status != 0) {
    ReportOSError("SampleBufferCreate", status);
    return;
  }

  scoped_refptr<AVSampleBuffer> sample_buffer(
      new AVSampleBuffer(cm_sample_buffer, input_buffer));
  output_cb_(sample_buffer);
}

bool AvcAVVideoSampleBufferBuilder::RefreshAVCFormatDescription(
    const AvcParameterSets& parameter_sets) {
  SB_DCHECK(parameter_sets.format() == AvcParameterSets::kAnnexB);

  if (format_description_) {
    CFRelease(format_description_);
  }
  auto parameter_sets_headless =
      parameter_sets.ConvertTo(AvcParameterSets::kHeadless);

  auto parameter_set_addresses = parameter_sets_headless.GetAddresses();
  auto parameter_set_sizes = parameter_sets_headless.GetSizesInBytes();
  SB_DCHECK(parameter_set_addresses.size() == parameter_set_sizes.size());

  const size_t kAvccLengthInBytes = 4;
  OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
      kCFAllocatorDefault, parameter_set_addresses.size(),
      parameter_set_addresses.data(), parameter_set_sizes.data(),
      kAvccLengthInBytes, &format_description_);
  if (status != 0) {
    ReportOSError("RefreshAVCFormatDescription", status);
    return false;
  }
  return true;
}

}  // namespace starboard
