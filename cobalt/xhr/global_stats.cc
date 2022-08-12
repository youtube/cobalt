// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/xhr/global_stats.h"

#include "base/memory/singleton.h"
#include "cobalt/base/c_val.h"

namespace cobalt {
namespace xhr {

// static
GlobalStats* GlobalStats::GetInstance() {
  return base::Singleton<GlobalStats>::get();
}

GlobalStats::GlobalStats()
    : num_xhrs_("Count.XHR", 0, "Total number of currently active XHRs."),
      xhr_memory_("Memory.XHR", 0, "Memory allocated by XHRs in bytes.") {}

GlobalStats::~GlobalStats() {}

bool GlobalStats::CheckNoLeaks() {
  DCHECK(num_xhrs_ == 0);
  DCHECK(xhr_memory_ == 0);
  return num_xhrs_ == 0 && xhr_memory_ == 0;
}

void GlobalStats::Add(xhr::XMLHttpRequest* object) { ++num_xhrs_; }

void GlobalStats::Remove(xhr::XMLHttpRequest* object) { --num_xhrs_; }

void GlobalStats::IncreaseXHRMemoryUsage(size_t delta) { xhr_memory_ += delta; }

void GlobalStats::DecreaseXHRMemoryUsage(size_t delta) {
  DCHECK_GE(xhr_memory_.value(), delta);
  xhr_memory_ -= delta;
}

}  // namespace xhr
}  // namespace cobalt
