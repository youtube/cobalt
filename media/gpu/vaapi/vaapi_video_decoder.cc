// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_decoder.h"

#include <limits>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/format_utils.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/gpu/av1_decoder.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/av1_vaapi_video_decoder_delegate.h"
#include "media/gpu/vaapi/h264_vaapi_video_decoder_delegate.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/gpu/vaapi/vp8_vaapi_video_decoder_delegate.h"
#include "media/gpu/vaapi/vp9_vaapi_video_decoder_delegate.h"
#include "media/media_buildflags.h"
#include "ui/gfx/buffer_format_util.h"

#if BUILDFLAG(ENABLE_PLATFORM_HEVC_DECODING)
#include "media/gpu/vaapi/h265_vaapi_video_decoder_delegate.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace media {

namespace {

// Size of the timestamp cache, needs to be large enough for frame-reordering.
constexpr size_t kTimestampCacheSize = 128;

absl::optional<VideoPixelFormat> GetPixelFormatForBitDepth(uint8_t bit_depth) {
  constexpr auto kSupportedBitDepthAndGfxFormats = base::MakeFixedFlatMap<
      uint8_t, gfx::BufferFormat>({
#if defined(USE_OZONE)
    {8u, gfx::BufferFormat::YUV_420_BIPLANAR}, {10u, gfx::BufferFormat::P010},
#else
    {8u, gfx::BufferFormat::RGBX_8888},
#endif  // defined(USE_OZONE)
  });
  if (!base::Contains(kSupportedBitDepthAndGfxFormats, bit_depth)) {
    VLOGF(1) << "Unsupported bit depth: " << base::strict_cast<int>(bit_depth);
    return absl::nullopt;
  }
  return GfxBufferFormatToVideoPixelFormat(
      kSupportedBitDepthAndGfxFormats.at(bit_depth));
}

inline int RoundDownToEven(int x) {
  DCHECK_GE(x, 0);
  return x - (x % 2);
}

inline int RoundUpToEven(int x) {
  DCHECK_GE(x, 0);
  CHECK_LT(x, std::numeric_limits<int>::max());
  return x + (x % 2);
}

}  // namespace

// static
base::AtomicRefCount VaapiVideoDecoder::num_instances_(0);

VaapiVideoDecoder::DecodeTask::DecodeTask(scoped_refptr<DecoderBuffer> buffer,
                                          int32_t buffer_id,
                                          DecodeCB decode_done_cb)
    : buffer_(std::move(buffer)),
      buffer_id_(buffer_id),
      decode_done_cb_(std::move(decode_done_cb)) {}

VaapiVideoDecoder::DecodeTask::~DecodeTask() = default;

VaapiVideoDecoder::DecodeTask::DecodeTask(DecodeTask&&) = default;

// static
std::unique_ptr<VideoDecoderMixin> VaapiVideoDecoder::Create(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client) {
  const bool can_create_decoder =
      num_instances_.Increment() < kMaxNumOfInstances;
  if (!can_create_decoder) {
    num_instances_.Decrement();
    return nullptr;
  }
  return base::WrapUnique<VideoDecoderMixin>(new VaapiVideoDecoder(
      std::move(media_log), std::move(decoder_task_runner), std::move(client)));
}

// static
absl::optional<SupportedVideoDecoderConfigs>
VaapiVideoDecoder::GetSupportedConfigs() {
  return ConvertFromSupportedProfiles(
      VaapiWrapper::GetSupportedDecodeProfiles(),
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
      true /* allow_encrypted */);
#else
      false /* allow_encrypted */);
#endif
}

VaapiVideoDecoder::VaapiVideoDecoder(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client)
    : VideoDecoderMixin(std::move(media_log),
                        std::move(decoder_task_runner),
                        std::move(client)),
      buffer_id_to_timestamp_(kTimestampCacheSize),
      weak_this_factory_(this) {
  VLOGF(2);
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VaapiVideoDecoder::~VaapiVideoDecoder() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Abort all currently scheduled decode tasks.
  ClearDecodeTaskQueue(DecodeStatus::ABORTED);

  weak_this_factory_.InvalidateWeakPtrs();

  // Notify |decoder_delegate_| of an imminent VAContextID destruction, so it
  // can destroy any internal structures making use of it.
  if (decoder_delegate_)
    decoder_delegate_->OnVAContextDestructionSoon();

  // Destroy explicitly to DCHECK() that |vaapi_wrapper_| references are held
  // inside the accelerator in |decoder_|, by the |allocated_va_surfaces_| and
  // of course by this class. To clear |allocated_va_surfaces_| we have to first
  // DestroyContext().
  decoder_ = nullptr;
  if (vaapi_wrapper_) {
    vaapi_wrapper_->DestroyContext();
    allocated_va_surfaces_.clear();

    DCHECK(vaapi_wrapper_->HasOneRef());
    vaapi_wrapper_ = nullptr;
  }

  num_instances_.Decrement();
}

void VaapiVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                   bool /*low_delay*/,
                                   CdmContext* cdm_context,
                                   InitCB init_cb,
                                   const OutputCB& output_cb,
                                   const WaitingCB& waiting_cb) {
  DVLOGF(2) << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.IsValidConfig());

  // Reinitializing the decoder is allowed if there are no pending decodes.
  if (current_decode_task_ || !decode_task_queue_.empty() ||
      state_ == State::kExpectingReset) {
    LOG(ERROR)
        << "Don't call Initialize() while there are pending decode tasks";
    std::move(init_cb).Run(StatusCode::kVaapiReinitializedDuringDecode);
    return;
  }

  DCHECK(state_ == State::kError || state_ == State::kUninitialized ||
         state_ == State::kWaitingForInput);
  if (state_ != State::kUninitialized) {
    DVLOGF(3) << "Reinitializing decoder";

    // Notify |decoder_delegate_| of an imminent VAContextID destruction, so it
    // can destroy any internal structures making use of it.
    decoder_delegate_->OnVAContextDestructionSoon();

    decoder_ = nullptr;
    DCHECK(vaapi_wrapper_);
    // To clear |allocated_va_surfaces_|, we have to first DestroyContext().
    vaapi_wrapper_->DestroyContext();
    allocated_va_surfaces_.clear();

    DCHECK(vaapi_wrapper_->HasOneRef());
    vaapi_wrapper_ = nullptr;
    decoder_delegate_ = nullptr;

    // |cdm_context_ref_| is reset after |decoder_| because we passed
    // |cdm_context_ref_->GetCdmContext()| when creating the |decoder_|, so we
    // don't want |decoder_| to have a dangling pointer. We also destroy
    // |cdm_event_cb_registration_| before |cdm_context_ref_| so that we have a
    // CDM at the moment of destroying the callback registration.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    cdm_event_cb_registration_ = nullptr;
#endif
    cdm_context_ref_ = nullptr;
    transcryption_ = false;

    SetState(State::kUninitialized);
  }
  DCHECK(!current_decode_task_);
  DCHECK(decode_task_queue_.empty());

  // Destroying the |decoder_| during re-initialization should release all
  // output buffers (and there should be no output buffers to begin with if the
  // decoder was previously uninitialized).
  DCHECK(output_frames_.empty());

  if (config.is_encrypted()) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    SetErrorState("encrypted content is not supported");
    std::move(init_cb).Run(StatusCode::kEncryptedContentUnsupported);
    return;
#else
    if (!cdm_context || !cdm_context->GetChromeOsCdmContext()) {
      SetErrorState("cannot support encrypted stream w/out ChromeOsCdmContext");
      std::move(init_cb).Run(StatusCode::kDecoderMissingCdmForEncryptedContent);
      return;
    }
    bool encrypted_av1_support = false;
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_AV1)
    encrypted_av1_support = true;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    encrypted_av1_support = base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kLacrosUseChromeosProtectedAv1);
#endif
    if (config.codec() != VideoCodec::kH264 &&
        config.codec() != VideoCodec::kVP9 &&
        (config.codec() != VideoCodec::kAV1 || !encrypted_av1_support) &&
        config.codec() != VideoCodec::kHEVC) {
      SetErrorState(
          base::StringPrintf("%s is not supported for encrypted content",
                             GetCodecName(config.codec()).c_str()));
      std::move(init_cb).Run(StatusCode::kEncryptedContentUnsupported);
      return;
    }
    cdm_event_cb_registration_ = cdm_context->RegisterEventCB(
        base::BindRepeating(&VaapiVideoDecoder::OnCdmContextEvent,
                            weak_this_factory_.GetWeakPtr()));
    cdm_context_ref_ = cdm_context->GetChromeOsCdmContext()->GetCdmContextRef();
    // On AMD the content is transcrypted by the pipeline before reaching us,
    // but we still need to do special handling with it.
    transcryption_ = (VaapiWrapper::GetImplementationType() ==
                      VAImplementation::kMesaGallium);
#endif
#if BUILDFLAG(ENABLE_PLATFORM_HEVC_DECODING)
  } else if (config.codec() == VideoCodec::kHEVC &&
             !base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kEnableClearHevcForTesting)) {
    SetErrorState("clear HEVC content is not supported");
    std::move(init_cb).Run(StatusCode::kClearContentUnsupported);
    return;
#endif
  }

  // Initialize VAAPI wrapper.
  const VideoCodecProfile profile = config.profile();
  vaapi_wrapper_ = VaapiWrapper::CreateForVideoCodec(
#if BUILDFLAG(IS_CHROMEOS_ASH)
      (!cdm_context_ref_ || transcryption_) ? VaapiWrapper::kDecode
                                            : VaapiWrapper::kDecodeProtected,
#else
      VaapiWrapper::kDecode,
#endif
      profile,
      transcryption_ ? EncryptionScheme::kUnencrypted
                     : config.encryption_scheme(),
      base::BindRepeating(&ReportVaapiErrorToUMA,
                          "Media.VaapiVideoDecoder.VAAPIError"));
  UMA_HISTOGRAM_BOOLEAN("Media.VaapiVideoDecoder.VaapiWrapperCreationSuccess",
                        vaapi_wrapper_.get());
  if (!vaapi_wrapper_.get()) {
    SetErrorState(
        base::StringPrintf("failed initializing VaapiWrapper for profile %s, ",
                           GetProfileName(profile).c_str()));
    std::move(init_cb).Run(StatusCode::kDecoderUnsupportedProfile);
    return;
  }

  profile_ = profile;
  color_space_ = config.color_space_info();
  hdr_metadata_ = config.hdr_metadata();
  encryption_scheme_ = transcryption_ ? EncryptionScheme::kUnencrypted
                                      : config.encryption_scheme();
  auto accel_status = CreateAcceleratedVideoDecoder();
  if (!accel_status.is_ok()) {
    SetErrorState("failed to create decoder delegate");
    std::move(init_cb).Run(std::move(accel_status));
    return;
  }

  aspect_ratio_ = config.aspect_ratio();

  output_cb_ = std::move(output_cb);
  waiting_cb_ = std::move(waiting_cb);
  SetState(State::kWaitingForInput);

  // Notify client initialization was successful.
  std::move(init_cb).Run(OkStatus());
}

void VaapiVideoDecoder::OnCdmContextEvent(CdmContext::Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event != CdmContext::Event::kHasAdditionalUsableKey)
    return;

  // Invoke the callback we'd get for a protected session update because this is
  // the same thing, it's a trigger that there are new keys, so if we were
  // waiting for a key we should fetch them again.
  ProtectedSessionUpdate(true);
}

void VaapiVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                               DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4) << "Queuing input buffer, id: " << next_buffer_id_ << ", size: "
            << (buffer->end_of_stream() ? 0 : buffer->data_size());

  // If we're in the error state, immediately fail the decode task.
  if (state_ == State::kError) {
    // VideoDecoder interface: |decode_cb| can't be called from within Decode().
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecodeStatus::DECODE_ERROR));
    return;
  }

  if (!buffer->end_of_stream())
    buffer_id_to_timestamp_.Put(next_buffer_id_, buffer->timestamp());

  decode_task_queue_.emplace(std::move(buffer), next_buffer_id_,
                             std::move(decode_cb));

  // Generate the next positive buffer id. Don't let it overflow because that
  // behavior is undefined for signed integers, we mask it down to 30 bits to
  // avoid that problem.
  next_buffer_id_ = (next_buffer_id_ + 1) & 0x3fffffff;

  // If we were waiting for input buffers, start decoding again.
  if (state_ == State::kWaitingForInput) {
    DCHECK(!current_decode_task_);
    SetState(State::kDecoding);
    ScheduleNextDecodeTask();
  }
}

void VaapiVideoDecoder::ScheduleNextDecodeTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kDecoding);
  DCHECK(!current_decode_task_);
  DCHECK(!decode_task_queue_.empty());

  // Dequeue the next decode task.
  current_decode_task_ = std::move(decode_task_queue_.front());
  decode_task_queue_.pop();
  if (!current_decode_task_->buffer_->end_of_stream()) {
    decoder_->SetStream(current_decode_task_->buffer_id_,
                        *current_decode_task_->buffer_);
  }

  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiVideoDecoder::HandleDecodeTask, weak_this_));
}

void VaapiVideoDecoder::HandleDecodeTask() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != State::kDecoding)
    return;

  DCHECK_EQ(state_, State::kDecoding);
  DCHECK(current_decode_task_);

  // Check whether a flush was requested.
  if (current_decode_task_->buffer_->end_of_stream()) {
    Flush();
    return;
  }

  TRACE_EVENT_BEGIN0("media,gpu", "VaapiVideoDecoder::Decode");
  AcceleratedVideoDecoder::DecodeResult decode_result = decoder_->Decode();
  TRACE_EVENT_END0("media,gpu", "VaapiVideoDecoder::Decode");
  switch (decode_result) {
    case AcceleratedVideoDecoder::kRanOutOfStreamData:
      // Decoding was successful, notify client and try to schedule the next
      // task. Switch to the idle state if we ran out of buffers to decode.
      std::move(current_decode_task_->decode_done_cb_).Run(DecodeStatus::OK);
      current_decode_task_ = absl::nullopt;
      if (!decode_task_queue_.empty()) {
        ScheduleNextDecodeTask();
      } else {
        SetState(State::kWaitingForInput);
      }
      break;
    case AcceleratedVideoDecoder::kConfigChange:
      // A new set of output buffers is requested. We either didn't have any
      // output buffers yet or encountered a resolution change.
      // After the pipeline flushes all frames, ApplyResolutionChange() will be
      // called and we can start changing resolution.
      DCHECK(client_);
      SetState(State::kChangingResolution);
      client_->PrepareChangeResolution();
      break;
    case AcceleratedVideoDecoder::kRanOutOfSurfaces:
      // No more surfaces to decode into available, wait until client returns
      // video frames to the frame pool.
      SetState(State::kWaitingForOutput);
      break;
    case AcceleratedVideoDecoder::kNeedContextUpdate:
      SetErrorState("context updates not supported");
      break;
    case AcceleratedVideoDecoder::kDecodeError:
      UMA_HISTOGRAM_BOOLEAN("Media.VaapiVideoDecoder.DecodeError", true);
      SetErrorState("error decoding stream");
      break;
    case AcceleratedVideoDecoder::kTryAgain:
      DVLOG(1) << "Decoder going into the waiting for protected state";
      DCHECK_NE(encryption_scheme_, EncryptionScheme::kUnencrypted);
      SetState(State::kWaitingForProtected);
      // If we have lost our protected HW session, it should be recoverable, so
      // indicate that we have lost our decoder state so it can be reloaded.
      if (decoder_delegate_->HasInitiatedProtectedRecovery())
        waiting_cb_.Run(WaitingReason::kDecoderStateLost);
      break;
  }
}

