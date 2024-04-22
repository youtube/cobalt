// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/gbm_wrapper.h"

#include <gbm.h>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/posix/eintr_wrapper.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/gbm_device.h"

#if !defined(MINIGBM)
#include <dlfcn.h>
#include <fcntl.h>
#include <xf86drm.h>

#include "base/strings/stringize_macros.h"
#endif

namespace gbm_wrapper {

namespace {

uint32_t GetHandleForPlane(struct gbm_bo* bo, int plane) {
  return gbm_bo_get_handle_for_plane(bo, plane).u32;
}

uint32_t GetStrideForPlane(struct gbm_bo* bo, int plane) {
  return gbm_bo_get_stride_for_plane(bo, plane);
}

uint32_t GetOffsetForPlane(struct gbm_bo* bo, int plane) {
  return gbm_bo_get_offset(bo, plane);
}

int GetPlaneCount(struct gbm_bo* bo) {
  return gbm_bo_get_plane_count(bo);
}

int GetPlaneFdForBo(gbm_bo* bo, size_t plane) {
#if defined(MINIGBM)
  return gbm_bo_get_plane_fd(bo, plane);
#else
  const int plane_count = GetPlaneCount(bo);
  DCHECK(plane_count > 0 && plane < static_cast<size_t>(plane_count));

  // System linux gbm (or Mesa gbm) does not provide fds per plane basis. Thus,
  // get plane handle and use drm ioctl to get a prime fd out of it avoid having
  // two different branches for minigbm and Mesa gbm here.
  gbm_device* gbm_dev = gbm_bo_get_device(bo);
  int dev_fd = gbm_device_get_fd(gbm_dev);
  DCHECK_GE(dev_fd, 0);

  uint32_t plane_handle = GetHandleForPlane(bo, plane);

  int fd = -1;
  int ret;
  // Use DRM_RDWR to allow the fd to be mappable in another process.
  ret = drmPrimeHandleToFD(dev_fd, plane_handle, DRM_CLOEXEC | DRM_RDWR, &fd);

  // Older DRM implementations blocked DRM_RDWR, but gave a read/write mapping
  // anyways
  if (ret)
    ret = drmPrimeHandleToFD(dev_fd, plane_handle, DRM_CLOEXEC, &fd);

  return ret ? ret : fd;
#endif
}

size_t GetSizeOfPlane(gbm_bo* bo,
                      uint32_t format,
                      const gfx::Size& size,
                      size_t plane) {
#if defined(MINIGBM)
  return gbm_bo_get_plane_size(bo, plane);
#else
  DCHECK(!size.IsEmpty());

  // Get row size of the plane, stride and subsampled height to finally get the
  // size of a plane in bytes.
  const gfx::BufferFormat buffer_format =
      ui::GetBufferFormatFromFourCCFormat(format);
  const base::CheckedNumeric<size_t> stride_for_plane =
      GetStrideForPlane(bo, plane);
  const base::CheckedNumeric<size_t> subsampled_height =
      size.height() /
      gfx::SubsamplingFactorForBufferFormat(buffer_format, plane);

  // Apply subsampling factor to get size in bytes.
  const base::CheckedNumeric<size_t> checked_plane_size =
      subsampled_height * stride_for_plane;

  return checked_plane_size.ValueOrDie();
#endif
}

}  // namespace

class Buffer final : public ui::GbmBuffer {
 public:
  Buffer(struct gbm_bo* bo,
         uint32_t format,
         uint32_t flags,
         uint64_t modifier,
         const gfx::Size& size,
         gfx::NativePixmapHandle handle)
      : bo_(bo),
        format_(format),
        format_modifier_(modifier),
        flags_(flags),
        size_(size),
        handle_(std::move(handle)) {
    handle_.supports_zero_copy_webgpu_import = SupportsZeroCopyWebGPUImport();
  }

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  ~Buffer() override {
    DCHECK(!mmap_data_);
    gbm_bo_destroy(bo_.ExtractAsDangling());
  }

  uint32_t GetFormat() const override { return format_; }
  uint64_t GetFormatModifier() const override { return format_modifier_; }
  uint32_t GetFlags() const override { return flags_; }
  // TODO(reveman): This should not be needed once crbug.com/597932 is fixed,
  // as the size would be queried directly from the underlying bo.
  gfx::Size GetSize() const override { return size_; }
  gfx::BufferFormat GetBufferFormat() const override {
    return ui::GetBufferFormatFromFourCCFormat(format_);
  }
  bool AreFdsValid() const override {
    if (handle_.planes.empty())
      return false;

    for (const auto& plane : handle_.planes) {
      if (!plane.fd.is_valid())
        return false;
    }
    return true;
  }
  size_t GetNumPlanes() const override { return handle_.planes.size(); }
  int GetPlaneFd(size_t plane) const override {
    DCHECK_LT(plane, handle_.planes.size());
    return handle_.planes[plane].fd.get();
  }

  bool SupportsZeroCopyWebGPUImport() const override {
    // NOT supported if the buffer is multi-planar and its planes are disjoint.
    size_t plane_count = GetNumPlanes();
    if (plane_count > 1) {
      uint32_t handle = GetPlaneHandle(0);
      for (size_t plane = 1; plane < plane_count; ++plane) {
        if (GetPlaneHandle(plane) != handle) {
          return false;
        }
      }
    }
    return true;
  }

  uint32_t GetPlaneStride(size_t plane) const override {
    DCHECK_LT(plane, handle_.planes.size());
    return handle_.planes[plane].stride;
  }
  size_t GetPlaneOffset(size_t plane) const override {
    DCHECK_LT(plane, handle_.planes.size());
    return handle_.planes[plane].offset;
  }
  size_t GetPlaneSize(size_t plane) const override {
    DCHECK_LT(plane, handle_.planes.size());
    return static_cast<size_t>(handle_.planes[plane].size);
  }
  uint32_t GetPlaneHandle(size_t plane) const override {
    DCHECK_LT(plane, handle_.planes.size());
    return GetHandleForPlane(bo_, plane);
  }
  uint32_t GetHandle() const override { return gbm_bo_get_handle(bo_).u32; }
  gfx::NativePixmapHandle ExportHandle() const override {
    return CloneHandleForIPC(handle_);
  }

  sk_sp<SkSurface> GetSurface() override {
    DCHECK(!mmap_data_);
    uint32_t stride;
    void* addr;
    addr =
#if defined(MINIGBM)
        gbm_bo_map2(bo_, 0, 0, gbm_bo_get_width(bo_), gbm_bo_get_height(bo_),
                    GBM_BO_TRANSFER_READ_WRITE, &stride, &mmap_data_, 0);
#else
        gbm_bo_map(bo_, 0, 0, gbm_bo_get_width(bo_), gbm_bo_get_height(bo_),
                   GBM_BO_TRANSFER_READ_WRITE, &stride, &mmap_data_);
#endif

    if (!addr)
      return nullptr;
    SkImageInfo info =
        SkImageInfo::MakeN32Premul(size_.width(), size_.height());
    SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
    return SkSurface::MakeRasterDirectReleaseProc(
        info, addr, stride, &Buffer::UnmapGbmBo, this, &props);
  }

 private:
  static void UnmapGbmBo(void* pixels, void* context) {
    Buffer* buffer = static_cast<Buffer*>(context);
    gbm_bo_unmap(buffer->bo_, buffer->mmap_data_);
    buffer->mmap_data_ = nullptr;
  }

  raw_ptr<gbm_bo> bo_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION void* mmap_data_ = nullptr;

  const uint32_t format_;
  const uint64_t format_modifier_;
  const uint32_t flags_;

  const gfx::Size size_;

  gfx::NativePixmapHandle handle_;
};

std::unique_ptr<Buffer> CreateBufferForBO(struct gbm_bo* bo,
                                          uint32_t format,
                                          const gfx::Size& size,
                                          uint32_t flags) {
  DCHECK(bo);
  gfx::NativePixmapHandle handle;

  const uint64_t modifier = gbm_bo_get_modifier(bo);
  const int plane_count = GetPlaneCount(bo);
  // The Mesa's gbm implementation explicitly checks whether plane count <= and
  // returns 1 if the condition is true. Nevertheless, use a DCHECK here to make
  // sure the condition is not broken there.
  DCHECK_GT(plane_count, 0);
  // Ensure there are no differences in integer signs by casting any possible
  // values to size_t.
  for (size_t i = 0; i < static_cast<size_t>(plane_count); ++i) {
    // The fd returned by gbm_bo_get_fd is not ref-counted and need to be
    // kept open for the lifetime of the buffer.
    base::ScopedFD fd(GetPlaneFdForBo(bo, i));

    if (!fd.is_valid()) {
      PLOG(ERROR) << "Failed to export buffer to dma_buf";
      gbm_bo_destroy(bo);
      return nullptr;
    }

    handle.planes.emplace_back(
        GetStrideForPlane(bo, i), GetOffsetForPlane(bo, i),
        GetSizeOfPlane(bo, format, size, i), std::move(fd));
  }

  handle.modifier = modifier;
  return std::make_unique<Buffer>(bo, format, flags, modifier, size,
                                  std::move(handle));
}

class Device final : public ui::GbmDevice {
 public:
  Device(gbm_device* device) : device_(device) {}

  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  ~Device() override { gbm_device_destroy(device_); }

  std::unique_ptr<ui::GbmBuffer> CreateBuffer(uint32_t format,
                                              const gfx::Size& size,
                                              uint32_t flags) override {
    struct gbm_bo* bo =
        gbm_bo_create(device_, size.width(), size.height(), format, flags);
    if (!bo) {
#if DCHECK_IS_ON()
      const char fourcc_as_string[5] = {
          static_cast<char>(format), static_cast<char>(format >> 8),
          static_cast<char>(format >> 16), static_cast<char>(format >> 24), 0};

      DVLOG(2) << "Failed to create GBM BO, " << fourcc_as_string << ", "
               << size.ToString() << ", flags: 0x" << std::hex << flags
               << "; gbm_device_is_format_supported() = "
               << gbm_device_is_format_supported(device_, format, flags);
#endif
      return nullptr;
    }

    return CreateBufferForBO(bo, format, size, flags);
  }

  std::unique_ptr<ui::GbmBuffer> CreateBufferWithModifiers(
      uint32_t format,
      const gfx::Size& size,
      uint32_t flags,
      const std::vector<uint64_t>& modifiers) override {
    if (modifiers.empty())
      return CreateBuffer(format, size, flags);

    std::vector<uint64_t> filtered_modifiers =
        GetFilteredModifiers(format, flags, modifiers);
    struct gbm_bo* bo = nullptr;
    while (filtered_modifiers.size() > 0) {
      bo = gbm_bo_create_with_modifiers(device_, size.width(), size.height(),
                                        format, filtered_modifiers.data(),
                                        filtered_modifiers.size());
      if (!bo) {
        return nullptr;
      }

      struct gbm_import_fd_modifier_data fd_data;
      fd_data.width = size.width();
      fd_data.height = size.height();
      fd_data.format = format;
      fd_data.num_fds = gbm_bo_get_plane_count(bo);
      fd_data.modifier = gbm_bo_get_modifier(bo);

      // Store fds in the vector of base::ScopedFDs. Will be released
      // automatically.
      std::vector<base::ScopedFD> fds;
      for (size_t i = 0; i < static_cast<size_t>(fd_data.num_fds); ++i) {
        fds.emplace_back(GetPlaneFdForBo(bo, i));
        fd_data.fds[i] = fds.back().get();
        fd_data.strides[i] = gbm_bo_get_stride_for_plane(bo, i);
        fd_data.offsets[i] = gbm_bo_get_offset(bo, i);
      }

      struct gbm_bo* bo_import =
          gbm_bo_import(device_, GBM_BO_IMPORT_FD_MODIFIER, &fd_data, flags);
      if (bo_import) {
        gbm_bo_destroy(bo_import);
        break;
      } else {
        gbm_bo_destroy(bo);
        bo = nullptr;
        AddModifierToBlocklist(format, flags, fd_data.modifier);
        filtered_modifiers =
            GetFilteredModifiers(format, flags, filtered_modifiers);
      }
    }
    if (!bo) {
      return nullptr;
    }

    return CreateBufferForBO(bo, format, size, flags);
  }

