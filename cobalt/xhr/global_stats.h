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

#ifndef COBALT_XHR_GLOBAL_STATS_H_
#define COBALT_XHR_GLOBAL_STATS_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "cobalt/base/c_val.h"

namespace cobalt {
namespace xhr {
class XMLHttpRequest;

// This singleton class is used to track DOM-related statistics.
class GlobalStats {
 public:
  GlobalStats(const GlobalStats&) = delete;
  GlobalStats& operator=(const GlobalStats&) = delete;

  static GlobalStats* GetInstance();

  bool CheckNoLeaks();

  void Add(xhr::XMLHttpRequest* object);
  void Remove(xhr::XMLHttpRequest* object);
  void IncreaseXHRMemoryUsage(size_t delta);
  void DecreaseXHRMemoryUsage(size_t delta);

 private:
  GlobalStats();
  ~GlobalStats();

  // XHR-related tracking
  base::CVal<int> num_xhrs_;
  base::CVal<base::cval::SizeInBytes> xhr_memory_;

  friend struct base::DefaultSingletonTraits<GlobalStats>;
};

}  // namespace xhr
}  // namespace cobalt

#endif  // COBALT_XHR_GLOBAL_STATS_H_
