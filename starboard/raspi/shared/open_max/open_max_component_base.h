// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_COMPONENT_BASE_H_
#define STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_COMPONENT_BASE_H_

// OMX_SKIP64BIT is required for using VC GPU code.
#define OMX_SKIP64BIT 1

#include <IL/OMX_Broadcom.h>
#include <interface/vcos/vcos.h>
#include <interface/vcos/vcos_logging.h>
#include <interface/vmcs_host/vchost.h>

#include <vector>

#include "starboard/common/condition_variable.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/shared/internal_only.h"

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

// This serves as a base class for OpenMaxComponent to contain code to forward
// OMX callbacks and manage OMX events in order to simplify the code of
// OpenMaxComponent.
class OpenMaxComponentBase {
 protected:
  struct EventDescription {
    OMX_EVENTTYPE event;
    OMX_U32 data1;
    OMX_U32 data2;
    OMX_PTR event_data;
  };

  typedef std::vector<EventDescription> EventDescriptions;

  explicit OpenMaxComponentBase(const char* name);
  ~OpenMaxComponentBase();

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

  void SendCommand(OMX_COMMANDTYPE command, int param);
  void WaitForCommandCompletion();
  void SendCommandAndWaitForCompletion(OMX_COMMANDTYPE command, int param);

  OMX_ERRORTYPE OnEvent(OMX_EVENTTYPE event,
                        OMX_U32 data1,
                        OMX_U32 data2,
                        OMX_PTR event_data);

  virtual void OnErrorEvent(OMX_U32 data1, OMX_U32 data2,
                            OMX_PTR event_data) = 0;
  virtual void OnOutputSettingChanged() = 0;
  virtual OMX_ERRORTYPE OnEmptyBufferDone(OMX_BUFFERHEADERTYPE* buffer) = 0;
  virtual void OnFillBufferDone(OMX_BUFFERHEADERTYPE* buffer) = 0;

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

  OMX_HANDLETYPE handle_;
  int input_port_;
  int output_port_;

 private:
  Mutex mutex_;
  ConditionVariable event_condition_variable_;
  EventDescriptions event_descriptions_;
};

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard

#endif  // STARBOARD_RASPI_SHARED_OPEN_MAX_OPEN_MAX_COMPONENT_BASE_H_
