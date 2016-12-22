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

OpenMaxComponent::OpenMaxComponent(const char* name)
    : OpenMaxComponentBase(name),
      output_setting_changed_(false),
      outstanding_output_buffers_(0) {}

void OpenMaxComponent::Start() {
  SendCommandAndWaitForCompletion(OMX_CommandStateSet, OMX_StateIdle);
  EnableInputPortAndAllocateBuffers();
  SendCommandAndWaitForCompletion(OMX_CommandStateSet, OMX_StateExecuting);
}

void OpenMaxComponent::Flush() {
  DisableOutputPort();
  SendCommandAndWaitForCompletion(OMX_CommandFlush, input_port_);
  SendCommandAndWaitForCompletion(OMX_CommandFlush, output_port_);
}

int OpenMaxComponent::WriteData(const void* data, int size, DataType type,
                                SbTime timestamp) {
  int offset = 0;

  while (offset < size) {
    OMX_BUFFERHEADERTYPE* buffer_header = GetUnusedInputBuffer();
    if (buffer_header == NULL) {
      return offset;
    }

    int size_to_append = size - offset;
    if (size_to_append > buffer_header->nAllocLen) {
      size_to_append = buffer_header->nAllocLen;
      buffer_header->nFlags = 0;
    } else if (type == kDataEOS) {
      buffer_header->nFlags = OMX_BUFFERFLAG_EOS;
    } else {
      buffer_header->nFlags = 0;
    }
    buffer_header->nOffset = 0;
    buffer_header->nFilledLen = size_to_append;

    buffer_header->nTimeStamp.nLowPart = timestamp;
    buffer_header->nTimeStamp.nHighPart = timestamp >> 32;

    memcpy(buffer_header->pBuffer, reinterpret_cast<const char*>(data) + offset,
           size_to_append);
    offset += size_to_append;

    OMX_ERRORTYPE error = OMX_EmptyThisBuffer(handle_, buffer_header);
    SB_DCHECK(error == OMX_ErrorNone);
  }

  return offset;
}

bool OpenMaxComponent::WriteEOS() {
  OMX_BUFFERHEADERTYPE* buffer_header;
  if ((buffer_header = GetUnusedInputBuffer()) == NULL) {
    return false;
  }

  buffer_header->nOffset = 0;
  buffer_header->nFilledLen = 0;
  buffer_header->nFlags = OMX_BUFFERFLAG_EOS;

  OMX_ERRORTYPE error = OMX_EmptyThisBuffer(handle_, buffer_header);
  SB_DCHECK(error == OMX_ErrorNone);
  return true;
}

OMX_BUFFERHEADERTYPE* OpenMaxComponent::GetOutputBuffer() {
  bool enable_output_port = false;

  {
    ScopedLock scoped_lock(mutex_);

    if (!output_setting_changed_ && output_buffers_.empty()) {
      return NULL;
    }

    enable_output_port = output_setting_changed_ &&
                         filled_output_buffers_.empty() &&
                         outstanding_output_buffers_ == 0;
  }

  if (enable_output_port) {
    EnableOutputPortAndAllocateBuffer();
    output_setting_changed_ = false;
  }

  ScopedLock scoped_lock(mutex_);
  if (filled_output_buffers_.empty()) {
    return NULL;
  }
  OMX_BUFFERHEADERTYPE* buffer = filled_output_buffers_.front();
  filled_output_buffers_.pop();
  ++outstanding_output_buffers_;
  return buffer;
}

void OpenMaxComponent::DropOutputBuffer(OMX_BUFFERHEADERTYPE* buffer) {
  {
    ScopedLock scoped_lock(mutex_);
    SB_DCHECK(outstanding_output_buffers_ > 0);
    --outstanding_output_buffers_;

    if (output_setting_changed_) {
      return;
    }
  }
  buffer->nFilledLen = 0;
  OMX_ERRORTYPE error = OMX_FillThisBuffer(handle_, buffer);
  SB_DCHECK(error == OMX_ErrorNone);
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
}

void OpenMaxComponent::OnErrorEvent(OMX_U32 data1, OMX_U32 data2,
                                    OMX_PTR /* event_data */) {
  SB_NOTREACHED() << "OMX_EventError received with " << std::hex << data1
                  << " " << data2;
}

void OpenMaxComponent::DisableOutputPort() {
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
  }
  outstanding_output_buffers_ = 0;
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
    free_input_buffers_.push(buffer);
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
    output_buffers_[i]->nFilledLen = 0;
    OMX_ERRORTYPE error = OMX_FillThisBuffer(handle_, output_buffers_[i]);
    SB_DCHECK(error == OMX_ErrorNone);
  }
}

OMX_BUFFERHEADERTYPE* OpenMaxComponent::GetUnusedInputBuffer() {
  ScopedLock scoped_lock(mutex_);
  if (!free_input_buffers_.empty()) {
    OMX_BUFFERHEADERTYPE* buffer_header = free_input_buffers_.front();
    free_input_buffers_.pop();
    return buffer_header;
  }
  return NULL;
}

void OpenMaxComponent::OnOutputSettingChanged() {
  ScopedLock scoped_lock(mutex_);
  output_setting_changed_ = true;
}

OMX_ERRORTYPE OpenMaxComponent::OnEmptyBufferDone(
    OMX_BUFFERHEADERTYPE* buffer) {
  ScopedLock scoped_lock(mutex_);
  free_input_buffers_.push(buffer);
}

void OpenMaxComponent::OnFillBufferDone(OMX_BUFFERHEADERTYPE* buffer) {
  ScopedLock scoped_lock(mutex_);
  filled_output_buffers_.push(buffer);
}

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard
