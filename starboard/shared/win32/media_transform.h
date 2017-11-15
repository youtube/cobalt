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

#ifndef STARBOARD_SHARED_WIN32_MEDIA_TRANSFORM_H_
#define STARBOARD_SHARED_WIN32_MEDIA_TRANSFORM_H_

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <wrl\client.h>

#include <vector>

#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {
namespace shared {
namespace win32 {

// Wrapper class for IMFTransform with the following functionalities:
// 1. State management:
//    It supports a one way life cycle from "kCanAcceptInput/kCanProvideOutput"
//    to "kDraining" then to "kDrained".
// 2. Manages states like input/output types, various attributes, and sample
//    protection, etc.
// 3. Send message to the underlying transform.
// This simplifies the implementation of higher level decoder mechanism that
// may deal with two IMFTransforms: one decoder and one decryptor.
class MediaTransform {
 public:
  enum { kStreamId = 0 };
  explicit MediaTransform(CLSID clsid);
  explicit MediaTransform(
      const Microsoft::WRL::ComPtr<IMFTransform>& transform);

  // By default, the input throttle is disabled, and inputs can be written to
  // the transform until rejected; i.e. the transform is kept full of inputs.
  // However, some transforms may never report that they are full, so enabling
  // the input throttle will allow inputs only when the transform reports that
  // it needs more input.
  void EnableInputThrottle(bool enable) { throttle_inputs_ = enable; }

  bool TryWrite(const Microsoft::WRL::ComPtr<IMFSample>& input);
  Microsoft::WRL::ComPtr<IMFSample> TryRead(
      Microsoft::WRL::ComPtr<IMFMediaType>* new_type);
  void Drain();
  bool draining() const;
  bool drained() const;
  // Once the transform is drained, this function can be called to allow the
  // transform to accept new input as a newly created transform.  This allows
  // the reuse of existing transform without re-negotiating all types and
  // attributes.
  void ResetFromDrained();

  Microsoft::WRL::ComPtr<IMFMediaType> GetCurrentInputType();
  void SetInputType(const Microsoft::WRL::ComPtr<IMFMediaType>& type);
  std::vector<Microsoft::WRL::ComPtr<IMFMediaType>> GetAvailableInputTypes();

  Microsoft::WRL::ComPtr<IMFMediaType> GetCurrentOutputType();
  void SetOutputType(const Microsoft::WRL::ComPtr<IMFMediaType>& type);
  std::vector<Microsoft::WRL::ComPtr<IMFMediaType>> GetAvailableOutputTypes();
  void SetOutputTypeBySubType(GUID subtype);

  Microsoft::WRL::ComPtr<IMFAttributes> GetAttributes();
  Microsoft::WRL::ComPtr<IMFAttributes> GetOutputStreamAttributes();

  Microsoft::WRL::ComPtr<IMFSampleProtection> GetSampleProtection();
  void GetStreamCount(DWORD* input_streamcount, DWORD* output_stream_count);

  void SendMessage(MFT_MESSAGE_TYPE msg, ULONG_PTR data = 0);

  // Reset the media transform to its original state.
  void Reset();

 private:
  enum State { kCanAcceptInput, kCanProvideOutput, kDraining, kDrained };

  void PrepareOutputDataBuffer(MFT_OUTPUT_DATA_BUFFER* output_data_buffer);
  HRESULT ProcessOutput(Microsoft::WRL::ComPtr<IMFSample>* sample);

  Microsoft::WRL::ComPtr<IMFTransform> transform_;

  ::starboard::shared::starboard::ThreadChecker thread_checker_;
  State state_;
  bool stream_begun_;
  bool discontinuity_;
  bool throttle_inputs_;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_MEDIA_TRANSFORM_H_
