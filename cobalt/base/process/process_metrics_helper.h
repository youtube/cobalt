// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BASE_PROCESS_PROCESS_METRICS_HELPER_H_
#define COBALT_BASE_PROCESS_PROCESS_METRICS_HELPER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"

namespace base {

class ProcessMetricsHelper {
 public:
  using CPUUsagePerThread = std::vector<std::pair<std::string, TimeDelta>>;
  using ReadCallback = OnceCallback<Optional<std::string>()>;
  using Fields = std::vector<std::string>;

  static int GetClockTicksPerS();
  static void PopulateClockTicksPerS();
  static TimeDelta GetCumulativeCPUUsage();
  static Value GetCumulativeCPUUsagePerThread();

 private:
  friend class ProcessMetricsHelperTest;

  static int GetClockTicksPerS(ReadCallback, ReadCallback);
  static Fields GetProcStatFields(ReadCallback, std::initializer_list<int>);
  static Fields GetProcStatFields(const FilePath&, std::initializer_list<int>);
  static TimeDelta GetCPUUsage(ReadCallback, int);
  static TimeDelta GetCPUUsage(const FilePath&, int);
};

}  // namespace base

#endif  // COBALT_BASE_PROCESS_PROCESS_METRICS_HELPER_H_
