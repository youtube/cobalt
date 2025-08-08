// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/impression_detail.h"

namespace notifications {

ImpressionDetail::ImpressionDetail()
    : current_max_daily_show(0), num_shown_today(0), num_negative_events(0) {}

ImpressionDetail::ImpressionDetail(
    size_t current_max_daily_show,
    size_t num_shown_today,
    size_t num_negative_events,
    absl::optional<base::Time> last_negative_event_ts,
    absl::optional<base::Time> last_shown_ts)
    : current_max_daily_show(current_max_daily_show),
      num_shown_today(num_shown_today),
      num_negative_events(num_negative_events),
      last_negative_event_ts(last_negative_event_ts),
      last_shown_ts(last_shown_ts) {}

ImpressionDetail::ImpressionDetail(const ImpressionDetail& other) = default;

ImpressionDetail::~ImpressionDetail() = default;

bool ImpressionDetail::operator==(const ImpressionDetail& other) const {
  return current_max_daily_show == other.current_max_daily_show &&
         num_shown_today == other.num_shown_today &&
         num_negative_events == other.num_negative_events &&
         last_negative_event_ts == other.last_negative_event_ts &&
         last_shown_ts == other.last_shown_ts;
}

}  // namespace notifications
