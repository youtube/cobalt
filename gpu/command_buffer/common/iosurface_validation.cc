// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/iosurface_validation.h"

#include <CoreVideo/CoreVideo.h>
#include <IOSurface/IOSurfaceRef.h>

namespace gpu {

bool ValidateIOSurface(const gfx::ScopedIOSurface& io_surface,
                       viz::SharedImageFormat format,
                       gfx::Size size,
                       std::string* out_error_str) {
  // Validate top-level IOSurface format.
  uint32_t io_surface_format = IOSurfaceGetPixelFormat(io_surface.get());
  // Always treat RGBA as BGRA. It makes no difference for validating size since
  // they have the same size, and avoids the complexity of actually trying to
  // figure out if conversion should happen.
  if (io_surface_format == kCVPixelFormatType_32RGBA) {
    io_surface_format = kCVPixelFormatType_32BGRA;
  }
  const bool override_rgba_to_bgra = true;
  std::optional<uint32_t> expected_format =
      gfx::SharedImageFormatToIOSurfacePixelFormat(format,
                                                   override_rgba_to_bgra);
  if (io_surface_format != expected_format) {
    if (out_error_str) {
      *out_error_str =
          "IOSurface pixel format does not match specified shared "
          "image format.";
    }
    return false;
  }

  // Validate top-level IOSurface dimensions.
  if (IOSurfaceGetWidth(io_surface.get()) !=
          static_cast<size_t>(size.width()) ||
      IOSurfaceGetHeight(io_surface.get()) !=
          static_cast<size_t>(size.height())) {
    if (out_error_str) {
      *out_error_str = "IOSurface size does not match specified size.";
    }
    return false;
  }

  // Ensure the IOSurface has at least as many planes as the requested format.
  // For single-planar IOSurfaces, IOSurfaceGetPlaneCount returns 0.
  size_t io_surface_plane_count =
      std::max<size_t>(1, IOSurfaceGetPlaneCount(io_surface.get()));
  if (io_surface_plane_count < static_cast<size_t>(format.NumberOfPlanes())) {
    if (out_error_str) {
      *out_error_str = "IOSurface plane count is too small.";
    }
    return false;
  }

  // Validate per-plane dimensions and stride. A malformed IOSurface could
  // have planes with dimensions inconsistent with its top-level size and
  // format, leading to out-of-bounds access during buffer operations.
  for (int plane_index = 0; plane_index < format.NumberOfPlanes();
       ++plane_index) {
    gfx::Size plane_size = format.GetPlaneSize(plane_index, size);
    if (IOSurfaceGetWidthOfPlane(io_surface.get(), plane_index) !=
            static_cast<size_t>(plane_size.width()) ||
        IOSurfaceGetHeightOfPlane(io_surface.get(), plane_index) !=
            static_cast<size_t>(plane_size.height())) {
      if (out_error_str) {
        *out_error_str = "IOSurface plane size does not match specified size.";
      }
      return false;
    }

    // Ensure the IOSurface has enough bytes per row for the plane to prevent
    // potential out-of-bounds access when copying or accessing the buffer.
    size_t io_surface_bytes_per_row =
        IOSurfaceGetBytesPerRowOfPlane(io_surface.get(), plane_index);
    size_t min_bytes_per_row;
    if (format.is_single_plane()) {
      CHECK(!format.IsCompressed());
      min_bytes_per_row = static_cast<size_t>(format.BytesPerPixel()) *
                          static_cast<size_t>(plane_size.width());
    } else {
      min_bytes_per_row =
          static_cast<size_t>(format.MultiplanarStorageBytesPerChannel()) *
          static_cast<size_t>(format.NumChannelsInPlane(plane_index)) *
          static_cast<size_t>(plane_size.width());
    }
    if (io_surface_bytes_per_row < min_bytes_per_row) {
      if (out_error_str) {
        *out_error_str = "IOSurface bytes per row is too small.";
      }
      return false;
    }
  }

  return true;
}

}  // namespace gpu
