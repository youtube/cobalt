// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_COBALT_OES_EGL_IMAGE_EXTERNAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_COBALT_OES_EGL_IMAGE_EXTERNAL_H_

#include "third_party/blink/renderer/modules/webgl/webgl_extension.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace blink {

class ExceptionState;
class HTMLVideoElement;

// Implements the Blink-side WebGL extension OES_EGL_image_external,
// enabling JavaScript to bind video frames from HTMLVideoElement to
// GL_TEXTURE_EXTERNAL_OES targets.
//
// Lifetime: Lifecycle is tied directly to the parent WebGLRenderingContextBase
// that instantiated it. Managed via Blink's Garbage Collector (oilpan).
//
// Threading: Main-thread bound (Blink execution context thread).
class OESEGLImageExternal final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  explicit OESEGLImageExternal(WebGLRenderingContextBase*);

  WebGLExtensionName GetName() const override;

  void EGLImageTargetTexture2DOES(GLenum target,
                                  HTMLVideoElement* video,
                                  ExceptionState& exception_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_COBALT_OES_EGL_IMAGE_EXTERNAL_H_
