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

#include "starboard/shared/starboard/media/media_support_internal.h"

#include <d3d11.h>
#include <d3d12.h>
#include <mfapi.h>
#include <mfidl.h>
#include <wrl/client.h>

#include "starboard/shared/uwp/application_uwp.h"
#include "third_party/libvpx_xb1/libvpx/vpx/vp8dx.h"
#include "third_party/libvpx_xb1/libvpx/vpx/vpx_decoder.h"

namespace {

using ::starboard::shared::uwp::ApplicationUwp;

const int kMaxVpxGpuDecodeTargetWidth = 3840;
const int kMaxVpxGpuDecodeTargetHeight = 2160;
const int kMaxVpxGpuDecoderCpuCoreUse = 4;

}  // namespace

#if SB_HAS(MEDIA_WEBM_VP9_SUPPORT)

namespace {
// Cache the VP9 support status since the check may be expensive.
enum Vp9Support { kVp9SupportUnknown, kVp9SupportYes, kVp9SupportNo };
Vp9Support s_vp9_hw_support = kVp9SupportUnknown;
Vp9Support s_vp9_gpu_support = kVp9SupportUnknown;
}  // namespace

// Check for VP9 support. Since this is used by a starboard function, it
// cannot depend on other modules (e.g. ANGLE).
SB_EXPORT bool IsVp9HwDecoderSupported() {
  if (s_vp9_hw_support == kVp9SupportUnknown) {
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

    s_vp9_hw_support = SUCCEEDED(hr) ? kVp9SupportYes : kVp9SupportNo;
  }
  return s_vp9_hw_support == kVp9SupportYes;
}

SB_EXPORT bool IsVp9GPUDecoderSupported() {
  if (s_vp9_gpu_support == kVp9SupportUnknown) {
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> d3d12queue;
    ApplicationUwp* application_uwp = ApplicationUwp::Get();
    SB_CHECK(application_uwp->GetD3D12Device());
    HRESULT hr = application_uwp->GetD3D12Device()->CreateCommandQueue(
        &desc, IID_PPV_ARGS(&d3d12queue));
    if (FAILED(hr)) {
      SB_LOG(WARNING)
          << "Could not create DX12 command queue: cannot use GPU VP9";
      return false;
    }
    vpx_codec_ctx vpx_context = {};
    vpx_codec_dec_cfg_t vpx_config = {};
    vpx_config.w = kMaxVpxGpuDecodeTargetWidth;
    vpx_config.h = kMaxVpxGpuDecodeTargetHeight;
    vpx_config.threads = kMaxVpxGpuDecoderCpuCoreUse;

    vpx_config.hw_device = application_uwp->GetD3D12Device().Get();
    vpx_config.hw_command_queue = d3d12queue.Get();
    vpx_codec_err_t status =
        vpx_codec_dec_init(&vpx_context, vpx_codec_vp9_dx(), &vpx_config,
                           VPX_CODEC_USE_FRAME_THREADING);
    s_vp9_gpu_support =
        (status == VPX_CODEC_OK) ? kVp9SupportYes : kVp9SupportNo;
    vpx_codec_destroy(&vpx_context);
  }
  return s_vp9_gpu_support == kVp9SupportYes;
}

#else  // SB_HAS(MEDIA_WEBM_VP9_SUPPORT)

bool IsVp9HwDecoderSupported() {
  return false;
}

bool IsVp9GPUDecoderSupported() {
  return false;
}

#endif

SB_EXPORT bool SbMediaIsVideoSupported(SbMediaVideoCodec video_codec,
                                       int frame_width,
                                       int frame_height,
                                       int64_t bitrate,
                                       int fps) {
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
  if (bitrate > SB_MEDIA_MAX_VIDEO_BITRATE_IN_BITS_PER_SECOND) {
    return false;
  }
  if (fps > 60) {
    return false;
  }
  if (video_codec == kSbMediaVideoCodecH264) {
    return true;
  }
  if (video_codec == kSbMediaVideoCodecVp9) {
    if (IsVp9HwDecoderSupported())
      return true;
    if (IsVp9GPUDecoderSupported())
      return true;
  }
  return false;
}
