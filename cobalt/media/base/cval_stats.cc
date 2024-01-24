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

namespace cobalt {
namespace media {

CValContainer::CValContainer(std::string name,
                             int max_samples_before_calculation)
    : cval_name_{name},
      max_samples_before_calculation_{max_samples_before_calculation},
      latest_{
          "Media." + name + ".Latest", {}, "Latest sample in microseconds."},
      average_{
          "Media." + name + ".Average", {}, "Average sample in microseconds."},
      maximum_{
          "Media." + name + ".Maximum", {}, "Maximum sample in microseconds."},
      median_{
          "Media." + name + ".Median", {}, "Median sample in microseconds."},
      minimum_{
          "Media." + name + ".Minimum", {}, "Minimum sample in microseconds."} {
}

<<<<<<< HEAD
void CValContainer::UpdateCVal(SbTime event_timing) {
  latest_ = event_timing;
  accumulated_sample_count_++;

  if (!first_sample_added_) {
    minimum_ = event_timing;
    maximum_ = event_timing;
    first_sample_added_ = true;
  }

  samples_[sample_write_index_] = event_timing;
  sample_write_index_ = (sample_write_index_ + 1) % kMaxSamples;

  if (accumulated_sample_count_ % max_samples_before_calculation_ == 0) {
    std::vector<SbTime> copy;
=======
void CValContainer::UpdateCVal(TimeDelta event_time) {
  latest_ = event_time;
  accumulated_sample_count_++;

  if (!first_sample_added_) {
    minimum_ = event_time;
    maximum_ = event_time;
    first_sample_added_ = true;
  }

  samples_[sample_write_index_] = event_time;
  sample_write_index_ = (sample_write_index_ + 1) % kMaxSamples;

  if (accumulated_sample_count_ % max_samples_before_calculation_ == 0) {
    std::vector<TimeDelta> copy;
>>>>>>> 29389bbcbba ([media] Replace instances of int64_t with base::Time (#2236))
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

    auto local_average = std::accumulate(copy.begin(), copy.end(), TimeDelta());
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
<<<<<<< HEAD
      SbTimeGetMonotonicNow();
=======
      Time::Now();
>>>>>>> 29389bbcbba ([media] Replace instances of int64_t with base::Time (#2236))
}

void CValStats::StopTimer(MediaTiming event_type,
                          std::string pipeline_identifier) {
  if (!enabled_) return;

  auto key = std::make_pair(event_type, pipeline_identifier);
  DCHECK(running_timers_.find(key) != running_timers_.end());

<<<<<<< HEAD
  SbTime time_taken = SbTimeGetMonotonicNow() - running_timers_[key];
=======
  TimeDelta time_taken = Time::Now() - running_timers_[key];
>>>>>>> 29389bbcbba ([media] Replace instances of int64_t with base::Time (#2236))
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
