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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_TOOL_MEMORY_SIZE_BINNER_TOOL_H_
#define COBALT_BROWSER_MEMORY_TRACKER_TOOL_MEMORY_SIZE_BINNER_TOOL_H_

#include <string>

#include "base/compiler_specific.h"
#include "cobalt/browser/memory_tracker/tool/params.h"
#include "cobalt/browser/memory_tracker/tool/tool_impl.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

// This tool inspects a memory scope and reports on the memory usage.
// The output will be a CSV file printed to stdout representing
// the number of memory allocations for objects. The objects are binned
// according to the size of the memory allocation. Objects within the same
// power of two are binned together. For example 1024 will be binned with 1025.
class MemorySizeBinnerTool : public AbstractTool {
 public:
  // memory_scope_name represents the memory scope that is to be investigated.
  explicit MemorySizeBinnerTool(const std::string& memory_scope_name);

  void Run(Params* params) override;
  std::string tool_name() const override;

 private:
  std::string memory_scope_name_;
};

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_MEMORY_SIZE_BINNER_TOOL_H_
