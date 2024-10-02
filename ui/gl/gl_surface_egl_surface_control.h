// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_EGL_SURFACE_CONTROL_H_
#define UI_GL_GL_SURFACE_EGL_SURFACE_CONTROL_H_

#include <android/native_window.h>
#include <memory>
#include <queue>

#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/frame_data.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/presenter.h"

namespace base {
class SingleThreadTaskRunner;

namespace android {
class ScopedHardwareBufferFenceSync;
}  // namespace android
}  // namespace base

namespace gl {

class ScopedANativeWindow;
class ScopedJavaSurfaceControl;

class GL_EXPORT GLSurfaceEGLSurfaceControl : public Presenter {
 public:
  GLSurfaceEGLSurfaceControl(
      gl::ScopedANativeWindow window,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  GLSurfaceEGLSurfaceControl(
      gl::ScopedJavaSurfaceControl scoped_java_surface_control,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  bool Initialize();
  void Destroy();
  // Presenter implementation.
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;

  bool ScheduleOverlayPlane(
      OverlayImage image,
      std::unique_ptr<gfx::GpuFence> gpu_fence,
      const gfx::OverlayPlaneData& overlay_plane_data) override;
  void PreserveChildSurfaceControls() override;

  void Present(SwapCompletionCallback completion_callback,
               PresentationCallback presentation_callback,
               gfx::FrameData data) override;

  bool SupportsPlaneGpuFences() const override;
  void SetFrameRate(float frame_rate) override;
  void SetChoreographerVsyncIdForNextFrame(
      absl::optional<int64_t> choreographer_vsync_id) override;

 private:
  GLSurfaceEGLSurfaceControl(
      scoped_refptr<gfx::SurfaceControl::Surface> root_surface,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~GLSurfaceEGLSurfaceControl() override;

  struct SurfaceState {
    SurfaceState();
    SurfaceState(const gfx::SurfaceControl::Surface& parent,
                 const std::string& name);
    ~SurfaceState();

    SurfaceState(SurfaceState&& other);
    SurfaceState& operator=(SurfaceState&& other);

    int z_order = 0;
    raw_ptr<AHardwareBuffer> hardware_buffer = nullptr;
    gfx::Rect dst;
    gfx::Rect src;
    gfx::OverlayTransform transform = gfx::OVERLAY_TRANSFORM_NONE;
    bool opaque = true;
    gfx::ColorSpace color_space;
    absl::optional<gfx::HDRMetadata> hdr_metadata;

    // Indicates whether buffer for this layer was updated in the currently
    // pending transaction, or the last transaction submitted if there isn't
    // one pending.
    bool buffer_updated_in_pending_transaction = true;

    // Indicates whether the |surface| will be visible or hidden.
    bool visibility = true;
    scoped_refptr<gfx::SurfaceControl::Surface> surface;
  };

  struct ResourceRef {
    ResourceRef();
    ~ResourceRef();

    ResourceRef(ResourceRef&& other);
    ResourceRef& operator=(ResourceRef&& other);

    scoped_refptr<gfx::SurfaceControl::Surface> surface;
    std::unique_ptr<base::android::ScopedHardwareBufferFenceSync> scoped_buffer;
  };
  using ResourceRefs = base::flat_map<ASurfaceControl*, ResourceRef>;

  struct PendingPresentationCallback {
    PendingPresentationCallback();
    ~PendingPresentationCallback();

    PendingPresentationCallback(PendingPresentationCallback&& other);
    PendingPresentationCallback& operator=(PendingPresentationCallback&& other);

    base::TimeTicks available_time;
    base::TimeTicks ready_time;
    base::TimeTicks latch_time;

    base::ScopedFD present_fence;
    PresentationCallback callback;
  };

  struct PrimaryPlaneFences {
    PrimaryPlaneFences();
    ~PrimaryPlaneFences();

    PrimaryPlaneFences(PrimaryPlaneFences&& other);
    PrimaryPlaneFences& operator=(PrimaryPlaneFences&& other);

    base::ScopedFD available_fence;
    base::ScopedFD ready_fence;
  };

  using TransactionId = uint64_t;
  class TransactionAckTimeoutManager {
   public:
    TransactionAckTimeoutManager(
        scoped_refptr<base::SingleThreadTaskRunner> task_runner);

    TransactionAckTimeoutManager(const TransactionAckTimeoutManager&) = delete;
    TransactionAckTimeoutManager& operator=(
        const TransactionAckTimeoutManager&) = delete;

    ~TransactionAckTimeoutManager();

    void ScheduleHangDetection();
    void OnTransactionAck();

   private:
    void OnTransactionTimeout(TransactionId transaction_id);

    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
    TransactionId current_transaction_id_ = 0;
    TransactionId last_acked_transaction_id_ = 0;
    base::CancelableOnceClosure hang_detection_cb_;
  };

  void CommitPendingTransaction(SwapCompletionCallback completion_callback,
                                PresentationCallback callback);

  // Called on the |gpu_task_runner_| when a transaction is acked by the
  // framework.
  void OnTransactionAckOnGpuThread(
      SwapCompletionCallback completion_callback,
      PresentationCallback presentation_callback,
      ResourceRefs released_resources,
      absl::optional<PrimaryPlaneFences> primary_plane_fences,
      gfx::SurfaceControl::TransactionStats transaction_stats);

  // Called on the |gpu_task_runner_| when a transaction is committed by the
  // framework.
  void OnTransactionCommittedOnGpuThread();

  void AdvanceTransactionQueue();
  void CheckPendingPresentationCallbacks();

  const std::string child_surface_name_;

  // Holds the surface state changes made since the last call to SwapBuffers.
  absl::optional<gfx::SurfaceControl::Transaction> pending_transaction_;
  size_t pending_surfaces_count_ = 0u;
  // Resources in the pending frame, for which updates are being
  // collected in |pending_transaction_|. These are resources for which the
  // pending transaction has a ref but they have not been applied and
  // transferred to the framework.
  ResourceRefs pending_frame_resources_;

  // The fences associated with the primary plane (renderer by the display
  // compositor) for the pending frame.
  absl::optional<PrimaryPlaneFences> primary_plane_fences_;

  // Transactions waiting to be applied once the previous transaction is acked.
  std::queue<gfx::SurfaceControl::Transaction> pending_transaction_queue_;

  // PresentationCallbacks for transactions which have been acked but their
  // present fence has not fired yet.
  std::queue<PendingPresentationCallback> pending_presentation_callback_queue_;

  // The list of Surfaces and the corresponding state based on the most recent
  // updates.
  std::vector<SurfaceState> surface_list_;

  // Resources in the previous transaction sent or queued to be sent to the
  // framework. The framework is assumed to retain ownership of these resources
  // until the next frame update.
  ResourceRefs current_frame_resources_;

  // The root surface tied to the ANativeWindow that places the content of this
  // Presenter in the java view tree.
  scoped_refptr<gfx::SurfaceControl::Surface> root_surface_;

  // Numberf of transactions that was applied and we are waiting for it to be
  // committed (if commit is enabled) or acked (if commit is not enabled).
  uint32_t num_transaction_commit_or_ack_pending_ = 0u;

  float frame_rate_ = 0;
  bool frame_rate_update_pending_ = false;

  base::CancelableOnceClosure check_pending_presentation_callback_queue_task_;

  // Set if a swap failed and the surface is no longer usable.
  bool surface_lost_ = false;

  TransactionAckTimeoutManager transaction_ack_timeout_manager_;

  bool preserve_children_ = false;

  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

  // Use target deadline API instead of queuing transactions and submitting
  // after previous transaction is ack-ed.
  const bool use_target_deadline_;
  const bool using_on_commit_callback_;

  absl::optional<int64_t> choreographer_vsync_id_for_next_frame_;

  base::WeakPtrFactory<GLSurfaceEGLSurfaceControl> weak_factory_{this};
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_EGL_SURFACE_CONTROL_H_
