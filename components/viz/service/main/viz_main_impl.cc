// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/main/viz_main_impl.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/viz/common/buildflags.h"
#include "components/viz/common/features.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/performance_hint/hint_session.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/service/gpu_init.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "media/gpu/buildflags.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "skia/ext/legacy_display_globals.h"

namespace {

std::unique_ptr<base::Thread> CreateAndStartIOThread() {
  // TODO(sad): We do not need the IO thread once gpu has a separate process.
  // It should be possible to use |main_task_runner_| for doing IO tasks.
  base::Thread::Options thread_options(base::MessagePumpType::IO, 0);
  // TODO(reveman): Remove this in favor of setting it explicitly for each
  // type of process.
  thread_options.thread_type = base::ThreadType::kCompositing;
  auto io_thread = std::make_unique<base::Thread>("GpuIOThread");
  CHECK(io_thread->StartWithOptions(std::move(thread_options)));
  return io_thread;
}

}  // namespace

namespace viz {

VizMainImpl::ExternalDependencies::ExternalDependencies() = default;

VizMainImpl::ExternalDependencies::~ExternalDependencies() = default;

VizMainImpl::ExternalDependencies::ExternalDependencies(
    ExternalDependencies&& other) = default;

VizMainImpl::ExternalDependencies& VizMainImpl::ExternalDependencies::operator=(
    ExternalDependencies&& other) = default;

VizMainImpl::VizMainImpl(Delegate* delegate,
                         ExternalDependencies dependencies,
                         std::unique_ptr<gpu::GpuInit> gpu_init)
    : delegate_(delegate),
      dependencies_(std::move(dependencies)),
      gpu_init_(std::move(gpu_init)),
      gpu_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK(gpu_init_);

  // TODO(crbug.com/609317): Remove this when Mus Window Server and GPU are
  // split into separate processes. Until then this is necessary to be able to
  // run Mushrome (chrome with mus) with Mus running in the browser process.
  if (dependencies_.power_monitor_source) {
    base::PowerMonitor::Initialize(
        std::move(dependencies_.power_monitor_source));
  }

  if (!dependencies_.io_thread_task_runner)
    io_thread_ = CreateAndStartIOThread();

  if (dependencies_.viz_compositor_thread_runner) {
    viz_compositor_thread_runner_ = dependencies_.viz_compositor_thread_runner;
  } else {
    viz_compositor_thread_runner_impl_ =
        std::make_unique<VizCompositorThreadRunnerImpl>();
    viz_compositor_thread_runner_ = viz_compositor_thread_runner_impl_.get();
  }
  if (delegate_) {
    delegate_->PostCompositorThreadCreated(
        viz_compositor_thread_runner_->task_runner());
  }

  if (!gpu_init_->gpu_info().in_process_gpu && dependencies_.ukm_recorder) {
    // NOTE: If the GPU is running in the browser process, we can use the
    // browser's UKMRecorder.
    ukm::DelegatingUkmRecorder::Get()->AddDelegate(
        dependencies_.ukm_recorder->GetWeakPtr());
  }

  gpu_service_ = std::make_unique<GpuServiceImpl>(
      gpu_init_->gpu_info(), gpu_init_->TakeWatchdogThread(), io_task_runner(),
      gpu_init_->gpu_feature_info(), gpu_init_->gpu_preferences(),
      gpu_init_->gpu_info_for_hardware_gpu(),
      gpu_init_->gpu_feature_info_for_hardware_gpu(),
      gpu_init_->gpu_extra_info(), gpu_init_->vulkan_implementation(),
      base::BindOnce(&VizMainImpl::ExitProcess, base::Unretained(this)));

  {
    // Gather the thread IDs of display GPU, and IO for performance hint.
    // These are the viz threads that are on the critical path of all frames.
    base::flat_set<base::PlatformThreadId> gpu_process_thread_ids;

    CompositorGpuThread* compositor_gpu_thread =
        gpu_service_->compositor_gpu_thread();
    gpu_process_thread_ids.insert(compositor_gpu_thread
                                      ? compositor_gpu_thread->GetThreadId()
                                      : base::PlatformThread::CurrentId());

    base::WaitableEvent event;
    base::PlatformThreadId io_thread_id = base::kInvalidThreadId;
    io_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::PlatformThreadId* io_thread_id,
                          base::WaitableEvent* event) {
                         *io_thread_id = base::PlatformThread::CurrentId();
                         event->Signal();
                       },
                       &io_thread_id, &event));
    event.Wait();
    gpu_process_thread_ids.insert(io_thread_id);

    base::RepeatingClosure wake_up_closure;
    if (viz_compositor_thread_runner_->CreateHintSessionFactory(
            std::move(gpu_process_thread_ids), &wake_up_closure)) {
      gpu_service_->SetWakeUpGpuClosure(std::move(wake_up_closure));
    }
  }

  VizDebugger::GetInstance();
}

VizMainImpl::~VizMainImpl() {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());

  // The compositor holds on to some resources from gpu service. So destroy the
  // compositor first, before destroying the gpu service. However, before the
  // compositor is destroyed, close the binding, so that the gpu service doesn't
  // need to process commands from the host as it is shutting down.
  receiver_.reset();

  // If the VizCompositorThread was started and owned by VizMainImpl, then this
  // will block until the thread has been shutdown. All RootCompositorFrameSinks
  // must be destroyed before now, otherwise the compositor thread will deadlock
  // waiting for a response from the blocked GPU thread.
  // For the non-owned case for Android WebView, Viz does not communicate with
  // this thread so there is no need to shutdown viz first.
  viz_compositor_thread_runner_ = nullptr;
  viz_compositor_thread_runner_impl_.reset();

  if (dependencies_.ukm_recorder)
    ukm::DelegatingUkmRecorder::Get()->RemoveDelegate(
        dependencies_.ukm_recorder.get());
}

void VizMainImpl::Bind(mojo::PendingReceiver<mojom::VizMain> receiver) {
  receiver_.Bind(std::move(receiver));
}

void VizMainImpl::CreateGpuService(
    mojo::PendingReceiver<mojom::GpuService> pending_receiver,
    mojo::PendingRemote<mojom::GpuHost> pending_gpu_host,
    mojo::PendingRemote<
        discardable_memory::mojom::DiscardableSharedMemoryManager>
        discardable_memory_manager,
    base::UnsafeSharedMemoryRegion activity_flags_region,
    gfx::FontRenderParams::SubpixelRendering subpixel_rendering) {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());

  mojo::Remote<mojom::GpuHost> gpu_host(std::move(pending_gpu_host));

  // If GL is disabled then don't try to collect GPUInfo, we're not using GPU.
  if (gl::GetGLImplementation() != gl::kGLImplementationDisabled)
    gpu_service_->UpdateGPUInfo();

  if (!gpu_init_->init_successful()) {
    LOG(ERROR) << "Exiting GPU process due to errors during initialization";
    GpuServiceImpl::FlushPreInitializeLogMessages(gpu_host.get());
    gpu_service_.reset();
    gpu_host->DidFailInitialize();
    if (delegate_)
      delegate_->OnInitializationFailed();
    return;
  }

  if (!gpu_init_->gpu_info().in_process_gpu) {
    // If the GPU is running in the browser process, discardable memory manager
    // has already been initialized.
    discardable_shared_memory_manager_ = base::MakeRefCounted<
        discardable_memory::ClientDiscardableSharedMemoryManager>(
        std::move(discardable_memory_manager), io_task_runner());
    base::DiscardableMemoryAllocator::SetInstance(
        discardable_shared_memory_manager_.get());
  }

  skia::LegacyDisplayGlobals::SetCachedPixelGeometry(
      gfx::FontRenderParams::SubpixelRenderingToSkiaPixelGeometry(
          subpixel_rendering));

  gpu_service_->InitializeWithHost(
      gpu_host.Unbind(),
      gpu::GpuProcessActivityFlags(std::move(activity_flags_region)),
      gpu_init_->TakeDefaultOffscreenSurface(),
      dependencies_.sync_point_manager, dependencies_.shared_image_manager,
      dependencies_.scheduler, dependencies_.shutdown_event);
  gpu_service_->Bind(std::move(pending_receiver));

  if (!pending_frame_sink_manager_params_.is_null()) {
    CreateFrameSinkManagerInternal(
        std::move(pending_frame_sink_manager_params_));
    pending_frame_sink_manager_params_.reset();
  }
  if (delegate_)
    delegate_->OnGpuServiceConnection(gpu_service_.get());
}

