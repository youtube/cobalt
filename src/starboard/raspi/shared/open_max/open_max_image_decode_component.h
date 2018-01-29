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

#ifndef STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_IMAGE_DECODE_COMPONENT_H_
#define STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_IMAGE_DECODE_COMPONENT_H_

#include "starboard/decode_target.h"
#include "starboard/raspi/shared/open_max/open_max_component.h"
#include "starboard/raspi/shared/open_max/open_max_egl_render_component.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

// Facilitate using the GPU to decode images of various formats. This
// encapsulates the "OMX.broadcom.image_decode" component.
class OpenMaxImageDecodeComponent : private OpenMaxComponent {
 public:
  OpenMaxImageDecodeComponent();

  // Return the enumerated image format if the given mime type is supported.
  // If the mime type is not supported, then OMX_IMAGE_CodingMax is returned.
  static OMX_IMAGE_CODINGTYPE GetCompressionFormat(const char* mime_type);

  // Decode the given data to a decode target.
  SbDecodeTarget Decode(SbDecodeTargetGraphicsContextProvider* provider,
                        const char* mime_type,
                        SbDecodeTargetFormat output_format,
                        const void* data,
                        int data_size);

 private:
  void SetInputFormat(const char* mime_type, SbDecodeTargetFormat format);
  void SetOutputFormat(OMX_COLOR_FORMATTYPE format, int width, int height);
  int ProcessOutput();

  // OpenMaxComponent callbacks
  bool OnEnableOutputPort(OMXParamPortDefinition* port_definition) override;

  enum State {
    kStateIdle,
    kStateInputReady,
    kStateSetTunnelOutput,
    kStateOutputReady,
    kStateDecodeDone,
  };

  OpenMaxEglRenderComponent render_component_;
  State state_;
  SbDecodeTargetGraphicsContextProvider* graphics_context_provider_;
  SbDecodeTargetPrivate* target_;
  OMX_IMAGE_CODINGTYPE input_format_;
};

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard

#endif  // STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_IMAGE_DECODE_COMPONENT_H_