void VaapiVideoDecoder::ClearDecodeTaskQueue(DecodeStatus status) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (current_decode_task_) {
    std::move(current_decode_task_->decode_done_cb_).Run(status);
    current_decode_task_ = absl::nullopt;
  }

  while (!decode_task_queue_.empty()) {
    std::move(decode_task_queue_.front().decode_done_cb_).Run(status);
    decode_task_queue_.pop();
  }
}

scoped_refptr<VASurface> VaapiVideoDecoder::CreateSurface() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kDecoding);
  DCHECK(current_decode_task_);

  // Get a video frame from the video frame pool.
  DCHECK(client_);
  DmabufVideoFramePool* frame_pool = client_->GetVideoFramePool();
  DCHECK(frame_pool);
  scoped_refptr<VideoFrame> frame = frame_pool->GetFrame();
  if (!frame) {
    // Ask the video frame pool to notify us when new frames are available, so
    // we can retry the current decode task.
    frame_pool->NotifyWhenFrameAvailable(
        base::BindOnce(&VaapiVideoDecoder::NotifyFrameAvailable, weak_this_));
    return nullptr;
  }

  // |frame|s coming from ARC++ are not GpuMemoryBuffer-backed, but they have
  // DmaBufs whose fd numbers are consistent along the lifetime of the VA
  // surfaces they back.
  DCHECK(frame->GetGpuMemoryBuffer() || frame->HasDmaBufs());
  const gfx::GpuMemoryBufferId frame_id =
      frame->GetGpuMemoryBuffer()
          ? frame->GetGpuMemoryBuffer()->GetId()
          : gfx::GpuMemoryBufferId(frame->DmabufFds()[0].get());

  scoped_refptr<VASurface> va_surface;
  if (!base::Contains(allocated_va_surfaces_, frame_id)) {
    scoped_refptr<gfx::NativePixmap> pixmap =
        CreateNativePixmapDmaBuf(frame.get());
    if (!pixmap) {
      SetErrorState("failed to create NativePixmap from VideoFrame");
      return nullptr;
    }

    va_surface = vaapi_wrapper_->CreateVASurfaceForPixmap(
        std::move(pixmap), cdm_context_ref_ || transcryption_);
    if (!va_surface || va_surface->id() == VA_INVALID_ID) {
      SetErrorState("failed to create VASurface from VideoFrame");
      return nullptr;
    }

    allocated_va_surfaces_[frame_id] = va_surface;
  } else {
    va_surface = allocated_va_surfaces_[frame_id];
    DCHECK_EQ(frame->coded_size(), va_surface->size());
  }

  // Store the mapping between surface and video frame, so we know which video
  // frame to output when the surface is ready. It's also important to keep a
  // reference to the video frame during decoding, as the frame will be
  // automatically returned to the pool when the last reference is dropped.
  VASurfaceID surface_id = va_surface->id();
  DCHECK_EQ(output_frames_.count(surface_id), 0u);
  output_frames_[surface_id] = frame;

  // When the decoder is done using the frame for output or reference, it will
  // drop its reference to the surface. We can then safely remove the associated
  // video frame from |output_frames_|. To be notified when this happens we wrap
  // the surface in another surface with ReleaseVideoFrame() as destruction
  // observer.
  VASurface::ReleaseCB release_frame_cb =
      base::BindOnce(&VaapiVideoDecoder::ReleaseVideoFrame, weak_this_);

  return new VASurface(surface_id, frame->layout().coded_size(),
                       va_surface->format(), std::move(release_frame_cb));
}

