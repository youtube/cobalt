// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/uwp/decoder_utils.h"

#include <windows.graphics.display.core.h>

#include "starboard/common/log.h"
#include "starboard/shared/uwp/application_uwp.h"
#include "starboard/shared/uwp/async_utils.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "third_party/angle/include/GLES2/gl2.h"

namespace starboard {
namespace shared {
namespace uwp {
namespace {

using ::starboard::shared::uwp::ApplicationUwp;
using ::starboard::shared::uwp::WaitForResult;
using Windows::Graphics::Display::Core::HdmiDisplayHdr2086Metadata;
using Windows::Graphics::Display::Core::HdmiDisplayHdrOption;
using Windows::Graphics::Display::Core::HdmiDisplayInformation;
using Windows::Graphics::Display::Core::HdmiDisplayMode;

constexpr uint16_t Scale(float value, float factor) {
  return static_cast<uint16_t>(value * factor);
}

template <typename EglFunctionType>
EglFunctionType GetEglProcAddr(const char* name) {
  EglFunctionType egl_function =
      reinterpret_cast<EglFunctionType>(eglGetProcAddress(name));
  SB_DCHECK(egl_function != nullptr);
  return egl_function;
}

}  // namespace

Microsoft::WRL::ComPtr<ID3D11Device> GetDirectX11Device(void* display) {
  static PFNEGLQUERYDISPLAYATTRIBEXTPROC s_egl_query_display_attrib_ext =
      GetEglProcAddr<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(
          "eglQueryDisplayAttribEXT");

  static PFNEGLQUERYDEVICEATTRIBEXTPROC s_egl_query_device_attrib_ext =
      GetEglProcAddr<PFNEGLQUERYDEVICEATTRIBEXTPROC>("eglQueryDeviceAttribEXT");

  intptr_t egl_device = 0;
  s_egl_query_display_attrib_ext(display, EGL_DEVICE_EXT, &egl_device);
  SB_DCHECK(egl_device != 0);
  SB_DCHECK(glGetError() == GL_NO_ERROR);

  intptr_t device = 0;
  s_egl_query_device_attrib_ext(reinterpret_cast<EGLDeviceEXT>(egl_device),
                                EGL_D3D11_DEVICE_ANGLE, &device);
  SB_DCHECK(device != 0);
  SB_DCHECK(glGetError() == GL_NO_ERROR);

  return reinterpret_cast<ID3D11Device*>(device);
}

void UpdateHdrColorMetadataToCurrentDisplay(
    const SbMediaColorMetadata& color_metadata) {
  const SbMediaMasteringMetadata& md = color_metadata.mastering_metadata;
  HdmiDisplayHdr2086Metadata hdr2086_metadata{
      Scale(md.primary_r_chromaticity_x, 50000.),
      Scale(md.primary_r_chromaticity_y, 50000.),
      Scale(md.primary_g_chromaticity_x, 50000.),
      Scale(md.primary_g_chromaticity_y, 50000.),
      Scale(md.primary_b_chromaticity_x, 50000.),
      Scale(md.primary_b_chromaticity_y, 50000.),
      Scale(md.white_point_chromaticity_x, 50000.),
      Scale(md.white_point_chromaticity_y, 50000.),
      Scale(md.luminance_max, 10000.),
      Scale(md.luminance_min, 10000.),
      Scale(color_metadata.max_cll, 10000.),
      Scale(color_metadata.max_fall, 10000.)};
  auto hdmi_info = HdmiDisplayInformation::GetForCurrentView();
  HdmiDisplayMode ^ mode = ApplicationUwp::Get()->GetPreferredModeHdr();
  bool result = WaitForResult(hdmi_info->RequestSetCurrentDisplayModeAsync(
      mode, HdmiDisplayHdrOption::Eotf2084, hdr2086_metadata));
  SB_CHECK(result);
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
