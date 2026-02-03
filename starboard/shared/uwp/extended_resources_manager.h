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

#ifndef STARBOARD_SHARED_UWP_EXTENDED_RESOURCES_MANAGER_H_
#define STARBOARD_SHARED_UWP_EXTENDED_RESOURCES_MANAGER_H_

#include <D3D12.h>
#include <agile.h>

#include <atomic>

#include "starboard/common/atomic.h"
#include "starboard/common/mutex.h"
#include "starboard/common/queue.h"
#include "starboard/configuration.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {
namespace shared {
namespace uwp {

// TODO: Refactor this class using CoreDispatcher.

// Manages the acquiring and releasing of extended resources, and related
// objects like the d3d12 device and command queue.
class ExtendedResourcesManager {
 public:
  ExtendedResourcesManager();
  ~ExtendedResourcesManager();

  static ExtendedResourcesManager* GetInstance();
  void Run();

  void AcquireExtendedResources();
  void ReleaseExtendedResources();
  void Quit();
  void ResetNonrecoverableFailure();
  void ReleaseBuffersHeap();

  // Returns true when the d3d12 device, buffer heap
  // and command queue can be used.
  bool GetD3D12Objects(Microsoft::WRL::ComPtr<ID3D12Device>* device,
                       Microsoft::WRL::ComPtr<ID3D12Heap>* buffer_heap,
                       void** command_queue);

  bool IsGpuDecoderReady() const {
    return is_av1_shader_compiled_ && is_vp9_shader_compiled_;
  }

  // This is called when it is found that the D3D12 driver is in an
  // error state that can not be recovered from.
  void OnNonrecoverableFailure();
  bool HasNonrecoverableFailure() { return is_nonrecoverable_failure_.load(); }

  // Returns false if the application should exit instead of suspend.
  bool IsSafeToSuspend() { return !is_nonrecoverable_failure_.load(); }

 private:
  enum Event {
    kTimeout,  // Returned by Queue::Poll() when there is no pending event.
    kAcquireExtendedResources,
    kCompileShaders,
    kReleaseExtendedResources,
    kQuit
  };

  bool GetD3D12ObjectsInternal();
  bool AcquireExtendedResourcesInternal();
  bool StartCompileShaders();
  void CompileShadersAsynchronously();
  void ReleaseExtendedResourcesInternal();

  // Must be called while holding mutex_.
  void OnNonrecoverableFailure_Locked() {
    is_nonrecoverable_failure_.store(true);
  }

  static ExtendedResourcesManager* s_instance_;

  shared::starboard::ThreadChecker thread_checker_;
  Mutex mutex_;
  atomic_bool is_extended_resources_acquired_;

  std::atomic_bool is_av1_shader_compiled_ = {false};
  std::atomic_bool is_vp9_shader_compiled_ = {false};

  static constexpr int kMaxConsecutiveAcquireFailures = 5;
  std::atomic<int> consecutive_acquire_failures_ = {0};
  std::atomic_bool is_nonrecoverable_failure_ = {false};
  Queue<Event> event_queue_;
  Microsoft::WRL::ComPtr<ID3D12Device> d3d12device_;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12queue_;
  // heap for frame buffers (for the decoder and output queue) memory allocation
  Microsoft::WRL::ComPtr<ID3D12Heap> d3d12FrameBuffersHeap_;

  // This is set to true when a release of extended resources is requested.
  // Anything delaying the release should be expedited when this is set.
  atomic_bool pending_extended_resources_release_;

  // This condition variable is used to synchronize changes to
  // is_extended_resources_acquired_.
  ConditionVariable acquisition_condition_;
};

}  // namespace uwp
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_UWP_EXTENDED_RESOURCES_MANAGER_H_
