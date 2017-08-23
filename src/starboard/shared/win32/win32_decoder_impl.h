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

#ifndef STARBOARD_SHARED_WIN32_WIN32_DECODER_IMPL_H_
#define STARBOARD_SHARED_WIN32_WIN32_DECODER_IMPL_H_

#include <D3D11.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <wrl\client.h>

#include <string>
#include <vector>

#include "starboard/common/scoped_ptr.h"
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/shared/win32/media_common.h"
#include "starboard/types.h"

// TODO: Win32 code shouldn't depend on Xb1 code.  Refactor this.
#include "starboard/xb1/shared/playready/drm_system_playready.h"

namespace starboard {
namespace shared {
namespace win32 {

class MediaBufferConsumerInterface {
 public:
  virtual void Consume(Microsoft::WRL::ComPtr<IMFMediaBuffer> media_buffer,
                       int64_t win32_timestamp) = 0;

  virtual void OnNewOutputType(const ComPtr<IMFMediaType>& type) = 0;

 protected:
  ~MediaBufferConsumerInterface() {}
};

struct Subsample {
  uint32_t clear_bytes;
  uint32_t encrypted_bytes;
};

class DecoderImpl {
 public:
  DecoderImpl(const std::string& type,
              MediaBufferConsumerInterface* media_buffer_consumer,
              SbDrmSystem drm_system);
  ~DecoderImpl();

  static Microsoft::WRL::ComPtr<IMFTransform> CreateDecoder(CLSID clsid);

  void ActivateDecryptor();
  bool TryWriteInputBuffer(const void* data,
                           int size,
                           std::int64_t win32_timestamp,
                           const std::vector<uint8_t>& key_id,
                           const std::vector<uint8_t>& iv,
                           const std::vector<Subsample>& subsamples);

  void DrainDecoder();

  bool DeliverOutputOnAllTransforms();
  bool DeliverOneOutputOnAllTransforms();

  void set_decoder(Microsoft::WRL::ComPtr<IMFTransform> decoder);

  bool has_decoder() const { return decoder_.Get(); }

 private:
  static void SendMFTMessage(IMFTransform* transform, MFT_MESSAGE_TYPE msg);

  void PrepareOutputDataBuffer(Microsoft::WRL::ComPtr<IMFTransform> transform,
                               MFT_OUTPUT_DATA_BUFFER* output_data_buffer);
  Microsoft::WRL::ComPtr<IMFSample> DeliverOutputOnTransform(
      Microsoft::WRL::ComPtr<IMFTransform> transform);
  void DeliverDecodedSample(Microsoft::WRL::ComPtr<IMFSample> sample);

  const std::string type_;  // For debugging purpose.
  // This is the target for the all completed media buffers.
  MediaBufferConsumerInterface* media_buffer_consumer_;
  ::starboard::xb1::shared::playready::SbDrmSystemPlayready* drm_system_;
  bool discontinuity_;

  Microsoft::WRL::ComPtr<IMFTransform> decryptor_;
  Microsoft::WRL::ComPtr<IMFSample> cached_decryptor_output_;
  Microsoft::WRL::ComPtr<IMFTransform> decoder_;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_WIN32_DECODER_IMPL_H_
