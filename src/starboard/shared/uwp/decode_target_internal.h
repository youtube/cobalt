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

#ifndef STARBOARD_SHARED_UWP_DECODE_TARGET_INTERNAL_H_
#define STARBOARD_SHARED_UWP_DECODE_TARGET_INTERNAL_H_

#include <D3d11_1.h>
#include <mfidl.h>
#include <wrl/client.h>

#include "starboard/atomic.h"
#include "starboard/decode_target.h"
#include "starboard/shared/libvpx_xb1/vpx_xb1_video_decoder.h"

struct SbDecodeTargetPrivate {
  template <typename T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;

  SbAtomic32 refcount;

  // Publicly accessible information about the decode target.
  SbDecodeTargetInfo info;

  ComPtr<ID3D11Texture2D> d3d_texture;

  // EGLSurface is defined as void* in "third_party/angle/include/EGL/egl.h".
  // Use void* directly here to avoid `egl.h` being included broadly.
  void* surface[2];

  ComPtr<ID3D11Texture2D> d3d_textures_[3];
  void* surface_[3] = {};
  ComPtr<ID3D11Device> d3d_device_ = nullptr;

  SbDecodeTargetPrivate(
      const ComPtr<ID3D11Device>& d3d_device,
      const ComPtr<ID3D11VideoDevice1>& video_device,
      const ComPtr<ID3D11VideoContext>& video_context,
      const ComPtr<ID3D11VideoProcessorEnumerator>& video_enumerator,
      const ComPtr<ID3D11VideoProcessor>& video_processor,
      const ComPtr<IMFSample>& video_sample,
      const RECT& video_area);

  // This constructor works properly for Xbox One platform only
  SbDecodeTargetPrivate(const ComPtr<ID3D11Device>& d3d_device,
                        const vpx_image_t* img,
                        BYTE* frame_buf_nv12);

  ~SbDecodeTargetPrivate();

  // Update the existing texture with the given video_sample's data.
  // If the current object is not compatible with the new video_sample, then
  // this will return false, and the caller should just create a new
  // decode target for the sample.
  bool Update(const ComPtr<ID3D11Device>& d3d_device,
              const ComPtr<ID3D11VideoDevice1>& video_device,
              const ComPtr<ID3D11VideoContext>& video_context,
              const ComPtr<ID3D11VideoProcessorEnumerator>& video_enumerator,
              const ComPtr<ID3D11VideoProcessor>& video_processor,
              const ComPtr<IMFSample>& video_sample,
              const RECT& video_area);
  void CreateGLSurfacesNV12(const ComPtr<ID3D11Texture2D>& texture,
                            int display_width,
                            int display_height);
  void AddRef();
  void Release();
};

#endif  // STARBOARD_SHARED_UWP_DECODE_TARGET_INTERNAL_H_
