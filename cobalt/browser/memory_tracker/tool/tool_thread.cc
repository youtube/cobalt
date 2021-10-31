// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/memory_tracker/tool/tool_thread.h"

#include "base/time/time.h"
#include "cobalt/browser/memory_tracker/tool/params.h"
#include "cobalt/browser/memory_tracker/tool/tool_impl.h"
#include "nb/analytics/memory_tracker.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {
namespace {
// NoMemoryTracking will disable memory tracking while in the current scope of
// execution. When the object is destroyed it will reset the previous state
// of allocation tracking.
// Example:
//   void Foo() {
//     NoMemoryTracking no_memory_tracking_in_scope;
//     int* ptr = new int();  // ptr is not tracked.
//     delete ptr;
//     return;    // Previous memory tracking state is restored.
//   }
class NoMemoryTracking {
 public:
  explicit NoMemoryTracking(nb::analytics::MemoryTracker* owner)
      : prev_val_(false), owner_(owner) {
    if (owner_) {
      prev_val_ = owner_->IsMemoryTrackingEnabled();
      owner_->SetMemoryTrackingEnabled(false);
    }
  }
  ~NoMemoryTracking() {
    if (owner_) {
      owner_->SetMemoryTrackingEnabled(prev_val_);
    }
  }

 private:
  bool prev_val_;
  nb::analytics::MemoryTracker* owner_;
};

}  // namespace.

ToolThread::ToolThread(nb::analytics::MemoryTracker* memory_tracker,
                       AbstractTool* tool, AbstractLogger* logger)
    : Super(tool->tool_name()),
      params_(
          new Params(memory_tracker, logger, base::Time::NowFromSystemTime())),
      tool_(tool) {
  Start();
}

ToolThread::~ToolThread() {
  nb::analytics::MemoryTracker* memory_tracker = params_->memory_tracker();
  if (memory_tracker) {
    memory_tracker->SetMemoryTrackerDebugCallback(nullptr);
  }
  Join();
  tool_.reset();
  params_.reset();
}

void ToolThread::Join() {
  params_->set_finished(true);
  Super::Join();
}

void ToolThread::Run() {
  NoMemoryTracking no_mem_tracking_in_this_scope(params_->memory_tracker());
  // This tool will run until the finished_ if flipped to false.
  tool_->Run(params_.get());
}

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt
