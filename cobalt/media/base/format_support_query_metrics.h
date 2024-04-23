// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_BASE_FORMAT_SUPPORT_QUERY_METRICS_H_
#define COBALT_MEDIA_BASE_FORMAT_SUPPORT_QUERY_METRICS_H_

#include <string>

#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "starboard/media.h"

namespace cobalt {
namespace media {

enum class FormatSupportQueryAction : uint8_t {
  UNKNOWN_ACTION,
  HTML_MEDIA_ELEMENT_CAN_PLAY_TYPE,
  MEDIA_SOURCE_IS_TYPE_SUPPORTED,
};

class FormatSupportQueryMetrics {
 public:
  FormatSupportQueryMetrics(
      const base::TickClock* clock = base::DefaultTickClock::GetInstance());
  ~FormatSupportQueryMetrics() = default;

  void RecordAndLogQuery(FormatSupportQueryAction query_action,
                         const std::string& mime_type,
                         const std::string& key_system,
                         SbMediaSupportType support_type);
  static void PrintAndResetMetrics();

 private:
  void RecordQueryLatencyUMA(FormatSupportQueryAction query_action,
                             base::TimeDelta action_duration);

#if !defined(COBALT_BUILD_TYPE_GOLD)
  static constexpr int kMaxCachedQueryDurations = 150;
  static constexpr int kMaxQueryDescriptionLength = 350;

  static base::TimeDelta cached_query_durations_[kMaxCachedQueryDurations];
  static char max_query_description_[kMaxQueryDescriptionLength];
  static base::TimeDelta max_query_duration_;
  static base::TimeDelta total_query_duration_;
  static int total_num_queries_;
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

  base::TimeTicks start_time_;
  const base::TickClock* clock_;
};


}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_FORMAT_SUPPORT_QUERY_METRICS_H_
