// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_CONTEXT_CREATION_ATTRIBUTES_CORE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_CONTEXT_CREATION_ATTRIBUTES_CORE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CORE_EXPORT CanvasContextCreationAttributesCore {
  DISALLOW_NEW();

 public:
  CanvasContextCreationAttributesCore();
  CanvasContextCreationAttributesCore(
      blink::CanvasContextCreationAttributesCore const&);
  virtual ~CanvasContextCreationAttributesCore();

  enum class WillReadFrequently { kTrue, kFalse, kUndefined };

  bool alpha = true;
  bool antialias = true;
  PredefinedColorSpace color_space = PredefinedColorSpace::kSRGB;
  bool depth = true;
  bool fail_if_major_performance_caveat = false;
  bool desynchronized = false;
  CanvasPixelFormat pixel_format = CanvasPixelFormat::kUint8;
  bool premultiplied_alpha = true;
  bool preserve_drawing_buffer = false;
  String power_preference = "default";
  bool stencil = false;
  // Help to determine whether to use GPU or CPU for the canvas.
  WillReadFrequently will_read_frequently = WillReadFrequently::kUndefined;
  bool xr_compatible = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_CONTEXT_CREATION_ATTRIBUTES_CORE_H_
