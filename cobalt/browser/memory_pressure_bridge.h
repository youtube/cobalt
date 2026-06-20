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

#ifndef COBALT_BROWSER_MEMORY_PRESSURE_BRIDGE_H_
#define COBALT_BROWSER_MEMORY_PRESSURE_BRIDGE_H_

#include <memory>

#include "base/memory/memory_pressure_listener.h"
#include "starboard/extension/memory_pressure.h"

namespace cobalt {
namespace browser {

// A bridge that listens to Chromium's base::MemoryPressureListener events
// and propagates them to the Starboard MemoryPressure extension if available.
// This allows Cobalt's browser-level memory pressure signals to be routed
// to Starboard-level allocators for memory reclaiming (e.g. decommitting pages).
class MemoryPressureBridge {
 public:
  MemoryPressureBridge();
  ~MemoryPressureBridge();

  MemoryPressureBridge(const MemoryPressureBridge&) = delete;
  MemoryPressureBridge& operator=(const MemoryPressureBridge&) = delete;

 private:
  void OnMemoryPressure(base::MemoryPressureListener::MemoryPressureLevel level);

  std::unique_ptr<base::MemoryPressureListener> listener_;
  const StarboardExtensionMemoryPressureApi* extension_ = nullptr;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_PRESSURE_BRIDGE_H_
