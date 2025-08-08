// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_MULTI_DRAW_COMMON_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_MULTI_DRAW_COMMON_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/modules/webgl/webgl_extension.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace blink {

class V8UnionInt32ArrayAllowSharedOrLongSequence;
class V8UnionUint32ArrayAllowSharedOrUnsignedLongSequence;
class WebGLExtensionScopedContext;

class WebGLMultiDrawCommon {
 protected:
  bool ValidateDrawcount(WebGLExtensionScopedContext* scoped,
                         const char* function_name,
                         GLsizei drawcount);

  bool ValidateArray(WebGLExtensionScopedContext* scoped,
                     const char* function_name,
                     const char* outOfBoundsDescription,
                     size_t size,
                     GLuint offset,
                     GLsizei drawcount);

  static base::span<const int32_t> MakeSpan(
      const V8UnionInt32ArrayAllowSharedOrLongSequence* array);

  static base::span<const uint32_t> MakeSpan(
      const V8UnionUint32ArrayAllowSharedOrUnsignedLongSequence* array);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_MULTI_DRAW_COMMON_H_
