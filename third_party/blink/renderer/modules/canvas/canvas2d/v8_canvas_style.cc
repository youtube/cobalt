// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/v8_canvas_style.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_color_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_gradient.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_pattern.h"
#include "third_party/blink/renderer/core/css/cssom/css_color_value.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_gradient.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_pattern.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style.h"

namespace blink {

bool ExtractV8CanvasStyle(v8::Isolate* isolate,
                          v8::Local<v8::Value> value,
                          V8CanvasStyle& style,
                          ExceptionState& exception_state) {
  // Check for string first as it's the most common.
  if (value->IsString()) {
    style.string = NativeValueTraits<IDLString>::NativeValue(isolate, value,
                                                             exception_state);
    if (UNLIKELY(exception_state.HadException())) {
      return false;
    }
    style.type = V8CanvasStyleType::kString;
    return true;
  }
  if (auto* pattern = V8CanvasPattern::ToWrappable(isolate, value)) {
    style.pattern = pattern;
    style.type = V8CanvasStyleType::kPattern;
    return true;
  }
  if (auto* gradient = V8CanvasGradient::ToWrappable(isolate, value)) {
    style.type = V8CanvasStyleType::kGradient;
    style.gradient = gradient;
    return true;
  }
  if (auto* color_value = V8CSSColorValue::ToWrappable(isolate, value)) {
    style.type = V8CanvasStyleType::kCSSColorValue;
    style.css_color_value = color_value->ToColor();
    return true;
  }

  // This case also handles non-string types that may be converted to strings
  // (such as numbers).
  style.string = NativeValueTraits<IDLString>::NativeValue(isolate, value,
                                                           exception_state);
  if (UNLIKELY(exception_state.HadException()))
    return false;
  style.type = V8CanvasStyleType::kString;
  return true;
}

v8::Local<v8::Value> CanvasStyleToV8(ScriptState* script_state,
                                     const CanvasStyle& style) {
  // All the types have been validated by this point, so that it's safe to use
  // ToLocalChecked().
  if (CanvasGradient* gradient = style.GetCanvasGradient()) {
    return ToV8Traits<CanvasGradient>::ToV8(script_state, gradient)
        .ToLocalChecked();
  }
  if (CanvasPattern* pattern = style.GetCanvasPattern()) {
    return ToV8Traits<CanvasPattern>::ToV8(script_state, pattern)
        .ToLocalChecked();
  }
  return ToV8Traits<IDLString>::ToV8(script_state, style.GetColorAsString())
      .ToLocalChecked();
}

}  // namespace blink
