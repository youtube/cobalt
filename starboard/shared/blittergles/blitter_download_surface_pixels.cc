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

#include <GLES2/gl2.h>

#include <memory>

#include "starboard/common/log.h"
#include "starboard/memory.h"
#include "starboard/shared/blittergles/blitter_context.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/blittergles/blitter_surface.h"
#include "starboard/shared/gles/gl_call.h"

namespace {

bool CopyPixels(SbBlitterPixelDataFormat pixel_format,
                int height,
                int width,
                int pitch_in_bytes,
                void* out_data) {
  GLenum format =
      pixel_format == kSbBlitterPixelDataFormatA8 ? GL_ALPHA : GL_RGBA;
  uint8_t* out_pixel_data = static_cast<uint8_t*>(out_data);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, format, GL_UNSIGNED_BYTE, out_pixel_data);
  if (glGetError() != GL_NO_ERROR) {
    SB_DLOG(ERROR) << ": Failed to read pixels.";
    return false;
  }
  starboard::shared::blittergles::ChangeDataFormat(
      kSbBlitterPixelDataFormatRGBA8, pixel_format, pitch_in_bytes, height,
      out_pixel_data);

  return true;
}

}  // namespace

bool SbBlitterDownloadSurfacePixels(SbBlitterSurface surface,
                                    SbBlitterPixelDataFormat pixel_format,
                                    int pitch_in_bytes,
                                    void* out_pixel_data) {
  if (!SbBlitterIsSurfaceValid(surface)) {
    SB_DLOG(ERROR) << ": Invalid surface.";
    return false;
  }
  if (out_pixel_data == NULL) {
    SB_DLOG(ERROR) << ": out_pixel_data cannot be NULL.";
    return false;
  }
  if (!SbBlitterIsPixelFormatSupportedByDownloadSurfacePixels(surface,
                                                              pixel_format)) {
    SB_DLOG(ERROR) << ": Given pixel format isn't supported by surface.";
    return false;
  }
  if (pitch_in_bytes <
      surface->info.width * SbBlitterBytesPerPixelForFormat(pixel_format)) {
    SB_DLOG(ERROR) << ": Output pitch must be >= (surface width * bytes per "
                   << "pixel).";
    return false;
  }
  if (pitch_in_bytes % SbBlitterBytesPerPixelForFormat(pixel_format) != 0) {
    SB_DLOG(ERROR) << ": Output pitch must be divisible by bytes per pixel.";
    return false;
  }

  if (surface->render_target != NULL) {
    starboard::shared::blittergles::SbBlitterContextRegistry* context_registry =
        starboard::shared::blittergles::GetBlitterContextRegistry();
    starboard::ScopedLock lock(context_registry->mutex);

    SbBlitterContextPrivate::ScopedCurrentContext scoped_current_context(
        context_registry->context, surface->render_target);
    if (scoped_current_context.InitializationError()) {
      return false;
    }
    GL_CALL(glFinish());
    return CopyPixels(pixel_format, surface->info.height, surface->info.width,
                      pitch_in_bytes, out_pixel_data);
  }

  std::unique_ptr<SbBlitterRenderTargetPrivate> dummy_render_target(
      new SbBlitterRenderTargetPrivate());
  dummy_render_target->swap_chain = kSbBlitterInvalidSwapChain;
  dummy_render_target->surface = surface;
  dummy_render_target->width = surface->info.width;
  dummy_render_target->height = surface->info.height;
  dummy_render_target->device = surface->device;
  if (!dummy_render_target->SetFramebuffer()) {
    return false;
  }

  starboard::shared::blittergles::SbBlitterContextRegistry* context_registry =
      starboard::shared::blittergles::GetBlitterContextRegistry();
  starboard::ScopedLock lock(context_registry->mutex);

  SbBlitterContextPrivate::ScopedCurrentContext scoped_current_context(
      context_registry->context, dummy_render_target.get());
  if (scoped_current_context.InitializationError()) {
    return false;
  }
  GL_CALL(glFinish());
  bool success =
      CopyPixels(pixel_format, surface->info.height, surface->info.width,
                 pitch_in_bytes, out_pixel_data);
  GL_CALL(glDeleteFramebuffers(1, &dummy_render_target->framebuffer_handle));
  return success;
}
