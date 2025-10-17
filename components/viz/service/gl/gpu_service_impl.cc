// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/gpu_service_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/allocator/partition_alloc_support.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/startup_metric_utils/gpu/startup_metric_utils.h"
#include "components/version_info/version_info.h"
#include "components/viz/common/features.h"
#include "components/viz/common/resources/peak_gpu_memory_tracker_util.h"
#include "components/viz/service/gl/gpu_log_message_manager.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/service/dawn_caching_interface.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/gpu_peak_memory.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "gpu/ipc/service/image_decode_accelerator_worker.h"
#include "gpu/vulkan/buildflags.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "media/mojo/services/gpu_mojo_media_client.h"
#include "media/mojo/services/mojo_video_encode_accelerator_provider.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLInterface.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/ui_base_features.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/init/create_gr_gl_interface.h"
#include "ui/gl/init/gl_factory.h"
#include "url/gurl.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_image_decode_accelerator_worker.h"
#endif  // BUILDFLAG(USE_VAAPI)

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "components/viz/service/gl/throw_uncaught_exception.h"
#include "media/base/android/media_codec_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "components/chromeos_camera/gpu_mjpeg_decode_accelerator_factory.h"
#include "components/chromeos_camera/mojo_jpeg_encode_accelerator_service.h"
#include "components/chromeos_camera/mojo_mjpeg_decode_accelerator_service.h"

#if BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)
#include "chromeos/ash/experiences/arc/video_accelerator/gpu_arc_video_decode_accelerator.h"
#include "chromeos/ash/experiences/arc/video_accelerator/gpu_arc_video_decoder.h"
#include "chromeos/ash/experiences/arc/video_accelerator/gpu_arc_video_encode_accelerator.h"
#include "chromeos/ash/experiences/arc/video_accelerator/gpu_arc_video_protected_buffer_allocator.h"
#include "chromeos/ash/experiences/arc/video_accelerator/protected_buffer_manager.h"
#include "chromeos/ash/experiences/arc/video_accelerator/protected_buffer_manager_proxy.h"
#endif  // BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
#include "components/viz/common/overlay_state/win/overlay_state_service.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing_factory.h"
#include "media/base/win/mf_feature_checks.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gl/dcomp_surface_registry.h"
#include "ui/gl/direct_composition_support.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "ui/base/cocoa/quartz_util.h"
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#endif

#if BUILDFLAG(SKIA_USE_METAL)
#include "components/viz/common/gpu/metal_context_provider.h"
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
#include "base/test/clang_profiling.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif  // BUILDFLAG(IS_OZONE)

namespace viz {

namespace {

// The names emitted for GPU initialization trace events.
// This code may be removed after the following investigation:
// crbug.com/1350257
constexpr char kGpuInitializationEventCategory[] = "latency";
constexpr char kGpuInitializationEvent[] = "GpuInitialization";

bool IsAcceleratedJpegDecodeSupported() {
#if BUILDFLAG(IS_CHROMEOS)
  return chromeos_camera::GpuMjpegDecodeAcceleratorFactory::
      IsAcceleratedJpegDecodeSupported();
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool WillGetGmbConfigFromGpu() {
#if BUILDFLAG(IS_OZONE)
  // Ozone/X11 requires gpu initialization to be done before it can determine
  // what formats gmb can use. This limitation comes from the requirement to
  // have GLX bindings initialized. The buffer formats will be passed through
  // gpu extra info.
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .fetch_buffer_formats_for_gmb_on_gpu;
#else
  return false;
#endif
}

void RunGetPeakGpuMemoryUsageCallbackOnMainThread(
    GpuServiceImpl::GetPeakMemoryUsageCallback callback,
    uint64_t peak_memory,
    base::flat_map<gpu::GpuPeakMemoryAllocationSource, uint64_t>
        allocation_per_source) {
  std::move(callback).Run(peak_memory, std::move(allocation_per_source));
}

}  // namespace

GpuServiceImpl::GpuServiceImpl(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const std::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
    const std::optional<gpu::GpuFeatureInfo>& gpu_feature_info_for_hardware_gpu,
    const gfx::GpuExtraInfo& gpu_extra_info,
    InitParams init_params)
    : main_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      io_runner_(std::move(init_params.io_runner)),
      watchdog_thread_(std::move(init_params.watchdog_thread)),
      gpu_preferences_(gpu_preferences),
      gpu_info_(gpu_info),
      gpu_feature_info_(gpu_feature_info),
      gpu_driver_bug_workarounds_(
          gpu_feature_info_.enabled_gpu_driver_bug_workarounds),
      gpu_info_for_hardware_gpu_(gpu_info_for_hardware_gpu),
      gpu_feature_info_for_hardware_gpu_(gpu_feature_info_for_hardware_gpu),
      gpu_extra_info_(gpu_extra_info),
#if BUILDFLAG(ENABLE_VULKAN)
      vulkan_implementation_(init_params.vulkan_implementation),
#endif
      clear_shader_cache_(base::FeatureList::IsEnabled(
          features::kClearGrShaderDiskCacheOnInvalidPrefix)) {
  DCHECK(!io_runner_->BelongsToCurrentThread());

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)
  protected_buffer_manager_ = new arc::ProtectedBufferManager();
#endif  // BUILDFLAG(IS_CHROMEOS) &&
        // BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)

#if BUILDFLAG(ENABLE_VULKAN)
  if (vulkan_implementation_) {
    bool is_native_vulkan =
        gpu_preferences_.use_vulkan == gpu::VulkanImplementationName::kNative ||
        gpu_preferences_.use_vulkan ==
            gpu::VulkanImplementationName::kForcedNative;
    // With swiftshader the vendor_id is 0xffff. For some tests gpu_info is not
    // initialized, so the vendor_id is 0.
    bool is_native_gl =
        gpu_info_.gpu.vendor_id != 0xffff && gpu_info_.gpu.vendor_id != 0;

    const bool is_thread_safe =
        features::IsDrDcEnabled() && !gpu_driver_bug_workarounds_.disable_drdc;
    // If GL is using a real GPU, the gpu_info will be passed in and vulkan will
    // use the same GPU.
    vulkan_context_provider_ = VulkanInProcessContextProvider::Create(
        vulkan_implementation_, gpu_preferences_.vulkan_heap_memory_limit,
        gpu_preferences_.vulkan_sync_cpu_memory_limit, is_thread_safe,
        (is_native_vulkan && is_native_gl) ? &gpu_info_ : nullptr);
    if (!vulkan_context_provider_) {
      DLOG(ERROR) << "Failed to create Vulkan context provider.";
    }
  }
#endif

#if BUILDFLAG(USE_DAWN) || BUILDFLAG(SKIA_USE_DAWN)
  dawn_caching_interface_factory_ =
      std::make_unique<gpu::webgpu::DawnCachingInterfaceFactory>();
#endif

  if (gpu_preferences_.gr_context_type == gpu::GrContextType::kGraphiteDawn) {
#if BUILDFLAG(SKIA_USE_DAWN)
    dawn_context_provider_ = std::move(init_params.dawn_context_provider);

    if (dawn_context_provider_) {
      // GpuServiceImpl holds the instance of DawnContextProvider, so it
      // outlives the DawnContextProvider.
      auto cache_blob_callback = base::BindRepeating(
          [](GpuServiceImpl* self, const std::string& key,
             const std::string& blob) {
            self->StoreBlobToDisk(gpu::kGraphiteDawnGpuDiskCacheHandle, key,
                                  blob);
          },
          base::Unretained(this));
      auto caching_interface = dawn_caching_interface_factory_->CreateInstance(
          gpu::kGraphiteDawnGpuDiskCacheHandle, std::move(cache_blob_callback));
      dawn_context_provider_->SetCachingInterface(std::move(caching_interface));
    }
#endif  // BUILDFLAG(SKIA_USE_DAWN)
  } else if (gpu_preferences_.gr_context_type ==
             gpu::GrContextType::kGraphiteMetal) {
#if BUILDFLAG(SKIA_USE_METAL)
    metal_context_provider_ = MetalContextProvider::Create();
    if (!metal_context_provider_) {
      DLOG(ERROR) << "Failed to create Metal context provider for Graphite.";
    }
#endif  // BUILDFLAG(SKIA_USE_METAL)
  }

#if BUILDFLAG(USE_VAAPI_IMAGE_CODECS)
  image_decode_accelerator_worker_ =
      media::VaapiImageDecodeAcceleratorWorker::Create();
#endif  // BUILDFLAG(USE_VAAPI_IMAGE_CODECS)

#if BUILDFLAG(IS_WIN)
  if (media::SupportMediaFoundationClearPlayback()) {
    // Initialize the OverlayStateService using the GPUServiceImpl task
    // sequence.
    auto* overlay_state_service = OverlayStateService::GetInstance();
    overlay_state_service->Initialize(
        base::SequencedTaskRunner::GetCurrentDefault());
  }
#endif

  gpu_memory_buffer_factory_ = gpu::GpuMemoryBufferFactory::CreateNativeType(
      vulkan_context_provider(), io_runner_);

  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

GpuServiceImpl::GpuServiceImpl()
    : clear_shader_cache_(base::FeatureList::IsEnabled(
          features::kClearGrShaderDiskCacheOnInvalidPrefix)) {}

GpuServiceImpl::~GpuServiceImpl() {
  if (main_runner_) {
    DCHECK(main_runner_->BelongsToCurrentThread());
  }

  // Ensure we don't try to exit when already in the process of exiting.
  is_exiting_.Set();

  bind_task_tracker_.TryCancelAll();

  if (!in_host_process()) {
    GpuLogMessageManager::GetInstance()->ShutdownLogging();
  }

#if BUILDFLAG(IS_WIN)
  gl::DirectCompositionOverlayCapsMonitor::GetInstance()->RemoveObserver(this);
#endif

  // Destroy the receiver on the IO thread.
  {
    base::WaitableEvent wait;
    auto destroy_receiver_task = base::BindOnce(
        [](mojo::Receiver<mojom::GpuService>* receiver,
           base::WaitableEvent* wait) {
          receiver->reset();
          wait->Signal();
        },
        &receiver_, base::Unretained(&wait));
    if (io_runner_ &&
        io_runner_->PostTask(FROM_HERE, std::move(destroy_receiver_task))) {
      wait.Wait();
    }
  }

  if (watchdog_thread_)
    watchdog_thread_->OnGpuProcessTearDown();

  compositor_gpu_thread_.reset();
  media_gpu_channel_manager_.reset();
  gpu_channel_manager_.reset();

  // Destroy |gpu_memory_buffer_factory_| on the IO thread since its weakptrs
  // are checked there.
  {
    base::WaitableEvent wait;
    auto destroy_gmb_factory = base::BindOnce(
        [](std::unique_ptr<gpu::GpuMemoryBufferFactory> gmb_factory,
           base::WaitableEvent* wait) {
          gmb_factory.reset();
          wait->Signal();
        },
        std::move(gpu_memory_buffer_factory_), base::Unretained(&wait));

    if (io_runner_ &&
        io_runner_->PostTask(FROM_HERE, std::move(destroy_gmb_factory))) {
      // |gpu_memory_buffer_factory_| holds a raw pointer to
      // |vulkan_context_provider_|. Waiting here enforces the correct order
      // of destruction.
      wait.Wait();
    }
  }

  // Scheduler must be destroyed before sync point manager is destroyed.
  owned_scheduler_.reset();
  owned_sync_point_manager_.reset();
  owned_shared_image_manager_.reset();

  // The image decode accelerator worker must outlive the GPU channel manager so
  // that it doesn't get any decode requests during/after destruction.
  DCHECK(!gpu_channel_manager_);
  image_decode_accelerator_worker_.reset();

  // Signal this event before destroying the child process. That way all
  // background threads can cleanup. For example, in the renderer the
  // RenderThread instances will be able to notice shutdown before the render
  // process begins waiting for them to exit.
  if (owned_shutdown_event_)
    owned_shutdown_event_->Signal();
}

void GpuServiceImpl::UpdateGPUInfo() {
  DCHECK(main_runner_->BelongsToCurrentThread());
  DCHECK(!gpu_host_);

  gpu_info_.jpeg_decode_accelerator_supported =
      IsAcceleratedJpegDecodeSupported();

  if (image_decode_accelerator_worker_) {
    gpu_info_.image_decode_accelerator_supported_profiles =
        image_decode_accelerator_worker_->GetSupportedProfiles();
  }

  // Record initialization only after collecting the GPU info because that can
  // take a significant amount of time.
  base::TimeTicks now = base::TimeTicks::Now();
  gpu_info_.initialization_time = now - start_time_;
  startup_metric_utils::GetGpu().RecordGpuInitialized(now);

  // This metric code may be removed after the following investigation:
  // crbug.com/1350257
  UMA_HISTOGRAM_CUSTOM_TIMES("GPU.GPUInitializationTime.V4",
                             gpu_info_.initialization_time,
                             base::Milliseconds(5), base::Seconds(90), 100);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      kGpuInitializationEventCategory, kGpuInitializationEvent,
      TRACE_ID_LOCAL(this), start_time_);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      kGpuInitializationEventCategory, kGpuInitializationEvent,
      TRACE_ID_LOCAL(this), now);
}

void GpuServiceImpl::UpdateGPUInfoGL() {
  DCHECK(main_runner_->BelongsToCurrentThread());
  gpu::CollectGraphicsInfoGL(&gpu_info_, GetContextState()->display());
  gpu_host_->DidUpdateGPUInfo(gpu_info_);
}

