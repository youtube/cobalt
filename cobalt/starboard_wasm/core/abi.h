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

// Records shared by every Starboard Wasm import module. Pointers in import
// signatures are 32-bit offsets into the guest's linear memory; records carry
// a leading `struct_size` so they can grow.

#ifndef COBALT_STARBOARD_WASM_CORE_ABI_H_
#define COBALT_STARBOARD_WASM_CORE_ABI_H_

#include <stdint.h>

namespace cobalt {
namespace starboard_wasm {

// An asynchronous notification from the host, drained with
// `sb_sys.poll_events`. `type` values are defined by the module that queues
// the event; `handle` identifies that module's object (for example a player).
struct sb_event {
  uint32_t struct_size;
  uint32_t type;
  uint32_t handle;
  uint32_t reserved0;
  int64_t a;
  int64_t b;
};

static_assert(sizeof(sb_event) == 32, "sb_event layout");

enum sb_log_level {
  SB_LOG_ERROR = 0,
  SB_LOG_WARNING = 1,
  SB_LOG_INFO = 2,
  SB_LOG_VERBOSE = 3,
};

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_CORE_ABI_H_
