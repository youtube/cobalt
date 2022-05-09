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

#ifndef STARBOARD_SHARED_WIN32_DX_CONTEXT_VIDEO_DECODER_H_
#define STARBOARD_SHARED_WIN32_DX_CONTEXT_VIDEO_DECODER_H_

#include <wrl\client.h>  // For ComPtr.

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IMFDXGIDeviceManager;

namespace starboard {
namespace shared {
namespace win32 {

struct HardwareDecoderContext {
  Microsoft::WRL::ComPtr<ID3D11Device> dx_device_out;
  Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> dxgi_device_manager_out;
};

HardwareDecoderContext GetDirectXForHardwareDecoding();

}  // namespace win32
}  // namespace shared
}  // namespace starboard
#endif  // STARBOARD_SHARED_WIN32_DX_CONTEXT_VIDEO_DECODER_H_
