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

#include "starboard/tvos/shared/media/av_sample_buffer_audio_renderer.h"

#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/tvos/shared/media/avutil/utils.h"
#include "starboard/tvos/shared/media/playback_capabilities.h"

static NSString* kAVSBARStatusKeyPath = @"status";

namespace starboard {
namespace {

const int64_t kCheckPlaybackStatusIntervalUsec = 20000;  // 20ms
const int kPrerollFrameCount = 1024 * 8;
const int64_t kMinBufferedAudioBeforeEOSUsec = 10 * 1000000;  // 10s

bool HasRemoteAudioOutput() {
  // SbPlayerBridge::GetAudioConfigurations() reads up to 32 configurations. The
  // limit here is to avoid infinite loop and also match
  // SbPlayerBridge::GetAudioConfigurations().
  constexpr size_t kMaxAudioConfigurations = 32;
  SbMediaAudioConfiguration configuration;
  size_t index = 0;
  while (index < kMaxAudioConfigurations &&
         PlaybackCapabilities::GetAudioConfiguration(index, &configuration)) {
    switch (configuration.connector) {
      case kSbMediaAudioConnectorUnknown:
      case kSbMediaAudioConnectorAnalog:
      case kSbMediaAudioConnectorBuiltIn:
      case kSbMediaAudioConnectorHdmi:
      case kSbMediaAudioConnectorSpdif:
      case kSbMediaAudioConnectorUsb:
        break;
      case kSbMediaAudioConnectorBluetooth:
      case kSbMediaAudioConnectorRemoteWired:
      case kSbMediaAudioConnectorRemoteWireless:
      case kSbMediaAudioConnectorRemoteOther:
        return true;
    }
    index++;
  }
  return false;
}

}  // namespace

AVSBAudioRenderer::AVSBAudioRenderer(const AudioStreamInfo& audio_stream_info,
                                     SbDrmSystem drm_system)
    : audio_stream_info_(audio_stream_info) {
  if (drm_system && DrmSystemPlatform::IsSupported(drm_system)) {
    drm_system_ = static_cast<DrmSystemPlatform*>(drm_system);
  }
  sample_buffer_builder_.reset(AVAudioSampleBufferBuilder::CreateBuilder(
      audio_stream_info_, drm_system_));

  @autoreleasepool {
    audio_renderer_ = [[AVSampleBufferAudioRenderer alloc] init];

    ObserverRegistry::RegisterObserver(&observer_);
    status_observer_ = avutil::CreateKVOProxyObserver(std::bind(
        &AVSBAudioRenderer::OnStatusChanged, this, std::placeholders::_1));
    [audio_renderer_ addObserver:status_observer_
                      forKeyPath:kAVSBARStatusKeyPath
                         options:0
                         context:nil];

    // Audio route could change during playback playing. When that happens, we
    // don't have to restart the playback on tvOS. The platform would mix
    // up/down the audio output for us and the next playback will get the right
    // audio capability. If we report a kSbPlayerErrorCapabilityChanged, the h5
    // player will try to re-create the player immediately. If this happens when
    // hdmi is being unplugged/plugged, the re-creation may fail and the h5
    // player will fallback to use AVPlayer.
  }  // @autoreleasepool
}

AVSBAudioRenderer::~AVSBAudioRenderer() {
  SB_DCHECK(BelongsToCurrentThread());

  @autoreleasepool {
    [audio_renderer_ removeObserver:status_observer_
                         forKeyPath:kAVSBARStatusKeyPath];
  }  // @autoreleasepool
  ObserverRegistry::UnregisterObserver(&observer_);
}

void AVSBAudioRenderer::Initialize(const ErrorCB& error_cb,
                                   const PrerolledCB& prerolled_cb,
                                   const EndedCB& ended_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(error_cb);
  SB_DCHECK(prerolled_cb);
  SB_DCHECK(ended_cb);
  SB_DCHECK(!error_cb_);
  SB_DCHECK(!prerolled_cb_);
  SB_DCHECK(!ended_cb_);

  error_cb_ = error_cb;
  prerolled_cb_ = prerolled_cb;
  ended_cb_ = ended_cb;

  // SampleBufferBuilder may have error during initialization.
  if (sample_buffer_builder_->HasError()) {
    ReportError(sample_buffer_builder_->GetErrorMessage());
    return;
  }

  // Check if audio outputs have changed.
  bool has_remote_audio_output = HasRemoteAudioOutput();
  PlaybackCapabilities::ReloadAudioConfigurations();
  if (has_remote_audio_output != HasRemoteAudioOutput()) {
    error_occurred_ = true;
    error_cb_(kSbPlayerErrorCapabilityChanged,
              "Audio output configurations have changed.");
    return;
  }
}

void AVSBAudioRenderer::WriteSamples(const InputBuffers& input_buffers) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffers.size() == 1);
  SB_DCHECK(input_buffers[0]);
  SB_DCHECK(!eos_written_);
  SB_DCHECK(sample_buffer_builder_);

  if (error_occurred_) {
    return;
  }

  const auto& input_buffer = input_buffers[0];
  SB_DCHECK(CanAcceptMoreData());

  if (IsAudioSampleInfoSubstantiallyDifferent(
          audio_stream_info_, input_buffer->audio_stream_info())) {
    audio_stream_info_ = input_buffer->audio_stream_info();
    sample_buffer_builder_.reset(AVAudioSampleBufferBuilder::CreateBuilder(
        audio_stream_info_, drm_system_));
    if (sample_buffer_builder_->HasError()) {
      ReportError(sample_buffer_builder_->GetErrorMessage());
      return;
    }
  }

  CMSampleBufferRef sample_buffer;
  size_t frames_in_buffer;
  if (!sample_buffer_builder_->BuildSampleBuffer(
          input_buffer, media_time_offset_, &sample_buffer,
          &frames_in_buffer)) {
    ReportError(sample_buffer_builder_->GetErrorMessage());
    return;
  }

  int64_t buffer_end_time =
      input_buffer->timestamp() +
      frames_in_buffer * 1000000 / audio_stream_info_.samples_per_second;

  if (buffer_end_time < seek_to_time_) {
    return;
  }

  const SbDrmSampleInfo* drm_info = input_buffer->drm_info();
  if (drm_system_ && drm_info) {
    CFArrayRef attachments =
        CMSampleBufferGetSampleAttachmentsArray(sample_buffer, YES);
    // There should be only 1 sample attachment here for non opus audio.
    SB_DCHECK(CFArrayGetCount(attachments) == 1);

    CFMutableDictionaryRef attachment =
        (CFMutableDictionaryRef)CFArrayGetValueAtIndex(attachments, 0);

    // Attach content key and cryptor data to sample buffer.
    AVContentKey* content_key = drm_system_->GetContentKey(
        drm_info->identifier, drm_info->identifier_size);
    SB_DCHECK(content_key);

    NSError* error;
    BOOL result =
        AVSampleBufferAttachContentKey(sample_buffer, content_key, &error);
    if (!result) {
      std::stringstream ss;
      ss << "Failed to attach content key.";
      avutil::AppendAVErrorDetails(error, &ss);
      ReportError(ss.str());
      return;
    }

    CFDataRef cryptor_info = CFDataCreate(
        NULL,
        reinterpret_cast<const unsigned char*>(drm_info->subsample_mapping),
        drm_info->subsample_count * sizeof(SbDrmSubSampleMapping));
    static NSString* kCMSampleAttachmentKey_CryptorSubsampleAuxiliaryData =
        @"CryptorSubsampleAuxiliaryData";
    CFDictionarySetValue(
        attachment,
        (__bridge CFStringRef)
            kCMSampleAttachmentKey_CryptorSubsampleAuxiliaryData,
        cryptor_info);
  }

  @autoreleasepool {
    [audio_renderer_ enqueueSampleBuffer:sample_buffer];
  }  // @autoreleasepool

  last_buffer_end_time_ = buffer_end_time;

  if (seeking_ && last_buffer_end_time_ >= seek_to_time_) {
    prerolled_frames_ += frames_in_buffer;
    if (prerolled_frames_ >= kPrerollFrameCount) {
      seeking_ = false;
      Schedule(prerolled_cb_);
    }
  }

  CFRelease(sample_buffer);
}

void AVSBAudioRenderer::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(!eos_written_);
  SB_DCHECK(sample_buffer_builder_);

  if (error_occurred_) {
    return;
  }
  if (seeking_) {
    seeking_ = false;
    Schedule(prerolled_cb_);
  }
  eos_written_ = true;
  CheckIfStreamEnded();

  // Write silence buffer if buffered audio is less than
  // |kMinBufferedAudioBeforeEOSUsec|.
  if (last_buffer_end_time_ - seek_to_time_ < kMinBufferedAudioBeforeEOSUsec) {
    @autoreleasepool {
      const size_t silence_frames_to_write =
          (kMinBufferedAudioBeforeEOSUsec -
           (last_buffer_end_time_ - seek_to_time_)) *
          audio_stream_info_.samples_per_second / 1000000;
      size_t written_silence_frames = 0;
      while (written_silence_frames < silence_frames_to_write) {
        CMSampleBufferRef sample_buffer;
        size_t frames_in_buffer = 0;
        int64_t timestamp = last_buffer_end_time_ + media_time_offset_ +
                            written_silence_frames * 1000000 /
                                audio_stream_info_.samples_per_second;
        if (!sample_buffer_builder_->BuildSilenceSampleBuffer(
                timestamp, &sample_buffer, &frames_in_buffer)) {
          ReportError(sample_buffer_builder_->GetErrorMessage());
          return;
        }
        [audio_renderer_ enqueueSampleBuffer:sample_buffer];
        written_silence_frames += frames_in_buffer;
        CFRelease(sample_buffer);
      }
    }  // @autoreleasepool
  }
}

