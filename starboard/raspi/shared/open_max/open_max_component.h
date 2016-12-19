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
#include "starboard/raspi/shared/open_max/open_max_component_base.h"
#include "starboard/shared/internal_only.h"
#include "starboard/thread.h"
#include "starboard/time.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

class OpenMaxComponent : protected OpenMaxComponentBase {
 public:
  explicit OpenMaxComponent(const char* name);

  void Start();
  void Flush();

  int WriteData(const void* data, int size, SbTime timestamp);
  void WriteEOS();

  OMX_BUFFERHEADERTYPE* PeekNextOutputBuffer();
  void DropNextOutputBuffer();

 protected:
  ~OpenMaxComponent();

 private:
  virtual bool OnEnableInputPort(OMXParamPortDefinition* port_definition) {
    return false;
  }
  virtual void* AllocateInputBuffer(OMX_U32 size) { return NULL; }
  virtual void FreeInputBuffer(void* p) { SB_NOTREACHED(); }
  virtual bool OnEnableOutputPort(OMXParamPortDefinition* port_definition) {
    return false;
  }
  virtual void OnReadyToPeekOutputBuffer() {}

  void DisableOutputPort();

  void EnableInputPortAndAllocateBuffers();
  void Attach(OpenMaxComponent* component);
  void EnableOutputPortAndAllocateBuffer();
  OMX_BUFFERHEADERTYPE* GetUnusedInputBuffer();

  void OnOutputSettingChanged() SB_OVERRIDE;
  OMX_ERRORTYPE OnEmptyBufferDone(OMX_BUFFERHEADERTYPE* buffer) SB_OVERRIDE;
  void OnFillBufferDone(OMX_BUFFERHEADERTYPE* buffer) SB_OVERRIDE;
  void RunFillBufferLoop();

  static void* FillBufferThreadEntryPoint(void* context);

  Mutex mutex_;
  bool output_setting_changed_;
  std::vector<OMX_BUFFERHEADERTYPE*> input_buffers_;
  std::queue<OMX_BUFFERHEADERTYPE*> free_input_buffers_;
  std::vector<OMX_BUFFERHEADERTYPE*> output_buffers_;
  std::queue<OMX_BUFFERHEADERTYPE*> filled_output_buffers_;
  std::queue<OMX_BUFFERHEADERTYPE*> free_output_buffers_;

  SbThread fill_buffer_thread_;
  bool kill_fill_buffer_thread_;
  ConditionVariable output_available_condition_variable_;
};

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard

#endif  // STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_COMPONENT_H_