void VaapiVideoDecoder::SurfaceReady(scoped_refptr<VASurface> va_surface,
                                     int32_t buffer_id,
                                     const gfx::Rect& visible_rect,
                                     const VideoColorSpace& color_space) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kDecoding);

  // Find the timestamp associated with |buffer_id|. It's possible that a
  // surface is output multiple times for different |buffer_id|s (e.g. VP9
  // show_existing_frame feature). This means we need to output the same frame
  // again with a different timestamp.
  // On some rare occasions it's also possible that a single DecoderBuffer
  // produces multiple surfaces with the same |buffer_id|, so we shouldn't
  // remove the timestamp from the cache.
  const auto it = buffer_id_to_timestamp_.Peek(buffer_id);
  DCHECK(it != buffer_id_to_timestamp_.end());
  base::TimeDelta timestamp = it->second;

  // Find the frame associated with the surface. We won't erase it from
  // |output_frames_| yet, as the decoder might still be using it for reference.
  DCHECK_EQ(output_frames_.count(va_surface->id()), 1u);
  scoped_refptr<VideoFrame> video_frame = output_frames_[va_surface->id()];

  // Set the timestamp at which the decode operation started on the
  // |video_frame|. If the frame has been outputted before (e.g. because of VP9
  // show-existing-frame feature) we can't overwrite the timestamp directly, as
  // the original frame might still be in use. Instead we wrap the frame in
  // another frame with a different timestamp.
  if (video_frame->timestamp().is_zero())
    video_frame->set_timestamp(timestamp);

  if (video_frame->visible_rect() != visible_rect ||
      video_frame->timestamp() != timestamp) {
    gfx::Size natural_size = aspect_ratio_.GetNaturalSize(visible_rect);
    scoped_refptr<VideoFrame> wrapped_frame = VideoFrame::WrapVideoFrame(
        video_frame, video_frame->format(), visible_rect, natural_size);
    wrapped_frame->set_timestamp(timestamp);

    video_frame = std::move(wrapped_frame);
  }

  if (cdm_context_ref_ && !transcryption_) {
    // For protected content we also need to set the ID for validating protected
    // surfaces in the VideoFrame metadata so we can check if the surface is
    // still valid once we get to the compositor stage.
    uint32_t protected_instance_id = vaapi_wrapper_->GetProtectedInstanceID();
    video_frame->metadata().hw_protected_validation_id = protected_instance_id;

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Additionally, we store the VA-API protected session ID so that it can be
    // re-used for scaling the decoded video frame later in the pipeline.
    VAProtectedSessionID va_protected_session_id =
        vaapi_wrapper_->GetProtectedSessionID();

    static_assert(
        std::is_same<decltype(va_protected_session_id),
                     decltype(
                         video_frame->metadata()
                             .hw_va_protected_session_id)::value_type>::value,
        "The type of VideoFrameMetadata::hw_va_protected_session_id "
        "does not match the type exposed by VaapiWrapper");
    video_frame->metadata().hw_va_protected_session_id =
        va_protected_session_id;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  const auto gfx_color_space = color_space.ToGfxColorSpace();
  if (gfx_color_space.IsValid())
    video_frame->set_color_space(gfx_color_space);
  video_frame->set_hdr_metadata(hdr_metadata_);
  output_cb_.Run(std::move(video_frame));
}

void VaapiVideoDecoder::ApplyResolutionChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == State::kChangingResolution ||
         state_ == State::kWaitingForInput);
  DCHECK(output_frames_.empty());
  VLOGF(2);

  if (cdm_context_ref_ && !transcryption_) {
    // Get the screen resolutions so we can determine if we should pre-scale
    // content during decoding to maximize use of overlay downscaling since
    // protected content requires overlays currently.
    // NOTE: Only use this for protected content as other requirements for using
    // it are tied to protected content.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::ChromeOsCdmFactory::GetScreenResolutions(BindToCurrentLoop(
        base::BindOnce(&VaapiVideoDecoder::ApplyResolutionChangeWithScreenSizes,
                       weak_this_)));
    return;
#endif
  }
  ApplyResolutionChangeWithScreenSizes(std::vector<gfx::Size>());
}

void VaapiVideoDecoder::ApplyResolutionChangeWithScreenSizes(
    const std::vector<gfx::Size>& screen_resolutions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == State::kChangingResolution ||
         state_ == State::kWaitingForInput || state_ == State::kResetting ||
         state_ == State::kError);
  DCHECK(output_frames_.empty());
  VLOGF(2);
  // If we are not in the state for changing resolution, then skip doing it. For
  // all the other states, those can occur because something happened after the
  // async call to get the screen sizes in ApplyResolutionChange(), and in that
  // case we will get another resolution change event when the decoder parses
  // the resolution and notifies us.
  if (state_ != State::kChangingResolution)
    return;

  const uint8_t bit_depth = decoder_->GetBitDepth();
  const absl::optional<VideoPixelFormat> format =
      GetPixelFormatForBitDepth(bit_depth);
  if (!format) {
    SetErrorState(base::StringPrintf("unsupported bit depth: %d", bit_depth));
    return;
  }

  // Notify |decoder_delegate_| of an imminent VAContextID destruction, so it
  // can destroy any internal structures making use of it.
  decoder_delegate_->OnVAContextDestructionSoon();

  // All pending decode operations will be completed before triggering a
  // resolution change, so we can safely DestroyContext() here; that, in turn,
  // allows for clearing the |allocated_va_surfaces_|.
  vaapi_wrapper_->DestroyContext();
  allocated_va_surfaces_.clear();

  const gfx::Rect decoder_visible_rect = decoder_->GetVisibleRect();
  const gfx::Size decoder_pic_size = decoder_->GetPicSize();
  if (decoder_pic_size.IsEmpty()) {
    SetErrorState("|decoder_| returned an empty picture size");
    return;
  }
  gfx::Rect output_visible_rect = decoder_visible_rect;
  gfx::Size output_pic_size = decoder_pic_size;
  const auto format_fourcc = Fourcc::FromVideoPixelFormat(*format);
  CHECK(format_fourcc);
  if (!screen_resolutions.empty()) {
    // Ideally we would base this off visible size, but that can change
    // midstream without forcing a config change, so we need to scale the
    // overall decoded image and then apply that same relative scaling to the
    // visible rect later.
    CHECK(cdm_context_ref_);
    gfx::Size max_desired_size;
    const float pic_aspect = static_cast<float>(decoder_pic_size.width()) /
                             decoder_pic_size.height();
    for (const auto& screen : screen_resolutions) {
      if (screen.IsEmpty())
        continue;
      int target_width;
      int target_height;
      const float screen_aspect =
          static_cast<float>(screen.width()) / screen.height();
      if (pic_aspect >= screen_aspect) {
        // Constrain on width.
        if (screen.width() < decoder_pic_size.width()) {
          target_width = screen.width();
          target_height =
              base::checked_cast<int>(std::lround(target_width / pic_aspect));
        } else {
          target_width = decoder_pic_size.width();
          target_height = decoder_pic_size.height();
        }
      } else {
        // Constrain on height.
        if (screen.height() < decoder_pic_size.height()) {
          target_height = screen.height();
          target_width =
              base::checked_cast<int>(std::lround(target_height * pic_aspect));
        } else {
          target_height = decoder_pic_size.height();
          target_width = decoder_pic_size.width();
        }
      }
      if (target_width > max_desired_size.width() ||
          target_height > max_desired_size.height()) {
        max_desired_size.SetSize(target_width, target_height);
      }
    }
    if (!max_desired_size.IsEmpty() &&
        max_desired_size.width() < decoder_pic_size.width()) {
      // Fix this so we are sure it's on a multiple of two to deal with
      // subsampling.
      max_desired_size.set_width(RoundUpToEven(max_desired_size.width()));
      max_desired_size.set_height(RoundUpToEven(max_desired_size.height()));
      const auto decode_to_output_scale_factor =
          static_cast<float>(max_desired_size.width()) /
          decoder_pic_size.width();
      output_pic_size = max_desired_size;
      output_visible_rect = ScaleToEnclosedRect(decoder_visible_rect,
                                                decode_to_output_scale_factor);
      // Make the dimensions even numbered to align with other requirements
      // later in the pipeline.
      output_visible_rect.set_width(
          RoundDownToEven(output_visible_rect.width()));
      output_visible_rect.set_height(
          RoundDownToEven(output_visible_rect.height()));
      CHECK(gfx::Rect(output_pic_size).Contains(output_visible_rect));
    }
  }

  if (profile_ != decoder_->GetProfile()) {
    // When a profile is changed, we need to re-initialize VaapiWrapper.
    profile_ = decoder_->GetProfile();
    auto new_vaapi_wrapper = VaapiWrapper::CreateForVideoCodec(
#if BUILDFLAG(IS_CHROMEOS_ASH)
        (!cdm_context_ref_ || transcryption_) ? VaapiWrapper::kDecode
                                              : VaapiWrapper::kDecodeProtected,
#else
        VaapiWrapper::kDecode,
#endif
        profile_, encryption_scheme_,
        base::BindRepeating(&ReportVaapiErrorToUMA,
                            "Media.VaapiVideoDecoder.VAAPIError"));
    if (!new_vaapi_wrapper.get()) {
      SetErrorState("failed (re)creating VaapiWrapper");
      return;
    }
    decoder_delegate_->set_vaapi_wrapper(new_vaapi_wrapper.get());
    vaapi_wrapper_ = std::move(new_vaapi_wrapper);
  }

  if (!vaapi_wrapper_->CreateContext(decoder_pic_size)) {
    SetErrorState("failed creating VAContext");
    return;
  }

  const gfx::Size decoder_natural_size =
      aspect_ratio_.GetNaturalSize(decoder_visible_rect);
  auto status_or_layout = client_->PickDecoderOutputFormat(
      /*candidates=*/{{*format_fourcc, decoder_pic_size}}, decoder_visible_rect,
      decoder_natural_size, output_visible_rect.size(),
      decoder_->GetRequiredNumOfPictures(),
      /*use_protected=*/!!cdm_context_ref_,
      /*need_aux_frame_pool=*/true);
  if (status_or_layout.has_error()) {
    if (std::move(status_or_layout).error().code() == StatusCode::kAborted) {
      DVLOGF(2) << "The frame pool initialization is aborted.";
      SetState(State::kExpectingReset);
    } else {
      SetErrorState("failed Initialize()ing the frame pool");
    }
    return;
  }

  DCHECK(current_decode_task_);
  // Retry the current decode task.
  SetState(State::kDecoding);
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiVideoDecoder::HandleDecodeTask, weak_this_));
}

