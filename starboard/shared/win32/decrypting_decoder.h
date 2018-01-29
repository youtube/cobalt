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

#ifndef STARBOARD_SHARED_WIN32_DECRYPTING_DECODER_H_
#define STARBOARD_SHARED_WIN32_DECRYPTING_DECODER_H_

#include <D3D11.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <wrl\client.h>

#include <string>
#include <vector>

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/shared/win32/drm_system_playready.h"
#include "starboard/shared/win32/media_common.h"
#include "starboard/shared/win32/media_transform.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace win32 {

// This class maintains a MediaTransform based decoding stream.  When the
// stream is encrypted, it also contains a MediaTransform based decryptor and
// manages the interaction between the decryptor and the decoder.
class DecryptingDecoder {
 public:
  DecryptingDecoder(const std::string& type,
                    scoped_ptr<MediaTransform> decoder,
                    SbDrmSystem drm_system);
  ~DecryptingDecoder();

  MediaTransform* GetDecoder() { return decoder_.get(); }

  bool TryWriteInputBuffer(const scoped_refptr<InputBuffer>& input_buffer,
                           int bytes_to_skip_in_sample);

  // Return true if there is any internal actions succeeded, this implies that
  // the caller can call this function again to process further.
  // |output| contains the decrypted and decoded output if there is any.
  // |new_type| contains the new output type in case the output type of the
  // decoding stream is changed.
  bool ProcessAndRead(ComPtr<IMFSample>* output,
                      ComPtr<IMFMediaType>* new_type);
  void Drain();
  void Reset();

 private:
  bool ActivateDecryptor();

  // TODO: Clarify the thread pattern of this class.
  const std::string type_;  // For debugging purpose.
  SbDrmSystemPlayready* drm_system_;

  scoped_ptr<MediaTransform> decryptor_;
  scoped_ptr<MediaTransform> decoder_;

  scoped_refptr<InputBuffer> last_input_buffer_;
  ComPtr<IMFSample> last_input_sample_;

  ComPtr<IMFSample> pending_decryptor_output_;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_DECRYPTING_DECODER_H_
