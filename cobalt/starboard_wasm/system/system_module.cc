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

#include <string>

#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "cobalt/starboard_wasm/core/abi.h"
#include "cobalt/starboard_wasm/core/event_queue.h"
#include "cobalt/starboard_wasm/core/import_table.h"

namespace cobalt {
namespace starboard_wasm {

namespace {

class SystemModule : public Module {
 public:
  explicit SystemModule(const ModuleContext& context)
      : events_(context.events) {}

  const char* name() const override { return "system"; }
  void Bind(ImportTable& table) override;

  EventQueue& events() { return *events_; }

 private:
  const raw_ref<EventQueue> events_;
};

int64_t NowUs(SystemModule& /*module*/, Call& /*call*/) {
  return MonotonicNowUs();
}

uint32_t PollEvents(SystemModule& module,
                    Call& call,
                    uint32_t out_ptr,
                    uint32_t count) {
  uint8_t* out = call.Array(out_ptr, count, sizeof(sb_event));
  return out ? module.events().Poll(out, count) : 0;
}

void Log(SystemModule& /*module*/,
         Call& call,
         uint32_t level,
         uint32_t msg_ptr,
         uint32_t msg_len) {
  const uint8_t* msg = call.Bytes(msg_ptr, msg_len);
  const std::string text =
      msg ? std::string(reinterpret_cast<const char*>(msg), msg_len)
          : std::string();
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

void SystemModule::Bind(ImportTable& table) {
  table.Add<&NowUs, /*kHot=*/true>("sb_sys", "now_us");
  table.Add<&PollEvents, /*kHot=*/true>("sb_sys", "poll_events");
  table.Add<&Log>("sb_sys", "log");
}

}  // namespace

std::unique_ptr<Module> CreateSystemModule(const ModuleContext& context) {
  return std::make_unique<SystemModule>(context);
}

}  // namespace starboard_wasm
}  // namespace cobalt
