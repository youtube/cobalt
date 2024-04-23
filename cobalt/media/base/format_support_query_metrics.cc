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

#include "cobalt/media/base/format_support_query_metrics.h"

#include <algorithm>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "starboard/common/string.h"

namespace cobalt {
namespace media {

#if !defined(COBALT_BUILD_TYPE_GOLD)

namespace {

std::string CreateQueryDescription(FormatSupportQueryAction query_action,
                                   const std::string& mime_type,
                                   const std::string& key_system,
                                   SbMediaSupportType support_type,
                                   base::TimeDelta query_duration) {
  auto get_query_name_str = [](FormatSupportQueryAction query_action) {
    switch (query_action) {
      case FormatSupportQueryAction::HTML_MEDIA_ELEMENT_CAN_PLAY_TYPE:
        return "HTMLMediaElement::canPlayType";
      case FormatSupportQueryAction::MEDIA_SOURCE_IS_TYPE_SUPPORTED:
        return "MediaSource::isTypeSupported";
      case FormatSupportQueryAction::UNKNOWN_ACTION:
      default:
        return "Unknown Action";
    }
  };

  auto get_support_type_str = [](SbMediaSupportType support_type) {
    switch (support_type) {
      case kSbMediaSupportTypeNotSupported:
        return " -> not supported/false";
      case kSbMediaSupportTypeMaybe:
        return " -> maybe/true";
      case kSbMediaSupportTypeProbably:
        return " -> probably/true";
      default:
        NOTREACHED();
        return "";
    }
  };

  return starboard::FormatString(
      "%s(%s%s%s, %" PRId64 " us", get_query_name_str(query_action),
      mime_type.c_str(),
      (key_system.empty() ? ")" : ", " + key_system + ")").c_str(),
      get_support_type_str(support_type), query_duration.InMicroseconds());
}

}  // namespace

// static
base::TimeDelta FormatSupportQueryMetrics::cached_query_durations_
    [kMaxCachedQueryDurations] = {};
char FormatSupportQueryMetrics::max_query_description_
    [kMaxQueryDescriptionLength] = {};
base::TimeDelta FormatSupportQueryMetrics::max_query_duration_ =
    base::TimeDelta();
base::TimeDelta FormatSupportQueryMetrics::total_query_duration_ =
    base::TimeDelta();
int FormatSupportQueryMetrics::total_num_queries_ = 0;

#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

FormatSupportQueryMetrics::FormatSupportQueryMetrics(
    const base::TickClock* clock)
    : clock_(clock) {
  start_time_ = clock->NowTicks();
}

void FormatSupportQueryMetrics::RecordQueryLatencyUMA(
    FormatSupportQueryAction query_action, base::TimeDelta action_duration) {
  switch (query_action) {
    case FormatSupportQueryAction::MEDIA_SOURCE_IS_TYPE_SUPPORTED: {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Cobalt.Media.MediaSource.IsTypeSupported.Timing", action_duration,
          base::TimeDelta::FromMicroseconds(1),
          base::TimeDelta::FromMilliseconds(5), 50);
      break;
    }
    case FormatSupportQueryAction::HTML_MEDIA_ELEMENT_CAN_PLAY_TYPE: {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Cobalt.Media.HTMLMediaElement.CanPlayType.Timing", action_duration,
          base::TimeDelta::FromMicroseconds(1),
          base::TimeDelta::FromMilliseconds(5), 50);
      break;
    }
    case FormatSupportQueryAction::UNKNOWN_ACTION:
    default:
      break;
  }
}

void FormatSupportQueryMetrics::RecordAndLogQuery(
    FormatSupportQueryAction query_action, const std::string& mime_type,
    const std::string& key_system, SbMediaSupportType support_type) {
  base::TimeDelta query_duration = clock_->NowTicks() - start_time_;

  RecordQueryLatencyUMA(query_action, query_duration);

#if !defined(COBALT_BUILD_TYPE_GOLD)
  total_query_duration_ += query_duration;

  std::string query_description = CreateQueryDescription(
      query_action, mime_type, key_system, support_type, query_duration);
  LOG(INFO) << query_description;

  if (total_num_queries_ < SB_ARRAY_SIZE_INT(cached_query_durations_)) {
    cached_query_durations_[total_num_queries_] = query_duration;
  }

  if (query_duration > max_query_duration_) {
    max_query_duration_ = query_duration;
    base::strlcpy(max_query_description_, query_description.c_str(),
                  SB_ARRAY_SIZE_INT(max_query_description_));
  }

  ++total_num_queries_;
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
}

// static
void FormatSupportQueryMetrics::PrintAndResetMetrics() {
#if !defined(COBALT_BUILD_TYPE_GOLD)
  if (total_num_queries_ == 0) {
    LOG(INFO) << "Format support query metrics:\n\tNumber of queries: 0";
    return;
  }

  auto get_median = []() {
    int num_elements = std::min(total_num_queries_,
                                SB_ARRAY_SIZE_INT(cached_query_durations_));
    int middle_index = num_elements / 2;
    std::nth_element(cached_query_durations_,
                     cached_query_durations_ + middle_index,
                     cached_query_durations_ + num_elements);
    auto middle_element = cached_query_durations_[middle_index];
    return middle_element.InMicroseconds();
  };

  LOG(INFO) << "Format support query metrics:\n\tNumber of queries: "
            << total_num_queries_ << "\n\tTotal query time: "
            << total_query_duration_.InMicroseconds()
            << " us\n\tAverage query time: "
            << total_query_duration_.InMicroseconds() / total_num_queries_
            << " us\n\tMedian query time: ~" << get_median()
            << " us\n\tLongest query: " << max_query_description_;

  max_query_description_[0] = {};
  max_query_duration_ = {};
  total_query_duration_ = {};
  total_num_queries_ = {};
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
}


}  // namespace media
}  // namespace cobalt