  std::unique_ptr<ui::GbmBuffer> CreateBufferFromHandle(
      uint32_t format,
      const gfx::Size& size,
      gfx::NativePixmapHandle handle) override {
    if (handle.planes.empty()) {
      LOG(ERROR) << "Importing handle with no planes";
      return nullptr;
    }
    if (handle.planes[0].offset != 0u) {
      LOG(ERROR) << "Unsupported handle: expected an offset of 0 for the first "
                    "plane; got "
                 << handle.planes[0].offset;
      return nullptr;
    }

    int gbm_flags = 0;
    if ((gbm_flags = GetSupportedGbmFlags(format)) == 0) {
      LOG(ERROR) << "gbm format not supported: " << format;
      return nullptr;
    }

    struct gbm_import_fd_modifier_data fd_data;
    fd_data.width = size.width();
    fd_data.height = size.height();
    fd_data.format = format;
    fd_data.num_fds = handle.planes.size();
    fd_data.modifier = handle.modifier;

    DCHECK_LE(handle.planes.size(), 3u);
    for (size_t i = 0; i < handle.planes.size(); ++i) {
      fd_data.fds[i] = handle.planes[i < handle.planes.size() ? i : 0].fd.get();
      fd_data.strides[i] = handle.planes[i].stride;
      fd_data.offsets[i] = handle.planes[i].offset;
    }

    // The fd passed to gbm_bo_import is not ref-counted and need to be
    // kept open for the lifetime of the buffer.
    struct gbm_bo* bo =
        gbm_bo_import(device_, GBM_BO_IMPORT_FD_MODIFIER, &fd_data, gbm_flags);
    if (!bo) {
      LOG(ERROR) << "nullptr returned from gbm_bo_import";
      return nullptr;
    }

    return std::make_unique<Buffer>(bo, format, gbm_flags, handle.modifier,
                                    size, std::move(handle));
  }

  bool CanCreateBufferForFormat(uint32_t format) override {
    return GetSupportedGbmFlags(format) != 0;
  }

#if defined(MINIGBM)
  int GetSupportedGbmFlags(uint32_t format) {
    int gbm_flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING;
    if (gbm_device_is_format_supported(device_, format, gbm_flags))
      return gbm_flags;
    gbm_flags = GBM_BO_USE_TEXTURING;
    if (gbm_device_is_format_supported(device_, format, gbm_flags))
      return gbm_flags;
    return 0;
  }
#else
  int GetSupportedGbmFlags(uint32_t format) {
    if (gbm_device_is_format_supported(device_, format, GBM_BO_USE_SCANOUT))
      return GBM_BO_USE_SCANOUT;
    return 0;
  }
#endif

 private:
  std::vector<uint64_t> GetFilteredModifiers(
      uint32_t format,
      uint32_t flags,
      const std::vector<uint64_t>& modifiers) {
    std::vector<uint64_t> filtered_modifiers = modifiers;

    for (const auto& [entry_format, entry_flags, entry_modifier] :
         modifier_blocklist_) {
      if (entry_format == format && entry_flags == flags) {
        filtered_modifiers.erase(
            std::remove(filtered_modifiers.begin(), filtered_modifiers.end(),
                        entry_modifier),
            filtered_modifiers.end());
      }
    }

    return filtered_modifiers;
  }

  void AddModifierToBlocklist(uint32_t format,
                              uint32_t flags,
                              uint64_t modifier) {
    modifier_blocklist_.push_back({format, flags, modifier});
  }

  const raw_ptr<gbm_device> device_;
  std::vector<std::tuple<uint32_t, uint32_t, uint64_t>> modifier_blocklist_;
};

}  // namespace gbm_wrapper

namespace ui {

std::unique_ptr<GbmDevice> CreateGbmDevice(int fd) {
  gbm_device* device = gbm_create_device(fd);
  if (!device)
    return nullptr;
  return std::make_unique<gbm_wrapper::Device>(device);
}

}  // namespace ui
