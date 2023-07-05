// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/uwp/extended_resources_manager.h"

#include <ppltasks.h>

#include "starboard/common/condition_variable.h"
#include "starboard/common/semaphore.h"
#include "starboard/common/string.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"
#include "starboard/shared/win32/video_decoder.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "starboard/xb1/shared/internal_shims.h"
#if defined(INTERNAL_BUILD)
#include "internal/starboard/xb1/av1_video_decoder.h"
#include "internal/starboard/xb1/vpx_video_decoder.h"
#include "third_party/internal/libvpx_xb1/libvpx/d3dx12.h"
#endif  // defined(INTERNAL_BUILD)

namespace starboard {
namespace shared {
namespace uwp {

namespace {

using Microsoft::WRL::ComPtr;
using ::starboard::shared::starboard::media::MimeSupportabilityCache;
using Windows::Foundation::Metadata::ApiInformation;
#if defined(INTERNAL_BUILD)
using ::starboard::xb1::shared::Av1VideoDecoder;
using ::starboard::xb1::shared::VpxVideoDecoder;
#endif  // defined(INTERNAL_BUILD)

const SbTime kReleaseTimeout = kSbTimeSecond;

bool IsExtendedResourceModeRequired() {
  if (!::starboard::xb1::shared::CanAcquire()) {
    return false;
  }
  bool is_erm_required =
      !shared::win32::VideoDecoder::IsHardwareVp9DecoderSupported();
  SB_LOG(INFO) << "Extended resources mode"
               << (is_erm_required ? " is required." : " isn't required.");
  return is_erm_required;
}

}  // namespace

// static
ExtendedResourcesManager* ExtendedResourcesManager::s_instance_;

ExtendedResourcesManager::ExtendedResourcesManager()
    : acquisition_condition_(mutex_) {
  SB_DCHECK(!s_instance_);
  s_instance_ = this;
}

ExtendedResourcesManager::~ExtendedResourcesManager() {
  SB_DCHECK(s_instance_ == this);
  s_instance_ = NULL;
}

// static
ExtendedResourcesManager* ExtendedResourcesManager::GetInstance() {
  return s_instance_;
}

void ExtendedResourcesManager::Run() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (!IsExtendedResourceModeRequired()) {
    // Running a simplified loop that only cares about the kQuit event.
    while (event_queue_.Get() != kQuit) {
    }
    return;
  }

