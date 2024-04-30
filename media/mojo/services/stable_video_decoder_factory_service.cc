// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/stable_video_decoder_factory_service.h"

#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/viz/common/switches.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/gpu_video_decode_accelerator_factory.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/ipc/service/vda_video_decoder.h"
#include "media/mojo/services/mojo_media_client.h"
#include "media/mojo/services/mojo_video_decoder_service.h"
#include "media/mojo/services/stable_video_decoder_service.h"
#include "media/video/video_decode_accelerator.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace media {

namespace {

// This is a lighter alternative to using a GpuMojoMediaClient. While we could
// use a GpuMojoMediaClient, that would be abusing the abstraction a bit since
// that class is too semantically coupled with the GPU process through things
// like its |gpu_task_runner_| and |media_gpu_channel_manager_| members.
class MojoMediaClientImpl : public MojoMediaClient {
 public:
  MojoMediaClientImpl(const gpu::GpuFeatureInfo& gpu_feature_info,
                      bool enable_direct_video_decoder)
      : gpu_driver_bug_workarounds_(
            gpu_feature_info.enabled_gpu_driver_bug_workarounds),
        enable_direct_video_decoder_(enable_direct_video_decoder) {}
  MojoMediaClientImpl(const MojoMediaClientImpl&) = delete;
  MojoMediaClientImpl& operator=(const MojoMediaClientImpl&) = delete;
  ~MojoMediaClientImpl() override = default;

  // MojoMediaClient implementation.
  std::vector<SupportedVideoDecoderConfig> GetSupportedVideoDecoderConfigs()
      final {
    absl::optional<std::vector<SupportedVideoDecoderConfig>> configs;
    switch (GetDecoderImplementationType()) {
      case VideoDecoderType::kVaapi:
      case VideoDecoderType::kV4L2:
        configs = VideoDecoderPipeline::GetSupportedConfigs(
            GetDecoderImplementationType(), gpu_driver_bug_workarounds_);
        break;
      case VideoDecoderType::kVda: {
        // Note that we pass a default-constructed gpu::GpuPreferences.
        // GpuVideoDecodeAcceleratorFactory::GetDecoderCapabilities() uses the
        // preferences only to check if accelerated video decoding is disabled.
        // However, if we're here, we know that accelerated video decoding is
        // enabled since the browser process checks for this.
        VideoDecodeAccelerator::Capabilities capabilities =
            GpuVideoAcceleratorUtil::ConvertGpuToMediaDecodeCapabilities(
                GpuVideoDecodeAcceleratorFactory::GetDecoderCapabilities(
                    gpu::GpuPreferences(), gpu_driver_bug_workarounds_));
        configs = ConvertFromSupportedProfiles(
            capabilities.supported_profiles,
            capabilities.flags & VideoDecodeAccelerator::Capabilities::
                                     SUPPORTS_ENCRYPTED_STREAMS);
        break;
      }
      default:
        NOTREACHED();
    }
    return configs.value_or(std::vector<SupportedVideoDecoderConfig>{});
  }
  VideoDecoderType GetDecoderImplementationType() final {
    if (!enable_direct_video_decoder_) {
      return VideoDecoderType::kVda;
    }

    // TODO(b/195769334): how can we keep this in sync with
    // VideoDecoderPipeline::GetDecoderType()?
#if BUILDFLAG(USE_VAAPI)
    return VideoDecoderType::kVaapi;
#elif BUILDFLAG(USE_V4L2_CODEC)
    return VideoDecoderType::kV4L2;
#else
#error StableVideoDecoderFactoryService should only be built on platforms that
#error support video decode acceleration through either VA-API or V4L2.
#endif
  }
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      MediaLog* media_log,
      mojom::CommandBufferIdPtr command_buffer_id,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder)
      final {
    // For out-of-process video decoding, |command_buffer_id| is not used and
    // should not be supplied.
    DCHECK(!command_buffer_id);

    DCHECK(!oop_video_decoder);

    std::unique_ptr<MediaLog> log =
        media_log ? media_log->Clone()
                  : std::make_unique<media::NullMediaLog>();

    if (GetDecoderImplementationType() == VideoDecoderType::kVda) {
      if (!gpu_task_runner_) {
        gpu_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
            {base::WithBaseSyncPrimitives(), base::MayBlock()},
            base::SingleThreadTaskRunnerThreadMode::DEDICATED);
      }
      // Note that we pass a default-constructed gpu::GpuPreferences.
      // VdaVideoDecoder::Create() uses the preferences only to check if
      // accelerated video decoding is disabled. However, if we're here, we know
      // that accelerated video decoding is enabled since the browser process
      // checks for this.
      return VdaVideoDecoder::Create(
          /*parent_task_runner=*/std::move(task_runner), gpu_task_runner_,
          std::move(log), target_color_space, gpu::GpuPreferences(),
          gpu_driver_bug_workarounds_,
          /*get_stub_cb=*/base::NullCallback(),
          VideoDecodeAccelerator::Config::OutputMode::IMPORT);
    } else {
      return VideoDecoderPipeline::Create(
          gpu_driver_bug_workarounds_,
          /*client_task_runner=*/std::move(task_runner),
          std::make_unique<PlatformVideoFramePool>(),
          /*frame_converter=*/nullptr,
          VideoDecoderPipeline::DefaultPreferredRenderableFourccs(),
          std::move(log),
          /*oop_video_decoder=*/{});
    }
  }

 private:
  // A "GPU" thread. With traditional hardware video decoding that runs in the
  // GPU process, this would be the thread needed to access specific GPU
  // functionality. For out-of-process video decoding, this isn't really the
  // "GPU" thread, but we use the terminology of the VdaVideoDecoder::Create()
  // (as such this member is only used when using the VdaVideoDecoder).
  //
  // TODO(b/195769334): could we get rid of this and just use the same task
  // runner for the |parent_task_runner| and |gpu_task_runner| parameters of
  // VdaVideoDecoder::Create(). For now, we've made it a dedicated thread in
  // case the VdaVideoDecoder or any of the underlying components rely on a
  // separate GPU thread.
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  const gpu::GpuDriverBugWorkarounds gpu_driver_bug_workarounds_;
  const bool enable_direct_video_decoder_;
};

}  // namespace

