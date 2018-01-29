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

#include "starboard/raspi/shared/open_max/open_max_image_decode_component.h"

#include <algorithm>

#include "starboard/configuration.h"
#include "starboard/memory.h"
#include "starboard/raspi/shared/open_max/decode_target_create.h"
#include "starboard/raspi/shared/open_max/decode_target_internal.h"
#include "starboard/thread.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

namespace {

struct MimeToFormat {
  const char* mime_type;
  OMX_IMAGE_CODINGTYPE format;
};

const MimeToFormat kSupportedFormats[] = {
    {"image/jpeg", OMX_IMAGE_CodingJPEG},
    // The image decode component doesn't handle PNG very well.
    //   { "image/png", OMX_IMAGE_CodingPNG },
    // GIF decoding should work, but it has not been tested.
    //   { "image/gif", OMX_IMAGE_CodingGIF },
};

}  // namespace

OpenMaxImageDecodeComponent::OpenMaxImageDecodeComponent()
    : OpenMaxComponent("OMX.broadcom.image_decode"),
      state_(kStateIdle),
      graphics_context_provider_(NULL),
      target_(NULL),
      input_format_(OMX_IMAGE_CodingMax) {}

// static
OMX_IMAGE_CODINGTYPE OpenMaxImageDecodeComponent::GetCompressionFormat(
    const char* mime_type) {
  for (int index = 0; index < SB_ARRAY_SIZE_INT(kSupportedFormats); ++index) {
    if (strcmp(mime_type, kSupportedFormats[index].mime_type) == 0) {
      return kSupportedFormats[index].format;
    }
  }
  return OMX_IMAGE_CodingMax;
}

SbDecodeTarget OpenMaxImageDecodeComponent::Decode(
    SbDecodeTargetGraphicsContextProvider* provider,
    const char* mime_type,
    SbDecodeTargetFormat output_format,
    const void* data,
    int data_size) {
  SB_DCHECK(target_ == NULL);

  graphics_context_provider_ = provider;
  SetInputFormat(mime_type, output_format);

  // Start the component and enable the input port.
  Start();
  state_ = kStateInputReady;

  // Process the encoded data. This will call OnEnableOutputPort() when the
  // component is ready to start filling the output.
  int data_size_written = 0;
  for (;;) {
    int write_size = data_size - data_size_written;
    if (write_size > 0) {
      write_size =
          WriteData(reinterpret_cast<const uint8_t*>(data) + data_size_written,
                    write_size, kDataEOS, SbTimeGetMonotonicNow());
      SB_DCHECK(write_size >= 0);
      data_size_written += write_size;
    }
    int output_size = ProcessOutput();
    if (state_ == kStateDecodeDone) {
      break;
    } else if (write_size == 0 && output_size == 0) {
      // Wait for buffers to become available.
      SbThreadSleep(kSbTimeMillisecond);
    }
  }

  // Clean up.
  state_ = kStateIdle;
  CloseTunnel();
  return target_;
}

void OpenMaxImageDecodeComponent::SetInputFormat(const char* mime_type,
                                                 SbDecodeTargetFormat format) {
  OMXParam<OMX_IMAGE_PARAM_PORTFORMATTYPE, OMX_IndexParamImagePortFormat>
      port_format;
  GetInputPortParam(&port_format);
  input_format_ = GetCompressionFormat(mime_type);
  port_format.eCompressionFormat = input_format_;
  SB_DCHECK(port_format.eCompressionFormat != OMX_IMAGE_CodingMax);
  SetPortParam(port_format);
}

void OpenMaxImageDecodeComponent::SetOutputFormat(OMX_COLOR_FORMATTYPE format,
                                                  int width,
                                                  int height) {
  target_ = DecodeTargetCreate(graphics_context_provider_,
                               kSbDecodeTargetFormat1PlaneRGBA, width, height);
  target_->info.is_opaque =
      (input_format_ == OMX_IMAGE_CodingPNG) ? false : true;
  render_component_.SetOutputImage(target_->images[0]);
}

int OpenMaxImageDecodeComponent::ProcessOutput() {
  OMX_BUFFERHEADERTYPE* buffer = GetOutputBuffer();
  int output_size = 0;
  if (buffer) {
    SB_DCHECK(state_ == kStateOutputReady);
    bool is_end_of_stream = (buffer->nFlags & OMX_BUFFERFLAG_EOS) != 0;
    output_size = 1;  // Signal to caller that some data was processed.
    DropOutputBuffer(buffer);
    if (is_end_of_stream) {
      state_ = kStateDecodeDone;
    }
  }

  return output_size;
}

bool OpenMaxImageDecodeComponent::OnEnableOutputPort(
    OMXParamPortDefinition* port_definition) {
  if (port_definition->nPortIndex == output_port_) {
    // Our output port has been enabled. Tunnel to the egl render
    // component to decode into a texture.
    SB_DCHECK(state_ == kStateInputReady);
    render_component_.ForwardPortCallbacks(this);
    SetOutputComponent(&render_component_);
    state_ = kStateSetTunnelOutput;
    return false;
  } else if (state_ == kStateSetTunnelOutput) {
    // Tunnelled component's output port was just enabled.
    state_ = kStateOutputReady;
    return false;
  } else {
    // Got final settings for the tunnelled component's output port.
    SB_DCHECK(state_ == kStateOutputReady);
    SetOutputFormat(port_definition->format.video.eColorFormat,
                    port_definition->format.video.nFrameWidth,
                    port_definition->format.video.nFrameHeight);
    return false;
  }
}

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard
