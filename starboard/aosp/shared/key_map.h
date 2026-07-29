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

#ifndef STARBOARD_AOSP_SHARED_KEY_MAP_H_
#define STARBOARD_AOSP_SHARED_KEY_MAP_H_

#include "starboard/key.h"

namespace starboard {

// Returns the SbKey an Android key code (AKEYCODE_*) stands for, or
// kSbKeyUnknown for the keys Starboard does not handle.
SbKey AndroidKeyCodeToSbKey(int key_code);

// Returns true for the keys Starboard is told about but that Android still has
// to act on itself.
bool SystemHandlesKeyCode(int key_code);

}  // namespace starboard

#endif  // STARBOARD_AOSP_SHARED_KEY_MAP_H_
