// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_BASE_CVAL_STATS_H_
#define COBALT_MEDIA_BASE_CVAL_STATS_H_


#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "cobalt/base/c_val.h"


namespace cobalt {
namespace media {

using base::Time;
using base::TimeDelta;

enum class MediaTiming {
  SbPlayerCreate,
  SbPlayerDestroy,
  SbPlayerWriteSamples
};

// size of the sample array
constexpr size_t kMaxSamples = 100;
// number of samples taken before we trigger historical calculations
constexpr int kMediaDefaultMaxSamplesBeforeCalculation = 5;
constexpr int kMediaHighFrequencyMaxSamplesBeforeCalculation = 25;

class CValContainer {
 public:
  CValContainer(std::string name, int max_samples_before_calculation);
  ~CValContainer() = default;

  void StartTimer();
  void StopTimer();
  void UpdateCVal(TimeDelta event_time_usec);

  // for testing only
  size_t GetSampleIndex() { return sample_write_index_; }
  size_t GetNumberSamples() {
    return std::min(accumulated_sample_count_, kMaxSamples);
  }

 public:
  int max_samples_before_calculation_{0};
  std::string cval_name_;

 private:
  base::CVal<TimeDelta, base::CValPublic> latest_;
  base::CVal<TimeDelta, base::CValPublic> average_;
  base::CVal<TimeDelta, base::CValPublic> maximum_;
  base::CVal<TimeDelta, base::CValPublic> median_;
  base::CVal<TimeDelta, base::CValPublic> minimum_;

  TimeDelta samples_[kMaxSamples];
  size_t sample_write_index_{0};
  size_t accumulated_sample_count_{0};
  bool first_sample_added_{false};

  Time latest_time_start_;
  Time latest_time_stop_;
};

class CValStats {
 public:
  CValStats();
  ~CValStats() = default;

  void StartTimer(MediaTiming event_type, std::string pipeline_identifier);
  void StopTimer(MediaTiming event_type, std::string pipeline_identifier);
  void Enable(bool enabled) { enabled_ = enabled; }

  // just for testing
  size_t GetBufferIndexForTest();
  size_t GetBufferSampleSizeForTest();

 private:
  std::map<MediaTiming, CValContainer> cval_containers_;
  std::map<std::pair<MediaTiming, std::string>, Time> running_timers_;
  bool enabled_{false};
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_CVAL_STATS_H_
