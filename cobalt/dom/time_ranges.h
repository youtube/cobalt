// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_TIME_RANGES_H_
#define COBALT_DOM_TIME_RANGES_H_

#include <vector>

#include "cobalt/script/exception_state.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The TimeRanges interface is used to describe a series of time ranges. Each of
// them has a start time and an end time.
//   https://www.w3.org/TR/html50/embedded-content-0.html#timeranges
//
// Note that in our implementation it is always normalized, i.e. all contained
// ranges are sorted ascendantly and not overlapped.  The limits of the range
// are [-Infinity, Infinity].
class TimeRanges : public script::Wrappable {
 public:
  TimeRanges() {}
  TimeRanges(double start, double end) { Add(start, end); }

  // Web API: TimeRanges
  //
  uint32 length() const { return static_cast<uint32>(ranges_.size()); }
  double Start(uint32 index, script::ExceptionState* exception_state) const;
  double End(uint32 index, script::ExceptionState* exception_state) const;

  // Custom, not in any spec.
  //
  // If the time is contained inside any range.
  bool Contains(double time) const;
  // Add the range into the ranges. Merge overlapping ranges if necessary.
  void Add(double start, double end);
  // Returns time if Contain(time) is true. Otherwise returns a time in the
  // ranges that is closest to time.
  double Nearest(double time) const;

  // Returns a copy of the current object.
  scoped_refptr<TimeRanges> Clone() const;
  // Returns a TimeRanges object contains the union of the current object and
  // |that|.
  scoped_refptr<TimeRanges> UnionWith(
      const scoped_refptr<TimeRanges>& that) const;
  // Returns a TimeRanges object contains the intersection of the current object
  // and |that|.
  scoped_refptr<TimeRanges> IntersectWith(
      const scoped_refptr<TimeRanges>& that) const;

  DEFINE_WRAPPABLE_TYPE(TimeRanges);

 private:
  scoped_refptr<TimeRanges> Invert() const;

  class TimeRange {
   public:
    TimeRange(double start, double end) : start_(start), end_(end) {}

    double start() const { return start_; }
    double end() const { return end_; }

    bool IsOverlapped(const TimeRange& that) const;
    void MergeWith(const TimeRange& that);
    bool Contains(double time) const { return start_ <= time && time <= end_; }

   private:
    double start_;
    double end_;
  };

  // Compare functor for std::lower_bound.
  static bool LessThan(const TimeRange& range, double time) {
    return range.end() < time;
  }

  std::vector<TimeRange> ranges_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TIME_RANGES_H_
