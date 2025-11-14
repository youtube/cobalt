// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#import "starboard/tvos/shared/media/video_decoder.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <functional>
#include <vector>

#include "starboard/shared/starboard/decode_target/decode_target_context_runner.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/tvos/shared/media/decode_target_internal.h"
#include "starboard/tvos/shared/media/egl_adapter.h"
#include "starboard/tvos/shared/starboard_application.h"

namespace starboard {

namespace {

const size_t kDecodeTargetReleaseQueueDepth = 1;

// Increment when UIApplicationWillResignActiveNotification is received.
volatile std::atomic_int32_t s_application_inactive_counter{0};

void OnUIApplicationWillResignActive(CFNotificationCenterRef center,
                                     void* observer,
                                     CFStringRef name,
                                     const void* object,
                                     CFDictionaryRef user_info) {
  // Note that the handler may be registered multiple times if more than one
  // videos are playing.  In such case the counter will be incremented multiple
  // times when the app is inactive, this is ok given that we only care whether
  // the counter has been changed during the life time of the video decoder.
  s_application_inactive_counter.fetch_add(1, std::memory_order_relaxed);
}

class VideoFrameImpl : public VideoFrame {
 public:
  VideoFrameImpl(int64_t presentation_time,
                 const std::function<void()>& destroy_frame_cb)
      : VideoFrame(presentation_time), destroy_frame_cb_(destroy_frame_cb) {
    SB_DCHECK(destroy_frame_cb_);
  }

  ~VideoFrameImpl() { destroy_frame_cb_(); }

 private:
  std::function<void()> destroy_frame_cb_;
};

class DecodeTargetData : public SbDecodeTargetPrivate::Data {
 public:
  DecodeTargetData(CVOpenGLESTextureCacheRef texture_cache,
                   CVImageBufferRef image_buffer)
      : texture_cache_(texture_cache), textures_() {
    SB_DCHECK(texture_cache);
    SB_DCHECK(image_buffer);
    SB_DCHECK(CFGetTypeID(image_buffer) == CVPixelBufferGetTypeID());

    CFRetain(texture_cache_);

    info_.format = kSbDecodeTargetFormat2PlaneYUVNV12;
    info_.is_opaque = true;

    const int kFormats[] = {GL_ALPHA, GL_LUMINANCE_ALPHA};
    for (int i = 0; i < kNumberOfPlanes; ++i) {
      info_.planes[i].width = CVPixelBufferGetWidthOfPlane(image_buffer, i);
      info_.planes[i].height = CVPixelBufferGetHeightOfPlane(image_buffer, i);
      info_.planes[i].content_region.left = 0;
      info_.planes[i].content_region.top = 0;
      info_.planes[i].content_region.right = info_.planes[i].width;
      info_.planes[i].content_region.bottom = info_.planes[i].height;

      CVReturn status = CVOpenGLESTextureCacheCreateTextureFromImage(
          kCFAllocatorDefault, texture_cache, image_buffer, nullptr,
          GL_TEXTURE_2D, kFormats[i], info_.planes[i].width,
          info_.planes[i].height, kFormats[i], GL_UNSIGNED_BYTE, i,
          &textures_[i]);
      SB_DCHECK(status == 0);
      if (status != 0) {
        Reset();
        return;
      }

      info_.planes[i].texture = CVOpenGLESTextureGetName(textures_[i]);
      info_.planes[i].gl_texture_target =
          CVOpenGLESTextureGetTarget(textures_[i]);
      info_.planes[i].gl_texture_format = kFormats[i];
    }

    info_.width = info_.planes[0].width;
    info_.height = info_.planes[0].height;
  }

  bool is_valid() const { return textures_[0] != nullptr; }

  const SbDecodeTargetInfo& info() const override { return info_; }

 private:
  ~DecodeTargetData() override { Reset(); }

  void Reset() {
    for (int i = 0; i < kNumberOfPlanes; ++i) {
      if (textures_[i]) {
        CFRelease(textures_[i]);
        textures_[i] = nullptr;
      }
    }
    if (texture_cache_) {
      CFRelease(texture_cache_);
      texture_cache_ = nullptr;
    }
  }

  static const int kNumberOfPlanes = 2;

  CVOpenGLESTextureCacheRef texture_cache_;
  CVOpenGLESTextureRef textures_[kNumberOfPlanes];
  SbDecodeTargetInfo info_;
};

void CreateDecodeTarget(CVOpenGLESTextureCacheRef texture_cache,
                        CVImageBufferRef image_buffer,
                        int64_t timestamp,
                        SbDecodeTarget* decode_target) {
  SB_DCHECK(decode_target);

  *decode_target = kSbDecodeTargetInvalid;

  scoped_refptr<DecodeTargetData> data(
      new DecodeTargetData(texture_cache, image_buffer));
  if (data->is_valid()) {
    *decode_target = new SbDecodeTargetPrivate(data, timestamp);
  }
}

const OSStatus kOSStatusNoError = 0;

OSStatus CreateFormatDescription(const AvcParameterSets& parameter_sets,
                                 CMFormatDescriptionRef* format_description) {
  SB_DCHECK(parameter_sets.format() == AvcParameterSets::kAnnexB);
  auto parameter_sets_headless =
      parameter_sets.ConvertTo(AvcParameterSets::kHeadless);

  auto parameter_set_addresses = parameter_sets_headless.GetAddresses();
  auto parameter_set_sizes = parameter_sets_headless.GetSizesInBytes();
  SB_DCHECK(parameter_set_addresses.size() == parameter_set_sizes.size());

  const size_t kAvccLengthInBytes = 4;

  return CMVideoFormatDescriptionCreateFromH264ParameterSets(
      kCFAllocatorDefault, parameter_set_addresses.size(),
      parameter_set_addresses.data(), parameter_set_sizes.data(),
      kAvccLengthInBytes, format_description);
}

}  // namespace

TvosVideoDecoder::TvosVideoDecoder(SbPlayerOutputMode output_mode,
                                   SbDecodeTargetGraphicsContextProvider*
                                       decode_target_graphics_context_provider)
    : application_inactive_counter_(
          s_application_inactive_counter.load(std::memory_order_acquire)),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider) {
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(), this,
      OnUIApplicationWillResignActive,
      CFSTR("UIApplicationWillResignActiveNotification"), NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);
}

TvosVideoDecoder::~TvosVideoDecoder() {
  Reset();

  CFNotificationCenterRemoveEveryObserver(CFNotificationCenterGetLocalCenter(),
                                          this);

  SB_DCHECK(decoded_images_.empty());

  // At this stage the decoder has been detached from the pipeline, and
  // GetCurrentDecodeTarget() won't be called.  It is safe to access
  // |current_decode_target_| without acquiring |decoded_images_mutex_|.
  if (SbDecodeTargetIsValid(current_decode_target_)) {
    DecodeTargetContextRunner(decode_target_graphics_context_provider_)
        .RunOnGlesContext(
            std::bind(SbDecodeTargetRelease, current_decode_target_));
  }

  for (auto decode_target : decode_targets_to_release_) {
    DecodeTargetContextRunner(decode_target_graphics_context_provider_)
        .RunOnGlesContext(std::bind(SbDecodeTargetRelease, decode_target));
  }

  if (texture_cache_ != nullptr) {
    CFRelease(texture_cache_);
  }
}

void TvosVideoDecoder::Initialize(const DecoderStatusCB& decoder_status_cb,
                                  const ErrorCB& error_cb) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(decoder_status_cb);
  SB_DCHECK(error_cb);

  decoder_status_cb_ = decoder_status_cb;
  error_cb_ = error_cb;

  @autoreleasepool {
    SBDEglAdapter* egl_adapter = SBDGetApplication().eglAdapter;
    EAGLContext* egl_context =
        [egl_adapter applicationContextForStarboardContext:
                         decode_target_graphics_context_provider_->egl_context];
    CVEAGLContext context = egl_context;
    CVReturn status = CVOpenGLESTextureCacheCreate(
        kCFAllocatorDefault, nullptr, context, nullptr, &texture_cache_);
    if (status != kOSStatusNoError) {
      ReportOSError("CVOpenGLESTextureCacheCreate", status);
    }
  }  // @autoreleasepool
}

void TvosVideoDecoder::WriteInputBuffers(const InputBuffers& input_buffers) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(input_buffers.size() == 1);
  SB_DCHECK(input_buffers[0]);

  if (!job_thread_) {
    job_thread_.reset(new JobThread("video_decoder"));
  }
  const auto& input_buffer = input_buffers[0];
  job_thread_->job_queue()->Schedule(std::bind(
      &TvosVideoDecoder::WriteInputBufferInternal, this, input_buffer));
  if (++decoding_frames_ < kMaxDecodingFrames) {
    decoder_status_cb_(kNeedMoreInput, nullptr);
  }
}

void TvosVideoDecoder::WriteEndOfStream() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (!job_thread_) {
    decoder_status_cb_(kBufferFull,
                       new VideoFrame(VideoFrame::kMediaTimeEndOfStream));
    return;
  }
  job_thread_->job_queue()->Schedule(
      std::bind(&TvosVideoDecoder::WriteEndOfStreamInternal, this));
}

void TvosVideoDecoder::Reset() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (job_thread_) {
    job_thread_->job_queue()->Schedule(
        std::bind(&TvosVideoDecoder::DestroyFormatAndSession, this));
    job_thread_.reset();
  }

  decoder_status_cb_(kReleaseAllFrames, nullptr);
  stream_ended_ = false;
  error_occurred_ = false;
  decoding_frames_.store(0);
  video_config_ = std::nullopt;
  frame_counter_ = 0;
  last_frame_ = nullptr;
  last_decoded_image_ = nullptr;
}

SbDecodeTarget TvosVideoDecoder::GetCurrentDecodeTarget() {
  std::lock_guard lock(decoded_images_mutex_);
  if (decoded_images_.empty()) {
    if (SbDecodeTargetIsValid(current_decode_target_)) {
      return new SbDecodeTargetPrivate(*current_decode_target_);
    }
    return kSbDecodeTargetInvalid;
  }

  if (SbDecodeTargetIsValid(current_decode_target_) &&
      current_decode_target_->timestamp ==
          decoded_images_.front()->timestamp()) {
    return new SbDecodeTargetPrivate(*current_decode_target_);
  }

  decode_targets_to_release_.push_back(current_decode_target_);
  if (decode_targets_to_release_.size() > kDecodeTargetReleaseQueueDepth) {
    SbDecodeTargetRelease(decode_targets_to_release_.front());
    decode_targets_to_release_.erase(decode_targets_to_release_.begin());
  }
  DecodeTargetContextRunner(decode_target_graphics_context_provider_)
      .RunOnGlesContext(std::bind(CreateDecodeTarget, texture_cache_,
                                  decoded_images_.front()->image_buffer(),
                                  decoded_images_.front()->timestamp(),
                                  &current_decode_target_));
  return new SbDecodeTargetPrivate(*current_decode_target_);
}

void TvosVideoDecoder::WriteInputBufferInternal(
    const scoped_refptr<InputBuffer>& input_buffer) {
  if (error_occurred_) {
    return;
  }

  const uint8_t* source_data = input_buffer->data();
  size_t data_size = input_buffer->size();
  if (input_buffer->video_sample_info().is_key_frame) {
    VideoConfig new_config(input_buffer->video_stream_info(),
                           input_buffer->data(), input_buffer->size());
    if (!new_config.is_valid()) {
      ReportGeneralError("Failed to parse video config.");
      return;
    }
    const auto& parameter_sets = new_config.avc_parameter_sets();
    if (!video_config_ || video_config_.value() != new_config) {
      video_config_ = new_config;
      auto status = RefreshFormatAndSession(parameter_sets);
      if (status != kOSStatusNoError) {
        ReportOSError("RefreshFormatAndSession", status);
        return;
      }
    }
    SB_DCHECK(parameter_sets.format() == AvcParameterSets::kAnnexB);
    source_data += parameter_sets.combined_size_in_bytes();
    data_size -= parameter_sets.combined_size_in_bytes();
  }

  CMBlockBufferRef block_buffer;

  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      NULL, NULL, data_size, NULL, NULL, 0, data_size,
      kCMBlockBufferAssureMemoryNowFlag, &block_buffer);
  if (status != kOSStatusNoError) {
    ReportOSError("BlockBufferCreate", status);
    return;
  }

  char* block_data;
  status = CMBlockBufferGetDataPointer(block_buffer, 0, &data_size, &data_size,
                                       &block_data);
  if (status != 0) {
    ReportOSError("BlockGetDataPointer", status);
    CFRelease(block_buffer);
    return;
  }

  if (!ConvertAnnexBToAvcc(source_data, data_size,
                           reinterpret_cast<uint8_t*>(block_data))) {
    ReportGeneralError("Failed to convert input data into avcc format");
    CFRelease(block_buffer);
    return;
  }

  CMSampleTimingInfo sample_timing;
  sample_timing.decodeTimeStamp = CMTimeMake(frame_counter_++, 1000000);
  sample_timing.presentationTimeStamp =
      CMTimeMake(input_buffer->timestamp(), 1000000);
  sample_timing.duration = CMTimeMake(LLONG_MAX, 1000000);

  CMSampleBufferRef sample_buffer;
  status = CMSampleBufferCreateReady(
      kCFAllocatorDefault, block_buffer, format_description_,
      1,  // numSamples
      1,  // numSamleTimingEntry,
      &sample_timing, 1, &data_size, &sample_buffer);
  CFRelease(block_buffer);

  if (status != kOSStatusNoError) {
    ReportOSError("CreateSampleBuffer", status);
    return;
  }

  status = VTDecompressionSessionDecodeFrameWithOutputHandler(
      session_, sample_buffer, 0, nullptr,
      ^(OSStatus decode_status, VTDecodeInfoFlags info_flags,
        CVImageBufferRef image_buffer, CMTime presentation_time,
        CMTime presentation_duration) {
        if (decode_status != kOSStatusNoError) {
          ReportOSError(
              "VTDecompressionSessionDecodeFrameWithOutputHandler callback",
              decode_status);
          return;
        }
        TvosVideoDecoder::OnCompletion(presentation_time.value, image_buffer);
      });
  CMSampleBufferInvalidate(sample_buffer);
  CFRelease(sample_buffer);
  if (status != kOSStatusNoError) {
    ReportOSError("VTDecompressionSessionDecodeFrameWithOutputHandler", status);
  }
}

void TvosVideoDecoder::WriteEndOfStreamInternal() {
  if (error_occurred_) {
    return;
  }

  stream_ended_ = true;
  if (last_frame_) {
    SB_DCHECK(last_decoded_image_);
    {
      std::lock_guard lock(decoded_images_mutex_);
      decoded_images_.push_back(last_decoded_image_);
    }
    decoder_status_cb_(kBufferFull, last_frame_);
    last_frame_ = nullptr;
    last_decoded_image_ = nullptr;
  }
  decoder_status_cb_(kBufferFull,
                     new VideoFrame(VideoFrame::kMediaTimeEndOfStream));
}

OSStatus TvosVideoDecoder::RefreshFormatAndSession(
    const AvcParameterSets& parameter_sets) {
  DestroyFormatAndSession();

  OSStatus status =
      CreateFormatDescription(parameter_sets, &format_description_);
  if (status != 0) {
    return status;
  }

  SB_DCHECK(format_description_);

  const void* key = kCVPixelBufferPixelFormatTypeKey;
  uint32_t v = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
  const void* value = CFNumberCreate(nullptr, kCFNumberSInt32Type, &v);
  CFDictionaryRef attributes =
      CFDictionaryCreate(nullptr, &key, &value, 1, nullptr, nullptr);
  status = VTDecompressionSessionCreate(
      kCFAllocatorDefault, format_description_,
      nullptr,     // videoDecoderSpecification
      attributes,  // (__bridge CFDictionaryRef)pixel_buffer_attributes,
      nullptr,     // callbackRecord
      &session_);
  CFRelease(value);
  CFRelease(attributes);
  return status;
}

void TvosVideoDecoder::DestroyFormatAndSession() {
  if (format_description_ != nullptr) {
    CFRelease(format_description_);
    format_description_ = nullptr;
  }

  if (session_ != nullptr) {
    VTDecompressionSessionInvalidate(session_);
    CFRelease(session_);
    session_ = nullptr;
  }
}

void TvosVideoDecoder::OnCompletion(int64_t presentation_time,
                                    CVImageBufferRef image_buffer) {
  SB_DCHECK(CFGetTypeID(image_buffer) == CVPixelBufferGetTypeID());
  SB_DCHECK(texture_cache_);

  decoding_frames_--;

  scoped_refptr<DecodedImage> current_decoded_image =
      new DecodedImage(presentation_time, image_buffer);
  scoped_refptr<VideoFrame> current_frame = new VideoFrameImpl(
      presentation_time, std::bind(&TvosVideoDecoder::DestroyFrame, this,
                                   current_decoded_image.get()));
  // Re-order decoded frames.
  if (!last_frame_) {
    SB_DCHECK(!last_decoded_image_);
    last_frame_ = current_frame;
    last_decoded_image_ = current_decoded_image;
    decoder_status_cb_(kNeedMoreInput, nullptr);
    return;
  }
  if (last_frame_->timestamp() < current_frame->timestamp()) {
    std::swap(last_frame_, current_frame);
    std::swap(last_decoded_image_, current_decoded_image);
  }
  {
    std::lock_guard lock(decoded_images_mutex_);
    decoded_images_.push_back(current_decoded_image);
  }
  decoder_status_cb_(kNeedMoreInput, current_frame);
}

void TvosVideoDecoder::DestroyFrame(DecodedImage* decoded_image) {
  {
    std::lock_guard lock(decoded_images_mutex_);
    // The list is usually small, so a linear search is ok.
    for (auto iter = decoded_images_.begin(); iter != decoded_images_.end();
         ++iter) {
      if (iter->get() == decoded_image) {
        decoded_images_.erase(iter);
        return;
      }
    }
  }
  if (last_decoded_image_ == decoded_image) {
    last_decoded_image_ = nullptr;
    return;
  }
  SB_NOTREACHED();
}

void TvosVideoDecoder::ReportOSError(const char* action, OSStatus status) {
  SB_DCHECK(status != 0);

  std::stringstream ss;
  if (application_inactive_counter_ ==
      s_application_inactive_counter.load(std::memory_order_acquire)) {
    ss << action << " failed with error " << status;
  } else {
    ss << action << " failed with error " << status
       << " after application is inactive";
  }

  ReportGeneralError(ss.str().c_str());
}

void TvosVideoDecoder::ReportGeneralError(const char* message) {
  error_occurred_ = true;
  SB_LOG(ERROR) << "TvosVideoDecoder: " << message;

  std::stringstream ss;
  ss << "TvosVideoDecoder: " << message;
  error_cb_(kSbPlayerErrorDecode, ss.str());
}

}  // namespace starboard
