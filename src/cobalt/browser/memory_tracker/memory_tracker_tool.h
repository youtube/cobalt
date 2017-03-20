// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_MEMORY_TRACKER_TOOL_H_
#define COBALT_BROWSER_MEMORY_TRACKER_MEMORY_TRACKER_TOOL_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

class MemoryTrackerTool {
 public:
  virtual ~MemoryTrackerTool() {}
};

// Instantiates the memory tracker tool from the command argument.
scoped_ptr<MemoryTrackerTool> CreateMemoryTrackerTool(
    const std::string& command_arg);

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_MEMORY_TRACKER_TOOL_H_
