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

#ifndef COBALT_WEB_GLOBAL_STATS_H_
#define COBALT_WEB_GLOBAL_STATS_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "cobalt/base/c_val.h"

namespace cobalt {
namespace web {

// This singleton class is used to track DOM-related statistics.
class GlobalStats {
 public:
  GlobalStats(const GlobalStats&) = delete;
  GlobalStats& operator=(const GlobalStats&) = delete;

  static GlobalStats* GetInstance();

  bool CheckNoLeaks();

  void AddEventListener();
  void RemoveEventListener();

  int GetNumEventListeners() const { return num_event_listeners_; }

  void StartJavaScriptEvent();
  void StopJavaScriptEvent();

 private:
  GlobalStats();
  ~GlobalStats();

  // Web-related tracking
  base::CVal<int, base::CValPublic> num_event_listeners_;

  base::CVal<int> num_active_java_script_events_;

  friend struct base::DefaultSingletonTraits<GlobalStats>;
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_GLOBAL_STATS_H_
