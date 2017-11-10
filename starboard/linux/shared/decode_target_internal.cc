// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/linux/shared/decode_target_internal.h"

#if SB_HAS(GLES2)
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "starboard/shared/gles/gl_call.h"
#endif

#include "starboard/decode_target.h"

SbDecodeTargetPrivate::Data::~Data() {
#if SB_HAS(GLES2)
  glDeleteTextures(1, &info.planes[0].texture);
  SB_DCHECK(glGetError() == GL_NO_ERROR);
#endif
}

namespace starboard {
namespace shared {

namespace {

using starboard::player::filter::CpuVideoFrame;

struct CreateParams {
  SbDecodeTarget decode_target_out;
  scoped_refptr<CpuVideoFrame> frame;
};

#if SB_HAS(GLES2)
void CreateWithContextRunner(void* context) {
  CreateParams* params = static_cast<CreateParams*>(context);

  SB_DCHECK(params->frame);
  SB_DCHECK(params->frame->format() == CpuVideoFrame::kYV12);
  static const SbDecodeTargetFormat format = kSbDecodeTargetFormat3PlaneYUVI420;
  static const int plane_count = 3;

  if (!SbDecodeTargetIsValid(params->decode_target_out)) {
    params->decode_target_out = new SbDecodeTargetPrivate;
    params->decode_target_out->data = new SbDecodeTargetPrivate::Data;
    params->decode_target_out->data->info.format = format;

    GLuint textures[3];
    GL_CALL(glGenTextures(plane_count, textures));
    for (int plane_index = 0; plane_index < plane_count; plane_index++) {
      params->decode_target_out->data->info.is_opaque = true;

      SbDecodeTargetInfoPlane& plane =
          params->decode_target_out->data->info.planes[plane_index];
      plane.texture = textures[plane_index];
      plane.gl_texture_target = GL_TEXTURE_2D;
      plane.content_region.left = 0.0f;
      plane.content_region.top = 0.0f;
      plane.width = 0;
      plane.height = 0;
    }
  }

  SbDecodeTargetInfo& target_info = params->decode_target_out->data->info;

  for (int plane_index = 0; plane_index < plane_count; plane_index++) {
    const CpuVideoFrame::Plane& video_frame_plane =
        params->frame->GetPlane(plane_index);

    GL_CALL(glBindTexture(
        GL_TEXTURE_2D,
        params->decode_target_out->data->info.planes[plane_index].texture));
    GL_CALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT,
                          video_frame_plane.pitch_in_bytes));
    if (target_info.planes[plane_index].width == video_frame_plane.width &&
        target_info.planes[plane_index].height == video_frame_plane.height) {
      // No need to reallocate texture object, only update pixels.
      GL_CALL(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, video_frame_plane.width,
                              video_frame_plane.height, GL_ALPHA,
                              GL_UNSIGNED_BYTE, video_frame_plane.data));

    } else {
      // As part of texture initialization, explicitly specify all parameters.
      GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
      GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
      GL_CALL(
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
      GL_CALL(
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

      GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, video_frame_plane.width,
                           video_frame_plane.height, 0, GL_ALPHA,
                           GL_UNSIGNED_BYTE, video_frame_plane.data));
    }

    GL_CALL(glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

    // Set sizes and regions.
    target_info.planes[plane_index].content_region.right =
        static_cast<float>(video_frame_plane.width);
    target_info.planes[plane_index].content_region.bottom =
        static_cast<float>(video_frame_plane.height);
    target_info.planes[plane_index].width = video_frame_plane.width;
    target_info.planes[plane_index].height = video_frame_plane.height;
  }

  target_info.width = params->frame->width();
  target_info.height = params->frame->height();
}
#endif

}  // namespace

SbDecodeTarget DecodeTargetCreate(
    SbDecodeTargetGraphicsContextProvider* provider,
    scoped_refptr<CpuVideoFrame> frame,
    SbDecodeTarget decode_target) {
  CreateParams params;
  params.decode_target_out = decode_target;
  params.frame = frame;

#if SB_HAS(GLES2)
  if (!provider) {
    if (SbDecodeTargetIsValid(params.decode_target_out)) {
      // Should the decode target have been created and the GLES context been
      // somehow lost, it is released without the context
      SbDecodeTargetRelease(params.decode_target_out);
    }
    params.decode_target_out = kSbDecodeTargetInvalid;
  } else {
    SbDecodeTargetRunInGlesContext(provider, &CreateWithContextRunner, &params);
  }
#endif

  return params.decode_target_out;
}

void DecodeTargetRelease(SbDecodeTargetGraphicsContextProvider*
                             decode_target_graphics_context_provider,
                         SbDecodeTarget decode_target) {
#if SB_HAS(GLES2)
  SbDecodeTargetReleaseInGlesContext(decode_target_graphics_context_provider,
                                     decode_target);
#endif
}

SbDecodeTarget DecodeTargetCopy(SbDecodeTarget decode_target) {
  SbDecodeTarget out_decode_target = new SbDecodeTargetPrivate;
  out_decode_target->data = decode_target->data;

  return out_decode_target;
}

}  // namespace shared
}  // namespace starboard
