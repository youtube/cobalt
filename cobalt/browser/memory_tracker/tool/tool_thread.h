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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_TOOL_TOOL_THREAD_H_
#define COBALT_BROWSER_MEMORY_TRACKER_TOOL_TOOL_THREAD_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/simple_thread.h"

namespace nb {
namespace analytics {
class MemoryTracker;
}  // namespace analytics
}  // namespace nb

namespace cobalt {
namespace browser {
namespace memory_tracker {

class AbstractLogger;
class AbstractTool;
class Params;

// ToolThread sets up the the thread environment to run an AbstractTool.
class ToolThread : public base::SimpleThread {
 public:
  typedef base::SimpleThread Super;
  ToolThread(nb::analytics::MemoryTracker* memory_tracker, AbstractTool* tool,
             AbstractLogger* logger);
  virtual ~ToolThread();

  void Join() override;
  void Run() override;

 private:
  scoped_ptr<Params> params_;
  scoped_ptr<AbstractTool> tool_;
};

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_TOOL_THREAD_H_
