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

OpenMaxComponent::OpenMaxComponent(const char* name, size_t minimum_output_size)
    : condition_variable_(mutex_),
      minimum_output_size_(minimum_output_size),
      handle_(NULL),
      input_port_(kInvalidPort),
      output_port_(kInvalidPort),
      output_setting_changed_(false),
      output_buffer_(NULL),
      output_buffer_filled_(false) {
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
  for (BufferHeaders::iterator iter = unused_buffers_.begin();
       iter != unused_buffers_.end(); ++iter) {
    OMX_ERRORTYPE error = OMX_FreeBuffer(handle_, input_port_, *iter);
    SB_DCHECK(error == OMX_ErrorNone);
  }
  WaitForCommandCompletion();

  SendCommand(OMX_CommandPortDisable, output_port_);
  if (output_buffer_) {
    OMX_ERRORTYPE error = OMX_FreeBuffer(handle_, output_port_, output_buffer_);
    SB_DCHECK(error == OMX_ErrorNone);
  }
  WaitForCommandCompletion();

  SendCommandAndWaitForCompletion(OMX_CommandStateSet, OMX_StateLoaded);
  OMX_FreeHandle(handle_);
}

void OpenMaxComponent::Start() {
  EnableInputPortAndAllocateBuffers();
  SendCommandAndWaitForCompletion(OMX_CommandStateSet, OMX_StateExecuting);
}

void OpenMaxComponent::Flush() {
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

bool OpenMaxComponent::ReadVideoFrame(VideoFrame* frame) {
  {
    ScopedLock scoped_lock(mutex_);
    if (output_buffer_ && !output_buffer_filled_) {
      return false;
    }
    if (!output_setting_changed_) {
      return false;
    }
  }
  SB_DCHECK(output_setting_changed_);
  if (!output_buffer_) {
    GetOutputPortParam(&output_port_definition_);
    SB_DCHECK(output_port_definition_.format.video.eColorFormat ==
              OMX_COLOR_FormatYUV420PackedPlanar);
    EnableOutputPortAndAllocateBuffer();
    return false;
  }

  if (output_buffer_->nFlags & OMX_BUFFERFLAG_EOS) {
    *frame = VideoFrame();
    return true;
  }
  SbMediaTime timestamp =
      ((output_buffer_->nTimeStamp.nHighPart * 0x100000000ull) +
       output_buffer_->nTimeStamp.nLowPart) *
      kSbMediaTimeSecond / kSbTimeSecond;
  int width = output_port_definition_.format.video.nFrameWidth;
  int height = output_port_definition_.format.video.nSliceHeight;
  int pitch = output_port_definition_.format.video.nStride;
  *frame = VideoFrame::CreateYV12Frame(
      width, height, pitch, timestamp, output_buffer_->pBuffer,
      output_buffer_->pBuffer + pitch * height,
      output_buffer_->pBuffer + pitch * height * 5 / 4);
  output_buffer_filled_ = false;
  output_buffer_->nFilledLen = 0;
  OMX_ERRORTYPE error = OMX_FillThisBuffer(handle_, output_buffer_);
  SB_DCHECK(error == OMX_ErrorNone);
  return true;
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
    condition_variable_.Wait();
  }
}

void OpenMaxComponent::SendCommandAndWaitForCompletion(OMX_COMMANDTYPE command,
                                                       int param) {
  SendCommand(command, param);
  WaitForCommandCompletion();
}

void OpenMaxComponent::EnableInputPortAndAllocateBuffers() {
  SB_DCHECK(unused_buffers_.empty());

  OMXParamPortDefinition port_definition;
  GetInputPortParam(&port_definition);

  SendCommand(OMX_CommandPortEnable, input_port_);

  unused_buffers_.resize(port_definition.nBufferCountActual);
  for (int i = 0; i != port_definition.nBufferCountActual; ++i) {
    OMX_ERRORTYPE error =
        OMX_AllocateBuffer(handle_, &unused_buffers_[i], input_port_, NULL,
                           port_definition.nBufferSize);
    SB_DCHECK(error == OMX_ErrorNone);
  }

  WaitForCommandCompletion();
}

void OpenMaxComponent::EnableOutputPortAndAllocateBuffer() {
  if (output_buffer_ != NULL) {
    return;
  }

  SendCommand(OMX_CommandPortEnable, output_port_);

  OMX_ERRORTYPE error = OMX_AllocateBuffer(
      handle_, &output_buffer_, output_port_, NULL,
      std::max(output_port_definition_.nBufferSize, minimum_output_size_));
  SB_DCHECK(error == OMX_ErrorNone);
  WaitForCommandCompletion();

  error = OMX_FillThisBuffer(handle_, output_buffer_);
  SB_DCHECK(error == OMX_ErrorNone);
}

OMX_BUFFERHEADERTYPE* OpenMaxComponent::GetUnusedInputBuffer() {
  for (;;) {
    ScopedLock scoped_lock(mutex_);
    if (!unused_buffers_.empty()) {
      OMX_BUFFERHEADERTYPE* buffer_header = unused_buffers_.back();
      unused_buffers_.pop_back();
      return buffer_header;
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
    return OMX_ErrorNone;
  }
  EventDescription event_desc;
  event_desc.event = event;
  event_desc.data1 = data1;
  event_desc.data2 = data2;
  event_desc.event_data = event_data;
  event_descriptions_.push_back(event_desc);
  condition_variable_.Signal();

  return OMX_ErrorNone;
}

OMX_ERRORTYPE OpenMaxComponent::OnEmptyBufferDone(
    OMX_BUFFERHEADERTYPE* buffer) {
  ScopedLock scoped_lock(mutex_);
  unused_buffers_.push_back(buffer);
}

void OpenMaxComponent::OnFillBufferDone(OMX_BUFFERHEADERTYPE* buffer) {
  ScopedLock scoped_lock(mutex_);
  SB_DCHECK(!output_buffer_filled_);
  output_buffer_filled_ = true;
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

}  // namespace open_max
}  // namespace shared
}  // namespace raspi
}  // namespace starboard
