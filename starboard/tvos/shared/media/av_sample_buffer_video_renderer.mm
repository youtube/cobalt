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

#include "starboard/tvos/shared/media/av_sample_buffer_video_renderer.h"

#import <AVKit/AVKit.h>
#import <UIKit/UIKit.h>
#include <libkern/OSByteOrder.h>

#import "starboard/tvos/shared/defines.h"
#include "starboard/tvos/shared/media/avutil/utils.h"
#import "starboard/tvos/shared/media/player_manager.h"
#import "starboard/tvos/shared/starboard_application.h"

static NSString* kAVSBDLStatusKeyPath = @"status";
static NSString* kAVSBDLOutputObscuredKeyPath =
    @"outputObscuredDueToInsufficientExternalProtection";

// From Apple doc and our experiments, frame rate 1 is the default value and
// won't make tvOS change the refresh rate.
constexpr int kDisplayCriteriaDefaultFrameRate = 1;

@interface SBDAVSampleBufferDisplayView : UIView
@end

@implementation SBDAVSampleBufferDisplayView

+ (Class)layerClass {
  return [AVSampleBufferDisplayLayer class];
}

@end

@interface SBAVAssetResourceLoaderDelegate
    : NSObject <AVAssetResourceLoaderDelegate> {
  NSString* content_;
}

- (instancetype)initWithTransferId:(SbMediaTransferId)transfer_id
                         frameRate:(int)frame_rate;

@end

@implementation SBAVAssetResourceLoaderDelegate

static NSString* kDummyMasterPlaylistUrl = @"cobalt://dummy_master.m3u8";

// Valid BANDWIDTH, VIDEO-RANGE, CODECS and FRAME-RATE are required for AVAsset
// to return a valid |preferredDisplayCriteria|. As long as these values are
// valid, they don't have any other effort on AVDisplayCriteria, so it's ok to
// use any valid value here.
static NSString* kDummyMasterPlaylistFormatString =
    @"#EXTM3U\n"
    @"#EXT-X-STREAM-INF:BANDWIDTH=29999999,VIDEO-RANGE=%@,CODECS=\"hvc1.2.4."
    @"L150.B0\",FRAME-RATE=%d\n"
    @"cobalt://dummy_video.m3u8r";

- (instancetype)initWithTransferId:(SbMediaTransferId)transfer_id
                         frameRate:(int)frame_rate {
  self = [super init];
  switch (transfer_id) {
    case kSbMediaTransferIdBt709:
    case kSbMediaTransferIdUnspecified:
    case kSbMediaTransferIdSmpte170M:
    case kSbMediaTransferIdSmpte240M:
      content_ = [NSString stringWithFormat:kDummyMasterPlaylistFormatString,
                                            @"SDR", frame_rate];
      break;
    case kSbMediaTransferId10BitBt2020:
    case kSbMediaTransferId12BitBt2020:
    case kSbMediaTransferIdSmpteSt2084:
      content_ = [NSString
          stringWithFormat:kDummyMasterPlaylistFormatString, @"PQ", frame_rate];
      break;
    case kSbMediaTransferIdAribStdB67:
      content_ = [NSString stringWithFormat:kDummyMasterPlaylistFormatString,
                                            @"HLG", frame_rate];
      break;
    case kSbMediaTransferIdLinear:
    case kSbMediaTransferIdGamma22:
    case kSbMediaTransferIdGamma24:
    case kSbMediaTransferIdGamma28:
    case kSbMediaTransferIdIec6196621:
    case kSbMediaTransferIdReserved0:
    case kSbMediaTransferIdReserved:
    case kSbMediaTransferIdCustom:
    case kSbMediaTransferIdUnknown:
    case kSbMediaTransferIdLog:
    case kSbMediaTransferIdLogSqrt:
    case kSbMediaTransferIdIec6196624:
    case kSbMediaTransferIdBt1361Ecg:
    case kSbMediaTransferIdSmpteSt2084NonHdr:
    case kSbMediaTransferIdSmpteSt4281:
      SB_NOTREACHED();
  }
  return self;
}

- (BOOL)resourceLoader:(AVAssetResourceLoader*)resourceLoader
    shouldWaitForLoadingOfRequestedResource:
        (AVAssetResourceLoadingRequest*)loadingRequest {
  [loadingRequest.dataRequest
      respondWithData:[content_ dataUsingEncoding:NSUTF8StringEncoding]];
  [loadingRequest finishLoading];
  return YES;
}

@end

namespace starboard {
namespace {

using std::placeholders::_1;

const int64_t kCheckPlaybackStatusIntervalUsec = 16000;
const size_t kCachedFramesLowWatermark = 16;
const size_t kCachedFramesHighWatermark = 40;
const int kRequiredBuffersInDisplayLayer = 16;

UIWindow* GetPlatformWindow() {
  SB_CHECK(!UIApplication.sharedApplication.supportsMultipleScenes);
  UIWindowScene* scene = reinterpret_cast<UIWindowScene*>(
      UIApplication.sharedApplication.connectedScenes.anyObject);
  return scene.keyWindow;
}

}  // namespace

AVSBVideoRenderer::AVSBVideoRenderer(const VideoStreamInfo& video_stream_info,
                                     SbDrmSystem drm_system)
    : video_stream_info_(video_stream_info),
      sample_buffer_builder_(
          AVVideoSampleBufferBuilder::CreateBuilder(video_stream_info)) {
  if (drm_system && DrmSystemPlatform::IsSupported(drm_system)) {
    drm_system_ = static_cast<DrmSystemPlatform*>(drm_system);
  }

  onApplicationMainThread(^{
    display_view_ = [[SBDAVSampleBufferDisplayView alloc]
        initWithFrame:[UIScreen mainScreen].bounds];
    display_layer_ = (AVSampleBufferDisplayLayer*)display_view_.layer;
    display_layer_.videoGravity = AVLayerVideoGravityResizeAspect;

    id<SBDStarboardApplication> application = SBDGetApplication();
    SBDWindowManager* windowManager = application.windowManager;
    [windowManager.currentApplicationWindow attachPlayerView:display_view_];
  });

  ObserverRegistry::RegisterObserver(&observer_);
  status_observer_ = avutil::CreateKVOProxyObserver(std::bind(
      &AVSBVideoRenderer::OnStatusChanged, this, std::placeholders::_1));
  [display_layer_ addObserver:status_observer_
                   forKeyPath:kAVSBDLStatusKeyPath
                      options:0
                      context:nil];
  [display_layer_ addObserver:status_observer_
                   forKeyPath:kAVSBDLOutputObscuredKeyPath
                      options:0
                      context:nil];

  CFNotificationCenterRef center = CFNotificationCenterGetLocalCenter();
  CFNotificationCenterAddObserver(
      center, this, &AVSBVideoRenderer::DisplayLayerNotificationCallback,
      (__bridge CFStringRef)
          AVSampleBufferDisplayLayerFailedToDecodeNotification,
      (__bridge const void*)display_layer_,
      CFNotificationSuspensionBehaviorDrop);

  // Notify tvOS the color range of current content.
  UpdatePreferredDisplayCriteria();
}

AVSBVideoRenderer::~AVSBVideoRenderer() {
  SB_DCHECK(BelongsToCurrentThread());

  CFNotificationCenterRef center = CFNotificationCenterGetLocalCenter();
  CFNotificationCenterRemoveObserver(
      center, this,
      (__bridge CFStringRef)
          AVSampleBufferDisplayLayerFailedToDecodeNotification,
      (__bridge const void*)display_layer_);

  @autoreleasepool {
    [display_layer_ removeObserver:status_observer_
                        forKeyPath:kAVSBDLStatusKeyPath];
    [display_layer_ removeObserver:status_observer_
                        forKeyPath:kAVSBDLOutputObscuredKeyPath];
  }  // @autoreleasepool
  ObserverRegistry::UnregisterObserver(&observer_);

  // Clear |video_sample_buffers_| to free memory before reset the builder.
  while (!video_sample_buffers_.empty()) {
    video_sample_buffers_.pop();
  }
  sample_buffer_builder_->Reset();

  SB_DCHECK(video_sample_buffers_.empty());

  SBDAVSampleBufferDisplayView* display_view = display_view_;
  AVSampleBufferDisplayLayer* display_layer = display_layer_;
  onApplicationMainThread(^{
    [display_layer flush];
    [display_view removeFromSuperview];

    // Clear preferred display criteria.
    GetPlatformWindow().avDisplayManager.preferredDisplayCriteria = nil;
  });
}

void AVSBVideoRenderer::Initialize(const ErrorCB& error_cb,
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

  sample_buffer_builder_->Initialize(
      std::bind(&AVSBVideoRenderer::OnSampleBufferBuilderOutput, this, _1),
      std::bind(&AVSBVideoRenderer::OnSampleBufferBuilderError, this, _1));
}

void AVSBVideoRenderer::WriteSamples(const InputBuffers& input_buffers) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffers.size() == 1);
  SB_DCHECK(input_buffers[0]);
  SB_DCHECK(!eos_written_);

  if (error_occurred_) {
    return;
  }

  SB_DCHECK(CanAcceptMoreData());

  buffers_in_sample_builder_++;
  const auto& input_buffer = input_buffers[0];
  sample_buffer_builder_->WriteInputBuffer(input_buffer, media_time_offset_);
}

void AVSBVideoRenderer::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(!eos_written_);

  if (error_occurred_) {
    return;
  }
  eos_written_ = true;
  sample_buffer_builder_->WriteEndOfStream();
  if (buffers_in_sample_builder_ == 0 && video_sample_buffers_.size() == 0) {
    if (seeking_) {
      seeking_ = false;
      Schedule(prerolled_cb_);
    }
    CheckIfStreamEnded();
  }
}

void AVSBVideoRenderer::Seek(int64_t seek_to_time) {
  SB_DCHECK(BelongsToCurrentThread());
  // Seek() is called in FilterBasedPlayerWorkerHandler. After Seek()
  // is called, SetMediaTimeOffset() will be called in AVSBSynchronizer.
  seeking_ = true;
  seek_to_time_ = seek_to_time;
  eos_written_ = false;
  // Clear |video_sample_buffers_| to free memories before reset the builder.
  while (!video_sample_buffers_.empty()) {
    video_sample_buffers_.pop();
  }
  sample_buffer_builder_->Reset();
  CancelPendingJobs();
  enqueue_sample_buffers_job_token_.ResetToInvalid();

  prerolled_frames_ = 0;
  pts_of_first_written_buffer_ = 0;
  pts_of_last_output_buffer_ = seek_to_time_;
  buffers_in_sample_builder_ = 0;
  total_written_frames_ = 0;
  total_dropped_frames_ = 0;
  frames_per_second_ = 0;
  is_first_sample_written_ = false;
  is_cached_frames_below_low_watermark = false;
  is_underflow_ = false;

  SB_DCHECK(video_sample_buffers_.size() == 0);
}

bool AVSBVideoRenderer::IsEndOfStreamWritten() const {
  SB_DCHECK(BelongsToCurrentThread());
  return eos_written_;
}
bool AVSBVideoRenderer::CanAcceptMoreData() const {
  SB_DCHECK(BelongsToCurrentThread());
  // The number returned by GetNumberOfCachedFrames() is not accruate. It's
  // based on current media time.
  return buffers_in_sample_builder_ + GetNumberOfCachedFrames() <=
         sample_buffer_builder_->GetMaxNumberOfCachedFrames();
}

void AVSBVideoRenderer::SetBounds(int z_index,
                                  int x,
                                  int y,
                                  int width,
                                  int height) {
  SBDAVSampleBufferDisplayView* display_view = display_view_;
  AVSampleBufferDisplayLayer* display_layer = display_layer_;
  onApplicationMainThread(^{
    float scale = [UIScreen mainScreen].scale;
    display_view.frame =
        CGRectMake(x / scale, y / scale, width / scale, height / scale);
    display_layer.zPosition = z_index;
  });
}

void AVSBVideoRenderer::SetMediaTimeOffset(int64_t media_time_offset) {
  SB_DCHECK(BelongsToCurrentThread());
  // SetMediaTimeOffset() is called in AVSBSynchronizer.
  media_time_offset_ = media_time_offset;
  // Flush |display_layer_| after AVSBSynchronizer set rate and time to zero in
  // AVSBSynchronizer::Seek().
  is_display_layer_flushing_ = true;
  AVSampleBufferDisplayLayer* display_layer = display_layer_;
  onApplicationMainThread(^{
    [display_layer flush];
    is_display_layer_flushing_ = false;
  });
}

bool AVSBVideoRenderer::IsUnderflow() {
  SB_DCHECK(BelongsToCurrentThread());

  const int kUnderflowLowWatermark = 4;
  const int kUnderflowHighWatermark = 16;
  int enqueued_frames = buffers_in_sample_builder_ + GetNumberOfCachedFrames();
  if (is_underflow_) {
    if (enqueued_frames > kUnderflowHighWatermark) {
      is_underflow_ = false;
    }
  } else {
    if (enqueued_frames < kUnderflowLowWatermark) {
      is_underflow_ = true;
    }
  }
  if (eos_written_) {
    is_underflow_ = false;
  }
  return is_underflow_;
}

// static
void AVSBVideoRenderer::DisplayLayerNotificationCallback(
    CFNotificationCenterRef center,
    void* observer,
    CFNotificationName name,
    const void* object,
    CFDictionaryRef user_info) {
  // The notification callback is called on main thread.
  AVSBVideoRenderer* renderer = static_cast<AVSBVideoRenderer*>(observer);
  int32_t lock_slot = ObserverRegistry::LockObserver(&renderer->observer_);
  if (lock_slot < 0) {
    // The observer was already unregistered.
    return;
  }
  NSError* error = (__bridge NSError*)CFDictionaryGetValue(
      user_info,
      (void*)AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey);
  SB_DCHECK(error);
  renderer->OnDecodeError(error);
  ObserverRegistry::UnlockObserver(lock_slot);
}

void AVSBVideoRenderer::ReportError(const std::string& message) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_LOG(ERROR) << message;
  if (!error_occurred_) {
    error_occurred_ = true;
    error_cb_(kSbPlayerErrorDecode, message);
  }
}

void AVSBVideoRenderer::UpdatePreferredDisplayCriteria() {
  AVDisplayManager* avDisplayManager = GetPlatformWindow().avDisplayManager;
  if (avDisplayManager.isDisplayCriteriaMatchingEnabled == YES) {
    NSURL* url = [NSURL URLWithString:kDummyMasterPlaylistUrl];

    // TODO: Support frame rate match if needed. Currently we use default value,
    // so that tvOS won't change display frame rate.
    SBAVAssetResourceLoaderDelegate* delegate =
        [[SBAVAssetResourceLoaderDelegate alloc]
            initWithTransferId:video_stream_info_.color_metadata.transfer
                     frameRate:kDisplayCriteriaDefaultFrameRate];
    AVURLAsset* asset = [AVURLAsset assetWithURL:url];
    [asset.resourceLoader setDelegate:delegate queue:dispatch_get_main_queue()];
    // Note that any sync inquiry of AVAsset which may trigger resource loader
    // delegate cannot be performed on the resource loader's queue thread.
    // Otherwise, it will cause deadlock.
    AVDisplayCriteria* criteria = asset.preferredDisplayCriteria;
    onApplicationMainThread(^{
      avDisplayManager.preferredDisplayCriteria = criteria;
    });
  }
}

