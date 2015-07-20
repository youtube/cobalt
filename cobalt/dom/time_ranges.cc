/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/time_ranges.h"

#include <algorithm>

#include "base/logging.h"
#include "cobalt/dom/dom_exception.h"

namespace cobalt {
namespace dom {

bool TimeRanges::TimeRange::IsOverlapped(const TimeRange& that) const {
  return end_ >= that.start_ && start_ <= that.end_;
}

void TimeRanges::TimeRange::MergeWith(const TimeRange& that) {
  DCHECK(IsOverlapped(that));
  start_ = std::min(start_, that.start_);
  end_ = std::max(end_, that.end_);
}

double TimeRanges::Start(uint32 index,
                         script::ExceptionState* exception_state) const {
  if (index >= ranges_.size()) {
    exception_state->SetException(
        make_scoped_refptr(new DOMException(DOMException::kIndexSizeErr)));
    // Return value should be ignored.
    return 0.0;
  }
  return ranges_[index].start();
}

double TimeRanges::End(uint32 index,
                       script::ExceptionState* exception_state) const {
  if (index >= ranges_.size()) {
    exception_state->SetException(
        make_scoped_refptr(new DOMException(DOMException::kIndexSizeErr)));
    // Return value should be ignored.
    return 0.0;
  }
  return ranges_[index].end();
}

bool TimeRanges::Contains(double time) const {
  std::vector<TimeRange>::const_iterator lower_bound =
      std::lower_bound(ranges_.begin(), ranges_.end(), time, LessThan);
  return (lower_bound != ranges_.end()) ? lower_bound->Contains(time) : false;
}

void TimeRanges::Add(double start, double end) {
  // Use index instead of iterator as we are going to modify ranges_.
  uint32 lower_bound = static_cast<uint32>(
      std::lower_bound(ranges_.begin(), ranges_.end(), start, LessThan) -
      ranges_.begin());
  ranges_.insert(ranges_.begin() + lower_bound, TimeRange(start, end));
  while (ranges_.size() > lower_bound + 1) {
    if (!ranges_[lower_bound].IsOverlapped(ranges_[lower_bound + 1])) {
      break;
    }
    ranges_[lower_bound].MergeWith(ranges_[lower_bound + 1]);
    ranges_.erase(ranges_.begin() + lower_bound + 1);
  }
}

double TimeRanges::Nearest(double time) const {
  // Assume an empty TimeRanges contains [0.0, 0.0] and return 0.0 in this case.
  if (length() == 0) return 0.0;

  std::vector<TimeRange>::const_iterator lower_bound =
      std::lower_bound(ranges_.begin(), ranges_.end(), time, LessThan);

  // Return the end of last range if time is after all ranges.
  if (lower_bound == ranges_.end()) {
    DCHECK_LT((lower_bound - 1)->end(), time);
    return (lower_bound - 1)->end();
  }

  if (lower_bound->Contains(time)) return time;

  // Return the start of first range if time is before all ranges.
  if (lower_bound == ranges_.begin()) {
    DCHECK_LT(time, lower_bound->start());
    return lower_bound->start();
  }

  // time is in between (lower_bound - 1) and lower_bound.
  double prev = (lower_bound - 1)->end();
  double next = lower_bound->start();
  DCHECK_LT(prev, time);
  DCHECK_LT(time, next);
  return (time - prev <= next - time) ? prev : next;
}

}  // namespace dom
}  // namespace cobalt
