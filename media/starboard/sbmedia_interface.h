// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef MEDIA_STARBOARD_SBMEDIA_INTERFACE_H_
#define MEDIA_STARBOARD_SBMEDIA_INTERFACE_H_

#include "media/base/media_export.h"
#include "starboard/media.h"

namespace media {

class MEDIA_EXPORT SbMediaInterface {
 public:
  virtual ~SbMediaInterface() = default;

  virtual SbMediaSupportType CanPlayMimeAndKeySystem(
      const char* mime,
      const char* key_system) = 0;
};

class MEDIA_EXPORT DefaultSbMediaInterface final : public SbMediaInterface {
 public:
  SbMediaSupportType CanPlayMimeAndKeySystem(const char* mime,
                                             const char* key_system) override;
};

// Returns a pointer to the global SbMediaInterface instance.
// By default, this returns a DefaultSbMediaInterface instance.
MEDIA_EXPORT SbMediaInterface* GetSbMediaInterface();

// Sets a custom SbMediaInterface for testing. Pass nullptr to restore the
// default.
MEDIA_EXPORT void SetSbMediaInterfaceForTesting(SbMediaInterface* interface);

}  // namespace media

#endif  // MEDIA_STARBOARD_SBMEDIA_INTERFACE_H_
