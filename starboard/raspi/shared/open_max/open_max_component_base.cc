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

#include "starboard/raspi/shared/open_max/open_max_component_base.h"

#include <algorithm>

#include "starboard/configuration.h"
#include "starboard/once.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

namespace {

const int kInvalidPort = -1;

const OMX_INDEXTYPE kPortTypes[] = {
    OMX_IndexParamAudioInit, OMX_IndexParamVideoInit, OMX_IndexParamImageInit,
    OMX_IndexParamOtherInit};

SbOnceControl s_open_max_initialization_once = SB_ONCE_INITIALIZER;

void DoInitializeOpenMax() {
  OMX_ERRORTYPE error = OMX_Init();
  SB_DCHECK(error == OMX_ErrorNone)
      << "OMX_Init() failed with error code: 0x" << std::hex << error << ".";
}

void InitializeOpenMax() {
  bool initialized =
      SbOnce(&s_open_max_initialization_once, DoInitializeOpenMax);
  SB_DCHECK(initialized);
}

}  // namespace

OpenMaxComponentBase::OpenMaxComponentBase(const char* name)
    : event_condition_variable_(mutex_),
      handle_(NULL),
      input_port_(kInvalidPort),
      output_port_(kInvalidPort) {
  InitializeOpenMax();

  OMX_CALLBACKTYPE callbacks;
  callbacks.EventHandler = OpenMaxComponentBase::EventHandler;
  callbacks.EmptyBufferDone = OpenMaxComponentBase::EmptyBufferDone;
  callbacks.FillBufferDone = OpenMaxComponentBase::FillBufferDone;

  OMX_ERRORTYPE error =
      OMX_GetHandle(&handle_, const_cast<char*>(name), this, &callbacks);
  SB_DCHECK(error == OMX_ErrorNone);

  for (size_t i = 0; i < SB_ARRAY_SIZE(kPortTypes); ++i) {
    OMX_PORT_PARAM_TYPE port;
    port.nSize = sizeof(OMX_PORT_PARAM_TYPE);
    port.nVersion.nVersion = OMX_VERSION;

    error = OMX_GetParameter(handle_, kPortTypes[i], &port);
    if (error == OMX_ErrorNone && port.nPorts == 2) {
      input_port_ = port.nStartPortNumber;
      output_port_ = input_port_ + 1;
      SendCommandAndWaitForCompletion(OMX_CommandPortDisable, input_port_);
      SendCommandAndWaitForCompletion(OMX_CommandPortDisable, output_port_);
      break;
    }
  }
  SB_CHECK(input_port_ != kInvalidPort);
  SB_CHECK(output_port_ != kInvalidPort);
  SB_DLOG(INFO) << "Opened \"" << name << "\" with port " << input_port_
                << " and " << output_port_;
}

OpenMaxComponentBase::~OpenMaxComponentBase() {
  if (!handle_) {
    return;
  }

  OMX_FreeHandle(handle_);
}

void OpenMaxComponentBase::SendCommand(OMX_COMMANDTYPE command, int param) {
  OMX_ERRORTYPE error = OMX_SendCommand(handle_, command, param, NULL);
  SB_DCHECK(error == OMX_ErrorNone);
}

void OpenMaxComponentBase::WaitForCommandCompletion() {
  for (;;) {
    ScopedLock scoped_lock(mutex_);
    for (EventDescriptions::iterator iter = event_descriptions_.begin();
         iter != event_descriptions_.end(); ++iter) {
      if (iter->event == OMX_EventCmdComplete) {
        event_descriptions_.erase(iter);
        return;
      }
      // Special case for OMX_CommandStateSet.
      if (iter->event == OMX_EventError && iter->data1 == OMX_ErrorSameState) {
        event_descriptions_.erase(iter);
        return;
      }
    }
    event_condition_variable_.Wait();
  }
}

void OpenMaxComponentBase::SendCommandAndWaitForCompletion(
    OMX_COMMANDTYPE command,
    int param) {
  SendCommand(command, param);
  WaitForCommandCompletion();
}

OMX_ERRORTYPE OpenMaxComponentBase::OnEvent(OMX_EVENTTYPE event,
                                            OMX_U32 data1,
                                            OMX_U32 data2,
                                            OMX_PTR event_data) {
  if (event == OMX_EventError && data1 != OMX_ErrorSameState) {
    OnErrorEvent(data1, data2, event_data);
    return OMX_ErrorNone;
  }

  if (event == OMX_EventPortSettingsChanged && data1 == output_port_) {
    OnOutputSettingChanged();
    return OMX_ErrorNone;
  }

  ScopedLock scoped_lock(mutex_);
  EventDescription event_desc;
  event_desc.event = event;
  event_desc.data1 = data1;
  event_desc.data2 = data2;
  event_desc.event_data = event_data;
  event_descriptions_.push_back(event_desc);
  event_condition_variable_.Signal();

  return OMX_ErrorNone;
}

// static
OMX_ERRORTYPE OpenMaxComponentBase::EventHandler(OMX_HANDLETYPE handle,
                                                 OMX_PTR app_data,
                                                 OMX_EVENTTYPE event,
                                                 OMX_U32 data1,
                                                 OMX_U32 data2,
                                                 OMX_PTR event_data) {
  SB_DCHECK(app_data != NULL);
  OpenMaxComponentBase* component =
      reinterpret_cast<OpenMaxComponentBase*>(app_data);
  SB_DCHECK(handle == component->handle_);

  return component->OnEvent(event, data1, data2, event_data);
}

// static
OMX_ERRORTYPE OpenMaxComponentBase::EmptyBufferDone(
    OMX_HANDLETYPE handle,
    OMX_PTR app_data,
    OMX_BUFFERHEADERTYPE* buffer) {
  SB_DCHECK(app_data != NULL);
  OpenMaxComponentBase* component =
      reinterpret_cast<OpenMaxComponentBase*>(app_data);
  SB_DCHECK(handle == component->handle_);

  return component->OnEmptyBufferDone(buffer);
}

// static
OMX_ERRORTYPE OpenMaxComponentBase::FillBufferDone(
    OMX_HANDLETYPE handle,
    OMX_PTR app_data,
    OMX_BUFFERHEADERTYPE* buffer) {
  SB_DCHECK(app_data != NULL);
  OpenMaxComponentBase* component =
      reinterpret_cast<OpenMaxComponentBase*>(app_data);
  SB_DCHECK(handle == component->handle_);

  component->OnFillBufferDone(buffer);

  return OMX_ErrorNone;
}

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard
