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

#ifndef STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_EGL_RENDER_COMPONENT_H_
#define STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_EGL_RENDER_COMPONENT_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "starboard/decode_target.h"
#include "starboard/raspi/shared/open_max/open_max_component.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

// Facilitate using the GPU to decode images of various formats. This
// encapsulates the "OMX.broadcom.egl_render" component.
class OpenMaxEglRenderComponent : public OpenMaxComponent {
 public:
  OpenMaxEglRenderComponent();

  void SetOutputImage(EGLImageKHR image) { output_image_ = image; }

  void ForwardPortCallbacks(OpenMaxComponent* callback_component) {
    callback_component_ = callback_component;
  }

 private:
  bool OnEnableInputPort(OMXParamPortDefinition* port_definition) override;
  bool OnEnableOutputPort(OMXParamPortDefinition* port_definition) override;
  OMX_BUFFERHEADERTYPE* AllocateBuffer(int port,
                                       int index,
                                       OMX_U32 size) override;

  EGLImageKHR output_image_;
  OpenMaxComponent* callback_component_;
};

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard

#endif  // STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_EGL_RENDER_COMPONENT_H_
