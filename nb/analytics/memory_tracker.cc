/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nb/analytics/memory_tracker.h"

#include "nb/analytics/memory_tracker_impl.h"
#include "nb/scoped_ptr.h"
#include "starboard/once.h"

namespace nb {
namespace analytics {
namespace {
SB_ONCE_INITIALIZE_FUNCTION(MemoryTrackerImpl, GetMemoryTrackerImplSingleton);
}  // namespace

MemoryTracker* MemoryTracker::Get() {
  MemoryTracker* t = GetMemoryTrackerImplSingleton();
  return t;
}

nb::scoped_ptr<MemoryTrackerPrintThread>
CreateDebugPrintThread(MemoryTracker* memory_tracker) {
  return nb::scoped_ptr<MemoryTrackerPrintThread>(
     new MemoryTrackerPrintThread(memory_tracker));
}

}  // namespace analytics
}  // namespace nb