#if BUILDFLAG(IS_ANDROID)
void GpuServiceImpl::InitializeWithHost(
    mojo::PendingRemote<mojom::GpuHost> pending_gpu_host,
    gpu::GpuProcessShmCount use_shader_cache_shm_count,
    scoped_refptr<gl::GLSurface> default_offscreen_surface,
    mojom::GpuServiceCreationParamsPtr creation_params,
    gpu::SyncPointManager* sync_point_manager,
    gpu::SharedImageManager* shared_image_manager,
    gpu::Scheduler* scheduler,
    base::WaitableEvent* shutdown_event,
    const gpu::SharedContextState::GrContextOptionsProvider*
        gr_context_options_provider) {
  if (!sync_point_manager) {
    sync_point_manager = CreateSyncPointManager();
  }

  if (!shared_image_manager) {
#if BUILDFLAG(IS_OZONE)
    shared_image_manager =
        CreateSharedImageManager(creation_params->supports_overlays);
#else
    shared_image_manager = CreateSharedImageManager();
#endif
  }

  if (!scheduler) {
    scheduler = CreateScheduler(sync_point_manager);
  }

  if (!shutdown_event) {
    shutdown_event = CreateShutdownEvent();
  }

  gr_context_options_provider_ = gr_context_options_provider;

  InitializeWithHostInternal(
      std::move(pending_gpu_host), std::move(use_shader_cache_shm_count),
      default_offscreen_surface, std::move(creation_params), sync_point_manager,
      shared_image_manager, scheduler, shutdown_event);
}
#else
void GpuServiceImpl::InitializeWithHost(
    mojo::PendingRemote<mojom::GpuHost> pending_gpu_host,
    gpu::GpuProcessShmCount use_shader_cache_shm_count,
    scoped_refptr<gl::GLSurface> default_offscreen_surface,
    mojom::GpuServiceCreationParamsPtr creation_params,
    base::WaitableEvent* shutdown_event) {
  gpu::SyncPointManager* sync_point_manager = CreateSyncPointManager();
#if BUILDFLAG(IS_OZONE)
  gpu::SharedImageManager* shared_image_manager =
      CreateSharedImageManager(creation_params->supports_overlays);
#else
  gpu::SharedImageManager* shared_image_manager = CreateSharedImageManager();
#endif
  gpu::Scheduler* scheduler = CreateScheduler(sync_point_manager);

  if (!shutdown_event) {
    shutdown_event = CreateShutdownEvent();
  }

  InitializeWithHostInternal(
      std::move(pending_gpu_host), std::move(use_shader_cache_shm_count),
      default_offscreen_surface, std::move(creation_params), sync_point_manager,
      shared_image_manager, scheduler, shutdown_event);

#if BUILDFLAG(IS_WIN)
  // shared_image_d3d must be initialized after we call
  // InitializeWithHostInternal as that is where the shared context state is
  // created.
  auto shared_context_state = GetContextState();
  if (shared_context_state) {
    gpu_info_.shared_image_d3d =
        gpu::D3DImageBackingFactory::IsD3DSharedImageSupported(
            shared_context_state->GetD3D11Device().Get(), gpu_preferences_);

    gpu_host_->DidUpdateGPUInfo(gpu_info_);
  }
#endif
}
#endif

void GpuServiceImpl::InitializeWithHostInternal(
    mojo::PendingRemote<mojom::GpuHost> pending_gpu_host,
    gpu::GpuProcessShmCount use_shader_cache_shm_count,
    scoped_refptr<gl::GLSurface> default_offscreen_surface,
    mojom::GpuServiceCreationParamsPtr creation_params,
    gpu::SyncPointManager* sync_point_manager,
    gpu::SharedImageManager* shared_image_manager,
    gpu::Scheduler* scheduler,
    base::WaitableEvent* shutdown_event) {
  DCHECK(main_runner_->BelongsToCurrentThread());

  scheduler_ = scheduler;
  shutdown_event_ = shutdown_event;

  mojo::Remote<mojom::GpuHost> gpu_host(std::move(pending_gpu_host));

#if BUILDFLAG(IS_LINUX)
  gpu_extra_info_.is_gmb_nv12_supported = IsGMBNV12Supported();
#endif

  gpu_host->DidInitialize(gpu_info_, gpu_feature_info_,
                          gpu_info_for_hardware_gpu_,
                          gpu_feature_info_for_hardware_gpu_, gpu_extra_info_);
  gpu_host_ = mojo::SharedRemote<mojom::GpuHost>(gpu_host.Unbind(), io_runner_);
  if (!in_host_process()) {
    // The global callback is reset from the dtor. So Unretained() here is safe.
    // Note that the callback can be called from any thread. Consequently, the
    // callback cannot use a WeakPtr.
    GpuLogMessageManager::GetInstance()->InstallPostInitializeLogHandler(
        base::BindRepeating(&GpuServiceImpl::RecordLogMessage,
                            base::Unretained(this)));
  }

  // Defer creation of the render thread. This is to prevent it from handling
  // IPC messages before the sandbox has been enabled and all other necessary
  // initialization has succeeded.
  gpu_channel_manager_ = std::make_unique<gpu::GpuChannelManager>(
      gpu_preferences_, this, watchdog_thread_.get(), main_runner_, io_runner_,
      scheduler_, sync_point_manager, shared_image_manager,
      gpu_memory_buffer_factory_.get(), gpu_feature_info_,
      std::move(use_shader_cache_shm_count),
      std::move(default_offscreen_surface),
      image_decode_accelerator_worker_.get(), vulkan_context_provider(),
      metal_context_provider(), dawn_context_provider(),
      dawn_caching_interface_factory(), gr_context_options_provider_);

  media_gpu_channel_manager_ = std::make_unique<media::MediaGpuChannelManager>(
      gpu_channel_manager_.get());

  // Create and Initialize compositor gpu thread.
  {
    CompositorGpuThread::CreateParams params;
    params.gpu_channel_manager = gpu_channel_manager_.get();
    params.display =
        gpu_channel_manager_->default_offscreen_surface()
            ? gpu_channel_manager_->default_offscreen_surface()->GetGLDisplay()
            : nullptr;
    params.enable_watchdog = !!watchdog_thread_;

#if BUILDFLAG(ENABLE_VULKAN)
    params.vulkan_implementation = vulkan_implementation_;
    params.device_queue = vulkan_context_provider_
                              ? vulkan_context_provider_->GetDeviceQueue()
                              : nullptr;
#endif
#if BUILDFLAG(SKIA_USE_DAWN)
    params.dawn_context_provider = dawn_context_provider_.get();
#endif
    compositor_gpu_thread_ = CompositorGpuThread::MaybeCreate(params);
  }

#if BUILDFLAG(IS_WIN)
  // Add GpuServiceImpl to DirectCompositionOverlayCapsMonitor observer list for
  // overlay and DXGI info update. This should be added after |gpu_host_| is
  // initialized.
  gl::DirectCompositionOverlayCapsMonitor::GetInstance()->AddObserver(this);
#endif
}

void GpuServiceImpl::Bind(
    mojo::PendingReceiver<mojom::GpuService> pending_receiver) {
  if (main_runner_->BelongsToCurrentThread()) {
    bind_task_tracker_.PostTask(
        io_runner_.get(), FROM_HERE,
        base::BindOnce(&GpuServiceImpl::Bind, base::Unretained(this),
                       std::move(pending_receiver)));
    return;
  }
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(pending_receiver));
}

void GpuServiceImpl::DisableGpuCompositing() {
  // Can be called from any thread.
  gpu_host_->DisableGpuCompositing();
}

scoped_refptr<gpu::SharedContextState> GpuServiceImpl::GetContextState() {
  DCHECK(main_runner_->BelongsToCurrentThread());
  gpu::ContextResult result;
  return gpu_channel_manager_->GetSharedContextState(&result);
}

void GpuServiceImpl::SetVisibilityChangedCallback(
    VisibilityChangedCallback callback) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  visibility_changed_callback_ = std::move(callback);
}

void GpuServiceImpl::RecordLogMessage(int severity,
                                      const std::string& header,
                                      const std::string& message) {
  // This can be run from any thread.
  gpu_host_->RecordLogMessage(severity, std::move(header), std::move(message));
}

#if BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)
void GpuServiceImpl::CreateArcVideoDecodeAccelerator(
    mojo::PendingReceiver<arc::mojom::VideoDecodeAccelerator> vda_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GpuServiceImpl::CreateArcVideoDecodeAcceleratorOnMainThread,
          weak_ptr_, std::move(vda_receiver)));
}

void GpuServiceImpl::CreateArcVideoDecoder(
    mojo::PendingReceiver<arc::mojom::VideoDecoder> vd_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuServiceImpl::CreateArcVideoDecoderOnMainThread,
                     weak_ptr_, std::move(vd_receiver)));
}

void GpuServiceImpl::CreateArcVideoEncodeAccelerator(
    mojo::PendingReceiver<arc::mojom::VideoEncodeAccelerator> vea_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GpuServiceImpl::CreateArcVideoEncodeAcceleratorOnMainThread,
          weak_ptr_, std::move(vea_receiver)));
}

void GpuServiceImpl::CreateArcVideoProtectedBufferAllocator(
    mojo::PendingReceiver<arc::mojom::VideoProtectedBufferAllocator>
        pba_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GpuServiceImpl::CreateArcVideoProtectedBufferAllocatorOnMainThread,
          weak_ptr_, std::move(pba_receiver)));
}

void GpuServiceImpl::CreateArcProtectedBufferManager(
    mojo::PendingReceiver<arc::mojom::ProtectedBufferManager> pbm_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GpuServiceImpl::CreateArcProtectedBufferManagerOnMainThread,
          weak_ptr_, std::move(pbm_receiver)));
}

void GpuServiceImpl::CreateArcVideoDecodeAcceleratorOnMainThread(
    mojo::PendingReceiver<arc::mojom::VideoDecodeAccelerator> vda_receiver) {
  CHECK(main_runner_->BelongsToCurrentThread());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<arc::GpuArcVideoDecodeAccelerator>(
          gpu_preferences_, gpu_channel_manager_->gpu_driver_bug_workarounds(),
          protected_buffer_manager_),
      std::move(vda_receiver));
}

void GpuServiceImpl::CreateArcVideoDecoderOnMainThread(
    mojo::PendingReceiver<arc::mojom::VideoDecoder> vd_receiver) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<arc::GpuArcVideoDecoder>(protected_buffer_manager_),
      std::move(vd_receiver));
}

void GpuServiceImpl::CreateArcVideoEncodeAcceleratorOnMainThread(
    mojo::PendingReceiver<arc::mojom::VideoEncodeAccelerator> vea_receiver) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<arc::GpuArcVideoEncodeAccelerator>(
          gpu_preferences_, gpu_channel_manager_->gpu_driver_bug_workarounds()),
      std::move(vea_receiver));
}

void GpuServiceImpl::CreateArcVideoProtectedBufferAllocatorOnMainThread(
    mojo::PendingReceiver<arc::mojom::VideoProtectedBufferAllocator>
        pba_receiver) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  auto gpu_arc_video_protected_buffer_allocator =
      arc::GpuArcVideoProtectedBufferAllocator::Create(
          protected_buffer_manager_);
  if (!gpu_arc_video_protected_buffer_allocator)
    return;
  mojo::MakeSelfOwnedReceiver(
      std::move(gpu_arc_video_protected_buffer_allocator),
      std::move(pba_receiver));
}

void GpuServiceImpl::CreateArcProtectedBufferManagerOnMainThread(
    mojo::PendingReceiver<arc::mojom::ProtectedBufferManager> pbm_receiver) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<arc::GpuArcProtectedBufferManagerProxy>(
          protected_buffer_manager_),
      std::move(pbm_receiver));
}
#endif  // BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)

void GpuServiceImpl::CreateJpegDecodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
        jda_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  chromeos_camera::MojoMjpegDecodeAcceleratorService::Create(
      std::move(jda_receiver),
      base::BindRepeating(
          &GpuServiceImpl::SetMjpegDecodeAcceleratorBeginFrameCB,
          base::Unretained(this)));
}

void GpuServiceImpl::CreateJpegEncodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
        jea_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  chromeos_camera::MojoJpegEncodeAcceleratorService::Create(
      std::move(jea_receiver));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
void GpuServiceImpl::RegisterDCOMPSurfaceHandle(
    mojo::PlatformHandle surface_handle,
    RegisterDCOMPSurfaceHandleCallback callback) {
  base::UnguessableToken token =
      gl::DCOMPSurfaceRegistry::GetInstance()->RegisterDCOMPSurfaceHandle(
          surface_handle.TakeHandle());
  std::move(callback).Run(token);
}

void GpuServiceImpl::UnregisterDCOMPSurfaceHandle(
    const base::UnguessableToken& token) {
  gl::DCOMPSurfaceRegistry::GetInstance()->UnregisterDCOMPSurfaceHandle(token);
}
#endif  // BUILDFLAG(IS_WIN)

void GpuServiceImpl::CreateVideoEncodeAcceleratorProvider(
    mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
        vea_provider_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());

  // Offload VEA providers to a dedicated runner. Things like loading profiles
  // and creating encoder might take quite some time, and they might block
  // processing of other mojo calls if executed on the current runner.
  scoped_refptr<base::SequencedTaskRunner> runner;
#if BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/40850116): Fuchsia does not support FIDL communication from
  // ThreadPool's worker threads.
  if (!vea_thread_) {
    base::Thread::Options thread_options(base::MessagePumpType::IO, /*size=*/0);
    vea_thread_ =
        std::make_unique<base::Thread>("GpuVideoEncodeAcceleratorThread");
    CHECK(vea_thread_->StartWithOptions(std::move(thread_options)));
  }
  runner = vea_thread_->task_runner();
