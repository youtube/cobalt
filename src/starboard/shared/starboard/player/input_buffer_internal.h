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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_INPUT_BUFFER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_INPUT_BUFFER_INTERNAL_H_

#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

// This class encapsulate a media buffer.  It holds a reference-counted object
// internally to ensure that the object of this class can be copied while the
// underlying resources allocated is properly managed.
// Note that pass this class as const reference doesn't guarantee that the
// content of the internal buffer won't be changed.
class InputBuffer {
 public:
  InputBuffer();
  InputBuffer(SbPlayerDeallocateSampleFunc deallocate_sample_func,
              SbPlayer player,
              void* context,
              const void* sample_buffer,
              int sample_buffer_size,
              SbMediaTime sample_pts,
              const SbMediaVideoSampleInfo* video_sample_info,
              const SbDrmSampleInfo* sample_drm_info);
  InputBuffer(const InputBuffer& that);
  ~InputBuffer();

  InputBuffer& operator=(const InputBuffer& that);

  const uint8_t* data() const;
  int size() const;
  SbMediaTime pts() const;
  const SbMediaVideoSampleInfo* video_sample_info() const;
  const SbDrmSampleInfo* drm_info() const;
  void SetDecryptedContent(const void* buffer, int size);

 private:
  class ReferenceCountedBuffer;

  ReferenceCountedBuffer* buffer_;
};

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_INPUT_BUFFER_INTERNAL_H_
