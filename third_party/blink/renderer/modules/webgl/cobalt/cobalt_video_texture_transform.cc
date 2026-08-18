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

#include "third_party/blink/renderer/modules/webgl/cobalt/cobalt_video_texture_transform.h"

#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_texture.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

CobaltVideoTextureTransform::CobaltVideoTextureTransform(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {}

WebGLExtensionName CobaltVideoTextureTransform::GetName() const {
  return kCobaltVideoTextureTransformName;
}

bool CobaltVideoTextureTransform::Supported(
    WebGLRenderingContextBase* context) {
#if BUILDFLAG(IS_ANDROID)
  // Currently specific to Android, where MediaCodec hardware decoders allocate
  // macroblock-aligned GraphicBuffer surfaces sampled via
  // GL_OES_EGL_image_external.
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_OES_EGL_image_external");
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

const char* CobaltVideoTextureTransform::ExtensionName() {
  return "COBALT_video_texture_transform";
}

Vector<float> CobaltVideoTextureTransform::getCurrentFrameTextureTransform(
    HTMLVideoElement* video) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost()) {
    return Vector<float>();
  }
  WebGLRenderingContextBase* context = scoped.Context();
  if (!context) {
    return Vector<float>();
  }

  // Retrieve the exact VideoFrame bound to GL_TEXTURE_EXTERNAL_OES by the
  // preceding transferVideoTexture() call. Only frames successfully bound
  // (with HasSharedImage() == true) are valid for rendering. If no real GPU
  // frame has been bound yet, return EMPTY to signal to the client that the
  // texture is not ready and drawing should be suppressed.
  scoped_refptr<media::VideoFrame> frame;
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  if (WebGLTexture* texture =
          context->texture_units_[context->active_texture_unit_]
              .texture_external_oes_binding_.Get()) {
    frame = texture->GetVideoFrame();
  }
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

  if (!frame || !frame->HasSharedImage()) {
    return Vector<float>();
  }

  const gfx::Size coded = frame->coded_size();
  const gfx::Rect visible = frame->visible_rect();
  const gfx::Size natural = frame->natural_size();
  if (coded.width() <= 0 || coded.height() <= 0) {
    return Vector<float>();
  }

  // Determine effective visible dimensions. At playback start or frame 1 of a
  // resolution shift, frame->visible_rect() may briefly equal coded_size()
  // before SurfaceTexture crop metadata is delivered. In that case, fall back
  // first to frame->natural_size() (which is known immediately from demuxer
  // stream metadata) and then to video->videoWidth() / videoHeight().
  int vis_w = visible.width();
  int vis_x = visible.x();
  if ((vis_w <= 0 || vis_w >= coded.width()) && natural.width() > 0 &&
      natural.width() < coded.width()) {
    vis_w = natural.width();
    vis_x = 0;
  }
  if (video) {
    const int video_w = static_cast<int>(video->videoWidth());
    if ((vis_w <= 0 || vis_w >= coded.width()) && video_w > 0 &&
        video_w < coded.width()) {
      vis_w = video_w;
      vis_x = 0;
    }
  }

  int vis_h = visible.height();
  int vis_y = visible.y();
  if ((vis_h <= 0 || vis_h >= coded.height()) && natural.height() > 0 &&
      natural.height() < coded.height()) {
    vis_h = natural.height();
    vis_y = 0;
  }
  if (video) {
    const int video_h = static_cast<int>(video->videoHeight());
    if ((vis_h <= 0 || vis_h >= coded.height()) && video_h > 0 &&
        video_h < coded.height()) {
      vis_h = video_h;
      vis_y = 0;
    }
  }

  const float coded_w = static_cast<float>(coded.width());
  const float coded_h = static_cast<float>(coded.height());

  // Map texture coordinates [0, 1] onto the visible region using a
  // full 1.0-texel inset on any padded axis, matching Android SurfaceTexture's
  // getTransformMatrix specification: insets by 1.0 texel so that bilinear
  // sampling across YUV 4:2:0 chroma subsampling boundaries never samples into
  // green chroma decoder padding. Axes with no padding (visible extent == coded
  // extent) use the identity mapping.
  float scale_x = 1.0f;
  float offset_x = 0.0f;
  if (vis_w > 0 && vis_w < coded.width()) {
    scale_x = (static_cast<float>(vis_w) - 2.0f) / coded_w;
    offset_x = (static_cast<float>(vis_x) + 1.0f) / coded_w;
  }

  float scale_y = 1.0f;
  float offset_y = 0.0f;
  if (vis_h > 0 && vis_h < coded.height()) {
    scale_y = (static_cast<float>(vis_h) - 2.0f) / coded_h;
    offset_y = (static_cast<float>(vis_y) + 1.0f) / coded_h;
  }

  Vector<float> transform;
  transform.push_back(scale_x);   // scaleX
  transform.push_back(scale_y);   // scaleY
  transform.push_back(offset_x);  // offsetX
  transform.push_back(offset_y);  // offsetY
  return transform;
}

}  // namespace blink
