// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_LINUX_X64X11_SYSTEM_GET_PROPERTY_IMPL_H_
#define STARBOARD_LINUX_X64X11_SYSTEM_GET_PROPERTY_IMPL_H_

#include "starboard/system.h"

// Omit namespace linux due to symbol name conflict.
namespace starboard {
namespace x64x11 {

bool CopyStringAndTestIfSuccess(char* out_value,
                                int value_length,
                                const char* from_value);

bool GetSystemProperty(SbSystemPropertyId property_id,
                       char* out_value,
                       int value_length);

}  // namespace x64x11
}  // namespace starboard

#endif  // STARBOARD_LINUX_X64X11_SYSTEM_GET_PROPERTY_IMPL_H_