  bool retrying_acquire = false;
  // Delay before retry acquiring to avoid pinning a core.
  constexpr SbTime kRetryDelay = kSbTimeSecond / 10;
  for (;;) {
    switch (retrying_acquire ? event_queue_.GetTimed(kRetryDelay)
                             : event_queue_.Get()) {
      case kTimeout:
        SB_DCHECK(retrying_acquire);
      // Fall through to acquire extended resources.
      case kAcquireExtendedResources:
        retrying_acquire = !AcquireExtendedResourcesInternal();
        if (!retrying_acquire) {
          retrying_acquire = !StartCompileShaders();
        }
        break;
      case kCompileShaders:
        CompileShadersAsynchronously();
        break;
      case kReleaseExtendedResources:
        retrying_acquire = false;
        ReleaseExtendedResourcesInternal();
        break;
      case kQuit:
        retrying_acquire = false;
        ReleaseExtendedResourcesInternal();
        return;
    }
  }
}

void ExtendedResourcesManager::AcquireExtendedResources() {
  // This is expected to be called from another thread only.
  SB_DCHECK(!thread_checker_.CalledOnValidThread());
  event_queue_.Put(kAcquireExtendedResources);
}

void ExtendedResourcesManager::ReleaseExtendedResources() {
  // This is expected to be called from another thread only. If called from the
  // worker thread, it can deadlock on the mutex below.
  SB_DCHECK(!thread_checker_.CalledOnValidThread());
  event_queue_.Put(kReleaseExtendedResources);

  // Skip any pending extended resources acquisitions.
  pending_extended_resources_release_.store(true);
  {
    // Wait until we successfully release the extended resources or timeout.
    ScopedLock scoped_lock(mutex_);
    while (is_extended_resources_acquired_.load()) {
      acquisition_condition_.Wait();
    }
  }
}

void ExtendedResourcesManager::Quit() {
  SB_DCHECK(!thread_checker_.CalledOnValidThread());
  event_queue_.Put(kQuit);
  pending_extended_resources_release_.store(true);
}

bool ExtendedResourcesManager::GetD3D12Objects(
    Microsoft::WRL::ComPtr<ID3D12Device>* device,
    void** command_queue) {
  if (HasNonrecoverableFailure()) {
    SB_LOG(WARNING) << "The D3D12 device has encountered a nonrecoverable "
                       "failure.";
    return false;
  }

  device->Reset();
  *command_queue = nullptr;

  ScopedTryLock scoped_lock(mutex_);
  if (!scoped_lock.is_locked()) {
    SB_LOG(INFO) << "GetD3D12Objects() failed"
                 << " because lock cannot be acquired.";
    return false;
  }
  if (!is_extended_resources_acquired_.load()) {
    SB_LOG(INFO) << "GetD3D12Objects() failed"
                 << " because extended resources mode is not acquired.";
    return false;
  }
  if (!GetD3D12ObjectsInternal()) {
    SB_LOG(INFO) << "GetD3D12Objects() failed"
                 << " because d3d12 objects can not be created.";
    return false;
  }

#if defined(INTERNAL_BUILD)
  // Verify that we can allocate one MB without getting an error. This should
  // detect a DXGI_ERROR_DEVICE_REMOVED failure mode.
  ComPtr<ID3D12Resource> res;
  D3D12_HEAP_PROPERTIES prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(1024 * 1024);
  HRESULT result = d3d12device_->CreateCommittedResource(
      &prop, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr, IID_PPV_ARGS(&res));
  if (result != S_OK) {
    SB_LOG(WARNING) << "The D3D12 device is not in a good state, can not use "
                       "GPU based decoders.";
    OnNonrecoverableFailure();
    return false;
  }
#endif  // defined(INTERNAL_BUILD)

  *device = d3d12device_;
  *command_queue = d3d12queue_.Get();
  return true;
}

bool ExtendedResourcesManager::GetD3D12ObjectsInternal() {
  if (!d3d12device_) {
    if (FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(&d3d12device_)))) {
      // GPU based vp9 decoding will be temporarily disabled.
      SB_LOG(WARNING) << "Failed to create d3d12 device.";
      return false;
    }
    SB_DCHECK(d3d12device_);
  }

  if (!d3d12queue_) {
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(d3d12device_->CreateCommandQueue(&desc,
                                                IID_PPV_ARGS(&d3d12queue_)))) {
      SB_LOG(WARNING) << "Failed to create d3d12 command queue.";
      return false;
    }
    SB_DCHECK(d3d12queue_);
  }

  return d3d12device_ && d3d12queue_;
}

bool ExtendedResourcesManager::AcquireExtendedResourcesInternal() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  ScopedLock scoped_lock(mutex_);

  if (HasNonrecoverableFailure()) {
    SB_LOG(WARNING)
        << "Encountered a nonrecoverable failure, ignoring acquire.";
    return false;
  }

  if (is_extended_resources_acquired_.load()) {
    SB_LOG(INFO) << "Skip acquiring extended resources, already acquired.";
  } else {
    if (pending_extended_resources_release_.load()) {
      SB_LOG(INFO) << "AcquireExtendedResourcesInternal() interrupted"
                      " by pending extended resources release.";
      return false;
    }
    auto extended_resources_mode_enable_task =
        concurrency::create_task(::starboard::xb1::shared::Acquire());

    Semaphore semaphore;
    extended_resources_mode_enable_task.then(
        [this, &semaphore](concurrency::task<bool> task) {
          try {
            if (task.get()) {
              is_extended_resources_acquired_.store(true);
              acquisition_condition_.Signal();
              SB_LOG(INFO) << "Successfully acquired extended resources.";
            } else {
              // TODO: Investigate if vp9 playback should be disabled.
              SB_LOG(INFO) << "Failed to acquire extended resources.";
            }
          } catch (const std::exception& e) {
            SB_LOG(ERROR) << "Exception on acquiring extended resources: "
                          << e.what();
          } catch (...) {
            SB_LOG(ERROR) << "Exception on acquiring extended resources.";
          }
          semaphore.Put();
        });
    if (semaphore.TakeWait(10 * kSbTimeSecond)) {
      acquisition_condition_.Signal();
      // If extended resource acquisition was not successful after the wait
      // time, signal a nonrecoverable failure, unless a release of
      // extended resources has since been requested.
      if (!is_extended_resources_acquired_.load() &&
          !pending_extended_resources_release_.load()) {
        SB_LOG(WARNING) << "Extended resource mode acquisition timed out";
        OnNonrecoverableFailure();
      }
    }
  }

  if (!is_extended_resources_acquired_.load()) {
    SB_LOG(INFO) << "AcquireExtendedResourcesInternal() failed.";
    return false;
  }
  return is_extended_resources_acquired_.load();
}

bool ExtendedResourcesManager::StartCompileShaders() {
  {
    ScopedLock scoped_lock(mutex_);
    if (HasNonrecoverableFailure()) {
      SB_LOG(WARNING)
          << "Encountered a nonrecoverable failure, ignoring shader compile.";
      return false;
    }
    if (pending_extended_resources_release_.load()) {
      SB_LOG(INFO) << "StartCompileShaders() interrupted"
                      " by pending extended resources release.";
      return false;
    }
    if (!is_extended_resources_acquired_.load()) {
      SB_LOG(INFO) << "StartCompileShaders() failed"
                      " because extended resources are not acquired.";
      return false;
    }
    if (!GetD3D12ObjectsInternal()) {
      SB_LOG(INFO) << "StartCompileShaders() failed"
                   << " because d3d12 objects can not be created.";
      return false;
    }
  }
  // Everything is ready to give a command for shaders compilation.
  // Note: once we returned "true" execution will not go here anymore to queue
  // an event to compile shaders again.
  event_queue_.Put(kCompileShaders);
  return true;
}

void ExtendedResourcesManager::CompileShadersAsynchronously() {
  // Shaders compilation may take several seconds that is why it is good to run
  // it asynchronously. We may not wait until its compilation is completed and
  // synchronize decoding with it. If real playback will start earlier than this
  // compilation completed then the decoder will compile shaders before playback
  // and they will be placed in cache as binaries for further reusing.
  Concurrency::create_task([this] {
    ScopedLock scoped_lock(mutex_);
#if defined(INTERNAL_BUILD)
    SB_LOG(INFO) << "Start to compile AV1 decoder shaders.";
    SB_DCHECK(!is_av1_shader_compiled_)
        << "Unexpected attempt to recompile AV1 decoder shaders.";
    if (HasNonrecoverableFailure()) {
      SB_LOG(WARNING) << "Encountered a nonrecoverable failure, ignoring "
                         "shader compile.";
      return;
    }
    if (Av1VideoDecoder::CompileShaders(d3d12device_, d3d12queue_.Get())) {
      is_av1_shader_compiled_ = true;
      SB_LOG(INFO) << "Gpu based AV1 decoder finished compiling its shaders.";
    } else {
      SB_LOG(WARNING) << "Failed to compile AV1 decoder shaders, next attempt "
                         "will happen right on the AV1 decoder instantiation.";
    }

    SB_LOG(INFO) << "Start to compile VP9 decoder shaders.";
    SB_DCHECK(!is_vp9_shader_compiled_)
        << "Unexpected attempt to recompile VP9 decoder shaders";
    if (HasNonrecoverableFailure()) {
      SB_LOG(WARNING) << "Encountered a nonrecoverable failure, ignoring "
                         "shader compile.";
      return;
    }

    if (VpxVideoDecoder::CompileShaders(d3d12device_, d3d12queue_.Get())) {
      is_vp9_shader_compiled_ = true;
      SB_LOG(INFO) << "Gpu based VP9 decoder finished compiling its shaders.";
    } else {
      // This warning means that not all the shaders has been compiled
      // successfully, It will try to compile the shaders again, right before
      // the start of playback, in function |VideoDecoder::InitializeCodec()|.
      SB_LOG(WARNING) << "Failed to compile VP9 decoder shaders, next attempt "
                         "will happen right on the VP9 decoder instantiation.";
    }
#endif  // defined(INTERNAL_BUILD)

    if (is_av1_shader_compiled_ && is_vp9_shader_compiled_) {
      MimeSupportabilityCache::GetInstance()->ClearCachedMimeSupportabilities();
    }
  });
}

void ExtendedResourcesManager::ReleaseExtendedResourcesInternal() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
#if defined(INTERNAL_BUILD)
  Av1VideoDecoder::ClearFrameBufferPool();
#endif  // defined(INTERNAL_BUILD)

  ScopedLock scoped_lock(mutex_);
  if (!is_extended_resources_acquired_.load()) {
    SB_LOG(INFO) << "Extended resources hasn't been acquired,"
                 << " no need to release.";
    return;
  }

  try {
    // Wait until all commands on the queue has been finished.
    if (d3d12device_ && d3d12queue_) {
      ComPtr<ID3D12Fence> fence;
      HRESULT hr = d3d12device_->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                             IID_PPV_ARGS(&fence));
      if (SUCCEEDED(hr)) {
        HANDLE event = CreateEvent(nullptr, false, false, nullptr);
        SB_DCHECK(event);

        // If createEvent() succeeds, we can use it to wait for the command
        // queue to complete.
        if (event) {
          fence->SetEventOnCompletion(1, event);
          d3d12queue_->Signal(fence.Get(), 1);

          DWORD result =
              WaitForSingleObject(event, 1000);  // Wait at most one second
          CloseHandle(event);
          if (result == WAIT_TIMEOUT) {
            SB_LOG(WARNING) << "Fence event completion timeout.";
            OnNonrecoverableFailure();
          }
        } else {
          SB_LOG(INFO) << "CreateEvent() failed with " << GetLastError();
        }
#if defined(INTERNAL_BUILD)
        Av1VideoDecoder::ReleaseShaders();
        VpxVideoDecoder::ReleaseShaders();
#endif  // #if defined(INTERNAL_BUILD)
        is_av1_shader_compiled_ = false;
        is_vp9_shader_compiled_ = false;
      } else {
        SB_LOG(INFO) << "CreateFence() failed with " << hr;
      }
    }

    if (d3d12queue_) {
#if !defined(COBALT_BUILD_TYPE_GOLD)
      d3d12queue_->AddRef();
      ULONG reference_count = d3d12queue_->Release();
      SB_DLOG(INFO) << "Reference count of |d3d12queue_| is "
                    << reference_count;
#endif
      d3d12queue_.Reset();
    }

    if (d3d12device_) {
#if !defined(COBALT_BUILD_TYPE_GOLD)
      d3d12device_->AddRef();
      ULONG reference_count = d3d12device_->Release();
      SB_DLOG(INFO) << "Reference count of |d3d12device_| is "
                    << reference_count;
#endif
      d3d12device_.Reset();
    }
  } catch (const std::exception& e) {
    SB_LOG(ERROR) << "Exception on releasing extended resources: " << e.what();
    OnNonrecoverableFailure();
  } catch (...) {
    SB_LOG(ERROR) << "Exception on releasing extended resources.";
    OnNonrecoverableFailure();
  }

  auto extended_resources_mode_disable_task =
      concurrency::create_task([] { ::starboard::xb1::shared::Release(); });

  Semaphore semaphore;
  extended_resources_mode_disable_task.then(
      [this, &semaphore](concurrency::task<void> task) {
        try {
          acquisition_condition_.Signal();
          // ReleaseExtendedResources has no return value but a call to get()
          // will bubble up any exceptions thrown during the task.
          task.get();
          SB_LOG(INFO) << "Released extended resources.";
        } catch (const std::exception& e) {
          SB_LOG(ERROR) << "Exception on releasing extended resources: "
                        << e.what();
          OnNonrecoverableFailure();
        } catch (...) {
          SB_LOG(ERROR) << "Exception on releasing extended resources.";
          OnNonrecoverableFailure();
        }
        is_extended_resources_acquired_.store(false);
        pending_extended_resources_release_.store(false);
        semaphore.Put();
      });
  if (!semaphore.TakeWait(kReleaseTimeout)) {
    acquisition_condition_.Signal();
    // If extended resources are still acquired or the release is still pending
    // after the wait time, signal a nonrecoverable failure.
    if (is_extended_resources_acquired_.load() ||
        pending_extended_resources_release_.load()) {
      SB_LOG(WARNING) << "Extended resource mode release timed out";
      OnNonrecoverableFailure();
    }
    is_extended_resources_acquired_.store(false);
    pending_extended_resources_release_.store(false);
  }
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