void AVSBAudioRenderer::SetVolume(double volume) {
  SB_DCHECK(BelongsToCurrentThread());
  @autoreleasepool {
    audio_renderer_.volume = volume;
  }  // @autoreleasepool
}

bool AVSBAudioRenderer::IsEndOfStreamWritten() const {
  SB_DCHECK(BelongsToCurrentThread());
  return eos_written_;
}

bool AVSBAudioRenderer::IsEndOfStreamPlayed() const {
  return eos_played_;
}

bool AVSBAudioRenderer::CanAcceptMoreData() const {
  SB_DCHECK(BelongsToCurrentThread());
  @autoreleasepool {
    return audio_renderer_.readyForMoreMediaData;
  }  // @autoreleasepool
}

void AVSBAudioRenderer::Seek(int64_t seek_to_time, int64_t media_time_offset) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(sample_buffer_builder_);

  seeking_ = true;
  seek_to_time_ = seek_to_time;
  media_time_offset_ = media_time_offset;
  prerolled_frames_ = 0;
  last_buffer_end_time_ = seek_to_time_;
  eos_written_ = false;
  is_underflow_ = false;
  CancelPendingJobs();
  sample_buffer_builder_->Reset();
  @autoreleasepool {
    [audio_renderer_ flush];
  }  // @autoreleasepool
}

bool AVSBAudioRenderer::IsUnderflow() {
  SB_DCHECK(BelongsToCurrentThread());

  const int64_t kUnderflowLowWatermarkUsec = 40000;    // 40ms
  const int64_t kUnderflowHighWatermarkUsec = 200000;  // 200ms
  int64_t enqueued_sample_in_ms = last_buffer_end_time_ - GetCurrentMediaTime();
  if (is_underflow_) {
    if (enqueued_sample_in_ms > kUnderflowHighWatermarkUsec) {
      is_underflow_ = false;
    }
  } else {
    if (enqueued_sample_in_ms < kUnderflowLowWatermarkUsec) {
      is_underflow_ = true;
    }
  }
  if (eos_written_) {
    is_underflow_ = false;
  }
  return is_underflow_;
}

void AVSBAudioRenderer::ReportError(const std::string& message) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_LOG(ERROR) << message;
  if (!error_occurred_) {
    error_occurred_ = true;
    error_cb_(kSbPlayerErrorDecode, message);
  }
}

int64_t AVSBAudioRenderer::GetCurrentMediaTime() const {
  SB_DCHECK(BelongsToCurrentThread());
  int64_t media_time =
      CMTimeConvertScale(CMTimebaseGetTime(audio_renderer_.timebase), 1000000,
                         kCMTimeRoundingMethod_QuickTime)
          .value;
  media_time = std::max(media_time - media_time_offset_, 0ll);
  return std::min(media_time, last_buffer_end_time_);
}

void AVSBAudioRenderer::CheckIfStreamEnded() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(eos_written_);

  if (GetCurrentMediaTime() >= last_buffer_end_time_) {
    eos_played_ = true;
    Schedule(ended_cb_);
  } else {
    Schedule(std::bind(&AVSBAudioRenderer::CheckIfStreamEnded, this),
             kCheckPlaybackStatusIntervalUsec);
  }
}

void AVSBAudioRenderer::OnStatusChanged(NSString* key_path) {
  // This function is called on main thread.
  if ([key_path isEqualToString:kAVSBARStatusKeyPath] &&
      audio_renderer_.status == AVQueuedSampleBufferRenderingStatusFailed) {
    int32_t lock_slot = ObserverRegistry::LockObserver(&observer_);
    if (lock_slot < 0) {
      // The observer was already unregistered.
      return;
    }
    std::stringstream ss;
    ss << "AVSBAR status failed (seek to " << seek_to_time_ << ", offset "
       << media_time_offset_ << "). ";
    avutil::AppendAVErrorDetails(audio_renderer_.error, &ss);
    // Avoid to run any callback between ObserverRegistry::LockObserver
    // and ObserverRegistry::UnlockObserver. If AVSBAudioRenderer is destroyed
    // in the callback, it may cause dead lock.
    Schedule(std::bind(&AVSBAudioRenderer::ReportError, this, ss.str()));
    ObserverRegistry::UnlockObserver(lock_slot);
  }
}

}  // namespace starboard
