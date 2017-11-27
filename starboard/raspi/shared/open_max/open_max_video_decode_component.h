// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_VIDEO_DECODE_COMPONENT_H_
#define STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_VIDEO_DECODE_COMPONENT_H_

#include <map>
#include <queue>

#include "starboard/common/ref_counted.h"
#include "starboard/raspi/shared/dispmanx_util.h"
#include "starboard/raspi/shared/open_max/open_max_component.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

// Encapsulate a "OMX.broadcom.video_decode" component.  Note that member
// functions of this class is expected to be called from ANY threads as this
// class works with the VideoDecoder filter, the OpenMAX component, and also
// manages the disposition of Dispmanx resource.
class OpenMaxVideoDecodeComponent : private OpenMaxComponent {
 public:
  typedef starboard::shared::starboard::player::VideoFrame VideoFrame;

  using OpenMaxComponent::Start;
  using OpenMaxComponent::Flush;
  using OpenMaxComponent::WriteData;
  using OpenMaxComponent::WriteEOS;

  OpenMaxVideoDecodeComponent();

  OMX_BUFFERHEADERTYPE* GetOutputBuffer();
  void DropOutputBuffer(OMX_BUFFERHEADERTYPE* buffer);

 private:
  bool OnEnableOutputPort(OMXParamPortDefinition* port_definition) override;

  OMXParamPortDefinition output_port_definition_;
};

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard

#endif  // STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_VIDEO_DECODE_COMPONENT_H_
