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

#include "starboard/raspi/shared/open_max/open_max_egl_render_component.h"

#include <algorithm>

#include "starboard/configuration.h"
#include "starboard/raspi/shared/open_max/decode_target_internal.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

OpenMaxEglRenderComponent::OpenMaxEglRenderComponent()
    : OpenMaxComponent("OMX.broadcom.egl_render"),
      output_image_(EGL_NO_IMAGE_KHR),
      callback_component_(NULL) {}

bool OpenMaxEglRenderComponent::OnEnableInputPort(
    OMXParamPortDefinition* port_definition) {
  if (callback_component_ != NULL) {
    return callback_component_->OnEnableInputPort(port_definition);
  } else {
    return false;
  }
}

bool OpenMaxEglRenderComponent::OnEnableOutputPort(
    OMXParamPortDefinition* port_definition) {
  if (callback_component_ != NULL) {
    return callback_component_->OnEnableOutputPort(port_definition);
  } else {
    return false;
  }
}

OMX_BUFFERHEADERTYPE* OpenMaxEglRenderComponent::AllocateBuffer(int port,
                                                                int index,
                                                                OMX_U32 size) {
  if (port == output_port_) {
    OMX_BUFFERHEADERTYPE* buffer;
    SB_DCHECK(output_image_ != EGL_NO_IMAGE_KHR);
    OMX_ERRORTYPE error =
        OMX_UseEGLImage(handle_, &buffer, port, NULL, output_image_);
    SB_DCHECK(error == OMX_ErrorNone);
    return buffer;
  } else {
    return OpenMaxComponent::AllocateBuffer(port, index, size);
  }
}

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard
