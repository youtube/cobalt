// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/blittergles/blitter_internal.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <memory>

#include "starboard/common/log.h"
#include "starboard/common/optional.h"
#include "starboard/memory.h"
#include "starboard/once.h"
#include "starboard/shared/gles/gl_call.h"

namespace starboard {
namespace shared {
namespace blittergles {

namespace {
// Keep track of a global device registry that will be accessed by calls to
// create/destroy devices.
SbBlitterDeviceRegistry* s_device_registry = NULL;
SbOnceControl s_device_registry_once_control = SB_ONCE_INITIALIZER;

void EnsureDeviceRegistryInitialized() {
  s_device_registry = new SbBlitterDeviceRegistry();
  s_device_registry->default_device = NULL;
}

void SetChannelOffsets(SbBlitterPixelDataFormat format, int (&channel)[4]) {
  switch (format) {
    case kSbBlitterPixelDataFormatRGBA8: {
      channel[0] = 3;
      channel[1] = 2;
      channel[2] = 1;
      channel[3] = 0;
      break;
    }
    case kSbBlitterPixelDataFormatARGB8: {
      channel[0] = 0;
      channel[1] = 3;
      channel[2] = 2;
      channel[3] = 1;
      break;
    }
    case kSbBlitterPixelDataFormatBGRA8: {
      channel[0] = 3;
      channel[1] = 0;
      channel[2] = 1;
      channel[3] = 2;
      break;
    }
    default: {
      SB_DLOG(ERROR) << ": Unexpected pixel data format.";
      SB_NOTREACHED();
    }
  }
}

void SwizzlePixels(SbBlitterPixelDataFormat in_format,
                   SbBlitterPixelDataFormat out_format,
                   int pitch_in_bytes,
                   int height,
                   uint8_t* data) {
  int index, in_channel[4], out_channel[4];
  uint8_t r_channel, g_channel, b_channel, a_channel;
  SetChannelOffsets(in_format, in_channel);
  SetChannelOffsets(out_format, out_channel);
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < pitch_in_bytes; j += 4) {
      index = pitch_in_bytes * i + j;
      r_channel = data[index + in_channel[0]];
      g_channel = data[index + in_channel[1]];
      b_channel = data[index + in_channel[2]];
      a_channel = data[index + in_channel[3]];
      data[index + out_channel[0]] = r_channel;
      data[index + out_channel[1]] = g_channel;
      data[index + out_channel[2]] = b_channel;
      data[index + out_channel[3]] = a_channel;
    }
  }
}

starboard::optional<GLuint> ComputeTextureHandle(SbBlitterSurface surface) {
  GLuint texture_handle = 0;
  glGenTextures(1, &texture_handle);
  if (texture_handle == 0) {
    SB_DLOG(ERROR) << ": Error creating new texture.";
    return starboard::nullopt;
  }
  glBindTexture(GL_TEXTURE_2D, texture_handle);
  if (glGetError() != GL_NO_ERROR) {
    GL_CALL(glDeleteTextures(1, &texture_handle));
    SB_DLOG(ERROR) << ": Error binding new texture.";
    return starboard::nullopt;
  }
  GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

  GLint pixel_format =
      surface->info.format == kSbBlitterSurfaceFormatRGBA8 ? GL_RGBA : GL_ALPHA;
  GL_CALL(glPixelStorei(GL_PACK_ALIGNMENT, 1));
  glTexImage2D(GL_TEXTURE_2D, 0, pixel_format, surface->info.width,
               surface->info.height, 0, pixel_format, GL_UNSIGNED_BYTE,
               surface->data);
  if (glGetError() != GL_NO_ERROR) {
    GL_CALL(glDeleteTextures(1, &texture_handle));
    SB_DLOG(ERROR) << ": Error allocating new texture backing.";
    return starboard::nullopt;
  }

  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
  return texture_handle;
}

}  // namespace

const EGLint kContextAttributeList[] = {EGL_CONTEXT_CLIENT_VERSION, 2,
                                        EGL_NONE};

const EGLint kConfigAttributeList[] = {EGL_RED_SIZE,
                                       8,
                                       EGL_GREEN_SIZE,
                                       8,
                                       EGL_BLUE_SIZE,
                                       8,
                                       EGL_ALPHA_SIZE,
                                       8,
                                       EGL_BUFFER_SIZE,
                                       32,
                                       EGL_SURFACE_TYPE,
                                       EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                                       EGL_COLOR_BUFFER_TYPE,
                                       EGL_RGB_BUFFER,
                                       EGL_CONFORMANT,
                                       EGL_OPENGL_ES2_BIT,
                                       EGL_RENDERABLE_TYPE,
                                       EGL_OPENGL_ES2_BIT,
                                       EGL_NONE};

SbBlitterDeviceRegistry* GetBlitterDeviceRegistry() {
  SbOnce(&s_device_registry_once_control, &EnsureDeviceRegistryInitialized);

  return s_device_registry;
}

void ChangeDataFormat(SbBlitterPixelDataFormat in_format,
                      SbBlitterPixelDataFormat out_format,
                      int pitch_in_bytes,
                      int height,
                      void* data) {
  uint8_t* data_bytes = static_cast<uint8_t*>(data);
  std::unique_ptr<uint8_t[]> temp_array(new uint8_t[pitch_in_bytes]);
  for (int i = 0; i < height / 2; ++i) {
    uint8_t* current_row = data_bytes + i * pitch_in_bytes;
    uint8_t* flip_row = data_bytes + pitch_in_bytes * (height - 1 - i);
    SbMemoryCopy(temp_array.get(), current_row, pitch_in_bytes);
    SbMemoryCopy(current_row, flip_row, pitch_in_bytes);
    SbMemoryCopy(flip_row, temp_array.get(), pitch_in_bytes);
  }

  if (in_format != out_format && in_format != kSbBlitterPixelDataFormatA8) {
    SwizzlePixels(in_format, out_format, pitch_in_bytes, height, data_bytes);
  }
}

}  // namespace blittergles
}  // namespace shared
}  // namespace starboard

bool SbBlitterSurfacePrivate::EnsureInitialized() {
  if (color_texture_handle != 0) {
    return true;
  }

  starboard::optional<GLuint> texture =
      starboard::shared::blittergles::ComputeTextureHandle(this);
  color_texture_handle = texture ? *texture : 0;
  if (texture && data != NULL) {
    SbMemoryDeallocate(data);
    data = NULL;
  }

  return texture.has_engaged();
}
