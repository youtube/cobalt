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

#include "starboard/shared/win32/media_transform.h"

#include "starboard/common/scoped_ptr.h"
#include "starboard/log.h"
#include "starboard/shared/win32/audio_transform.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/media_common.h"
#include "starboard/shared/win32/media_foundation_utils.h"

namespace starboard {
namespace shared {
namespace win32 {

using Microsoft::WRL::ComPtr;
using ::starboard::shared::starboard::ThreadChecker;

namespace {

template <typename T>
void ReleaseIfNotNull(T** ptr) {
  if (*ptr) {
    (*ptr)->Release();
    *ptr = NULL;
  }
}

}  // namespace

MediaTransform::MediaTransform(CLSID clsid)
    : thread_checker_(ThreadChecker::kSetThreadIdOnFirstCheck),
      state_(kCanAcceptInput),
      stream_begun_(false),
      discontinuity_(true),
      throttle_inputs_(false) {
  HRESULT hr = CreateDecoderTransform(clsid, &transform_);
  CheckResult(hr);
}

MediaTransform::MediaTransform(
    const Microsoft::WRL::ComPtr<IMFTransform>& transform)
    : transform_(transform),
      thread_checker_(ThreadChecker::kSetThreadIdOnFirstCheck),
      state_(kCanAcceptInput),
      stream_begun_(false),
      discontinuity_(true),
      throttle_inputs_(false) {
  SB_DCHECK(transform_);
}

bool MediaTransform::TryWrite(const ComPtr<IMFSample>& input) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != kCanAcceptInput) {
    return false;
  }

  if (!stream_begun_) {
    SendMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING);
    SendMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM);
    stream_begun_ = true;
  }

  HRESULT hr = transform_->ProcessInput(kStreamId, input.Get(), 0);

  if (SUCCEEDED(hr)) {
    // Some transforms do not return MF_E_NOTACCEPTING. To avoid flooding
    // these transforms, input is only allowed when ProcessOutput returns
    // MF_E_TRANSFORM_NEED_MORE_INPUT.
    if (throttle_inputs_) {
      state_ = kCanProvideOutput;
    }
    return true;
  }
  if (hr == MF_E_NOTACCEPTING) {
    state_ = kCanProvideOutput;
    return false;
  }
  SB_NOTREACHED() << "Unexpected return value " << hr;
  return false;
}

ComPtr<IMFSample> MediaTransform::TryRead(ComPtr<IMFMediaType>* new_type) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(new_type);

  if (state_ == kDrained) {
    return NULL;
  }

  ComPtr<IMFSample> sample;
  HRESULT hr = ProcessOutput(&sample);

  if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
    hr = transform_->GetOutputAvailableType(kStreamId,
                                            0,  // TypeIndex
                                            new_type->GetAddressOf());
    CheckResult(hr);
    SetOutputType(*new_type);

    hr = ProcessOutput(&sample);
  }

  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    if (state_ == kCanProvideOutput) {
      state_ = kCanAcceptInput;
    }
    if (state_ == kDraining) {
      state_ = kDrained;
    }
    return NULL;
  }

  if (FAILED(hr)) {
    // Sometimes the decryptor refuse to emit output after shutting down.
    SB_DCHECK(hr == MF_E_INVALIDREQUEST);
    if (state_ == kDraining) {
      state_ = kDrained;
    }
    return NULL;
  }

  SB_DCHECK(sample);

  if (discontinuity_) {
    sample->SetUINT32(MFSampleExtension_Discontinuity, TRUE);
    discontinuity_ = false;
  }

  return sample;
}

void MediaTransform::Drain() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(state_ != kDraining && state_ != kDrained);

  if (state_ == kDraining || state_ == kDrained) {
    return;
  }

  if (!stream_begun_) {
    state_ = kDrained;
    return;
  }

  SendMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM);
  SendMessage(MFT_MESSAGE_COMMAND_DRAIN);
  state_ = kDraining;
}

bool MediaTransform::draining() const {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  return state_ == kDraining;
}

bool MediaTransform::drained() const {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  return state_ == kDrained;
}

void MediaTransform::ResetFromDrained() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(drained());

  state_ = kCanAcceptInput;
  stream_begun_ = false;
  discontinuity_ = true;
}

ComPtr<IMFMediaType> MediaTransform::GetCurrentInputType() {
  ComPtr<IMFMediaType> type;
  HRESULT hr = transform_->GetInputCurrentType(kStreamId, type.GetAddressOf());
  CheckResult(hr);
  return type;
}

void MediaTransform::SetInputType(const ComPtr<IMFMediaType>& input_type) {
  HRESULT hr = transform_->SetInputType(0, input_type.Get(), 0);
  CheckResult(hr);
}

std::vector<ComPtr<IMFMediaType>> MediaTransform::GetAvailableInputTypes() {
  return GetAllInputMediaTypes(kStreamId, transform_.Get());
}

ComPtr<IMFMediaType> MediaTransform::GetCurrentOutputType() {
  ComPtr<IMFMediaType> type;
  HRESULT hr = transform_->GetOutputCurrentType(kStreamId, type.GetAddressOf());
  CheckResult(hr);
  return type;
}