void AVSBVideoRenderer::OnSampleBufferBuilderOutput(
    const scoped_refptr<AVSampleBuffer>& sample_buffer) {
  if (!BelongsToCurrentThread()) {
    Schedule(std::bind(&AVSBVideoRenderer::OnSampleBufferBuilderOutput, this,
                       sample_buffer));
    return;
  }

  buffers_in_sample_builder_--;
  SB_DCHECK(buffers_in_sample_builder_ >= 0);

  CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(
      sample_buffer->cm_sample_buffer(), YES);
  // There should be only 1 frame in the buffer.
  SB_DCHECK(CFArrayGetCount(attachments) == 1);

  CFMutableDictionaryRef attachment =
      (CFMutableDictionaryRef)CFArrayGetValueAtIndex(attachments, 0);

  int64_t presentation_timestamp = sample_buffer->presentation_timestamp();
  if (presentation_timestamp < seek_to_time_) {
    CFDictionarySetValue(attachment, kCMSampleAttachmentKey_DoNotDisplay,
                         kCFBooleanTrue);
  } else if (!is_first_sample_written_) {
    is_first_sample_written_ = true;
    pts_of_first_written_buffer_ = presentation_timestamp;
    CFDictionarySetValue(attachment, kCMSampleAttachmentKey_DisplayImmediately,
                         kCFBooleanTrue);
  }

  // Attach HDR10+ extra metadata if presents.
  const std::vector<uint8_t>& side_data =
      sample_buffer->input_buffer()->side_data();
  if (side_data.size() >= sizeof(uint64_t)) {
    // First 8 bytes of side_data in InputBuffer is the BlockAddID
    // element's value in Big Endian format.
    uint64_t block_add_id;
    memcpy(&block_add_id, side_data.data(), sizeof(block_add_id));
    // Apple uses little endian. In case of future change, we still check it
    // before swap.
    SB_DCHECK(CFByteOrderGetCurrent() == CFByteOrderLittleEndian);
    if (CFByteOrderGetCurrent() == CFByteOrderLittleEndian) {
      block_add_id = OSSwapInt64(block_add_id);
    }

    // https://www.webmproject.org/docs/container/#BlockAddID says "0x04
    // indicates ITU T.35 metadata as defined by ITU-T T.35 terminal codes."
    constexpr uint64_t kHdr10PlusBlockAddId = 0x04;
    if (block_add_id == kHdr10PlusBlockAddId) {
      CFDataRef hdr10_plus_metadata =
          CFDataCreate(NULL, side_data.data() + sizeof(block_add_id),
                       side_data.size() - sizeof(block_add_id));

      static NSString* kCMSampleAttachmentKey_HDR10PlusPerFrameData =
          @"HDR10PlusPerFrameData";
      CFDictionarySetValue(
          attachment,
          (__bridge CFStringRef)kCMSampleAttachmentKey_HDR10PlusPerFrameData,
          hdr10_plus_metadata);
    }
  }

  const SbDrmSampleInfo* drm_info = sample_buffer->input_buffer()->drm_info();
  if (drm_system_ && drm_info) {
    // Attach content key and cryptor data to sample buffer.
    AVContentKey* content_key = drm_system_->GetContentKey(
        drm_info->identifier, drm_info->identifier_size);
    SB_DCHECK(content_key);

    NSError* error;
    BOOL result = AVSampleBufferAttachContentKey(
        sample_buffer->cm_sample_buffer(), content_key, &error);
    if (!result) {
      std::stringstream ss;
      ss << "Failed to attach content key.";
      avutil::AppendAVErrorDetails(error, &ss);
      ReportError(ss.str());
      return;
    }

    static NSString* kCMSampleAttachmentKey_CryptorSubsampleAuxiliaryData =
        @"CryptorSubsampleAuxiliaryData";
    CFDataRef cryptor_info = CFDataCreate(
        NULL,
        reinterpret_cast<const unsigned char*>(drm_info->subsample_mapping),
        drm_info->subsample_count * sizeof(SbDrmSubSampleMapping));
    CFDictionarySetValue(
        attachment,
        (__bridge CFStringRef)
            kCMSampleAttachmentKey_CryptorSubsampleAuxiliaryData,
        cryptor_info);
  }

  // If the output of sample builder is already decoded, we can skip the frames
  // before |seek_to_time_| into AVSBDL.
  if (!sample_buffer_builder_->IsSampleAlreadyDecoded() ||
      presentation_timestamp >= seek_to_time_) {
    video_sample_buffers_.push(sample_buffer);
    total_written_frames_++;
    pts_of_last_output_buffer_ = presentation_timestamp;
    EnqueueSampleBuffers();

    if (seeking_ && presentation_timestamp >= seek_to_time_) {
      prerolled_frames_++;
      if (prerolled_frames_ >= sample_buffer_builder_->GetPrerollFrameCount()) {
        seeking_ = false;
        frames_per_second_ =
            1000000 * (prerolled_frames_ - 1) /
            (presentation_timestamp - pts_of_first_written_buffer_);
        // As we don't know the frame status after enqueuing it into AVSBDL, we
        // use an estimated decoding delay time to ensure the first frame is
        // ready when we start the playback.
        int64_t delayed_time =
            total_written_frames_ *
            sample_buffer_builder_->DecodingTimeNeededPerFrame();
        Schedule(prerolled_cb_, delayed_time);
      }
    }
  }
}