StableVideoDecoderFactoryService::StableVideoDecoderFactoryService(
    const gpu::GpuFeatureInfo& gpu_feature_info,
    bool enable_direct_video_decoder)
    : receiver_(this),
      mojo_media_client_(
          std::make_unique<MojoMediaClientImpl>(gpu_feature_info,
                                                enable_direct_video_decoder)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo_media_client_->Initialize();
}

StableVideoDecoderFactoryService::~StableVideoDecoderFactoryService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void StableVideoDecoderFactoryService::BindReceiver(
    mojo::PendingReceiver<stable::mojom::StableVideoDecoderFactory> receiver,
    base::OnceClosure disconnect_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The browser process should guarantee that BindReceiver() is only called
  // once.
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(std::move(disconnect_cb));
}

void StableVideoDecoderFactoryService::CreateStableVideoDecoder(
    mojo::PendingReceiver<stable::mojom::StableVideoDecoder> receiver,
    mojo::PendingRemote<stable::mojom::StableVideoDecoderTracker> tracker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<mojom::VideoDecoder> dst_video_decoder;
  if (video_decoder_creation_cb_for_testing_) {
    dst_video_decoder = video_decoder_creation_cb_for_testing_.Run(
        mojo_media_client_.get(), &cdm_service_context_);
  } else {
    dst_video_decoder = std::make_unique<MojoVideoDecoderService>(
        mojo_media_client_.get(), &cdm_service_context_,
        mojo::PendingRemote<stable::mojom::StableVideoDecoder>());
  }
  video_decoders_.Add(std::make_unique<StableVideoDecoderService>(
                          std::move(tracker), std::move(dst_video_decoder),
                          &cdm_service_context_),
                      std::move(receiver));
}

}  // namespace media
