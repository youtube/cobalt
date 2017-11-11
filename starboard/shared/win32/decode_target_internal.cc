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

#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/win32/error_utils.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "third_party/angle/include/GLES2/gl2.h"
#include "third_party/angle/include/GLES2/gl2ext.h"

namespace {

using Microsoft::WRL::ComPtr;
using starboard::shared::win32::CheckResult;

// {3C3A43AB-C69B-46C9-AA8D-B0CFFCD4596D}
const GUID kCobaltNv12BindChroma = {
    0x3c3a43ab,
    0xc69b,
    0x46c9,
    {0xaa, 0x8d, 0xb0, 0xcf, 0xfc, 0xd4, 0x59, 0x6d}};

// {C62BF18D-B5EE-46B1-9C31-F61BD8AE3B0D}
const GUID kCobaltDxgiBuffer = {
    0Xc62bf18d,
    0Xb5ee,
    0X46b1,
    {0X9c, 0X31, 0Xf6, 0X1b, 0Xd8, 0Xae, 0X3b, 0X0d}};

ComPtr<ID3D11Texture2D> AllocateTexture(
    const ComPtr<ID3D11Device>& d3d_device, int width, int height) {
  ComPtr<ID3D11Texture2D> texture;
  D3D11_TEXTURE2D_DESC texture_desc = {};
  texture_desc.Width = width;
  texture_desc.Height = height;
  texture_desc.MipLevels = 1;
  texture_desc.ArraySize = 1;
  texture_desc.Format = DXGI_FORMAT_NV12;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.SampleDesc.Quality = 0;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.BindFlags =
      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  CheckResult(d3d_device->CreateTexture2D(&texture_desc, nullptr,
      texture.GetAddressOf()));
  return texture;
}

ComPtr<ID3D11Texture2D> CreateTexture(
    const ComPtr<ID3D11Device>& d3d_device,
    const ComPtr<ID3D11VideoDevice1>& video_device,
    const ComPtr<ID3D11VideoContext>& video_context,
    const ComPtr<ID3D11VideoProcessorEnumerator>& video_enumerator,
    const ComPtr<ID3D11VideoProcessor>& video_processor,
    const ComPtr<IMFSample>& video_sample, const RECT& video_area) {
  ComPtr<ID3D11Texture2D> texture = AllocateTexture(d3d_device,
      video_area.right, video_area.bottom);

  ComPtr<IMFMediaBuffer> media_buffer;
  CheckResult(video_sample->GetBufferByIndex(0, media_buffer.GetAddressOf()));

  ComPtr<IMFDXGIBuffer> dxgi_buffer;
  CheckResult(media_buffer.As(&dxgi_buffer));

  ComPtr<ID3D11Texture2D> input_texture;
  CheckResult(dxgi_buffer->GetResource(IID_PPV_ARGS(&input_texture)));

  // The VideoProcessor needs to know what subset of the decoded
  // frame contains active pixels that should be displayed to the user.
  video_context->VideoProcessorSetStreamSourceRect(
      video_processor.Get(), 0, TRUE, &video_area);

  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_desc = {};
  input_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  input_desc.Texture2D.MipSlice = 0;
  dxgi_buffer->GetSubresourceIndex(&input_desc.Texture2D.ArraySlice);

  ComPtr<ID3D11VideoProcessorInputView> input_view;
  CheckResult(video_device->CreateVideoProcessorInputView(
      input_texture.Get(), video_enumerator.Get(), &input_desc,
      input_view.GetAddressOf()));

  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_desc = {};
  output_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
  output_desc.Texture2D.MipSlice = 0;

  ComPtr<ID3D11VideoProcessorOutputView> output_view;
  CheckResult(video_device->CreateVideoProcessorOutputView(
      texture.Get(), video_enumerator.Get(), &output_desc,
      output_view.GetAddressOf()));

  // We have a single video stream, which is enabled for display.
  D3D11_VIDEO_PROCESSOR_STREAM stream_info = {};
  stream_info.Enable = TRUE;
  stream_info.pInputSurface = input_view.Get();
  CheckResult(video_context->VideoProcessorBlt(
      video_processor.Get(), output_view.Get(), 0, 1, &stream_info));

  return texture;
}

}  // namespace

SbDecodeTargetPrivate::SbDecodeTargetPrivate(
    const ComPtr<ID3D11Device>& d3d_device,
    const ComPtr<ID3D11VideoDevice1>& video_device,
    const ComPtr<ID3D11VideoContext>& video_context,
    const ComPtr<ID3D11VideoProcessorEnumerator>& video_enumerator,
    const ComPtr<ID3D11VideoProcessor>& video_processor,
    const ComPtr<IMFSample>& video_sample, const RECT& video_area)
    : refcount(1) {
  SbMemorySet(&info, 0, sizeof(info));

  d3d_texture = CreateTexture(d3d_device, video_device,
      video_context, video_enumerator, video_processor, video_sample,
      video_area);

  info.format = kSbDecodeTargetFormat2PlaneYUVNV12;
  info.is_opaque = true;

  info.width = video_area.right;
  info.height = video_area.bottom;

  SbDecodeTargetInfoPlane* planeY = &(info.planes[kSbDecodeTargetPlaneY]);
  SbDecodeTargetInfoPlane* planeUV = &(info.planes[kSbDecodeTargetPlaneUV]);

  planeY->width = info.width;
  planeY->height = info.height;
  planeY->content_region.left = 0;
  planeY->content_region.top = info.height;
  planeY->content_region.right = info.width;
  planeY->content_region.bottom = 0;

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
  HRESULT hr = d3d_texture->SetPrivateData(kCobaltNv12BindChroma, 0, nullptr);
  SB_DCHECK(SUCCEEDED(hr));

  surface[0] = eglCreatePbufferFromClientBuffer(display, EGL_D3D_TEXTURE_ANGLE,
                                                d3d_texture.Get(), config,
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
  hr = d3d_texture->SetPrivateData(kCobaltNv12BindChroma, 1, &bind_chroma);
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
                                                d3d_texture.Get(), config,
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

  hr = d3d_texture->SetPrivateData(kCobaltDxgiBuffer, 0, nullptr);
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

void SbDecodeTargetPrivate::AddRef() {
  SbAtomicBarrier_Increment(&refcount, 1);
}

void SbDecodeTargetPrivate::Release() {
  int new_count = SbAtomicBarrier_Increment(&refcount, -1);
  SB_DCHECK(new_count >= 0);
  if (new_count == 0) {
    delete this;
  }
}

void SbDecodeTargetRelease(SbDecodeTarget decode_target) {
  if (SbDecodeTargetIsValid(decode_target)) {
    decode_target->Release();
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
