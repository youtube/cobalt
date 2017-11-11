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

#ifndef STARBOARD_SHARED_WIN32_DECODE_TARGET_INTERNAL_H_
#define STARBOARD_SHARED_WIN32_DECODE_TARGET_INTERNAL_H_

#include <D3d11_1.h>
#include <mfidl.h>
#include <wrl/client.h>

#include "starboard/atomic.h"
#include "starboard/decode_target.h"

struct SbDecodeTargetPrivate {
  SbAtomic32 refcount;

  // Publicly accessible information about the decode target.
  SbDecodeTargetInfo info;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d_texture;

  // EGLSurface is defined as void* in "third_party/angle/include/EGL/egl.h".
  // Use void* directly here to avoid `egl.h` being included broadly.
  void* surface[2];

  SbDecodeTargetPrivate(
      const Microsoft::WRL::ComPtr<ID3D11Device>& d3d_device,
      const Microsoft::WRL::ComPtr<ID3D11VideoDevice1>& video_device,
      const Microsoft::WRL::ComPtr<ID3D11VideoContext>& video_context,
      const Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>&
          video_enumerator,
      const Microsoft::WRL::ComPtr<ID3D11VideoProcessor>& video_processor,
      const Microsoft::WRL::ComPtr<IMFSample>& video_sample,
      const RECT& video_area);
  ~SbDecodeTargetPrivate();

  void AddRef();
  void Release();
};

#endif  // STARBOARD_SHARED_WIN32_DECODE_TARGET_INTERNAL_H_