#elif BUILDFLAG(IS_WIN)
  // Windows hardware encoder requires a COM STA thread.
  runner = base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()});
#else
  // MayBlock() because MF VEA can take long time running GetSupportedProfiles()
  if (base::FeatureList::IsEnabled(
          media::kUseSequencedTaskRunnerForMojoVEAProvider)) {
    runner = base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  } else {
    runner = base::ThreadPool::CreateSingleThreadTaskRunner({base::MayBlock()});
  }
#endif
  media::MojoVideoEncodeAcceleratorProvider::Create(
      std::move(vea_provider_receiver),
      base::BindRepeating(&media::GpuVideoEncodeAcceleratorFactory::CreateVEA),
      gpu_preferences_, gpu_channel_manager_->gpu_driver_bug_workarounds(),
      gpu_info_.active_gpu(), std::move(runner),
      media_gpu_channel_manager_->AsWeakPtr(), main_runner_);
}

void GpuServiceImpl::BindWebNNContextProvider(
    mojo::PendingReceiver<webnn::mojom::WebNNContextProvider> pending_receiver,
    int client_id) {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuServiceImpl::BindWebNNContextProvider, weak_ptr_,
                       std::move(pending_receiver), client_id));
    return;
  }

  if (!webnn_context_provider_) {
    scoped_refptr<gpu::SharedContextState> shared_context_state =
        GetContextState();
    if (!shared_context_state) {
      return;
    }
    // TODO(crbug.com/345352987): manage `WebNNContextProviderImpl` instance per
    // `client_id` in order to support memory metrics.
    webnn_context_provider_ = webnn::WebNNContextProviderImpl::Create(
        std::move(shared_context_state), gpu_feature_info_, gpu_info_,
        base::BindOnce(&GpuServiceImpl::LoseAllContexts, weak_ptr_),
        main_runner(), GetGpuScheduler(), client_id);
  }

  webnn_context_provider_->BindWebNNContextProvider(
      std::move(pending_receiver));
}

void GpuServiceImpl::GetVideoMemoryUsageStats(
    GetVideoMemoryUsageStatsCallback callback) {
  if (io_runner_->BelongsToCurrentThread()) {
    auto wrap_callback = base::BindPostTask(io_runner_, std::move(callback));
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::GetVideoMemoryUsageStats,
                                  weak_ptr_, std::move(wrap_callback)));
    return;
  }
  gpu::VideoMemoryUsageStats video_memory_usage_stats;
  gpu_channel_manager_->GetVideoMemoryUsageStats(&video_memory_usage_stats);
  std::move(callback).Run(video_memory_usage_stats);
}

void GpuServiceImpl::StartPeakMemoryMonitor(uint32_t sequence_num) {
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuServiceImpl::StartPeakMemoryMonitorOnMainThread,
                     weak_ptr_, sequence_num));
}

void GpuServiceImpl::GetPeakMemoryUsage(uint32_t sequence_num,
                                        GetPeakMemoryUsageCallback callback) {
  main_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GpuServiceImpl::GetPeakMemoryUsageOnMainThread,
                                weak_ptr_, sequence_num, std::move(callback)));
}

#if BUILDFLAG(IS_WIN)
void GpuServiceImpl::RequestDXGIInfo(RequestDXGIInfoCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GpuServiceImpl::RequestDXGIInfoOnMainThread,
                                weak_ptr_, std::move(callback)));
}

void GpuServiceImpl::RequestDXGIInfoOnMainThread(
    RequestDXGIInfoCallback callback) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  dxgi_info_ = gl::GetDirectCompositionHDRMonitorDXGIInfo();
  io_runner_->PostTask(FROM_HERE,
                       base::BindOnce(std::move(callback), dxgi_info_.Clone()));
}
#endif

void GpuServiceImpl::LoseAllContexts() {
  if (IsExiting())
    return;
  gpu_channel_manager_->LoseAllContexts();
  if (compositor_gpu_thread_)
    compositor_gpu_thread_->LoseContext();
}

void GpuServiceImpl::DidCreateContextSuccessfully() {
  gpu_host_->DidCreateContextSuccessfully();
}

void GpuServiceImpl::DidCreateOffscreenContext(const GURL& active_url) {
  gpu_host_->DidCreateOffscreenContext(active_url);
}

void GpuServiceImpl::DidDestroyChannel(int client_id) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  media_gpu_channel_manager_->RemoveChannel(client_id);
  gpu_host_->DidDestroyChannel(client_id);
}

void GpuServiceImpl::DidDestroyAllChannels() {
  DCHECK(main_runner_->BelongsToCurrentThread());
  gpu_host_->DidDestroyAllChannels();
}

void GpuServiceImpl::DidDestroyOffscreenContext(const GURL& active_url) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  gpu_host_->DidDestroyOffscreenContext(active_url);
}

void GpuServiceImpl::DidLoseContext(gpu::error::ContextLostReason reason,
                                    const GURL& active_url) {
  gpu_host_->DidLoseContext(reason, active_url);
}

std::string GpuServiceImpl::GetShaderPrefixKey() {
  if (shader_prefix_key_.empty()) {
    const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info_.active_gpu();
    std::string product =
        std::string(version_info::GetProductNameAndVersionForUserAgent());

    shader_prefix_key_ =
        product + "-" + gpu_info_.gl_vendor + "-" + gpu_info_.gl_renderer +
        "-" + active_gpu.driver_version + "-" + active_gpu.driver_vendor + "-" +
        base::SysInfo::ProcessCPUArchitecture();

#if BUILDFLAG(IS_ANDROID)
    std::string build_fp =
        base::android::BuildInfo::GetInstance()->android_build_fp();
    shader_prefix_key_ += "-" + build_fp;
#endif
  }

  return shader_prefix_key_;
}

void GpuServiceImpl::StoreBlobToDisk(const gpu::GpuDiskCacheHandle& handle,
                                     const std::string& key,
                                     const std::string& shader) {
  std::string prefix_key = key;
  if (GetHandleType(handle) == gpu::GpuDiskCacheType::kGlShaders) {
    std::string prefix = GetShaderPrefixKey();
    prefix_key = prefix + ":" + key;
  }
  gpu_host_->StoreBlobToDisk(handle, prefix_key, shader);
}

