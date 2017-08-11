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

namespace starboard {
namespace shared {
namespace win32 {

HardwareDecoderContext GetDirectXForHardwareDecoding() {
  HRESULT result = S_OK;
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  PFNEGLQUERYDISPLAYATTRIBEXTPROC query_display;

  query_display = reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(
      eglGetProcAddress("eglQueryDisplayAttribEXT"));
  SB_DCHECK(query_display != nullptr);

  PFNEGLQUERYDEVICEATTRIBEXTPROC query_device;
  query_device = reinterpret_cast<PFNEGLQUERYDEVICEATTRIBEXTPROC>(
      eglGetProcAddress("eglQueryDeviceAttribEXT"));
  SB_DCHECK(query_display != nullptr);

  intptr_t egl_device = 0;
  query_display(display, EGL_DEVICE_EXT, &egl_device);
  SB_DCHECK(egl_device != 0);

  intptr_t device;
  query_device(reinterpret_cast<EGLDeviceEXT>(egl_device),
      EGL_D3D11_DEVICE_ANGLE, &device);

  ID3D11Device* output_dx_device = reinterpret_cast<ID3D11Device*>(device);
  IMFDXGIDeviceManager* dxgi_device_mgr = nullptr;

  UINT token = 0;
  result = MFCreateDXGIDeviceManager(&token, &dxgi_device_mgr);
  SB_DCHECK(result == S_OK);
  SB_DCHECK(dxgi_device_mgr);

  result = dxgi_device_mgr->ResetDevice(output_dx_device, token);
  SB_DCHECK(SUCCEEDED(result));

  HardwareDecoderContext output = {
    output_dx_device,
    dxgi_device_mgr
  };
  return output;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
