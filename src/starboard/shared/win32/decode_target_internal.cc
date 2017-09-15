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

#include "starboard/shared/win32/decode_target_internal.h"

#include <D3D11.h>
#include <Mfidl.h>
#include <Mfobjects.h>
#include <wrl\client.h>  // For ComPtr.

#include "starboard/configuration.h"
#include "starboard/decode_target.h"
#include "starboard/memory.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/media_common.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "third_party/angle/include/GLES2/gl2.h"
#include "third_party/angle/include/GLES2/gl2ext.h"

using Microsoft::WRL::ComPtr;
using starboard::shared::win32::VideoFramePtr;
using starboard::shared::win32::CheckResult;

// {3C3A43AB-C69B-46C9-AA8D-B0CFFCD4596D}
static const GUID kCobaltNv12BindChroma = {
    0x3c3a43ab,
    0xc69b,
    0x46c9,
    {0xaa, 0x8d, 0xb0, 0xcf, 0xfc, 0xd4, 0x59, 0x6d}};

// {C62BF18D-B5EE-46B1-9C31-F61BD8AE3B0D}
static const GUID kCobaltDxgiBuffer = {
    0Xc62bf18d,
    0Xb5ee,
    0X46b1,
    {0X9c, 0X31, 0Xf6, 0X1b, 0Xd8, 0Xae, 0X3b, 0X0d}};

SbDecodeTargetPrivate::SbDecodeTargetPrivate(VideoFramePtr f) : frame(f) {
  SbMemorySet(&info, 0, sizeof(info));

  ComPtr<IMFSample> sample = static_cast<IMFSample*>(frame->native_texture());

  ComPtr<IMFMediaBuffer> media_buffer;
  HRESULT hr = sample->GetBufferByIndex(0, &media_buffer);
  CheckResult(hr);

  ComPtr<IMFDXGIBuffer> dxgi_buffer;
  hr = media_buffer.As(&dxgi_buffer);
  CheckResult(hr);
  SB_DCHECK(dxgi_buffer.Get());

  ComPtr<ID3D11Texture2D> d3texture;
  hr = dxgi_buffer->GetResource(IID_PPV_ARGS(&d3texture));
  CheckResult(hr);

  UINT array_index;
  dxgi_buffer->GetSubresourceIndex(&array_index);

  info.format = kSbDecodeTargetFormat2PlaneYUVNV12;
  info.is_opaque = true;

  D3D11_TEXTURE2D_DESC texture_desc;
  d3texture->GetDesc(&texture_desc);
  info.width = texture_desc.Width;
  info.height = texture_desc.Height;

  SbDecodeTargetInfoPlane* planeY = &(info.planes[kSbDecodeTargetPlaneY]);
  SbDecodeTargetInfoPlane* planeUV = &(info.planes[kSbDecodeTargetPlaneUV]);

  planeY->width = info.width;
  planeY->height = info.height;
  planeY->content_region.left = 0;
  planeY->content_region.top = info.height;
  planeY->content_region.right = frame->width();
  planeY->content_region.bottom = info.height - frame->height();

  planeUV->width = info.width / 2;
  planeUV->height = info.height / 2;
  planeUV->content_region.left = planeY->content_region.left / 2;
  planeUV->content_region.top = planeY->content_region.top / 2;
  planeUV->content_region.right = planeY->content_region.right / 2;
  planeUV->content_region.bottom = planeY->content_region.bottom / 2;

  EGLint luma_texture_attributes[] = {EGL_WIDTH,
                                      static_cast<EGLint>(info.width),
                                      EGL_HEIGHT,
                                      static_cast<EGLint>(info.height),
                                      EGL_TEXTURE_TARGET,
                                      EGL_TEXTURE_2D,
                                      EGL_TEXTURE_FORMAT,
                                      EGL_TEXTURE_RGBA,
                                      EGL_NONE};

  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  EGLConfig config;
  EGLint attribute_list[] = {
    EGL_SURFACE_TYPE,  // this must be first
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
    EGL_NONE
  };

  EGLint num_configs;
  bool ok = eglChooseConfig(
      display, attribute_list, &config, 1, &num_configs);
  SB_DCHECK(ok);
  SB_DCHECK(num_configs == 1);

  GLuint gl_textures[2] = {0};
  glGenTextures(2, gl_textures);
  SB_DCHECK(glGetError() == GL_NO_ERROR);

  // This tells ANGLE that the texture it creates should draw
  // the luma channel on R8.
  hr = d3texture->SetPrivateData(kCobaltNv12BindChroma, 0, nullptr);
  SB_DCHECK(SUCCEEDED(hr));

  // This lets ANGLE find out the subresource index / texture array index
  // to use.
  // Note: No AddRef here, since we clear this private data below.
  hr = d3texture->SetPrivateData(kCobaltDxgiBuffer, sizeof(IMFDXGIBuffer*),
                                 dxgi_buffer.GetAddressOf());
  SB_DCHECK(SUCCEEDED(hr));

  surface[0] = eglCreatePbufferFromClientBuffer(display, EGL_D3D_TEXTURE_ANGLE,
                                                d3texture.Get(), config,
                                                luma_texture_attributes);

  SB_DCHECK(surface[0] != EGL_NO_SURFACE);

  glBindTexture(GL_TEXTURE_2D, gl_textures[0]);
  SB_DCHECK(glGetError() == GL_NO_ERROR);

  ok = eglBindTexImage(display, surface[0], EGL_BACK_BUFFER);
  SB_DCHECK(ok);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  planeY->texture = gl_textures[0];
  planeY->gl_texture_target = GL_TEXTURE_2D;
  planeY->gl_texture_format = GL_RED_EXT;

  // This tells ANGLE that the texture it creates should draw
  // the chroma channel on R8G8.
  bool bind_chroma = true;
  hr = d3texture->SetPrivateData(kCobaltNv12BindChroma, 1, &bind_chroma);
  SB_DCHECK(SUCCEEDED(hr));

  EGLint chroma_texture_attributes[] = {
      EGL_WIDTH,
      static_cast<EGLint>(info.width) / 2,
      EGL_HEIGHT,
      static_cast<EGLint>(info.height) / 2,
      EGL_TEXTURE_TARGET,
      EGL_TEXTURE_2D,
      EGL_TEXTURE_FORMAT,
      EGL_TEXTURE_RGBA,
      EGL_NONE};
  surface[1] = eglCreatePbufferFromClientBuffer(display, EGL_D3D_TEXTURE_ANGLE,
                                                d3texture.Get(), config,
                                                chroma_texture_attributes);

  SB_DCHECK(surface[1] != EGL_NO_SURFACE);

  glBindTexture(GL_TEXTURE_2D, gl_textures[1]);
  SB_DCHECK(glGetError() == GL_NO_ERROR);

  ok = eglBindTexImage(display, surface[1], EGL_BACK_BUFFER);
  SB_DCHECK(ok);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  planeUV->texture = gl_textures[1];
  planeUV->gl_texture_target = GL_TEXTURE_2D;
  planeUV->gl_texture_format = GL_RG_EXT;

  hr = d3texture->SetPrivateData(kCobaltDxgiBuffer, 0, nullptr);
  SB_DCHECK(SUCCEEDED(hr));
}

SbDecodeTargetPrivate::~SbDecodeTargetPrivate() {
  glDeleteTextures(1, &(info.planes[kSbDecodeTargetPlaneY].texture));
  glDeleteTextures(1, &(info.planes[kSbDecodeTargetPlaneUV].texture));

  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  eglReleaseTexImage(display, surface[0], EGL_BACK_BUFFER);
  eglDestroySurface(display, surface[0]);

  eglReleaseTexImage(display, surface[1], EGL_BACK_BUFFER);
  eglDestroySurface(display, surface[1]);
}

void SbDecodeTargetRelease(SbDecodeTarget decode_target) {
  if (SbDecodeTargetIsValid(decode_target)) {
    delete decode_target;
  }
}

SbDecodeTargetFormat SbDecodeTargetGetFormat(SbDecodeTarget decode_target) {
  // Note that kSbDecodeTargetFormat2PlaneYUVNV12 represents DXGI_FORMAT_NV12.
  SB_DCHECK(kSbDecodeTargetFormat2PlaneYUVNV12 ==
            decode_target->info.format);
  return decode_target->info.format;
}

bool SbDecodeTargetGetInfo(SbDecodeTarget decode_target,
                           SbDecodeTargetInfo* out_info) {
  if (!out_info || !SbMemoryIsZero(out_info, sizeof(*out_info))) {
    SB_DCHECK(false) << "out_info must be zeroed out.";
    return false;
  }
  SbMemoryCopy(out_info, &decode_target->info, sizeof(*out_info));
  return true;
}
