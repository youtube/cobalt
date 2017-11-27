// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_TOOL_MALLOC_STATS_TOOL_H_
#define COBALT_BROWSER_MEMORY_TRACKER_TOOL_MALLOC_STATS_TOOL_H_

#include <string>

#include "cobalt/browser/memory_tracker/tool/params.h"
#include "cobalt/browser/memory_tracker/tool/tool_impl.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

// Generates a CSV table of the memory stats. This tool does not use
// per-allocation tracking and therefore has a lower memory footprint than
// most other tools.
class MallocStatsTool : public AbstractTool {
 public:
  MallocStatsTool();
  std::string tool_name() const override;
  void Run(Params* params) override;
};

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_MALLOC_STATS_TOOL_H_