void AVSBVideoRenderer::OnSampleBufferBuilderError(const std::string& message) {
  Schedule(std::bind(&AVSBVideoRenderer::ReportError, this, message));
}

void AVSBVideoRenderer::EnqueueSampleBuffers() {
  SB_DCHECK(BelongsToCurrentThread());
  if (is_display_layer_flushing_) {
    // EnqueueSampleBuffers() is called before [display_layer_ flush] is
    // finished. It should be very rare to happen.
    SB_LOG(WARNING) << "EnqueueSampleBuffers() is called before "
                       "AVSBDL::flush() is completeted.";
    Schedule(std::bind(&AVSBVideoRenderer::EnqueueSampleBuffers, this), 1000);
    return;
  }
  @autoreleasepool {
    int64_t media_time = GetCurrentMediaTime();
    // It's discouraged to enqueue more samples when readyForMoreMediaData is
    // NO. But readyForMoreMediaData often change from NO to YES asynchronously.
    // Sometimes the block passed in [AVSampleBufferDisplayLayer
    // requestMediaDataWhenReadyOnQueue: usingBlock:] is invoked very late.
    // Sometimes the block might be never invoked.
    int buffer_in_display_layer =
        GetNumberOfCachedFrames() - video_sample_buffers_.size();
    int buffers_to_enqueue =
        kRequiredBuffersInDisplayLayer - buffer_in_display_layer;
    while (!video_sample_buffers_.empty() &&
           (buffers_to_enqueue > 0 || display_layer_.readyForMoreMediaData)) {
      const scoped_refptr<AVSampleBuffer>& sample_buffer =
          video_sample_buffers_.front();
      if (!seeking_ &&
          sample_buffer->presentation_timestamp() - media_time <
              sample_buffer_builder_->DecodingTimeNeededPerFrame()) {
        SB_LOG(INFO) << "Dropping frame @ "
                     << sample_buffer->presentation_timestamp()
                     << " at media time " << media_time << " (diff: "
                     << sample_buffer->presentation_timestamp() - media_time
                     << ") with " << video_sample_buffers_.size()
                     << " frames in backlog, total dropped frames: "
                     << total_dropped_frames_;
        total_dropped_frames_++;
      }
      [display_layer_ enqueueSampleBuffer:sample_buffer->cm_sample_buffer()];
      video_sample_buffers_.pop();
      buffers_to_enqueue--;
    }

    UpdateCachedFramesWatermark();

    if (!video_sample_buffers_.empty() &&
        !enqueue_sample_buffers_job_token_.is_valid()) {
      enqueue_sample_buffers_job_token_ = Schedule(
          std::bind(&AVSBVideoRenderer::DelayedEnqueueSampleBuffers, this),
          16000);
    }

    if (eos_written_ && buffers_in_sample_builder_ == 0 &&
        video_sample_buffers_.size() == 0) {
      if (seeking_) {
        seeking_ = false;
        Schedule(prerolled_cb_);
      }
      CheckIfStreamEnded();
    }
  }  // @autoreleasepool
}

void AVSBVideoRenderer::DelayedEnqueueSampleBuffers() {
  enqueue_sample_buffers_job_token_.ResetToInvalid();
  EnqueueSampleBuffers();
}

void AVSBVideoRenderer::UpdateCachedFramesWatermark() {
  SB_DCHECK(BelongsToCurrentThread());
  // No need to check cached frames watermark during prerolling.
  if (seeking_) {
    return;
  }

  size_t number_of_cached_frames = GetNumberOfCachedFrames();
  if (is_cached_frames_below_low_watermark) {
    size_t high_watermark =
        std::min(kCachedFramesHighWatermark,
                 sample_buffer_builder_->GetMaxNumberOfCachedFrames());
    if (number_of_cached_frames >= high_watermark) {
      SB_LOG(INFO) << "Cached frames are above the high watermark, with "
                   << number_of_cached_frames << " frames in backlog.";
      is_cached_frames_below_low_watermark = false;
      sample_buffer_builder_->OnCachedFramesWatermarkHigh();
    }
  } else {
    size_t low_watermark =
        std::min<size_t>(kCachedFramesLowWatermark,
                         sample_buffer_builder_->GetPrerollFrameCount() / 2);
    if (number_of_cached_frames < low_watermark) {
      SB_LOG(INFO) << "Cached frames are below the low watermark, with "
                   << number_of_cached_frames << " frames in backlog.";
      is_cached_frames_below_low_watermark = true;
      sample_buffer_builder_->OnCachedFramesWatermarkLow();
    }
  }
}