bool VaapiVideoDecoder::NeedsBitstreamConversion() const {
  DCHECK(output_cb_) << "VaapiVideoDecoder hasn't been initialized";
  NOTREACHED();
  return (profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX) ||
         (profile_ >= HEVCPROFILE_MIN && profile_ <= HEVCPROFILE_MAX);
}

bool VaapiVideoDecoder::CanReadWithoutStalling() const {
  NOTIMPLEMENTED();
  NOTREACHED();
  return true;
}

int VaapiVideoDecoder::GetMaxDecodeRequests() const {
  NOTREACHED();
  return 4;
}

VideoDecoderType VaapiVideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kVaapi;
}

bool VaapiVideoDecoder::IsPlatformDecoder() const {
  return true;
}

bool VaapiVideoDecoder::NeedsTranscryption() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == State::kWaitingForInput);

  return transcryption_;
}

void VaapiVideoDecoder::ReleaseVideoFrame(VASurfaceID surface_id) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The decoder has finished using the frame associated with |surface_id| for
  // output or reference, so it's safe to drop our reference here. Once the
  // client drops its reference the frame will be automatically returned to the
  // pool for reuse.
  size_t num_erased = output_frames_.erase(surface_id);
  DCHECK_EQ(num_erased, 1u);
}

void VaapiVideoDecoder::NotifyFrameAvailable() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If we were waiting for output buffers, retry the current decode task.
  if (state_ == State::kWaitingForOutput) {
    DCHECK(current_decode_task_);
    SetState(State::kDecoding);
    decoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VaapiVideoDecoder::HandleDecodeTask, weak_this_));
  }
}

void VaapiVideoDecoder::ProtectedSessionUpdate(bool success) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    SetErrorState("terminating decoding after failed protected update");
    return;
  }

  // If we were waiting for a protected update, retry the current decode task.
  if (state_ != State::kWaitingForProtected)
    return;

  DCHECK(current_decode_task_);
  SetState(State::kDecoding);
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiVideoDecoder::HandleDecodeTask, weak_this_));
}

void VaapiVideoDecoder::Flush() {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kDecoding);
  DCHECK(current_decode_task_);
  DCHECK(current_decode_task_->buffer_->end_of_stream());
  DCHECK(decode_task_queue_.empty());

  // Flush will block until SurfaceReady() has been called for every frame
  // currently decoding.
  if (!decoder_->Flush()) {
    SetErrorState("failed to flush the decoder delegate");
    return;
  }

  // Put the decoder in an idle state, ready to resume. This will release all
  // VASurfaces currently held, so |output_frames_| should be empty after reset.
  decoder_->Reset();
  DCHECK(output_frames_.empty());

  // Notify the client flushing is done.
  std::move(current_decode_task_->decode_done_cb_).Run(DecodeStatus::OK);
  current_decode_task_ = absl::nullopt;

  // Wait for new decodes, no decode tasks should be queued while flushing.
  SetState(State::kWaitingForInput);
}

void VaapiVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If we encountered an error, skip reset and notify client.
  if (state_ == State::kError) {
    std::move(reset_cb).Run();
    return;
  }

  if (state_ == State::kChangingResolution ||
      state_ == State::kExpectingReset) {
    // Recreate |decoder_| and |decoder_delegate_| if we are either:
    // a) Reset() in the interim between calling |client_|s
    //    PrepareChangeResolution() and being called back on
    //    ApplyResolutionChange(), so the latter will find a fresh |decoder_|;
    // b) expecting a Reset() after the initialization of the frame pool was
    //    aborted.
    // Also give a chance to |decoder_delegate_| to release its internal data
    // structures.
    decoder_delegate_->OnVAContextDestructionSoon();
    if (!CreateAcceleratedVideoDecoder().is_ok()) {
      SetErrorState("failed to (re)create decoder/delegate");
      std::move(reset_cb).Run();
      return;
    }
  } else {
    // Put the decoder in an idle state, ready to resume. This will release all
    // VASurfaces currently held, so |output_frames_| should be empty after
    // reset.
    decoder_->Reset();
  }

  DCHECK(output_frames_.empty());
  SetState(State::kResetting);

  // Wait until any pending decode task has been aborted.
  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiVideoDecoder::ResetDone, weak_this_,
                                std::move(reset_cb)));
}

