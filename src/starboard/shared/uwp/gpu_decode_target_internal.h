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

#ifndef STARBOARD_SHARED_UWP_GPU_DECODE_TARGET_INTERNAL_H_
#define STARBOARD_SHARED_UWP_GPU_DECODE_TARGET_INTERNAL_H_

#include <D3d11_1.h>
#include <mfidl.h>
#include <wrl/client.h>

#include "starboard/atomic.h"
#include "starboard/decode_target.h"
#include "starboard/shared/libvpx_xb1/vpx_xb1_video_decoder.h"
#include "starboard/shared/win32/decode_target_internal.h"

struct GPUDecodeTargetPrivate : public SbDecodeTargetPrivate {
  template <typename T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;

  ComPtr<ID3D11Texture2D> d3d_textures[3];

  // EGLSurface is defined as void* in "third_party/angle/include/EGL/egl.h".
  // Use void* directly here to avoid `egl.h` being included broadly.
  void* surface[3];

  ComPtr<ID3D11Device> d3d_device_;

  // This constructor works properly for Xbox One platform only
  GPUDecodeTargetPrivate(const ComPtr<ID3D11Device>& d3d_device,
                         const vpx_image_t* img);

  ~GPUDecodeTargetPrivate() override;
};

#endif  // STARBOARD_SHARED_UWP_GPU_DECODE_TARGET_INTERNAL_H_
