// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/blitter.h"

#include <memory>

#include "starboard/common/log.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/blittergles/blitter_surface.h"

SbBlitterSurface SbBlitterCreateRenderTargetSurface(
    SbBlitterDevice device,
    int width,
    int height,
    SbBlitterSurfaceFormat surface_format) {
  if (!SbBlitterIsDeviceValid(device)) {
    SB_DLOG(ERROR) << ": Invalid device.";
    return kSbBlitterInvalidSurface;
  }
  if (!SbBlitterIsSurfaceFormatSupportedByRenderTargetSurface(device,
                                                              surface_format)) {
    SB_DLOG(ERROR) << ": Unsupported surface format.";
    return kSbBlitterInvalidSurface;
  }
  if (width <= 0 || height <= 0) {
    SB_DLOG(ERROR) << ": Height and width must both be > 0.";
    return kSbBlitterInvalidSurface;
  }

  std::unique_ptr<SbBlitterSurfacePrivate> surface(
      new SbBlitterSurfacePrivate());
  surface->device = device;
  surface->info.width = width;
  surface->info.height = height;
  surface->info.format = surface_format;
  surface->color_texture_handle = 0;
  if (!surface->SetTexture(NULL)) {
    return kSbBlitterInvalidSurface;
  }
  std::unique_ptr<SbBlitterRenderTargetPrivate> render_target(
      new SbBlitterRenderTargetPrivate());
  render_target->swap_chain = kSbBlitterInvalidSwapChain;
  render_target->surface = surface.get();
  render_target->width = width;
  render_target->height = height;
  render_target->device = device;
  render_target->framebuffer_handle = 0;
  surface->render_target = render_target.release();
  if (!surface->render_target->SetFramebuffer()) {
    return kSbBlitterInvalidSurface;
  }

  return surface.release();
}
