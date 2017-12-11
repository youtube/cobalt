// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/memory_tracker/tool/util.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "base/time.h"
#include "nb/bit_cast.h"
#include "starboard/string.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

const char kQuote[] = "\"";
const char kDelimiter[] = ",";
const char kNewLine[] = "\n";

std::string RemoveString(const std::string& haystack, const char* needle) {
  const size_t kNotFound = std::string::npos;

  // Base case. No modification needed.
  size_t pos = haystack.find(needle);
  if (pos == kNotFound) {
    return haystack;
  }
  const size_t n = SbStringGetLength(needle);
  std::string output;
  output.reserve(haystack.size());

  // Copy string, omitting the portion containing the "needle".
  std::copy(haystack.begin(), haystack.begin() + pos,
            std::back_inserter(output));
  std::copy(haystack.begin() + pos + n, haystack.end(),
            std::back_inserter(output));

  // Recursively remove same needle in haystack.
  return RemoveString(output, needle);
}

std::string SanitizeCSVKey(std::string key) {
  key = RemoveString(key, kQuote);
  key = RemoveString(key, kDelimiter);
  key = RemoveString(key, kNewLine);
  return key;
}

std::string InsertCommasIntoNumberString(const std::string& input) {
  typedef std::vector<char> CharVector;
  typedef CharVector::iterator CharIt;

  CharVector chars(input.begin(), input.end());
  std::reverse(chars.begin(), chars.end());

  CharIt curr_it = chars.begin();
  CharIt mid = std::find(chars.begin(), chars.end(), '.');
  if (mid == chars.end()) {
    mid = curr_it;
  }

  CharVector out(curr_it, mid);

  int counter = 0;
  for (CharIt it = mid; it != chars.end(); ++it) {
    if (counter != 0 && (counter % 3 == 0)) {
      out.push_back(',');
    }
    if (*it != '.') {
      counter++;
    }
    out.push_back(*it);
  }

  std::reverse(out.begin(), out.end());
  std::stringstream ss;
  for (size_t i = 0; i < out.size(); ++i) {
    ss << out[i];
  }
  return ss.str();
}

Timer::Timer(base::TimeDelta delta_time) {
  start_time_ = Now();
  time_before_expiration_ = delta_time;
}

Timer::Timer(base::TimeDelta delta_time, Timer::TimeFunctor time_functor) {
  time_function_override_ = time_functor;
  start_time_ = Now();
  time_before_expiration_ = delta_time;
}

void Timer::Restart() { start_time_ = Now(); }

bool Timer::UpdateAndIsExpired() {
  base::TimeTicks now_time = Now();
  base::TimeDelta delta_time = now_time - start_time_;
  if (delta_time >= time_before_expiration_) {
    start_time_ = now_time;
    return true;
  } else {
    return false;
  }
}

base::TimeTicks Timer::Now() {
  if (time_function_override_.is_null()) {
    return base::TimeTicks::HighResNow();
  } else {
    return time_function_override_.Run();
  }
}

void Timer::ScaleTriggerTime(double scale) {
  int64_t old_dt = time_before_expiration_.InMicroseconds();
  int64_t new_dt =
      static_cast<int64_t>(static_cast<double>(old_dt) * scale);
  time_before_expiration_ = base::TimeDelta::FromMicroseconds(new_dt);
}

Segment::Segment(const std::string* name,
                 const char* start_address,
                 const char* end_address)
    : name_(name), begin_(start_address), end_(end_address) {
}

void Segment::SplitAcrossPageBoundaries(size_t page_size,
                                        std::vector<Segment>* segments) const {
  if (size() == 0) {
    segments->push_back(*this);
    return;
  }

  uintptr_t page_start =
      nb::bit_cast<uintptr_t>(begin_) / page_size;
  uintptr_t page_end =
      nb::bit_cast<uintptr_t>(end_ - 1) / page_size;

  if (page_start == page_end) {
    segments->push_back(*this);
    return;
  }

  for (uintptr_t p = page_start; p <= page_end; ++p) {
    uintptr_t start_addr;
    if (p == page_start) {
      start_addr = nb::bit_cast<uintptr_t>(begin_);
    } else {
      start_addr = p * page_size;
    }

    uintptr_t end_addr;
    if (p == page_end) {
      end_addr = nb::bit_cast<uintptr_t>(end_);
    } else {
      end_addr = (p+1) * page_size;
    }

    const char* start = nb::bit_cast<const char*>(start_addr);
    const char* end = nb::bit_cast<const char*>(end_addr);
    segments->push_back(Segment(name_, start, end));
  }
}

bool Segment::Intersects(const Segment& other) const {
  size_t total_span = std::distance(
      std::min(begin_, other.begin()),
      std::max(end_, other.end()));

  bool intersects = (size() + other.size()) > total_span;
  return intersects;
}

bool Segment::operator<(const Segment& other) const {
  return begin_ < other.begin();
}

bool Segment::operator==(const Segment& other) const {
  if (begin_ == other.begin() && end_ == other.end()) {
    DCHECK(name_ == other.name_);
    return true;
  }
  return false;
}

size_t Segment::size() const {
  return std::distance(begin_, end_);
}

const char* BaseNameFast(const char* file_name) {
  // Case: Linux.
  const char* end_pos = file_name + SbStringGetLength(file_name);
  const char* last_forward_slash = SbStringFindLastCharacter(file_name, '/');
  if (last_forward_slash) {
    if (end_pos != last_forward_slash) {
      ++last_forward_slash;
    }
    return last_forward_slash;
  }

  // Case: Windows.
  const char* last_backward_slash = SbStringFindLastCharacter(file_name, '\\');
  if (last_backward_slash) {
    if (end_pos != last_backward_slash) {
      ++last_backward_slash;
    }
    return last_backward_slash;
  }
  return file_name;
}

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt
