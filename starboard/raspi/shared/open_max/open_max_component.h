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

#ifndef STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_COMPONENT_H_
#define STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_COMPONENT_H_

#include <queue>
#include <vector>

#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/raspi/shared/open_max/open_max_component_base.h"
#include "starboard/shared/internal_only.h"
#include "starboard/time.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

class OpenMaxComponent : protected OpenMaxComponentBase {
 public:
  enum DataType {
    kDataNonEOS,    // Do not flag any buffer as end of stream.
    kDataEOS,       // Flag the last buffer written as end of stream.
  };

  explicit OpenMaxComponent(const char* name);

  void Start();
  void Flush();

  // Write data to the input buffer. Returns the number of bytes written or
  // -1 if an error occurred.
  // This will return immediately once no more buffers are available.
  int WriteData(const void* data, int size, DataType type, SbTime timestamp);

  // Write an empty buffer that is flagged as the end of the input stream.
  // This will block until a buffer is available.
  bool WriteEOS();

  // Try to get the next output buffer.  The function may enable the output port
  // internally if the output port has never been enabled or if the output
  // format has been changed.  In both case it enables the output port in
  // response to an output setting change.
  OMX_BUFFERHEADERTYPE* GetOutputBuffer();
  void DropOutputBuffer(OMX_BUFFERHEADERTYPE* buffer);

 protected:
  ~OpenMaxComponent();
  // Callbacks available to children.
  void OnErrorEvent(OMX_U32 data1, OMX_U32 data2,
                    OMX_PTR event_data) SB_OVERRIDE;
  virtual bool OnEnableInputPort(OMXParamPortDefinition* port_definition) {
    return false;
  }
  virtual bool OnEnableOutputPort(OMXParamPortDefinition* port_definition) {
    return false;
  }

 private:
  void DisableOutputPort();

  void EnableInputPortAndAllocateBuffers();
  void EnableOutputPortAndAllocateBuffer();
  OMX_BUFFERHEADERTYPE* GetUnusedInputBuffer();

  // Callbacks not intended to be overridden by children.
  void OnOutputSettingChanged() SB_OVERRIDE;
  OMX_ERRORTYPE OnEmptyBufferDone(OMX_BUFFERHEADERTYPE* buffer) SB_OVERRIDE;
  void OnFillBufferDone(OMX_BUFFERHEADERTYPE* buffer) SB_OVERRIDE;

  Mutex mutex_;
  bool output_setting_changed_;
  std::vector<OMX_BUFFERHEADERTYPE*> input_buffers_;
  std::queue<OMX_BUFFERHEADERTYPE*> free_input_buffers_;
  std::vector<OMX_BUFFERHEADERTYPE*> output_buffers_;
  std::queue<OMX_BUFFERHEADERTYPE*> filled_output_buffers_;
  int outstanding_output_buffers_;
};

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard

#endif  // STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_COMPONENT_H_
