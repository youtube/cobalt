// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_context_attribute_helpers.h"

#include "third_party/blink/renderer/core/frame/settings.h"
#include "ui/gl/gpu_preference.h"

namespace blink {

WebGLContextAttributes* ToWebGLContextAttributes(
    const CanvasContextCreationAttributesCore& attrs) {
  WebGLContextAttributes* result = WebGLContextAttributes::Create();
  result->setAlpha(attrs.alpha);
  result->setDepth(attrs.depth);
  result->setStencil(attrs.stencil);
  result->setAntialias(attrs.antialias);
  result->setPremultipliedAlpha(attrs.premultiplied_alpha);
  result->setPreserveDrawingBuffer(attrs.preserve_drawing_buffer);
  result->setPowerPreference(attrs.power_preference);
  result->setFailIfMajorPerformanceCaveat(
      attrs.fail_if_major_performance_caveat);
  result->setXrCompatible(attrs.xr_compatible);
  result->setDesynchronized(attrs.desynchronized);
  return result;
}

Platform::ContextAttributes ToPlatformContextAttributes(
    const CanvasContextCreationAttributesCore& attrs,
    Platform::ContextType context_type,
    bool support_own_offscreen_surface) {
  Platform::ContextAttributes result;
  result.prefer_low_power_gpu =
      (PowerPreferenceToGpuPreference(attrs.power_preference) ==
       gl::GpuPreference::kLowPower);
  result.fail_if_major_performance_caveat =
      attrs.fail_if_major_performance_caveat;
  result.context_type = context_type;
  if (support_own_offscreen_surface) {
    // Only ask for alpha/depth/stencil/antialias if we may be using the default
    // framebuffer. They are not needed for standard offscreen rendering.
    result.support_alpha = attrs.alpha;
    result.support_depth = attrs.depth;
    result.support_stencil = attrs.stencil;
    result.support_antialias = attrs.antialias;
  }
  return result;
}

gl::GpuPreference PowerPreferenceToGpuPreference(String power_preference) {
  // This code determines the handling of the "default" power preference.
  if (power_preference == "high-performance")
    return gl::GpuPreference::kHighPerformance;
  return gl::GpuPreference::kLowPower;
}

}  // namespace blink
