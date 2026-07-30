// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_UTILS_H_

#include <limits>

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_layer_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_texture_type.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

template <typename T>
bool ValidateQuadLayerInit(const T* init, ExceptionState& exception_state) {
  if (init->hasWidth() &&
      init->width() <= std::numeric_limits<float>::epsilon()) {
    exception_state.ThrowTypeError("width must be greater than epsilon.");
    return false;
  }
  if (init->hasHeight() &&
      init->height() <= std::numeric_limits<float>::epsilon()) {
    exception_state.ThrowTypeError("height must be greater than epsilon.");
    return false;
  }
  return true;
}

template <typename T>
bool ValidateCylinderLayerInit(const T* init, ExceptionState& exception_state) {
  if (init->hasRadius() && init->radius() < 0.f) {
    exception_state.ThrowTypeError(
        "radius must be greater than or equal to zero.");
    return false;
  }
  if (init->hasAspectRatio() &&
      init->aspectRatio() < std::numeric_limits<float>::epsilon()) {
    exception_state.ThrowTypeError(
        "aspectRatio must be greater than or equal to epsilon.");
    return false;
  }
  if (init->hasCentralAngle() &&
      (init->centralAngle() < 0.f || init->centralAngle() > kTwoPiFloat)) {
    exception_state.ThrowTypeError(
        "The central angle must be in the range [0.f, 2pi].");
    return false;
  }
  return true;
}

template <typename T>
bool ValidateEquirectLayerInit(const T* init, ExceptionState& exception_state) {
  auto* space = DynamicTo<XRReferenceSpace>(init->space());
  if (!space) {
    exception_state.ThrowTypeError(
        "The 'space' parameter must be an XRReferenceSpace.");
    return false;
  }
  if (!space->IsStationary()) {
    exception_state.ThrowTypeError(
        "The 'space' parameter cannot be of type 'viewer'.");
    return false;
  }
  return true;
}

inline V8XRLayerLayout::Enum DetermineLayout(V8XRLayerLayout layout,
                                             V8XRTextureType::Enum texture_type,
                                             bool stereoscopic_views) {
  V8XRLayerLayout::Enum final_layout = layout.AsEnum();

  if (final_layout == V8XRLayerLayout::Enum::kDefault) {
    if (stereoscopic_views) {
      final_layout = V8XRLayerLayout::Enum::kStereo;
    } else {
      final_layout = V8XRLayerLayout::Enum::kMono;
    }
  }

  // We don't support 2-texture solution for "stereo". So if the texture type is
  // not "texture-array", we fall back to "stereo-left-right".
  if (final_layout == V8XRLayerLayout::Enum::kStereo &&
      texture_type == V8XRTextureType::Enum::kTexture) {
    final_layout = V8XRLayerLayout::Enum::kStereoLeftRight;
  }

  // "default" must be resolved.
  CHECK(final_layout != V8XRLayerLayout::Enum::kDefault);
  return final_layout;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_UTILS_H_
