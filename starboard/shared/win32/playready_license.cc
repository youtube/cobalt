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

#include "starboard/shared/win32/drm_system_playready.h"

#include "starboard/configuration.h"

namespace starboard {
namespace shared {
namespace win32 {

scoped_refptr<DrmSystemPlayready::License> DrmSystemPlayready::License::Create(
    const void* initialization_data,
    int initialization_data_size) {
  return NULL;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
