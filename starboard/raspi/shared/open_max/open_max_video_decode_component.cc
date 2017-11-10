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

#include "starboard/raspi/shared/open_max/open_max_video_decode_component.h"

#include <algorithm>

#include "starboard/configuration.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

namespace {

const char kVideoDecodeComponentName[] = "OMX.broadcom.video_decode";
const size_t kOMXOutputBufferCount = 24;
const int kMaxFrameWidth = 1920;
const int kMaxFrameHeight = 1088;
const size_t kMaxVideoFrameSize = kMaxFrameWidth * kMaxFrameHeight * 3 / 2;

typedef OMXParam<OMX_VIDEO_PARAM_PORTFORMATTYPE, OMX_IndexParamVideoPortFormat>
    OMXVideoParamPortFormat;

}  // namespace

OpenMaxVideoDecodeComponent::OpenMaxVideoDecodeComponent()
    : OpenMaxComponent(kVideoDecodeComponentName) {
  OMXVideoParamPortFormat port_format;
  GetInputPortParam(&port_format);
  port_format.eCompressionFormat = OMX_VIDEO_CodingAVC;
  SetPortParam(port_format);
}

OMX_BUFFERHEADERTYPE* OpenMaxVideoDecodeComponent::GetOutputBuffer() {
  if (OMX_BUFFERHEADERTYPE* buffer = OpenMaxComponent::GetOutputBuffer()) {
    buffer->pAppPrivate =
        new OMX_VIDEO_PORTDEFINITIONTYPE(output_port_definition_.format.video);
    return buffer;
  }

  return NULL;
}

void OpenMaxVideoDecodeComponent::DropOutputBuffer(
    OMX_BUFFERHEADERTYPE* buffer) {
  SB_DCHECK(buffer);
  delete reinterpret_cast<OMX_VIDEO_PORTDEFINITIONTYPE*>(buffer->pAppPrivate);
  buffer->pAppPrivate = NULL;
  OpenMaxComponent::DropOutputBuffer(buffer);
}

bool OpenMaxVideoDecodeComponent::OnEnableOutputPort(
    OMXParamPortDefinition* port_definition) {
  SB_DCHECK(port_definition);

  output_port_definition_ = *port_definition;
  SB_DCHECK(port_definition->format.video.eColorFormat ==
            OMX_COLOR_FormatYUV420PackedPlanar);
  port_definition->format.video.eColorFormat =
      OMX_COLOR_FormatYUV420PackedPlanar;
  port_definition->nBufferCountActual = kOMXOutputBufferCount;
  port_definition->nBufferSize =
      std::max(port_definition->nBufferSize, kMaxVideoFrameSize);
  return true;
}

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard
