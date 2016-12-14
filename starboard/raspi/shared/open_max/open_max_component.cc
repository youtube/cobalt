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

#include "starboard/raspi/shared/open_max/open_max_component.h"

#include <algorithm>

#include "starboard/configuration.h"
#include "starboard/once.h"
#include "starboard/thread.h"

namespace starboard {
namespace raspi {
namespace shared {
namespace open_max {

namespace {

const int kInvalidPort = -1;

OMX_INDEXTYPE kPortTypes[] = {OMX_IndexParamAudioInit, OMX_IndexParamVideoInit,
                              OMX_IndexParamImageInit, OMX_IndexParamOtherInit};

SbOnceControl s_open_max_initialization_once = SB_ONCE_INITIALIZER;

void DoInitializeOpenMax() {
  OMX_ERRORTYPE error = OMX_Init();
  SB_DCHECK(error == OMX_ErrorNone);
}

void InitializeOpenMax() {
  bool initialized =
      SbOnce(&s_open_max_initialization_once, DoInitializeOpenMax);
  SB_DCHECK(initialized);
}

}  // namespace

OpenMaxComponent::OpenMaxComponent(const char* name)
    : event_condition_variable_(mutex_),
      handle_(NULL),
      input_port_(kInvalidPort),
      output_port_(kInvalidPort),
      output_setting_changed_(false),
      fill_buffer_thread_(kSbThreadInvalid),
      kill_fill_buffer_thread_(false),
      output_available_condition_variable_(mutex_) {
  InitializeOpenMax();

  OMX_CALLBACKTYPE callbacks;
  callbacks.EventHandler = OpenMaxComponent::EventHandler;
  callbacks.EmptyBufferDone = OpenMaxComponent::EmptyBufferDone;
  callbacks.FillBufferDone = OpenMaxComponent::FillBufferDone;

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

  SendCommandAndWaitForCompletion(OMX_CommandStateSet, OMX_StateIdle);
}

OpenMaxComponent::~OpenMaxComponent() {
  if (!handle_) {
    return;
  }

  SendCommandAndWaitForCompletion(OMX_CommandStateSet, OMX_StateIdle);

  SendCommandAndWaitForCompletion(OMX_CommandFlush, input_port_);
  SendCommandAndWaitForCompletion(OMX_CommandFlush, output_port_);

  SendCommand(OMX_CommandPortDisable, input_port_);
  for (size_t i = 0; i < input_buffers_.size(); ++i) {
    OMX_ERRORTYPE error =
        OMX_FreeBuffer(handle_, input_port_, input_buffers_[i]);
    SB_DCHECK(error == OMX_ErrorNone);
  }
  WaitForCommandCompletion();

  DisableOutputPort();

  SendCommandAndWaitForCompletion(OMX_CommandStateSet, OMX_StateLoaded);
  OMX_FreeHandle(handle_);
}

void OpenMaxComponent::Start() {
  EnableInputPortAndAllocateBuffers();
  SendCommandAndWaitForCompletion(OMX_CommandStateSet, OMX_StateExecuting);
}

void OpenMaxComponent::Flush() {
  output_setting_changed_ = SbThreadIsValid(fill_buffer_thread_);
  DisableOutputPort();
  SendCommandAndWaitForCompletion(OMX_CommandFlush, input_port_);
  SendCommandAndWaitForCompletion(OMX_CommandFlush, output_port_);
}

void OpenMaxComponent::WriteData(const void* data,
                                 size_t size,
                                 SbTime timestamp) {
  size_t offset = 0;

  while (offset != size) {
    OMX_BUFFERHEADERTYPE* buffer_header = GetUnusedInputBuffer();

    int size_to_append = std::min(size - offset, buffer_header->nAllocLen);
    buffer_header->nOffset = 0;
    buffer_header->nFilledLen = size_to_append;
    buffer_header->nFlags = 0;

    buffer_header->nTimeStamp.nLowPart = timestamp;
    buffer_header->nTimeStamp.nHighPart = timestamp >> 32;

    memcpy(buffer_header->pBuffer, (const char*)data + offset, size_to_append);
    offset += size_to_append;

    OMX_ERRORTYPE error = OMX_EmptyThisBuffer(handle_, buffer_header);
    SB_DCHECK(error == OMX_ErrorNone);
  }
}

void OpenMaxComponent::WriteEOS() {
  OMX_BUFFERHEADERTYPE* buffer_header = GetUnusedInputBuffer();

  buffer_header->nOffset = 0;
  buffer_header->nFilledLen = 0;
  buffer_header->nFlags = OMX_BUFFERFLAG_EOS;

  OMX_ERRORTYPE error = OMX_EmptyThisBuffer(handle_, buffer_header);
  SB_DCHECK(error == OMX_ErrorNone);
}

OMX_BUFFERHEADERTYPE* OpenMaxComponent::PeekNextOutputBuffer() {
  {
    ScopedLock scoped_lock(mutex_);

    if (!output_setting_changed_ && output_buffers_.empty()) {
      return NULL;
    }
  }

  if (output_setting_changed_ && filled_output_buffers_.empty()) {
    EnableOutputPortAndAllocateBuffer();
    output_setting_changed_ = false;
  }

  ScopedLock scoped_lock(mutex_);
  return filled_output_buffers_.empty() ? NULL : filled_output_buffers_.front();
}

void OpenMaxComponent::DropNextOutputBuffer() {
  ScopedLock scoped_lock(mutex_);
  SB_DCHECK(!filled_output_buffers_.empty());
  OMX_BUFFERHEADERTYPE* buffer = filled_output_buffers_.front();
  filled_output_buffers_.pop();

  if (output_setting_changed_) {
    return;
  }

  unused_output_buffers_.push(buffer);
  output_available_condition_variable_.Signal();
}

void OpenMaxComponent::SendCommand(OMX_COMMANDTYPE command, int param) {
  OMX_ERRORTYPE error = OMX_SendCommand(handle_, command, param, NULL);
  SB_DCHECK(error == OMX_ErrorNone);
}

void OpenMaxComponent::WaitForCommandCompletion() {
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

void OpenMaxComponent::SendCommandAndWaitForCompletion(OMX_COMMANDTYPE command,
                                                       int param) {
  SendCommand(command, param);
  WaitForCommandCompletion();
}

void OpenMaxComponent::DisableOutputPort() {
  if (SbThreadIsValid(fill_buffer_thread_)) {
    {
      ScopedLock scoped_lock(mutex_);
      kill_fill_buffer_thread_ = true;
      output_available_condition_variable_.Signal();
    }
    SbThreadJoin(fill_buffer_thread_, NULL);
    fill_buffer_thread_ = kSbThreadInvalid;
    kill_fill_buffer_thread_ = false;
  }
  if (!output_buffers_.empty()) {
    SendCommandAndWaitForCompletion(OMX_CommandFlush, output_port_);

    SendCommand(OMX_CommandPortDisable, output_port_);
    for (size_t i = 0; i < output_buffers_.size(); ++i) {
      OMX_ERRORTYPE error =
          OMX_FreeBuffer(handle_, output_port_, output_buffers_[i]);
      SB_DCHECK(error == OMX_ErrorNone);
    }
    output_buffers_.clear();
    WaitForCommandCompletion();
    while (!filled_output_buffers_.empty()) {
      filled_output_buffers_.pop();
    }
    while (!unused_output_buffers_.empty()) {
      unused_output_buffers_.pop();
    }
  }
}

void OpenMaxComponent::EnableInputPortAndAllocateBuffers() {
  SB_DCHECK(input_buffers_.empty());

  OMXParamPortDefinition port_definition;
  GetInputPortParam(&port_definition);
  if (OnEnableInputPort(&port_definition)) {
    SetPortParam(port_definition);
  }

  SendCommand(OMX_CommandPortEnable, input_port_);

  for (int i = 0; i < port_definition.nBufferCountActual; ++i) {
    OMX_BUFFERHEADERTYPE* buffer;
    OMX_ERRORTYPE error = OMX_AllocateBuffer(handle_, &buffer, input_port_,
                                             NULL, port_definition.nBufferSize);
    SB_DCHECK(error == OMX_ErrorNone);
    input_buffers_.push_back(buffer);
    unused_input_buffers_.push(buffer);
  }

  WaitForCommandCompletion();
}

void OpenMaxComponent::EnableOutputPortAndAllocateBuffer() {
  DisableOutputPort();

  OMXParamPortDefinition output_port_definition;

  GetOutputPortParam(&output_port_definition);
  if (OnEnableOutputPort(&output_port_definition)) {
    SetPortParam(output_port_definition);
  }

  SendCommand(OMX_CommandPortEnable, output_port_);

  output_buffers_.reserve(output_port_definition.nBufferCountActual);
  for (int i = 0; i < output_port_definition.nBufferCountActual; ++i) {
    OMX_BUFFERHEADERTYPE* buffer;
    OMX_ERRORTYPE error =
        OMX_AllocateBuffer(handle_, &buffer, output_port_, NULL,
                           output_port_definition.nBufferSize);
    SB_DCHECK(error == OMX_ErrorNone);
    output_buffers_.push_back(buffer);
  }

  WaitForCommandCompletion();

  for (size_t i = 0; i < output_buffers_.size(); ++i) {
    unused_output_buffers_.push(output_buffers_[i]);
  }

  SB_DCHECK(!kill_fill_buffer_thread_);
  fill_buffer_thread_ =
      SbThreadCreate(0, kSbThreadPriorityHigh, kSbThreadNoAffinity, true,
                     "output_buffer_filler",
                     &OpenMaxComponent::FillBufferThreadEntryPoint, this);
}

OMX_BUFFERHEADERTYPE* OpenMaxComponent::GetUnusedInputBuffer() {
  for (;;) {
    {
      ScopedLock scoped_lock(mutex_);
      if (!unused_input_buffers_.empty()) {
        OMX_BUFFERHEADERTYPE* buffer_header = unused_input_buffers_.front();
        unused_input_buffers_.pop();
        return buffer_header;
      }
    }
    SbThreadSleep(kSbTimeMillisecond);
  }
  SB_NOTREACHED();
  return NULL;
}

OMX_ERRORTYPE OpenMaxComponent::OnEvent(OMX_EVENTTYPE event,
                                        OMX_U32 data1,
                                        OMX_U32 data2,
                                        OMX_PTR event_data) {
  if (event == OMX_EventError && data1 != OMX_ErrorSameState) {
    SB_NOTREACHED() << "OMX_EventError received with " << std::hex << data1
                    << " " << data2;
    return OMX_ErrorNone;
  }

  ScopedLock scoped_lock(mutex_);
  if (event == OMX_EventPortSettingsChanged && data1 == output_port_) {
    output_setting_changed_ = true;
    OnOutputSettingChanged();
    return OMX_ErrorNone;
  }
  EventDescription event_desc;
  event_desc.event = event;
  event_desc.data1 = data1;
  event_desc.data2 = data2;
  event_desc.event_data = event_data;
  event_descriptions_.push_back(event_desc);
  event_condition_variable_.Signal();

  return OMX_ErrorNone;
}

OMX_ERRORTYPE OpenMaxComponent::OnEmptyBufferDone(
    OMX_BUFFERHEADERTYPE* buffer) {
  ScopedLock scoped_lock(mutex_);
  unused_input_buffers_.push(buffer);
}

void OpenMaxComponent::OnFillBufferDone(OMX_BUFFERHEADERTYPE* buffer) {
  {
    ScopedLock scoped_lock(mutex_);
    if (output_setting_changed_) {
      return;
    }
    filled_output_buffers_.push(buffer);
  }
  OnOutputBufferFilled();
}

void OpenMaxComponent::RunFillBufferLoop() {
  for (;;) {
    OMX_BUFFERHEADERTYPE* buffer = NULL;
    {
      ScopedLock scoped_lock(mutex_);
      if (kill_fill_buffer_thread_) {
        break;
      }
      if (!unused_output_buffers_.empty()) {
        buffer = unused_output_buffers_.front();
        unused_output_buffers_.pop();
      } else {
        output_available_condition_variable_.Wait();
        continue;
      }
    }
    if (buffer) {
      buffer->nFilledLen = 0;

      OMX_ERRORTYPE error = OMX_FillThisBuffer(handle_, buffer);
      SB_DCHECK(error == OMX_ErrorNone);
    }
  }
}

// static
OMX_ERRORTYPE OpenMaxComponent::EventHandler(OMX_HANDLETYPE handle,
                                             OMX_PTR app_data,
                                             OMX_EVENTTYPE event,
                                             OMX_U32 data1,
                                             OMX_U32 data2,
                                             OMX_PTR event_data) {
  SB_DCHECK(app_data != NULL);
  OpenMaxComponent* component = reinterpret_cast<OpenMaxComponent*>(app_data);
  SB_DCHECK(handle == component->handle_);

  return component->OnEvent(event, data1, data2, event_data);
}

// static
OMX_ERRORTYPE OpenMaxComponent::EmptyBufferDone(OMX_HANDLETYPE handle,
                                                OMX_PTR app_data,
                                                OMX_BUFFERHEADERTYPE* buffer) {
  SB_DCHECK(app_data != NULL);
  OpenMaxComponent* component = reinterpret_cast<OpenMaxComponent*>(app_data);
  SB_DCHECK(handle == component->handle_);

  return component->OnEmptyBufferDone(buffer);
}

// static
OMX_ERRORTYPE OpenMaxComponent::FillBufferDone(OMX_HANDLETYPE handle,
                                               OMX_PTR app_data,
                                               OMX_BUFFERHEADERTYPE* buffer) {
  SB_DCHECK(app_data != NULL);
  OpenMaxComponent* component = reinterpret_cast<OpenMaxComponent*>(app_data);
  SB_DCHECK(handle == component->handle_);

  component->OnFillBufferDone(buffer);

  return OMX_ErrorNone;
}

// static
void* OpenMaxComponent::FillBufferThreadEntryPoint(void* context) {
  OpenMaxComponent* component = reinterpret_cast<OpenMaxComponent*>(context);
  component->RunFillBufferLoop();
  return NULL;
}

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard
