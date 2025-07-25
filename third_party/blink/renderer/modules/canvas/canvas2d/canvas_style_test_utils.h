// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_STYLE_TEST_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_STYLE_TEST_UTILS_H_

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_color_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_gradient.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_pattern.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_gradient.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_pattern.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8.h"

namespace blink {

// This file contains convenience functions for setting and getting stroke
// (or fill) style.

template <typename T>
void SetFillStyleHelper(BaseRenderingContext2D* ctx,
                        ScriptState* script_state,
                        T* value) {
  NonThrowableExceptionState exception_state;
  ctx->setFillStyle(script_state->GetIsolate(),
                    ToV8Traits<T>::ToV8(script_state, value).ToLocalChecked(),
                    exception_state);
}

void SetFillStyleString(BaseRenderingContext2D* ctx,
                        ScriptState* script_state,
                        const String& string);

void SetStrokeStyleString(BaseRenderingContext2D* ctx,
                          ScriptState* script_state,
                          const String& string);

String GetStrokeStyleAsString(BaseRenderingContext2D* ctx,
                              ScriptState* script_state);

String GetFillStyleAsString(BaseRenderingContext2D* ctx,
                            ScriptState* script_state);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_STYLE_TEST_UTILS_H_