Status VaapiVideoDecoder::CreateAcceleratedVideoDecoder() {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == State::kUninitialized ||
         state_ == State::kChangingResolution ||
         state_ == State::kExpectingReset);

  VaapiVideoDecoderDelegate::ProtectedSessionUpdateCB protected_update_cb =
      BindToCurrentLoop(base::BindRepeating(
          &VaapiVideoDecoder::ProtectedSessionUpdate, weak_this_));
  if (profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX) {
    auto accelerator = std::make_unique<H264VaapiVideoDecoderDelegate>(
        this, vaapi_wrapper_, std::move(protected_update_cb),
        cdm_context_ref_ ? cdm_context_ref_->GetCdmContext() : nullptr,
        encryption_scheme_);
    decoder_delegate_ = accelerator.get();

    decoder_.reset(
        new H264Decoder(std::move(accelerator), profile_, color_space_));
  } else if (profile_ >= VP8PROFILE_MIN && profile_ <= VP8PROFILE_MAX) {
    auto accelerator =
        std::make_unique<VP8VaapiVideoDecoderDelegate>(this, vaapi_wrapper_);
    decoder_delegate_ = accelerator.get();

    decoder_.reset(new VP8Decoder(std::move(accelerator)));
  } else if (profile_ >= VP9PROFILE_MIN && profile_ <= VP9PROFILE_MAX) {
    auto accelerator = std::make_unique<VP9VaapiVideoDecoderDelegate>(
        this, vaapi_wrapper_, std::move(protected_update_cb),
        cdm_context_ref_ ? cdm_context_ref_->GetCdmContext() : nullptr,
        encryption_scheme_);
    decoder_delegate_ = accelerator.get();

    decoder_.reset(
        new VP9Decoder(std::move(accelerator), profile_, color_space_));
  }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC_DECODING)
  else if (profile_ >= HEVCPROFILE_MIN && profile_ <= HEVCPROFILE_MAX) {
    auto accelerator = std::make_unique<H265VaapiVideoDecoderDelegate>(
        this, vaapi_wrapper_, std::move(protected_update_cb),
        cdm_context_ref_ ? cdm_context_ref_->GetCdmContext() : nullptr,
        encryption_scheme_);
    decoder_delegate_ = accelerator.get();

    decoder_.reset(
        new H265Decoder(std::move(accelerator), profile_, color_space_));
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC_DECODING)
  else if (profile_ >= AV1PROFILE_MIN && profile_ <= AV1PROFILE_MAX) {
    auto accelerator = std::make_unique<AV1VaapiVideoDecoderDelegate>(
        this, vaapi_wrapper_, std::move(protected_update_cb),
        cdm_context_ref_ ? cdm_context_ref_->GetCdmContext() : nullptr,
        encryption_scheme_);
    decoder_delegate_ = accelerator.get();

    decoder_.reset(new AV1Decoder(std::move(accelerator), profile_));
  } else {
    return Status(StatusCode::kDecoderUnsupportedProfile)
        .WithData("profile", profile_);
  }
  return OkStatus();
}

void VaapiVideoDecoder::ResetDone(base::OnceClosure reset_cb) {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kResetting);
  DCHECK(!current_decode_task_);
  DCHECK(decode_task_queue_.empty());

  std::move(reset_cb).Run();
  SetState(State::kWaitingForInput);
}

void VaapiVideoDecoder::SetState(State state) {
  DVLOGF(3) << static_cast<int>(state)
            << ", current state: " << static_cast<int>(state_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check whether the state change is valid.
  switch (state) {
    case State::kUninitialized:
      DCHECK(state_ == State::kWaitingForInput || state_ == State::kError);
      break;
    case State::kWaitingForInput:
      DCHECK(decode_task_queue_.empty());
      DCHECK(!current_decode_task_);
      DCHECK(state_ == State::kUninitialized || state_ == State::kDecoding ||
             state_ == State::kResetting);
      break;
    case State::kWaitingForProtected:
      DCHECK(!!cdm_context_ref_);
      FALLTHROUGH;
    case State::kWaitingForOutput:
      DCHECK(current_decode_task_);
      DCHECK_EQ(state_, State::kDecoding);
      break;
    case State::kDecoding:
      DCHECK(state_ == State::kWaitingForInput ||
             state_ == State::kWaitingForOutput ||
             state_ == State::kChangingResolution ||
             state_ == State::kWaitingForProtected);
      break;
    case State::kResetting:
      DCHECK(state_ == State::kWaitingForInput ||
             state_ == State::kWaitingForOutput || state_ == State::kDecoding ||
             state_ == State::kWaitingForProtected ||
             state_ == State::kChangingResolution ||
             state_ == State::kExpectingReset);
      ClearDecodeTaskQueue(DecodeStatus::ABORTED);
      break;
    case State::kChangingResolution:
      DCHECK_EQ(state_, State::kDecoding);
      break;
    case State::kExpectingReset:
      DCHECK_EQ(state_, State::kChangingResolution);
      break;
    case State::kError:
      ClearDecodeTaskQueue(DecodeStatus::DECODE_ERROR);
      break;
  }

  state_ = state;
}

void VaapiVideoDecoder::SetErrorState(std::string message) {
  LOG(ERROR) << message;
  if (media_log_)
    MEDIA_LOG(ERROR, media_log_) << "VaapiVideoDecoder: " << message;
  SetState(State::kError);
}

}  // namespace media
