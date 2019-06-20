// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/log.h"
#include "starboard/event.h"

#include "starboard/elf_loader/elf_loader.h"

starboard::elf_loader::ElfLoader g_elfLoader;

void (*g_sb_event_func)(const SbEvent*) = NULL;

void SbEventHandle(const SbEvent* event) {
  switch (event->type) {
    case kSbEventTypeStart: {
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
      if (!g_sb_event_func && data->argument_count == 2) {
        if (!g_elfLoader.Load(data->argument_values[1])) {
          SB_LOG(INFO) << "Failed to load library";
          return;
        }

        SB_LOG(INFO) << "Successfully loaded library\n";
        void* p = g_elfLoader.LookupSymbol("SbEventHandle");
        if (p != NULL) {
          SB_LOG(INFO) << "Symbol Lookup succeeded address=0x" << std::hex << p;
          g_sb_event_func = (void (*)(const SbEvent*))p;
          g_sb_event_func(event);
        } else {
          SB_LOG(INFO) << "Symbol Lookup failed\n";
        }
      }
      break;
    }
    default: {
      if (g_sb_event_func) {
        g_sb_event_func(event);
      }
    }
  }
}
