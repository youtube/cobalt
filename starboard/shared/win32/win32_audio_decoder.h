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

#ifndef STARBOARD_SHARED_WIN32_WIN32_AUDIO_DECODER_H_
#define STARBOARD_SHARED_WIN32_WIN32_AUDIO_DECODER_H_

#include <vector>

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/win32/media_common.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace win32 {

// AudioDecoder for Win32.
class AbstractWin32AudioDecoder {
 public:
  static scoped_ptr<AbstractWin32AudioDecoder> Create(
      SbMediaAudioCodec codec,
      SbMediaAudioFrameStorageType audio_frame_fmt,
      SbMediaAudioSampleType sample_type,
      const SbMediaAudioHeader& audio_header,
      SbDrmSystem drm_system);
  virtual ~AbstractWin32AudioDecoder() {}

  // INPUT:
  //
  virtual bool TryWrite(const scoped_refptr<InputBuffer>& buff) = 0;
  virtual void WriteEndOfStream() = 0;
  // OUTPUT
  //
  virtual DecodedAudioPtr ProcessAndRead() = 0;
  // Reset
  virtual void Reset() = 0;

  // Dynamically query the audio frequency to support HE-AAC.
  virtual int GetSamplesPerSecond() const = 0;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_WIN32_AUDIO_DECODER_H_
