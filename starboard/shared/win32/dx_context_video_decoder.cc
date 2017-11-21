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

#include "starboard/shared/win32/dx_context_video_decoder.h"

#include <D3D11.h>
#include <D3D11_4.h>
#include <mfapi.h>

#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"

#include "starboard/log.h"
#include "starboard/shared/win32/error_utils.h"

namespace starboard {
namespace shared {
namespace win32 {

namespace {

using Microsoft::WRL::ComPtr;

PFNEGLQUERYDISPLAYATTRIBEXTPROC GetQueryDisplayFunction() {
  PFNEGLQUERYDISPLAYATTRIBEXTPROC query_display;
  query_display = reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(
      eglGetProcAddress("eglQueryDisplayAttribEXT"));
  SB_DCHECK(query_display != nullptr);
  return query_display;
}

PFNEGLQUERYDEVICEATTRIBEXTPROC GetQueryDeviceFunction() {
  PFNEGLQUERYDEVICEATTRIBEXTPROC query_device;
  query_device = reinterpret_cast<PFNEGLQUERYDEVICEATTRIBEXTPROC>(
      eglGetProcAddress("eglQueryDeviceAttribEXT"));
  SB_DCHECK(query_device != nullptr);
  return query_device;
}

ComPtr<ID3D11Device> GetSharedDxForHardwareDecoding(intptr_t egl_device) {
  PFNEGLQUERYDEVICEATTRIBEXTPROC query_device = GetQueryDeviceFunction();
  intptr_t device = 0;
  query_device(reinterpret_cast<EGLDeviceEXT>(egl_device),
               EGL_D3D11_DEVICE_ANGLE, &device);

  ID3D11Device* output_dx_device = reinterpret_cast<ID3D11Device*>(device);
  return output_dx_device;
}

ComPtr<ID3D11Device> CreateNewDxForHardwareDecoding() {
  HRESULT hr;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d_device;
  hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                         nullptr, 0, D3D11_SDK_VERSION,
                         d3d_device.GetAddressOf(), nullptr, nullptr);

  return d3d_device;
}
}  // namespace.

HardwareDecoderContext GetDirectXForHardwareDecoding() {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  PFNEGLQUERYDISPLAYATTRIBEXTPROC query_display = GetQueryDisplayFunction();

  intptr_t egl_device = 0;
  query_display(display, EGL_DEVICE_EXT, &egl_device);

  ComPtr<ID3D11Device> d3d_device;
  if (egl_device != 0) {
    // Expected for production.
    d3d_device = GetSharedDxForHardwareDecoding(egl_device);
  } else {
    // Expected for nplb testing.
    d3d_device = CreateNewDxForHardwareDecoding();
  }

  // Generate DXGI device manager from DirectX.
  UINT reset_token = 0;
  ComPtr<IMFDXGIDeviceManager> device_manager;
  if (d3d_device.Get()) {
    HRESULT hr = MFCreateDXGIDeviceManager(&reset_token,
                                           device_manager.GetAddressOf());
    CheckResult(hr);
    hr = device_manager->ResetDevice(d3d_device.Get(), reset_token);
    CheckResult(hr);
  }

  HardwareDecoderContext output = {
    d3d_device,
    device_manager
  };
  return output;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
