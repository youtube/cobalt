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

#include "cobalt/script/v8c/isolate_fellowship.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/configuration/configuration.h"
#include "cobalt/script/v8c/switches.h"
#include "cobalt/script/v8c/v8c_tracing_controller.h"
#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/memory.h"

namespace cobalt {
namespace script {
namespace v8c {

namespace {
// This file will also be touched and rebuilt every time V8 is re-built
// according to the update_snapshot_time gyp target.
const char kIsolateFellowshipBuildTime[] = __DATE__ " " __TIME__;

// Configure v8's global command line flag options for Cobalt.
// It can be called more than once, but make sure it is called before any
// v8 instance is created.
void V8FlagsInit() {
  std::vector<std::string> kV8CommandLineFlags = {
    "--optimize_for_size",
    // Starboard disallow rwx memory access.
    "--write_protect_code_memory",
    // Cobalt's TraceMembers and
    // ScriptValue::*Reference do not currently
    // support incremental tracing.
    "--noincremental_marking_wrappers",
    "--noexpose_wasm",
    "--novalidate_asm",
  };

  if (!configuration::Configuration::GetInstance()->CobaltEnableJit()) {
    kV8CommandLineFlags.push_back("--jitless");
  }

  if (configuration::Configuration::GetInstance()->CobaltGcZeal()) {
    kV8CommandLineFlags.push_back("--gc_interval=1200");
  }

  for (auto flag_str : kV8CommandLineFlags) {
    v8::V8::SetFlagsFromString(flag_str.c_str(),
                               strlen(flag_str.c_str()));
  }
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kV8Flags)) {
    std::string all_flags =
        command_line->GetSwitchValueASCII(switches::kV8Flags);
    // V8::SetFlagsFromString is capable of taking multiple flags separated by
    // spaces.
    v8::V8::SetFlagsFromString(all_flags.c_str(), all_flags.length());
  }
#endif
}
}  // namespace

IsolateFellowship::IsolateFellowship() {
  TRACE_EVENT0("cobalt::script", "IsolateFellowship::IsolateFellowship");
  // TODO: Initialize V8 ICU stuff here as well.
  platform.reset(new CobaltPlatform(v8::platform::NewDefaultPlatform(
      0 /*thread_pool_size*/, v8::platform::IdleTaskSupport::kDisabled,
      v8::platform::InProcessStackDumping::kEnabled,
      std::unique_ptr<v8::TracingController>(new V8cTracingController()))));
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
  array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();

  // If a new snapshot data is needed, a temporary v8 isolate will be created
  // to write the snapshot data. We need to make sure all global command line
  // flags are set before that.
  V8FlagsInit();
}

IsolateFellowship::~IsolateFellowship() {
  TRACE_EVENT0("cobalt::script", "IsolateFellowship::~IsolateFellowship");
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();

  DCHECK(array_buffer_allocator);
  delete array_buffer_allocator;
  array_buffer_allocator = nullptr;

}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
