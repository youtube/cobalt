// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/directfb/blitter_internal.h"

#include "starboard/common/log.h"
#include "starboard/once.h"

namespace {
// Keep track of a global device registry that will be accessed by calls to
// create/destroy devices.
SbBlitterDeviceRegistry* s_device_registry = NULL;
SbOnceControl s_device_registry_once_control = SB_ONCE_INITIALIZER;

void EnsureDeviceRegistryInitialized() {
  s_device_registry = new SbBlitterDeviceRegistry();
  s_device_registry->default_device = NULL;
}
}  // namespace

SbBlitterDeviceRegistry* GetBlitterDeviceRegistry() {
  SbOnce(&s_device_registry_once_control, &EnsureDeviceRegistryInitialized);

  return s_device_registry;
}

bool SetScissorRectangle(SbBlitterContext context, IDirectFBSurface* surface) {
  // Setup the clipping region according to the context's stored scissor state.
  DFBRegion clip_region;
  clip_region.x1 = context->scissor.x;
  clip_region.y1 = context->scissor.y;
  clip_region.x2 = context->scissor.x + context->scissor.width;
  clip_region.y2 = context->scissor.y + context->scissor.height;

  if (surface->SetClip(surface, &clip_region) != DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": SetClip() failed.";
    return false;
  }

  return true;
}

namespace {
SbBlitterColor PremultiplyColor(SbBlitterColor color) {
  uint8_t alpha = SbBlitterAFromColor(color);
  float alpha_float = alpha / 255.0f;

  return SbBlitterColorFromRGBA(
      static_cast<uint8_t>(SbBlitterRFromColor(color) * alpha_float),
      static_cast<uint8_t>(SbBlitterGFromColor(color) * alpha_float),
      static_cast<uint8_t>(SbBlitterBFromColor(color) * alpha_float), alpha);
}
}  // namespace

bool SetDFBColor(IDirectFBSurface* surface, SbBlitterColor color) {
  if (surface->SetColor(surface, SbBlitterRFromColor(color),
                        SbBlitterGFromColor(color), SbBlitterBFromColor(color),
                        SbBlitterAFromColor(color)) != DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling SetColor().";
    return false;
  }

  return true;
}

bool SetupDFBSurfaceBlitStateFromBlitterContextState(
    SbBlitterContext context,
    SbBlitterSurface source,
    IDirectFBSurface* surface) {
  // Setup the DirectFB blending state on the surface as it is specified in
  // the Blitter API context.
  int blitting_flags = DSBLIT_NOFX;
  if (source->info.format == kSbBlitterSurfaceFormatA8) {
    // DirectFB A8 surfaces blit to (R, G, B, A) as (255, 255, 255, A), so in
    // order to maintain a premultiplied alpha environment, we must indicate
    // that the source color should be premultiplied as it is drawn.
    blitting_flags |= DSBLIT_SRC_PREMULTIPLY;
  }

  if (context->blending_enabled) {
    blitting_flags |= DSBLIT_BLEND_ALPHACHANNEL;
    surface->SetPorterDuff(surface, DSPD_SRC_OVER);
  } else {
    surface->SetPorterDuff(surface, DSPD_SRC);
  }

  if (context->modulate_blits_with_color) {
    blitting_flags |= DSBLIT_COLORIZE;
    if (context->blending_enabled) {
      blitting_flags |= DSBLIT_BLEND_COLORALPHA;
    }

    // If we have told DFB to premultiply for us, we don't need to manually
    // premultiply the color.
    SbBlitterColor color = (blitting_flags & DSBLIT_SRC_PREMULTIPLY)
                               ? context->current_color
                               : PremultiplyColor(context->current_color);
    if (!SetDFBColor(surface, color)) {
      SB_DLOG(ERROR) << __FUNCTION__
                     << ": Error calling SetUnpremultipliedColor().";
      return false;
    }
  }

  // Finally commit our DFB blitting flags to the surface.
  if (surface->SetBlittingFlags(surface, static_cast<DFBSurfaceBlittingFlags>(
                                             blitting_flags)) != DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling SetBlittingFlags().";
    return false;
  }

  if (!SetScissorRectangle(context, surface)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling SetScissorRectangle().";
    return false;
  }

  return true;
}

bool SetupDFBSurfaceDrawStateFromBlitterContextState(
    SbBlitterContext context,
    IDirectFBSurface* surface) {
  // Depending on the context blend state, set the DirectFB blend state flags
  // on the current render target surface.
  if (context->blending_enabled) {
    surface->SetDrawingFlags(surface,
                             static_cast<DFBSurfaceDrawingFlags>(DSDRAW_BLEND));
    surface->SetPorterDuff(surface, DSPD_SRC_OVER);
  } else {
    surface->SetDrawingFlags(surface, DSDRAW_NOFX);
    surface->SetPorterDuff(surface, DSPD_SRC);
  }

  // Setup the rectangle fill color value as specified.
  if (!SetDFBColor(surface, PremultiplyColor(context->current_color))) {
    SB_DLOG(ERROR) << __FUNCTION__
                   << ": Error calling SetUnpremultipliedColor().";
    return false;
  }

  if (!SetScissorRectangle(context, surface)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling SetScissorRectangle().";
    return false;
  }

  return true;
}
