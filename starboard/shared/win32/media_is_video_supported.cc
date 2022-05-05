// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/media_support_internal.h"

#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <wrl/client.h>

#include "starboard/configuration_constants.h"
#include "starboard/shared/starboard/media/media_util.h"

using ::starboard::shared::starboard::media::MimeType;

namespace {

#if SB_API_VERSION >= SB_RUNTIME_CONFIGS_VERSION || \
    defined(SB_HAS_MEDIA_WEBM_VP9_SUPPORT)
// Cache the VP9 support status since the check may be expensive.
enum Vp9Support { kVp9SupportUnknown, kVp9SupportYes, kVp9SupportNo };
Vp9Support s_vp9_support = kVp9SupportUnknown;

// Check for VP9 support. Since this is used by a starboard function, it
// cannot depend on other modules (e.g. ANGLE).
bool IsVp9Supported() {
  if (!kSbHasMediaWebmVp9Support) {
    return false;
  }

  if (s_vp9_support == kVp9SupportUnknown) {
    // Try initializing the VP9 decoder to determine if it is supported.
    HRESULT hr;

    Microsoft::WRL::ComPtr<ID3D11Device> d3d_device;
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                           nullptr, 0, D3D11_SDK_VERSION,
                           d3d_device.GetAddressOf(), nullptr, nullptr);

    UINT reset_token = 0;
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> device_manager;
    if (SUCCEEDED(hr)) {
      hr = MFCreateDXGIDeviceManager(&reset_token,
                                     device_manager.GetAddressOf());
    }
    if (SUCCEEDED(hr)) {
      hr = device_manager->ResetDevice(d3d_device.Get(), reset_token);
    }

    Microsoft::WRL::ComPtr<IMFTransform> transform;
    if (SUCCEEDED(hr)) {
      hr = CoCreateInstance(CLSID_MSVPxDecoder, nullptr, CLSCTX_INPROC_SERVER,
                            IID_PPV_ARGS(transform.GetAddressOf()));
    }

    if (SUCCEEDED(hr)) {
      hr = transform->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                                     ULONG_PTR(device_manager.Get()));
    }

    s_vp9_support = SUCCEEDED(hr) ? kVp9SupportYes : kVp9SupportNo;
  }
  return s_vp9_support == kVp9SupportYes;
}
#else   // SB_API_VERSION >= SB_RUNTIME_CONFIGS_VERSION ||
// defined(SB_HAS_MEDIA_WEBM_VP9_SUPPORT)
bool IsVp9Supported() {
  return false;
}
#endif  // SB_API_VERSION >= SB_RUNTIME_CONFIGS_VERSION ||
        // defined(SB_HAS_MEDIA_WEBM_VP9_SUPPORT)

}  // namespace

bool SbMediaIsVideoSupported(SbMediaVideoCodec video_codec,
                             const MimeType* mime_type,
                             int profile,
                             int level,
                             int bit_depth,
                             SbMediaPrimaryId primary_id,
                             SbMediaTransferId transfer_id,
                             SbMediaMatrixId matrix_id,
                             int frame_width,
                             int frame_height,
                             int64_t bitrate,
                             int fps,
                             bool decode_to_texture_required) {
  // Win32 platforms use decode-to-texture by default so there is no special
  // constraints if decode-to-texture support is specifically required.

  int max_width = 1920;
  int max_height = 1080;

  if (video_codec == kSbMediaVideoCodecVp9) {
// Vp9 supports 8k only in whitelisted platforms, up to 4k in the others.
#ifdef ENABLE_VP9_8K_SUPPORT
    max_width = 7680;
    max_height = 4320;
#else   // ENABLE_VP9_8K_SUPPORT
    max_width = 3840;
    max_height = 2160;
#endif  // ENABLE_VP9_8K_SUPPORT
  } else if (video_codec == kSbMediaVideoCodecH264) {
// Not all devices can support 4k H264; some (e.g. xb1) may crash in
// the decoder if provided too high of a resolution. Therefore
// platforms must explicitly opt-in to support 4k H264.
#ifdef ENABLE_H264_4K_SUPPORT
    max_width = 3840;
    max_height = 2160;
#endif  // ENABLE_H264_4K_SUPPORT
  }

  if (frame_width > max_width || frame_height > max_height) {
    return false;
  }

  // Is bitrate in range?
  if (bitrate > kSbMediaMaxVideoBitrateInBitsPerSecond) {
    return false;
  }
  if (fps > 60) {
    return false;
  }
  using ::starboard::shared::starboard::media::IsSDRVideo;
  if (!IsSDRVideo(bit_depth, primary_id, transfer_id, matrix_id)) {
    return false;
  }
  if (video_codec == kSbMediaVideoCodecH264) {
    return true;
  }
  if (video_codec == kSbMediaVideoCodecVp9) {
    return IsVp9Supported();
  }
  return false;
}
