// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_XB1_SHARED_INTERNAL_SHIMS_H_
#define STARBOARD_XB1_SHARED_INTERNAL_SHIMS_H_

#include <string>
#include "starboard/atomic.h"
#include "starboard/system.h"

namespace starboard {
namespace xb1 {
namespace shared {

bool CanAcquire();

Windows::Foundation::IAsyncOperation<bool> ^ Acquire();

void Release();

Platform::String ^ GetCertScope();

void GetSignature(Windows::Storage::Streams::IBuffer ^ message_buffer,
                  Windows::Storage::Streams::IBuffer ^ *signature);

}  // namespace shared
}  // namespace xb1
}  // namespace starboard

#endif  // STARBOARD_XB1_SHARED_INTERNAL_SHIMS_H_
