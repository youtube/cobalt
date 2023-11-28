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

#include "cobalt/media/base/cval_stats.h"

#include <numeric>

#include "starboard/common/time.h"

namespace cobalt {
namespace media {

CValContainer::CValContainer(std::string name,
                             int max_samples_before_calculation)
    : cval_name_{name},
      max_samples_before_calculation_{max_samples_before_calculation},
      latest_{"Media." + name + ".Latest", 0, "Latest sample in microseconds."},
      average_{"Media." + name + ".Average", 0,
               "Average sample in microseconds."},
      maximum_{"Media." + name + ".Maximum", 0,
               "Maximum sample in microseconds."},
      median_{"Media." + name + ".Median", 0, "Median sample in microseconds."},
      minimum_{"Media." + name + ".Minimum", 0,
               "Minimum sample in microseconds."} {}

void CValContainer::UpdateCVal(int64_t event_time_usec) {
  latest_ = event_time_usec;
  accumulated_sample_count_++;

  if (!first_sample_added_) {
    minimum_ = event_time_usec;
    maximum_ = event_time_usec;
    first_sample_added_ = true;
  }

  samples_[sample_write_index_] = event_time_usec;
  sample_write_index_ = (sample_write_index_ + 1) % kMaxSamples;

  if (accumulated_sample_count_ % max_samples_before_calculation_ == 0) {
    std::vector<int64_t> copy;
    int copy_size = std::min(accumulated_sample_count_, kMaxSamples);
    copy.reserve(copy_size);
    copy.assign(samples_, samples_ + copy_size);
    std::sort(copy.begin(), copy.end());
    if (copy.size() % 2 == 0) {
      median_ = (copy[copy.size() / 2 - 1] + copy[copy.size() / 2]) / 2;
    } else {
      median_ = copy[copy.size() / 2];
    }

    minimum_ = std::min(minimum_.value(), copy[0]);
    maximum_ = std::max(maximum_.value(), copy[copy.size() - 1]);

    auto local_average = std::accumulate(copy.begin(), copy.end(), 0);
    average_ += (local_average - average_.value()) / accumulated_sample_count_;
  }
}

CValStats::CValStats() {
  cval_containers_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(MediaTiming::SbPlayerCreate),
      std::forward_as_tuple("SbPlayerCreateTime",
                            kMediaDefaultMaxSamplesBeforeCalculation));
  cval_containers_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(MediaTiming::SbPlayerDestroy),
      std::forward_as_tuple("SbPlayerDestructionTime",
                            kMediaDefaultMaxSamplesBeforeCalculation));
  cval_containers_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(MediaTiming::SbPlayerWriteSamples),
      std::forward_as_tuple("SbPlayerWriteSamplesTime",
                            kMediaHighFrequencyMaxSamplesBeforeCalculation));
}

void CValStats::StartTimer(MediaTiming event_type,
                           std::string pipeline_identifier) {
  if (!enabled_) return;

  running_timers_[std::make_pair(event_type, pipeline_identifier)] =
      starboard::CurrentMonotonicTime();
}

void CValStats::StopTimer(MediaTiming event_type,
                          std::string pipeline_identifier) {
  if (!enabled_) return;

  auto key = std::make_pair(event_type, pipeline_identifier);
  DCHECK(running_timers_.find(key) != running_timers_.end());

  int64_t time_taken = starboard::CurrentMonotonicTime() - running_timers_[key];
  auto cval_container = cval_containers_.find(event_type);
  if (cval_container != cval_containers_.end()) {
    cval_container->second.UpdateCVal(time_taken);
  }
}

// for testing only
size_t CValStats::GetBufferIndexForTest() {
  DCHECK(cval_containers_.size() > 0);
  auto cval_container = cval_containers_.find(MediaTiming::SbPlayerCreate);
  if (cval_container != cval_containers_.end()) {
    return cval_container->second.GetSampleIndex();
  }
  return 0;
}

// for testing only
size_t CValStats::GetBufferSampleSizeForTest() {
  DCHECK(cval_containers_.size() > 0);
  auto cval_container = cval_containers_.find(MediaTiming::SbPlayerCreate);
  if (cval_container != cval_containers_.end()) {
    return cval_container->second.GetNumberSamples();
  }
  return 0;
}

}  // namespace media
}  // namespace cobalt
