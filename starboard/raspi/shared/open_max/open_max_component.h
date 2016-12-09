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

// OMX_SKIP64BIT is required for using VC GPU code.
#define OMX_SKIP64BIT 1

#include <IL/OMX_Broadcom.h>
#include <interface/vcos/vcos.h>
#include <interface/vcos/vcos_logging.h>
#include <interface/vmcs_host/vchost.h>

#include <queue>
#include <vector>

#include "starboard/condition_variable.h"
#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/shared/internal_only.h"
#include "starboard/time.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

template <typename ParamType, OMX_INDEXTYPE index>
struct OMXParam : public ParamType {
  static const OMX_INDEXTYPE Index = index;

  OMXParam() : ParamType() {
    ParamType::nSize = sizeof(ParamType);
    ParamType::nVersion.nVersion = OMX_VERSION;
  }
};

typedef OMXParam<OMX_PARAM_PORTDEFINITIONTYPE, OMX_IndexParamPortDefinition>
    OMXParamPortDefinition;
typedef OMXParam<OMX_VIDEO_PARAM_PORTFORMATTYPE, OMX_IndexParamVideoPortFormat>
    OMXVideoParamPortFormat;

class OpenMaxComponent {
 public:
  explicit OpenMaxComponent(const char* name);
  virtual ~OpenMaxComponent();

  void Start();
  void Flush();

  void WriteData(const void* data, size_t size, SbTime timestamp);
  void WriteEOS();

  OMX_BUFFERHEADERTYPE* PeekNextOutputBuffer();
  void DropNextOutputBuffer();

  template <typename ParamType>
  void GetInputPortParam(ParamType* param) const {
    param->nPortIndex = input_port_;
    OMX_ERRORTYPE error = OMX_GetParameter(handle_, ParamType::Index, param);
    SB_DCHECK(error == OMX_ErrorNone) << std::hex << "OMX_GetParameter("
                                      << ParamType::Index
                                      << ") failed with error " << error;
  }

  template <typename ParamType>
  void GetOutputPortParam(ParamType* param) const {
    param->nPortIndex = output_port_;
    OMX_ERRORTYPE error = OMX_GetParameter(handle_, ParamType::Index, param);
    SB_DCHECK(error == OMX_ErrorNone) << std::hex << "OMX_GetParameter("
                                      << ParamType::Index
                                      << ") failed with error " << error;
  }

  template <typename ParamType>
  void SetPortParam(const ParamType& param) const {
    OMX_ERRORTYPE error = OMX_SetParameter(handle_, ParamType::Index,
                                           const_cast<ParamType*>(&param));
    SB_DCHECK(error == OMX_ErrorNone) << std::hex << "OMX_SetParameter("
                                      << ParamType::Index
                                      << ") failed with error " << error;
  }

 private:
  struct EventDescription {
    OMX_EVENTTYPE event;
    OMX_U32 data1;
    OMX_U32 data2;
    OMX_PTR event_data;
  };

  typedef std::vector<EventDescription> EventDescriptions;

  virtual bool OnEnableInputPort(OMXParamPortDefinition* port_definition) {
    return false;
  }
  virtual bool OnEnableOutputPort(OMXParamPortDefinition* port_definition) {
    return false;
  }

  void SendCommand(OMX_COMMANDTYPE command, int param);
  void WaitForCommandCompletion();
  void SendCommandAndWaitForCompletion(OMX_COMMANDTYPE command, int param);
  void EnableInputPortAndAllocateBuffers();
  void EnableOutputPortAndAllocateBuffer();
  OMX_BUFFERHEADERTYPE* GetUnusedInputBuffer();

  OMX_ERRORTYPE OnEvent(OMX_EVENTTYPE event,
                        OMX_U32 data1,
                        OMX_U32 data2,
                        OMX_PTR event_data);
  OMX_ERRORTYPE OnEmptyBufferDone(OMX_BUFFERHEADERTYPE* buffer);
  void OnFillBufferDone(OMX_BUFFERHEADERTYPE* buffer);

  static OMX_ERRORTYPE EventHandler(OMX_HANDLETYPE handle,
                                    OMX_PTR app_data,
                                    OMX_EVENTTYPE event,
                                    OMX_U32 data1,
                                    OMX_U32 data2,
                                    OMX_PTR event_data);
  static OMX_ERRORTYPE EmptyBufferDone(OMX_HANDLETYPE handle,
                                       OMX_PTR app_data,
                                       OMX_BUFFERHEADERTYPE* buffer);
  static OMX_ERRORTYPE FillBufferDone(OMX_HANDLETYPE handle,
                                      OMX_PTR app_data,
                                      OMX_BUFFERHEADERTYPE* buffer);

  Mutex mutex_;
  ConditionVariable condition_variable_;
  OMX_HANDLETYPE handle_;
  int input_port_;
  int output_port_;
  bool output_setting_changed_;
  EventDescriptions event_descriptions_;
  std::vector<OMX_BUFFERHEADERTYPE*> input_buffers_;
  std::queue<OMX_BUFFERHEADERTYPE*> unused_input_buffers_;
  std::vector<OMX_BUFFERHEADERTYPE*> output_buffers_;
  std::queue<OMX_BUFFERHEADERTYPE*> filled_output_buffers_;
};

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard

#endif  // STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_COMPONENT_H_
