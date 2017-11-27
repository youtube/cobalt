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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_TOOL_PRINT_TOOL_H_
#define COBALT_BROWSER_MEMORY_TRACKER_TOOL_PRINT_TOOL_H_

#include <string>

#include "base/compiler_specific.h"
#include "cobalt/browser/memory_tracker/tool/params.h"
#include "cobalt/browser/memory_tracker/tool/tool_impl.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

// Start() is called when this object is created, and Cancel() & Join() are
// called during destruction.
class PrintTool : public AbstractTool {
 public:
  PrintTool();
  ~PrintTool() override;

  // Overridden so that the thread can exit gracefully.
  void Run(Params* params) override;
  std::string tool_name() const override;

 private:
  class CvalsMap;
  scoped_ptr<CvalsMap> cvals_map_;
};

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_PRINT_TOOL_H_
