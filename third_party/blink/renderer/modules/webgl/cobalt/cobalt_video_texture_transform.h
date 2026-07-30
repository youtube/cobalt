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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_COBALT_COBALT_VIDEO_TEXTURE_TRANSFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_COBALT_COBALT_VIDEO_TEXTURE_TRANSFORM_H_

#include "third_party/blink/renderer/modules/webgl/webgl_extension.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class HTMLVideoElement;
class WebGLRenderingContextBase;
class ExecutionContext;

// COBALT_video_texture_transform
//
// Android Cobalt WebGL extension that exposes the texture-coordinate transform
// (crop scale + offset) required to sample only the *visible* region of a
// decode-to-texture video frame that was bound via OES_EGL_image_external.
//
// This extension is currently specific to Android/AndroidTV builds of Cobalt.
// On Android, MediaCodec and hardware video decoders allocate underlying
// GraphicBuffer/AHardwareBuffer storage aligned to macroblock boundaries
// (e.g. allocating 1280x768 for a 1280x720 stream or 448x256 for 426x240) and
// expose the uncropped surface via GL_TEXTURE_EXTERNAL_OES. The Khronos
// OES_EGL_image_external specification omits an in-band transform query
// (on Android it is provided out-of-band by
// SurfaceTexture.getTransformMatrix()); this extension provides the equivalent
// capability to the Cobalt WebGL pipeline. Non-Android platforms do not exhibit
// macroblock padding artifacts and do not compile or expose this extension.
//
// Lifetime: tied to the parent WebGLRenderingContextBase (Oilpan).
// Threading: main-thread bound (Blink execution context thread).
class CobaltVideoTextureTransform final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  CobaltVideoTextureTransform(WebGLRenderingContextBase*,
                              ExecutionContext*);

  WebGLExtensionName GetName() const override;

  // Returns [scaleX, scaleY, offsetX, offsetY] that maps normalized [0, 1]
  // texture coordinates onto the visible region of |video|'s current frame:
  //
  //   sampled = texcoord * vec2(scaleX, scaleY) + vec2(offsetX, offsetY)
  //
  // Padded axes are inset by 1.0 texel (matching Android SurfaceTexture's
  // getTransformMatrix specification) so bilinear sampling across YUV 4:2:0
  // chroma subsampling boundaries never taps green chroma decoder padding;
  // unpadded axes map with scale=1.0, offset=0.0. Values are in
  // top-left-origin, un-flipped texture space.
  //
  // Returns an empty sequence [] when no valid GPU frame has been bound yet
  // to allow client shaders to suppress drawing until valid frame data and
  // transform matrices are available. Returns identity [1, 1, 0, 0] when a
  // valid frame has no padding.
  Vector<float> getCurrentFrameTextureTransform(HTMLVideoElement* video);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_COBALT_COBALT_VIDEO_TEXTURE_TRANSFORM_H_
