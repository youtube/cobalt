// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BASE_CLOCK_H_
#define COBALT_BASE_CLOCK_H_

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"

namespace base {

// A generic clock interface for querying the current time.  Inserting a
// level of indirection between querying for the current time allows one to
// have greater control over how their system reacts to time changes.
class Clock : public base::RefCountedThreadSafe<Clock> {
 public:
  virtual base::TimeDelta Now() = 0;

 protected:
  virtual ~Clock() {}
  friend class base::RefCountedThreadSafe<Clock>;
};

// The SystemClock calls in to the standard base::TimeTicks::HighResNow() method
// to obtain a time.
class SystemMonotonicClock : public Clock {
 public:
  SystemMonotonicClock() { origin_ = base::TimeTicks::HighResNow(); }

  base::TimeDelta Now() OVERRIDE {
    return base::TimeTicks::HighResNow() - origin_;
  }

 private:
  base::TimeTicks origin_;

  ~SystemMonotonicClock() OVERRIDE {}
};

// The OffsetClock takes a parent clock and an offset upon construction, and
// when queried for the time it returns the time of the parent clock offset by
// the specified offset.
class OffsetClock : public Clock {
 public:
  OffsetClock(const scoped_refptr<Clock> parent, const base::TimeDelta& origin)
      : parent_(parent), origin_(origin) {
    DCHECK(parent_);
  }

  base::TimeDelta Now() OVERRIDE { return parent_->Now() - origin_; }

  base::TimeDelta origin() const { return origin_; }

 private:
  ~OffsetClock() OVERRIDE {}

  scoped_refptr<Clock> parent_;
  const base::TimeDelta origin_;
};

}  // namespace base

#endif  // COBALT_BASE_CLOCK_H_
