// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_TOOL_INTERNAL_FRAGMENTATION_TOOL_H_
#define COBALT_BROWSER_MEMORY_TRACKER_TOOL_INTERNAL_FRAGMENTATION_TOOL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "cobalt/browser/memory_tracker/tool/tool_impl.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

class InternalFragmentationTool : public AbstractTool {
 public:
  InternalFragmentationTool();
  ~InternalFragmentationTool() override;
  // Interface AbstractMemoryTrackerTool
  std::string tool_name() const override;
  void Run(Params* params) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InternalFragmentationTool);
};

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_INTERNAL_FRAGMENTATION_TOOL_H_
