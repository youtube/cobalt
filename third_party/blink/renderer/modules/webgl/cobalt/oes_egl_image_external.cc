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

#include "third_party/blink/renderer/modules/webgl/cobalt/oes_egl_image_external.h"

#include <array>

#include "build/build_config.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "media/base/video_frame.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_texture.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

OESEGLImageExternal::OESEGLImageExternal(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_OES_EGL_image_external");
}

WebGLExtensionName OESEGLImageExternal::GetName() const {
  return kOESEGLImageExternalName;
}

bool OESEGLImageExternal::Supported(WebGLRenderingContextBase* context) {
#if BUILDFLAG(IS_ANDROID)
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_OES_EGL_image_external");
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

const char* OESEGLImageExternal::ExtensionName() {
  return "OES_EGL_image_external";
}

void OESEGLImageExternal::EGLImageTargetTexture2DOES(
    GLenum target,
    HTMLVideoElement* video,
    ExceptionState& exception_state) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost()) {
    return;
  }

  WebGLRenderingContextBase* context = scoped.Context();
  if (context->isContextLost()) {
    return;
  }

  if (target != GL_TEXTURE_EXTERNAL_OES) {
    context->SynthesizeGLError(GL_INVALID_ENUM, "EGLImageTargetTexture2DOES",
                               "invalid texture target");
    return;
  }

  if (!video) {
    context->SynthesizeGLError(GL_INVALID_VALUE, "EGLImageTargetTexture2DOES",
                               "no video element");
    return;
  }

  auto* execution_context = context->Host()->GetTopExecutionContext();
  if (!execution_context) {
    return;
  }

  if (!context->ValidateHTMLVideoElement(execution_context->GetSecurityOrigin(),
                                         "EGLImageTargetTexture2DOES", video,
                                         exception_state)) {
    return;
  }

  WebGLTexture* texture =
      context->ValidateTextureBinding("EGLImageTargetTexture2DOES", target);
  if (!texture) {
    return;
  }

  auto* wmp = video->GetWebMediaPlayer();
  if (!wmp) {
    return;
  }

  scoped_refptr<media::VideoFrame> media_video_frame =
      wmp->GetCurrentFrameThenUpdate();
  if (!media_video_frame) {
    return;
  }

  if (!media_video_frame->HasSharedImage()) {
    return;
  }

  auto* client_shared_image = media_video_frame->shared_image().get();
  if (!client_shared_image) {
    return;
  }

  viz::SharedImageFormat format = client_shared_image->format();
  if (format.is_multi_plane() && !format.PrefersExternalSampler()) {
    context->SynthesizeGLError(
        GL_INVALID_OPERATION, "EGLImageTargetTexture2DOES",
        "video frame format is not compatible with external texture");
    return;
  }

  if (client_shared_image->GetTextureTarget() != GL_TEXTURE_EXTERNAL_OES) {
    context->SynthesizeGLError(GL_INVALID_OPERATION,
                               "EGLImageTargetTexture2DOES",
                               "video frame is not an external texture");
    return;
  }

  gpu::gles2::GLES2Interface* gl = context->ContextGL();

  if (texture->GetMailbox() == client_shared_image->mailbox()) {
    gl->WaitSyncTokenCHROMIUM(
        media_video_frame->acquire_sync_token().GetConstData());
    gl->BindTexture(GL_TEXTURE_EXTERNAL_OES, texture->Object());
    texture->UpdateUnderlyingObject(texture->Object(), media_video_frame,
                                    /*has_shared_image_access=*/true);
    return;
  }

  struct TextureParameter {
    GLenum pname;
    GLint value;
  };
  std::array<TextureParameter, 4> preserved_parameters = {{
      {GL_TEXTURE_MIN_FILTER, 0},
      {GL_TEXTURE_MAG_FILTER, 0},
      {GL_TEXTURE_WRAP_S, 0},
      {GL_TEXTURE_WRAP_T, 0},
  }};

  const bool has_object = texture->Object() != 0;

  if (has_object) {
    gl->BindTexture(GL_TEXTURE_EXTERNAL_OES, texture->Object());
    for (auto& param : preserved_parameters) {
      gl->GetTexParameteriv(GL_TEXTURE_EXTERNAL_OES, param.pname, &param.value);
    }
  }

  gl->WaitSyncTokenCHROMIUM(
      media_video_frame->acquire_sync_token().GetConstData());

  GLuint new_texture_id = gl->CreateAndTexStorage2DSharedImageCHROMIUM(
      client_shared_image->mailbox().name);
  if (!new_texture_id) {
    context->SynthesizeGLError(GL_INVALID_OPERATION,
                               "EGLImageTargetTexture2DOES",
                               "failed to create shared image texture");
    return;
  }

  gl->BeginSharedImageAccessDirectCHROMIUM(
      new_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

  gl->BindTexture(GL_TEXTURE_EXTERNAL_OES, new_texture_id);

  if (has_object) {
    for (const auto& param : preserved_parameters) {
      gl->TexParameteri(GL_TEXTURE_EXTERNAL_OES, param.pname, param.value);
    }
  }

  texture->UpdateUnderlyingObject(new_texture_id, media_video_frame,
                                  /*has_shared_image_access=*/true);
}

}  // namespace blink