#if BUILDFLAG(IS_WIN)
void VizMainImpl::CreateInfoCollectionGpuService(
    mojo::PendingReceiver<mojom::InfoCollectionGpuService> pending_receiver) {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!info_collection_gpu_service_);
  DCHECK(gpu_init_->device_perf_info().has_value());

  info_collection_gpu_service_ = std::make_unique<InfoCollectionGpuServiceImpl>(
      gpu_thread_task_runner_, io_task_runner(),
      gpu_init_->device_perf_info().value(), gpu_init_->gpu_info().active_gpu(),
      std::move(pending_receiver));
}
#endif

#if BUILDFLAG(IS_ANDROID)
void VizMainImpl::SetHostProcessId(int32_t pid) {
  if (gpu_service_)
    gpu_service_->SetHostProcessId(pid);
}
#endif

void VizMainImpl::CreateFrameSinkManager(
    mojom::FrameSinkManagerParamsPtr params) {
  DCHECK(viz_compositor_thread_runner_);
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());
  if (!gpu_service_ || !gpu_service_->is_initialized()) {
    DCHECK(pending_frame_sink_manager_params_.is_null());
    pending_frame_sink_manager_params_ = std::move(params);
    return;
  }
  CreateFrameSinkManagerInternal(std::move(params));
}

void VizMainImpl::CreateFrameSinkManagerInternal(
    mojom::FrameSinkManagerParamsPtr params) {
  DCHECK(gpu_service_);
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());

  // When the host loses its connection to the viz process, it assumes the
  // process has crashed and tries to reinitialize it. However, it is possible
  // to have lost the connection for other reasons (e.g. deserialization
  // errors) and the viz process is already set up. We cannot recreate
  // FrameSinkManagerImpl, so just do a hard CHECK rather than crashing down the
  // road so that all crash reports caused by this issue look the same and have
  // the same signature. https://crbug.com/928845
  CHECK(!has_created_frame_sink_manager_);
  has_created_frame_sink_manager_ = true;

  viz_compositor_thread_runner_->CreateFrameSinkManager(std::move(params),
                                                        gpu_service_.get());
}

#if BUILDFLAG(USE_VIZ_DEBUGGER)
void VizMainImpl::FilterDebugStream(base::Value filter_data) {
  VizDebugger::GetInstance()->FilterDebugStream(std::move(filter_data));
}

void VizMainImpl::StartDebugStream(
    mojo::PendingRemote<mojom::VizDebugOutput> pending_debug_output) {
  VizDebugger::GetInstance()->StartDebugStream(std::move(pending_debug_output));
}

void VizMainImpl::StopDebugStream() {
  VizDebugger::GetInstance()->StopDebugStream();
}
#endif

void VizMainImpl::ExitProcess(ExitCode immediate_exit_code) {
  DCHECK(gpu_thread_task_runner_->BelongsToCurrentThread());

  if (!gpu_init_->gpu_info().in_process_gpu) {
    // Atomically shut down GPU process to make it faster and simpler.
    base::Process::TerminateCurrentProcessImmediately(
        static_cast<int>(immediate_exit_code));
  }

  // Close mojom::VizMain bindings first so the browser can't try to reconnect.
  receiver_.reset();

  DCHECK(!viz_compositor_thread_runner_);
  delegate_->QuitMainMessageLoop();
}

}  // namespace viz
