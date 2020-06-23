// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/script/v8c/wrapper_private.h"

#include "starboard/types.h"

namespace cobalt {
namespace script {
namespace v8c {

// This magic number helps identify objects as Cobalt platform objects.
void* const WrapperPrivate::kInternalFieldIdValue =
    reinterpret_cast<void*>(uintptr_t(0xc0ba1700));

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
