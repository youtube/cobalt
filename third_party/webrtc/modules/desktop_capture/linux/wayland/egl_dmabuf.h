/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_EGL_DMABUF_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_EGL_DMABUF_H_

#include <EGL/egl.h>
#include <EGL/eglplatform.h>
#include <GL/gl.h>
#include <gbm.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "modules/desktop_capture/desktop_geometry.h"

namespace webrtc {

constexpr dev_t DEVICE_ID_INVALID = static_cast<dev_t>(0);

class EglDrmDevice {
 public:
  struct EGLStruct {
    std::vector<std::string> extensions;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
  };

  struct PlaneData {
    int32_t fd;
    uint32_t stride;
    uint32_t offset;
  };

  EglDrmDevice(EGLDisplay display, dev_t device_id = DEVICE_ID_INVALID);
  EglDrmDevice(std::string render_node, dev_t device_id = DEVICE_ID_INVALID);

  ~EglDrmDevice();

  bool EnsureInitialized();
  bool IsInitialized() const { return initialized_; }
  dev_t GetDeviceId() const { return device_id_; }

  bool ImageFromDmaBuf(const DesktopSize& size,
                       uint32_t format,
                       const std::vector<PlaneData>& plane_datas,
                       uint64_t modifiers,
                       const DesktopVector& offset,
                       const DesktopSize& buffer_size,
                       uint8_t* data);
  std::vector<uint64_t> QueryDmaBufModifiers(uint32_t format);

 private:
  EGLStruct egl_;
  bool initialized_ = false;
  bool has_image_dma_buf_import_ext_ = false;
  dev_t device_id_ = DEVICE_ID_INVALID;

  struct GbmDeviceDeleter {
    void operator()(gbm_device* device) const {
      if (device) {
        gbm_device_destroy(device);
      }
    }
  };
  std::unique_ptr<gbm_device, GbmDeviceDeleter> gbm_device_;
  int32_t drm_fd_ = -1;
  std::string render_node_;

  GLuint fbo_ = 0;
  GLuint texture_ = 0;
};

class EglDmaBuf {
 public:
  EglDmaBuf();
  ~EglDmaBuf() = default;

  // Returns the DRM device to use for querying DMA-BUF modifiers and importing
  // frames. Device selection follows this priority order:
  //
  // 1. Platform device - created from Wayland platform EGL display during
  //    initialization if EGL platform extensions are available
  // 2. First enumerated device - fallback if platform device creation fails,
  //    uses EGL device enumeration to discover available DRM devices
  // 3. nullptr - if no devices are available
  EglDrmDevice* GetRenderDevice();

 private:
  bool CreatePlatformDevice();
  void EnumerateDrmDevices();

  std::map<dev_t, std::unique_ptr<EglDrmDevice>> devices_;
  std::unique_ptr<EglDrmDevice> default_platform_device_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_EGL_DMABUF_H_