void GpuServiceImpl::LoadedBlob(const gpu::GpuDiskCacheHandle& handle,
                                const std::string& key,
                                const std::string& data) {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::LoadedBlob, weak_ptr_,
                                  handle, key, data));
    return;
  }

  std::string no_prefix_key = key;
  if (GetHandleType(handle) == gpu::GpuDiskCacheType::kGlShaders) {
    std::string prefix = GetShaderPrefixKey();
    bool prefix_ok = !key.compare(0, prefix.length(), prefix);
    UMA_HISTOGRAM_BOOLEAN("GPU.ShaderLoadPrefixOK", prefix_ok);
    if (prefix_ok) {
      // Remove the prefix from the key before load.
      no_prefix_key = key.substr(prefix.length() + 1);
    } else {
      // If the prefix is not ok, its likely that all the other entries in the
      // cache will have prefix that does not matches. Clear the whole disk
      // cache in that case to remove all stale entries and make room for newer
      // entries.
      if (clear_shader_cache_) {
        gpu_host_->ClearGrShaderDiskCache();
      }
      return;
    }
  }
  gpu_channel_manager_->PopulateCache(handle, no_prefix_key, data);
}

void GpuServiceImpl::GetIsolationKey(
    int client_id,
    const blink::WebGPUExecutionContextToken& token,
    GetIsolationKeyCallback cb) {
  gpu_host_->GetIsolationKey(client_id, token, std::move(cb));
}

void GpuServiceImpl::MaybeExitOnContextLost(
    gpu::error::ContextLostReason context_lost_reason) {
  DCHECK(main_runner_->BelongsToCurrentThread());

  if (in_host_process()) {
    // We can't restart the GPU process when running in the host process;
    // instead, just hope for recovery from the context loss.
    return;
  }

  RestartGpuProcessForContextLoss("Context was lost.");
}

bool GpuServiceImpl::IsExiting() const {
  return is_exiting_.IsSet();
}

void GpuServiceImpl::EstablishGpuChannel(int32_t client_id,
                                         uint64_t client_tracing_id,
                                         bool is_gpu_host,
                                         EstablishGpuChannelCallback callback) {
  // This should always be called on the IO thread first.
  if (io_runner_->BelongsToCurrentThread()) {
    if (IsExiting()) {
      // We are already exiting so there is no point in responding. Close the
      // receiver so we can safely drop the callback.
      receiver_.reset();
      return;
    }

    if (gpu::IsReservedClientId(client_id)) {
      // This returns a null handle, which is treated by the client as a failure
      // case.
      std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                              gpu::GpuFeatureInfo(),
                              gpu::SharedImageCapabilities());
      return;
    }

    EstablishGpuChannelCallback wrap_callback =
        base::BindPostTask(io_runner_, std::move(callback));
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::EstablishGpuChannel,
                                  weak_ptr_, client_id, client_tracing_id,
                                  is_gpu_host, std::move(wrap_callback)));
    return;
  }

  auto channel_token = base::UnguessableToken::Create();
  gpu::GpuChannel* gpu_channel = gpu_channel_manager_->EstablishChannel(
      channel_token, client_id, client_tracing_id, is_gpu_host, gpu_extra_info_,
      gpu_memory_buffer_factory_.get());

  if (!gpu_channel) {
    // This returns a null handle, which is treated by the client as a failure
    // case.
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            gpu::SharedImageCapabilities());
    return;
  }
  mojo::MessagePipe pipe;
  gpu_channel->Init(pipe.handle0.release(), shutdown_event_);

  media_gpu_channel_manager_->AddChannel(client_id, channel_token);

  std::move(callback).Run(
      std::move(pipe.handle1), gpu_info_, gpu_feature_info_,
      gpu_channel->shared_image_stub()->factory()->MakeCapabilities());
}

void GpuServiceImpl::SetChannelClientPid(int32_t client_id,
                                         base::ProcessId client_pid) {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&GpuServiceImpl::SetChannelClientPid,
                                          weak_ptr_, client_id, client_pid));
    return;
  }

  // Note that the GpuService client must be trusted by definition, so DCHECKing
  // this condition is reasonable.
  DCHECK_NE(client_pid, base::kNullProcessId);
  gpu_channel_manager_->SetChannelClientPid(client_id, client_pid);
}

void GpuServiceImpl::SetChannelDiskCacheHandle(
    int32_t client_id,
    const gpu::GpuDiskCacheHandle& handle) {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::SetChannelDiskCacheHandle,
                                  weak_ptr_, client_id, handle));
    return;
  }
  gpu_channel_manager_->SetChannelDiskCacheHandle(client_id, handle);
}

void GpuServiceImpl::OnDiskCacheHandleDestoyed(
    const gpu::GpuDiskCacheHandle& handle) {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::OnDiskCacheHandleDestoyed,
                                  weak_ptr_, handle));
    return;
  }
  gpu_channel_manager_->OnDiskCacheHandleDestoyed(handle);
}

void GpuServiceImpl::CloseChannel(int32_t client_id) {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuServiceImpl::CloseChannel, weak_ptr_, client_id));
    return;
  }
  gpu_channel_manager_->RemoveChannel(client_id);
}

void GpuServiceImpl::SetWakeUpGpuClosure(base::RepeatingClosure closure) {
  wake_up_closure_ = std::move(closure);
}

void GpuServiceImpl::WakeUpGpu() {
  if (wake_up_closure_)
    wake_up_closure_.Run();
  if (main_runner_->BelongsToCurrentThread()) {
    WakeUpGpuOnMainThread();
    return;
  }
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuServiceImpl::WakeUpGpuOnMainThread, weak_ptr_));
}

void GpuServiceImpl::WakeUpGpuOnMainThread() {
  if (gpu_feature_info_.IsWorkaroundEnabled(gpu::WAKE_UP_GPU_BEFORE_DRAWING)) {
#if BUILDFLAG(IS_ANDROID)
    gpu_channel_manager_->WakeUpGpu();
#else
    NOTREACHED() << "WakeUpGpu() not supported on this platform.";
#endif
  }
}

void GpuServiceImpl::GpuSwitched(gl::GpuPreference active_gpu_heuristic) {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::GpuSwitched, weak_ptr_,
                                  active_gpu_heuristic));
    return;
  }
  DVLOG(1) << "GPU: GPU has switched";

  if (watchdog_thread_)
    watchdog_thread_->ReportProgress();

  if (!in_host_process()) {
    ui::GpuSwitchingManager::GetInstance()->NotifyGpuSwitched(
        active_gpu_heuristic);
  }
  GpuServiceImpl::UpdateGPUInfoGL();
}

void GpuServiceImpl::DisplayAdded() {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::DisplayAdded, weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: A monitor is plugged in";

  if (!in_host_process())
    ui::GpuSwitchingManager::GetInstance()->NotifyDisplayAdded();
}

void GpuServiceImpl::DisplayRemoved() {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::DisplayRemoved, weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: A monitor is unplugged ";

  if (!in_host_process())
    ui::GpuSwitchingManager::GetInstance()->NotifyDisplayRemoved();
}

void GpuServiceImpl::DisplayMetricsChanged() {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuServiceImpl::DisplayMetricsChanged, weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: Display Metrics changed";

  if (!in_host_process())
    ui::GpuSwitchingManager::GetInstance()->NotifyDisplayMetricsChanged();
}

void GpuServiceImpl::DestroyAllChannels() {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuServiceImpl::DestroyAllChannels, weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: Removing all contexts";
  gpu_channel_manager_->DestroyAllChannels();
}

void GpuServiceImpl::OnBackgroundCleanup() {
  OnBackgroundCleanupGpuMainThread();
  OnBackgroundCleanupCompositorGpuThread();
}

void GpuServiceImpl::OnBackgroundCleanupGpuMainThread() {
  // Currently only called on Android.
#if BUILDFLAG(IS_ANDROID)
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuServiceImpl::OnBackgroundCleanupGpuMainThread,
                       weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: Performing background cleanup";
  gpu_channel_manager_->OnBackgroundCleanup();
#else
  NOTREACHED();
#endif
}

void GpuServiceImpl::OnBackgroundCleanupCompositorGpuThread() {
  // Currently only called on Android.
#if BUILDFLAG(IS_ANDROID)
  if (compositor_gpu_thread_)
    compositor_gpu_thread_->OnBackgroundCleanup();
#else
  NOTREACHED();
#endif
}

void GpuServiceImpl::OnBackgrounded() {
  DCHECK(io_runner_->BelongsToCurrentThread());
  if (watchdog_thread_)
    watchdog_thread_->OnBackgrounded();
  if (compositor_gpu_thread_)
    compositor_gpu_thread_->OnBackgrounded();

  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuServiceImpl::OnBackgroundedOnMainThread, weak_ptr_));
}

void GpuServiceImpl::OnBackgroundedOnMainThread() {
  gpu_channel_manager_->OnApplicationBackgrounded();

  if (visibility_changed_callback_) {
    visibility_changed_callback_.Run(false);
    if (gpu_preferences_.enable_gpu_benchmarking_extension) {
      ++gpu_info_.visibility_callback_call_count;
      UpdateGPUInfoGL();
    }
  }

  base::allocator::PartitionAllocSupport::Get()->OnBackgrounded();
}

void GpuServiceImpl::OnForegrounded() {
  DCHECK(io_runner_->BelongsToCurrentThread());
  if (watchdog_thread_)
    watchdog_thread_->OnForegrounded();
  if (compositor_gpu_thread_)
    compositor_gpu_thread_->OnForegrounded();

  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuServiceImpl::OnForegroundedOnMainThread, weak_ptr_));
}

void GpuServiceImpl::OnForegroundedOnMainThread() {
  if (visibility_changed_callback_) {
    visibility_changed_callback_.Run(true);
    if (gpu_preferences_.enable_gpu_benchmarking_extension) {
      ++gpu_info_.visibility_callback_call_count;
      UpdateGPUInfoGL();
    }
  }
  gpu_channel_manager_->OnApplicationForegounded();
  base::allocator::PartitionAllocSupport::Get()->OnForegrounded();
}

#if !BUILDFLAG(IS_ANDROID)
void GpuServiceImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  // Forward the notification to the registry of MemoryPressureListeners.
  base::MemoryPressureListener::NotifyMemoryPressure(level);
}
#endif

#if BUILDFLAG(IS_APPLE)
void GpuServiceImpl::BeginCATransaction() {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(FROM_HERE, base::BindOnce(&ui::BeginCATransaction));
}

void GpuServiceImpl::CommitCATransaction(CommitCATransactionCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&ui::CommitCATransaction),
      base::BindPostTask(io_runner_, std::move(callback)));
}
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
void GpuServiceImpl::WriteClangProfilingProfile(
    WriteClangProfilingProfileCallback callback) {
  base::WriteClangProfilingProfile();
  std::move(callback).Run();
}
#endif

void GpuServiceImpl::Crash() {
  DCHECK(io_runner_->BelongsToCurrentThread());
  gl::Crash();
}

void GpuServiceImpl::Hang() {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(FROM_HERE, base::BindOnce(&gl::Hang));
}

void GpuServiceImpl::ThrowJavaException() {
  DCHECK(io_runner_->BelongsToCurrentThread());
#if BUILDFLAG(IS_ANDROID)
  ThrowUncaughtException();
#else
  NOTREACHED() << "Java exception not supported on this platform.";
#endif
}

void GpuServiceImpl::StartPeakMemoryMonitorOnMainThread(uint32_t sequence_num) {
  gpu_channel_manager_->StartPeakMemoryMonitor(sequence_num);
}

void GpuServiceImpl::GetPeakMemoryUsageOnMainThread(
    uint32_t sequence_num,
    GetPeakMemoryUsageCallback callback) {
  uint64_t peak_memory = 0u;
  auto allocation_per_source =
      gpu_channel_manager_->GetPeakMemoryUsage(sequence_num, &peak_memory);

  auto seq_loc = GetPeakMemoryUsageRequestLocation(sequence_num);
  if (seq_loc == SequenceLocation::kGpuProcess) {
    RunGetPeakGpuMemoryUsageCallbackOnMainThread(
        std::move(callback), peak_memory, std::move(allocation_per_source));
    return;
  }
  io_runner_->PostTask(FROM_HERE,
                       base::BindOnce(std::move(callback), peak_memory,
                                      std::move(allocation_per_source)));
}

gpu::Scheduler* GpuServiceImpl::GetGpuScheduler() {
  return scheduler_;
}

bool GpuServiceImpl::OnBeginFrameDerivedImpl(const BeginFrameArgs& args) {
  io_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&GpuServiceImpl::OnBeginFrameOnIO,
                                      base::Unretained(this), args));
  return true;
}

#if BUILDFLAG(IS_LINUX)
bool GpuServiceImpl::IsGMBNV12Supported() {
  CHECK(main_runner_->BelongsToCurrentThread());
  auto buffer_format = gfx::BufferFormat::YUV_420_BIPLANAR;
  auto buffer_usage = gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;

  if (!IsNativeBufferSupported(buffer_format, buffer_usage)) {
    return false;
  }
  auto size = gfx::Size(2, 2);

  // Note that |gmb_id| and |client_id| does not matter here as this is the
  // first GMB which will created and immediately destroyed.
  auto gmb_id = gfx::GpuMemoryBufferId(
      static_cast<int>(gpu::MappableSIClientGmbId::kGpuServiceImpl));
  auto client_id = gpu::kMappableSIClientId;
  auto gmb_handle = gpu_memory_buffer_factory_->CreateGpuMemoryBuffer(
      gmb_id, size, /*framebuffer_size=*/size, buffer_format, buffer_usage,
      client_id, gpu::kNullSurfaceHandle);

  // Destroy the gmb_handle since it will be no longer needed.
  gpu_memory_buffer_factory_->DestroyGpuMemoryBuffer(gmb_id, client_id);
  return !gmb_handle.is_null();
}
#endif

void GpuServiceImpl::OnBeginFrameSourcePausedChanged(bool paused) {}

void GpuServiceImpl::OnBeginFrameOnIO(const BeginFrameArgs& args) {
  DCHECK(io_runner_->BelongsToCurrentThread());

  if (mjpeg_decode_accelerator_begin_frame_cb_) {
    mjpeg_decode_accelerator_begin_frame_cb_->Run();
  }
}

void GpuServiceImpl::SetRequestBeginFrameForGpuServiceCB(
    RequestBeginFrameForGpuServiceCB cb) {
  request_begin_frame_for_gpu_service_cb_ = std::move(cb);
}

void GpuServiceImpl::SetMjpegDecodeAcceleratorBeginFrameCB(
    std::optional<base::RepeatingClosure> cb) {
  DCHECK(io_runner_->BelongsToCurrentThread());

  main_runner_->PostTask(FROM_HERE,
                         base::BindOnce(request_begin_frame_for_gpu_service_cb_,
                                        cb ? true : false));
  mjpeg_decode_accelerator_begin_frame_cb_ = std::move(cb);
}

#if BUILDFLAG(IS_WIN)
// Update Overlay and DXGI Info
void GpuServiceImpl::OnOverlayCapsChanged() {
  gpu::OverlayInfo old_overlay_info = gpu_info_.overlay_info;
  gpu::CollectHardwareOverlayInfo(&gpu_info_.overlay_info);

  // Update overlay info in the GPU process and send the updated data back to
  // the GPU host in the Browser process through mojom if the info has changed.
  if (old_overlay_info != gpu_info_.overlay_info)
    gpu_host_->DidUpdateOverlayInfo(gpu_info_.overlay_info);

  // Update DXGI adapter info in the GPU process through the GPU host mojom.
  auto old_dxgi_info = std::move(dxgi_info_);
  dxgi_info_ = gl::GetDirectCompositionHDRMonitorDXGIInfo();
  if (!mojo::Equals(dxgi_info_, old_dxgi_info)) {
    gpu_host_->DidUpdateDXGIInfo(dxgi_info_.Clone());
  }
}
#endif

gpu::SyncPointManager* GpuServiceImpl::CreateSyncPointManager() {
  owned_sync_point_manager_ = std::make_unique<gpu::SyncPointManager>();
  return owned_sync_point_manager_.get();
}

gpu::SharedImageManager* GpuServiceImpl::CreateSharedImageManager(
    bool supports_overlays) {
  // When using real buffers for testing overlay configurations, we need
  // access to SharedImageManager on the viz thread to obtain the buffer
  // corresponding to a mailbox.
  const bool display_context_on_another_thread =
      features::IsDrDcEnabled() && !gpu_driver_bug_workarounds_.disable_drdc;

  // |display_context_on_another_thread|, features::IsUsingRawDraw(),
  // kAlwaysUseRealBufferTestingOnOzone, and kSharedBitmapToSharedImage
  // requires |thread_safe_manager| to be true.
  bool thread_safe_manager = true;
  owned_shared_image_manager_ = std::make_unique<gpu::SharedImageManager>(
      thread_safe_manager, display_context_on_another_thread);
#if BUILDFLAG(IS_OZONE)
  owned_shared_image_manager_->SetSupportsOverlays(supports_overlays);
#endif
  return owned_shared_image_manager_.get();
}

gpu::Scheduler* GpuServiceImpl::CreateScheduler(
    gpu::SyncPointManager* sync_point_manager) {
  owned_scheduler_ = std::make_unique<gpu::Scheduler>(sync_point_manager);
  return owned_scheduler_.get();
}

base::WaitableEvent* GpuServiceImpl::CreateShutdownEvent() {
  owned_shutdown_event_ = std::make_unique<base::WaitableEvent>(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  return owned_shutdown_event_.get();
}

bool GpuServiceImpl::IsNativeBufferSupported(gfx::BufferFormat format,
                                             gfx::BufferUsage usage) {
  // Note that we are initializing the |supported_gmb_configurations_| here to
  // make sure gpu service have already initialized and required metadata like
  // supported buffer configurations have already been sent from browser
  // process to GPU process for wayland.
  if (!supported_gmb_configurations_inited_) {
    supported_gmb_configurations_inited_ = true;
    if (WillGetGmbConfigFromGpu()) {
      // Note that Chrome can be compiled with multiple OZONE platforms but
      // actual OZONE platform is chosen at run-time. Eg: Chrome can be
      // compiled with X11 and Wayland but Wayland can be chosen at runtime.
      // Hence using WillGetGmbConfigFromGpu() which will determine
      // configurations based on actual platform chosen at runtime.
#if BUILDFLAG(IS_OZONE_X11)
      for (const auto& config : gpu_extra_info_.gpu_memory_buffer_support_x11) {
        supported_gmb_configurations_.emplace(config);
      }
#endif  // BUILDFLAG(IS_OZONE_X11)
    } else {
      supported_gmb_configurations_ =
          gpu::GpuMemoryBufferSupport::GetNativeGpuMemoryBufferConfigurations();
    }
  }
  return supported_gmb_configurations_.find(gfx::BufferUsageAndFormat(
             usage, format)) != supported_gmb_configurations_.end();
}

void GpuServiceImpl::GetDawnInfo(bool collect_metrics,
                                 GetDawnInfoCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());

  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuServiceImpl::GetDawnInfoOnMain, base::Unretained(this),
                     collect_metrics, std::move(callback)));
}

void GpuServiceImpl::GetDawnInfoOnMain(bool collect_metrics,
                                       GetDawnInfoCallback callback) {
  DCHECK(main_runner_->BelongsToCurrentThread());

  std::vector<std::string> dawn_info_list;
  // Pause the watchdog around Dawn info collection since it is known to be
  // slow loading GPU drivers.
  if (watchdog_thread_) {
    watchdog_thread_->PauseWatchdog();
  }

  // Don't collect metrics if gpu watchdog is still running. Otherwise fast
  // timings will be recorded, and very-slow timings will crash and not
  // record, skewing the results.
  {
    SCOPED_UMA_HISTOGRAM_TIMER("GPU.Dawn.InfoCollectionTimeMS");
    gpu::CollectDawnInfo(gpu_preferences_, collect_metrics, &dawn_info_list);
  }

  if (watchdog_thread_) {
    watchdog_thread_->ResumeWatchdog();
  }

  io_runner_->PostTask(FROM_HERE,
                       base::BindOnce(std::move(callback), dawn_info_list));
}

#if BUILDFLAG(IS_ANDROID)
void GpuServiceImpl::SetHostProcessId(base::ProcessId pid) {
  host_process_id_ = pid;
}
#endif

GpuServiceImpl::InitParams::InitParams() = default;
GpuServiceImpl::InitParams::InitParams(InitParams&& other) = default;
GpuServiceImpl::InitParams& GpuServiceImpl::InitParams::operator=(
    InitParams&& other) = default;
GpuServiceImpl::InitParams::~InitParams() = default;

}  // namespace viz