void MediaTransform::SetOutputType(const ComPtr<IMFMediaType>& output_type) {
  HRESULT hr = transform_->SetOutputType(0, output_type.Get(), 0);
  CheckResult(hr);
}

std::vector<ComPtr<IMFMediaType>> MediaTransform::GetAvailableOutputTypes() {
  std::vector<ComPtr<IMFMediaType>> output_types;

  for (DWORD i = 0;; ++i) {
    ComPtr<IMFMediaType> curr_type;
    HRESULT hr = transform_->GetOutputAvailableType(kStreamId, i,
                                                    curr_type.GetAddressOf());
    if (FAILED(hr)) {
      break;
    }
    output_types.push_back(curr_type);
  }

  return output_types;
}

void MediaTransform::SetOutputTypeBySubType(GUID subtype) {
  for (int index = 0;; ++index) {
    ComPtr<IMFMediaType> media_type;
    HRESULT hr =
        transform_->GetOutputAvailableType(kStreamId, index, &media_type);
    if (SUCCEEDED(hr)) {
      GUID media_sub_type = {};
      media_type->GetGUID(MF_MT_SUBTYPE, &media_sub_type);
      if (media_sub_type == subtype) {
        SetOutputType(media_type);
        return;
      }
    } else {
      SB_DCHECK(hr == MF_E_NO_MORE_TYPES);
      break;
    }
  }
  SB_NOTREACHED();
}

ComPtr<IMFAttributes> MediaTransform::GetAttributes() {
  ComPtr<IMFAttributes> attributes;
  HRESULT hr = transform_->GetAttributes(attributes.GetAddressOf());
  CheckResult(hr);
  return attributes;
}

ComPtr<IMFAttributes> MediaTransform::GetOutputStreamAttributes() {
  ComPtr<IMFAttributes> attributes;
  HRESULT hr =
      transform_->GetOutputStreamAttributes(0, attributes.GetAddressOf());
  CheckResult(hr);
  return attributes;
}

ComPtr<IMFSampleProtection> MediaTransform::GetSampleProtection() {
  ComPtr<IMFSampleProtection> sample_protection;
  HRESULT hr = transform_.As(&sample_protection);
  CheckResult(hr);
  return sample_protection;
}

void MediaTransform::GetStreamCount(DWORD* input_stream_count,
                                    DWORD* output_stream_count) {
  SB_DCHECK(input_stream_count);
  SB_DCHECK(output_stream_count);
  HRESULT hr =
      transform_->GetStreamCount(input_stream_count, output_stream_count);
  CheckResult(hr);
}

void MediaTransform::SendMessage(MFT_MESSAGE_TYPE msg, ULONG_PTR data /*= 0*/) {
  HRESULT hr = transform_->ProcessMessage(msg, data);
  CheckResult(hr);
}

void MediaTransform::Reset() {
  if (stream_begun_) {
    SendMessage(MFT_MESSAGE_COMMAND_FLUSH);
  }
  state_ = kCanAcceptInput;
  discontinuity_ = true;
  thread_checker_.Detach();
}

void MediaTransform::PrepareOutputDataBuffer(
    MFT_OUTPUT_DATA_BUFFER* output_data_buffer) {
  output_data_buffer->dwStreamID = kStreamId;
  output_data_buffer->pSample = NULL;
  output_data_buffer->dwStatus = 0;
  output_data_buffer->pEvents = NULL;

  MFT_OUTPUT_STREAM_INFO output_stream_info;
  HRESULT hr = transform_->GetOutputStreamInfo(kStreamId, &output_stream_info);
  CheckResult(hr);

  static const DWORD kFlagAutoAllocateMemory =
      MFT_OUTPUT_STREAM_PROVIDES_SAMPLES |
      MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES;
  if ((output_stream_info.dwFlags & kFlagAutoAllocateMemory) != 0) {
    // Try to let the IMFTransform allocate the memory if possible.
    return;
  }

  ComPtr<IMFSample> sample;
  hr = MFCreateSample(&sample);
  CheckResult(hr);

  ComPtr<IMFMediaBuffer> buffer;
  hr = MFCreateAlignedMemoryBuffer(output_stream_info.cbSize,
                                   output_stream_info.cbAlignment, &buffer);
  CheckResult(hr);

  hr = sample->AddBuffer(buffer.Get());
  CheckResult(hr);

  output_data_buffer->pSample = sample.Detach();
}

HRESULT MediaTransform::ProcessOutput(ComPtr<IMFSample>* sample) {
  SB_DCHECK(sample);

  MFT_OUTPUT_DATA_BUFFER output_data_buffer;
  PrepareOutputDataBuffer(&output_data_buffer);

  const DWORD kFlags = 0;
  const DWORD kNumberOfBuffers = 1;
  DWORD status = 0;
  HRESULT hr = transform_->ProcessOutput(kFlags, kNumberOfBuffers,
                                         &output_data_buffer, &status);

  SB_DCHECK(!output_data_buffer.pEvents);

  *sample = output_data_buffer.pSample;
  ReleaseIfNotNull(&output_data_buffer.pEvents);
  ReleaseIfNotNull(&output_data_buffer.pSample);

  if (output_data_buffer.dwStatus == MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE) {
    hr = MF_E_TRANSFORM_NEED_MORE_INPUT;
  }

  return hr;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
