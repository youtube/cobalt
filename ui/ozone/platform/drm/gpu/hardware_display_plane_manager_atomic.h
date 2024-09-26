// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_MANAGER_ATOMIC_H_
#define UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_MANAGER_ATOMIC_H_

#include <stdint.h>
#include <memory>

#include "ui/gfx/gpu_fence_handle.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"

namespace ui {

class HardwareDisplayPlaneManagerAtomic : public HardwareDisplayPlaneManager {
 public:
  explicit HardwareDisplayPlaneManagerAtomic(DrmDevice* drm);

  HardwareDisplayPlaneManagerAtomic(const HardwareDisplayPlaneManagerAtomic&) =
      delete;
  HardwareDisplayPlaneManagerAtomic& operator=(
      const HardwareDisplayPlaneManagerAtomic&) = delete;

  ~HardwareDisplayPlaneManagerAtomic() override;

  // HardwareDisplayPlaneManager:
  bool Commit(CommitRequest commit_request, uint32_t flags) override;

  bool Commit(HardwareDisplayPlaneList* plane_list,
              scoped_refptr<PageFlipRequest> page_flip_request,
              gfx::GpuFenceHandle* release_fence) override;
  bool DisableOverlayPlanes(HardwareDisplayPlaneList* plane_list) override;

  bool SetColorCorrectionOnAllCrtcPlanes(
      uint32_t crtc_id,
      ScopedDrmColorCtmPtr ctm_blob_data) override;

  bool ValidatePrimarySize(const DrmOverlayPlane& primary,
                           const drmModeModeInfo& mode) override;

  void RequestPlanesReadyCallback(
      DrmOverlayPlaneList planes,
      base::OnceCallback<void(DrmOverlayPlaneList planes)> callback) override;
  bool SetPlaneData(HardwareDisplayPlaneList* plane_list,
                    HardwareDisplayPlane* hw_plane,
                    const DrmOverlayPlane& overlay,
                    uint32_t crtc_id,
                    const gfx::Rect& src_rect) override;

 private:
  bool InitializePlanes() override;
  std::unique_ptr<HardwareDisplayPlane> CreatePlane(uint32_t plane_id) override;
  void SetAtomicPropsForCommit(drmModeAtomicReq* atomic_request,
                               HardwareDisplayPlaneList* plane_list,
                               const std::vector<uint32_t>& crtcs,
                               bool test_only);

  bool SetCrtcProps(drmModeAtomicReq* atomic_request,
                    uint32_t crtc_id,
                    bool set_active,
                    uint32_t mode_id,
                    bool enable_vrr);
  bool SetConnectorProps(drmModeAtomicReq* atomic_request,
                         uint32_t connector_id,
                         uint32_t crtc_id);

  bool CommitColorMatrix(const CrtcProperties& crtc_props) override;
  bool CommitGammaCorrection(const CrtcProperties& crtc_props) override;
  bool AddOutFencePtrProperties(
      drmModeAtomicReq* property_set,
      const std::vector<uint32_t>& crtcs,
      std::vector<base::ScopedFD>* out_fence_fds,
      std::vector<base::ScopedFD::Receiver>* out_fence_fd_receivers);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_MANAGER_ATOMIC_H_
