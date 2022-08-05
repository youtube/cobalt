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

#include "cobalt/web/global_stats.h"

#include "base/memory/singleton.h"
#include "cobalt/base/c_val.h"

namespace cobalt {
namespace web {

// static
GlobalStats* GlobalStats::GetInstance() {
  return base::Singleton<GlobalStats>::get();
}

GlobalStats::GlobalStats()
    : num_event_listeners_("Count.WEB.EventListeners", 0,
                           "Total number of currently active event listeners."),
      num_active_java_script_events_(
          "Count.WEB.ActiveJavaScriptEvents", 0,
          "Total number of currently active JavaScript events.") {}

GlobalStats::~GlobalStats() {}

bool GlobalStats::CheckNoLeaks() {
  DCHECK(num_event_listeners_ == 0);
  DCHECK(num_active_java_script_events_ == 0);
  return num_event_listeners_ == 0 && num_active_java_script_events_ == 0;
}

void GlobalStats::AddEventListener() { ++num_event_listeners_; }

void GlobalStats::RemoveEventListener() { --num_event_listeners_; }

void GlobalStats::StartJavaScriptEvent() { ++num_active_java_script_events_; }

void GlobalStats::StopJavaScriptEvent() { --num_active_java_script_events_; }

}  // namespace web
}  // namespace cobalt
