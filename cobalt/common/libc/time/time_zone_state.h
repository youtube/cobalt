// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_COMMON_LIBC_TIME_TIME_ZONE_STATE_H_
#define COBALT_COMMON_LIBC_TIME_TIME_ZONE_STATE_H_

#include <atomic>
#include <memory>
#include <string>

#include "unicode/timezone.h"

#ifndef TZNAME_MAX
#define TZNAME_MAX 6
#endif

namespace cobalt {
namespace common {
namespace libc {
namespace time {

class TimeZoneState {
 public:
  TimeZoneState();
  ~TimeZoneState();

  // Returns the currently active ICU timezone object.
  std::shared_ptr<const icu::TimeZone> GetTimeZone();

  // Calculates the values for the global C variables (timezone, daylight,
  // tzname).
  void GetPosixTimezoneGlobals(long& out_timezone,
                               int& out_daylight,
                               char** out_tzname);

 private:
  // Parses the TZ environment variable and updates the internal timezone.
  // If TZ is unset or invalid, it falls back to the system default or UTC.
  void EnsureTimeZoneIsCreated();

  // Creates a timezone from a given IANA timezone ID (e.g.,
  // "America/Los_Angeles") and sets the relevant internal state.
  std::unique_ptr<icu::TimeZone> CreateTimeZoneFromIanaId(
      const std::string& tz_id);

  // Cached values to avoid recalculation.
  std::string cached_timezone_name_;
  std::shared_ptr<const icu::TimeZone> current_zone_;
  std::string std_name_;
  std::string dst_name_;

  // Static buffers for tzname to point to, managed by this class.
  // Note: This introduces a global state, but it is a necessary
  // and contained approach to correctly implement the `tzname` pointers,
  // which are expected to be pointers to static memory.
  static char std_name_buffer_[TZNAME_MAX + 1];
  static char dst_name_buffer_[TZNAME_MAX + 1];
};

}  // namespace time
}  // namespace libc
}  // namespace common
}  // namespace cobalt

#endif  // COBALT_COMMON_LIBC_TIME_TIME_ZONE_STATE_H_
