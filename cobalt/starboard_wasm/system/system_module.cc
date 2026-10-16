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

#include "cobalt/starboard_wasm/system/system_module.h"

#include <string_view>

#include "base/logging.h"
#include "cobalt/starboard_wasm/core/abi.h"
#include "cobalt/starboard_wasm/core/event_queue.h"
#include "cobalt/starboard_wasm/core/guest_memory.h"

namespace cobalt {
namespace starboard_wasm {

SystemModule::SystemModule(const ModuleContext& context)
    : events_(context.events) {}

SystemModule::~SystemModule() = default;

const char* SystemModule::name() const {
  return kName;
}

uint32_t SystemModule::PollEvents(GuestMemory& memory,
                                  uint32_t events_ptr,
                                  uint32_t count) {
  uint8_t* out = memory.Array(events_ptr, count, sizeof(sb_event));
  return out ? events_->Poll(out, count) : 0;
}

void SystemModule::Log(GuestMemory& memory,
                       uint32_t level,
                       uint32_t message_ptr,
                       uint32_t message_len) {
  const uint8_t* message = memory.Bytes(message_ptr, message_len);
  const std::string_view text =
      message ? std::string_view(reinterpret_cast<const char*>(message),
                                 message_len)
              : std::string_view();
  switch (level) {
    case SB_LOG_ERROR:
      LOG(ERROR) << "[wasm] " << text;
      break;
    case SB_LOG_WARNING:
      LOG(WARNING) << "[wasm] " << text;
      break;
    case SB_LOG_INFO:
      LOG(INFO) << "[wasm] " << text;
      break;
    default:
      VLOG(1) << "[wasm] " << text;
      break;
  }
}

std::unique_ptr<Module> CreateSystemModule(const ModuleContext& context) {
  return std::make_unique<SystemModule>(context);
}

}  // namespace starboard_wasm
}  // namespace cobalt