int64_t AVSBVideoRenderer::GetCurrentMediaTime() const {
  SB_DCHECK(BelongsToCurrentThread());
  int64_t media_time =
      CMTimeConvertScale(CMTimebaseGetTime(display_layer_.timebase), 1000000,
                         kCMTimeRoundingMethod_QuickTime)
          .value;
  media_time = std::max(media_time - media_time_offset_, 0ll);
  return std::min(media_time, pts_of_last_output_buffer_);
}

size_t AVSBVideoRenderer::GetNumberOfCachedFrames() const {
  SB_DCHECK(BelongsToCurrentThread());
  int64_t media_time = GetCurrentMediaTime();
  // When |seeking_| is true, AVSBDL would not consume any frames, the number
  // of cached frames must be |prerolled_frames_|.
  if (seeking_ == true) {
    return prerolled_frames_;
  }
  // During prerolling, AVSBSynchronizer will set media time to |seek_to_time_|
  // after Play() called. So, when |media_time| is 0, the media time is actually
  // unknown. In that case, we can simply use |seek_to_time_|.
  if (media_time == 0) {
    media_time = seek_to_time_;
  }
  // The more accurate calculation is
  // floor(enqueued_duration * frames_per_second_ / 1000000) + 1.
  // Simply use + 2 here to avoid float casting.
  int64_t enqueued_duration =
      std::max<int64_t>(pts_of_last_output_buffer_ - media_time, 0);
  return enqueued_duration * frames_per_second_ / 1000000 + 2;
}

void AVSBVideoRenderer::CheckIfStreamEnded() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(eos_written_);

  if (GetCurrentMediaTime() >= pts_of_last_output_buffer_) {
    Schedule(ended_cb_);
  } else {
    Schedule(std::bind(&AVSBVideoRenderer::CheckIfStreamEnded, this),
             kCheckPlaybackStatusIntervalUsec);
  }
}

void AVSBVideoRenderer::OnDecodeError(NSError* error) {
  // This function is only called in DisplayLayerNotificationCallback, which is
  // on main thread and already wrapped by ObserverRegistry::LockObserver
  // and ObserverRegistry::UnlockObserver.
  std::stringstream ss;
  ss << "AVSBDL decode failed (seek to " << seek_to_time_ << ", offset "
     << media_time_offset_ << "). ";
  avutil::AppendAVErrorDetails(error, &ss);
  // Avoid to run any callback between ObserverRegistry::LockObserver
  // and ObserverRegistry::UnlockObserver. If AVSBVideoRenderer is destroyed
  // in the callback, it may cause dead lock.
  Schedule(std::bind(&AVSBVideoRenderer::ReportError, this, ss.str()));
}

void AVSBVideoRenderer::OnStatusChanged(NSString* key_path) {
  // This function is called on main thread.
  int32_t lock_slot = ObserverRegistry::LockObserver(&observer_);
  if (lock_slot < 0) {
    // The observer was already unregistered.
    return;
  }
  // Avoid to run any callback between ObserverRegistry::LockObserver
  // and ObserverRegistry::UnlockObserver. If AVSBVideoRenderer is destroyed
  // in the callback, it may cause dead lock.
  if ([key_path isEqualToString:kAVSBDLStatusKeyPath] &&
      display_layer_.status == AVQueuedSampleBufferRenderingStatusFailed) {
    std::stringstream ss;
    ss << "AVSBDL status failed (seek to " << seek_to_time_ << ", offset "
       << media_time_offset_ << "). ";
    avutil::AppendAVErrorDetails(display_layer_.error, &ss);
    Schedule(std::bind(&AVSBVideoRenderer::ReportError, this, ss.str()));
  }
  if (drm_system_ && [key_path isEqualToString:kAVSBDLOutputObscuredKeyPath]) {
    drm_system_->OnOutputObscuredChanged(
        display_layer_.outputObscuredDueToInsufficientExternalProtection);
  }
  ObserverRegistry::UnlockObserver(lock_slot);
}

}  // namespace starboard
