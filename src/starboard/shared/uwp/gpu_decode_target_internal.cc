// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "starboard/shared/uwp/gpu_decode_target_internal.h"

#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/win32/error_utils.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "third_party/angle/include/GLES2/gl2.h"
#include "third_party/angle/include/GLES2/gl2ext.h"

using Microsoft::WRL::ComPtr;

GPUDecodeTargetPrivate::GPUDecodeTargetPrivate(
    const ComPtr<ID3D11Device>& d3d_device,
    const vpx_image_t* vpx_image)
    : d3d_device_(d3d_device) {
  GLuint gl_textures_yuv[3] = {};
  ComPtr<ID3D11Device1> device1;
  HRESULT hr = d3d_device.As(&device1);
  SB_DCHECK(SUCCEEDED(hr));
  if (vpx_image->bit_depth == 8) {
    info.format = kSbDecodeTargetFormat3PlaneYUVI420;
  } else {
    SB_DCHECK(vpx_image->bit_depth == 10);
    info.format = kSbDecodeTargetFormat3Plane10BitYUVI420;
  }
  info.is_opaque = true;
  info.width = vpx_image->d_w;
  info.height = vpx_image->d_h;
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLConfig config;
  EGLint attribute_list[] = {EGL_SURFACE_TYPE,  // this must be first
                             EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                             EGL_RED_SIZE,
                             8,
                             EGL_GREEN_SIZE,
                             8,
                             EGL_BLUE_SIZE,
                             8,
                             EGL_ALPHA_SIZE,
                             8,
                             EGL_BIND_TO_TEXTURE_RGBA,
                             EGL_TRUE,
                             EGL_RENDERABLE_TYPE,
                             EGL_OPENGL_ES2_BIT,
                             EGL_NONE};
  EGLint num_configs;
  bool ok = eglChooseConfig(display, attribute_list, &config, 1, &num_configs);
  SB_DCHECK(ok);
  glGenTextures(3, gl_textures_yuv);
  SB_DCHECK(glGetError() == GL_NO_ERROR);
  for (int i = 0; i < 3; ++i) {
    SB_DCHECK(vpx_image->dx_texture_handle[i] != 0);
    hr = device1->OpenSharedResource1(vpx_image->dx_texture_handle[i],
                                      IID_PPV_ARGS(&d3d_textures[i]));
    SB_DCHECK(SUCCEEDED(hr));
    const int w = (i == 0) ? info.width : info.width / 2;
    const int h = (i == 0) ? info.height : info.height / 2;
    EGLint texture_attributes[] = {EGL_WIDTH,
                                   static_cast<EGLint>(w),
                                   EGL_HEIGHT,
                                   static_cast<EGLint>(h),
                                   EGL_TEXTURE_TARGET,
                                   EGL_TEXTURE_2D,
                                   EGL_TEXTURE_FORMAT,
                                   EGL_TEXTURE_RGBA,
                                   EGL_NONE};
    surface[i] = eglCreatePbufferFromClientBuffer(
        display, EGL_D3D_TEXTURE_ANGLE, d3d_textures[i].Get(), config,
        texture_attributes);
    SB_DCHECK(glGetError() == GL_NO_ERROR && surface[i] != EGL_NO_SURFACE);
    glBindTexture(GL_TEXTURE_2D, gl_textures_yuv[i]);
    SB_DCHECK(glGetError() == GL_NO_ERROR);
    ok = eglBindTexImage(display, surface[i], EGL_BACK_BUFFER);
    SB_DCHECK(ok);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    SbDecodeTargetInfoPlane* plane = &info.planes[i];
    plane->width = w;
    plane->height = h;
    plane->content_region.left = 0;
    plane->content_region.bottom = 0;
    plane->content_region.top = plane->height;
    plane->content_region.right = plane->width;
    plane->texture = gl_textures_yuv[i];
    plane->gl_texture_target = GL_TEXTURE_2D;
    plane->gl_texture_format = GL_RED_EXT;
  }
}

GPUDecodeTargetPrivate::~GPUDecodeTargetPrivate() {
  glDeleteTextures(1, &(info.planes[kSbDecodeTargetPlaneY].texture));
  glDeleteTextures(1, &(info.planes[kSbDecodeTargetPlaneU].texture));
  glDeleteTextures(1, &(info.planes[kSbDecodeTargetPlaneV].texture));

  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  eglReleaseTexImage(display, surface[0], EGL_BACK_BUFFER);
  eglDestroySurface(display, surface[0]);

  eglReleaseTexImage(display, surface[1], EGL_BACK_BUFFER);
  eglDestroySurface(display, surface[1]);

  eglReleaseTexImage(display, surface[2], EGL_BACK_BUFFER);
  eglDestroySurface(display, surface[2]);
}
