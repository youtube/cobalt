// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/media.h"

int SbMediaGetAudioOutputCount() {
  // TODO(b/284140486, b/297426689): Tentatively returns 16 to ensure that all
  // audio output devices are checked in `IsAudioOutputSupported()`.  We should
  // revisit this, and probably remove `SbMediaGetAudioOutputCount()` completely
  // from Starboard.
  return 16;
}
