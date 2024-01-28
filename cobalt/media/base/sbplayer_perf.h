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

#ifndef COBALT_MEDIA_BASE_SBPLAYER_PERF_H_
#define COBALT_MEDIA_BASE_SBPLAYER_PERF_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/task/task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cobalt/media/base/sbplayer_interface.h"
#include "starboard/common/system_property.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

class SbPlayerPerf {
 public:
  // Constructs a measure pipeline that will execute on |task_runner|.
  explicit SbPlayerPerf(
      SbPlayerInterface* interface, SbPlayer player,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~SbPlayerPerf();

  void Start();
  void StartTask();
  void Stop();
  void StopTask();

 private:
  // Refer base/process/process_metrics.h for GetPlatformIndependentCPUUsage()
  void GetPlatformIndependentCPUUsage();
  std::string GetSbSystemProperty(SbSystemPropertyId property_id);

  // Message loop used to execute pipeline tasks.  It is thread-safe.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::RepeatingTimer timer_;

  SbPlayerInterface* interface_;
  SbPlayer player_;

  // TODO(borongchen): ensure when to log CPU statistics, as each video segment
  // will Play() once
  std::string sb_system_property_brand_name_;
  std::string sb_system_property_model_name_;
  std::string sb_system_property_platform_name_;

  int sb_number_of_processors_;
  Time start_timestamp_;
  Time timestamp_last_;
  int decoded_frames_last_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_SBPLAYER_PERF_H_
