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

#ifndef COBALT_COMMON_LIBC_TIME_ICU_TIME_SUPPORT_H_
#define COBALT_COMMON_LIBC_TIME_ICU_TIME_SUPPORT_H_

#include "cobalt/common/libc/no_destructor.h"
#include "cobalt/common/libc/time/time_zone_state.h"

namespace cobalt {
namespace common {
namespace libc {
namespace time {

// Singleton class that provides all time conversion and timezone services.
class IcuTimeSupport {
 public:
  static IcuTimeSupport* GetInstance();

  ~IcuTimeSupport();

  void GetPosixTimezoneGlobals(long& out_timezone,
                               int& out_daylight,
                               char** out_tzname);

 private:
  friend class cobalt::common::libc::NoDestructor<IcuTimeSupport>;
  IcuTimeSupport() = default;
  IcuTimeSupport(const IcuTimeSupport&) = delete;
  void operator=(const IcuTimeSupport&) = delete;

  TimeZoneState state_;
};

}  // namespace time
}  // namespace libc
}  // namespace common
}  // namespace cobalt

#endif  // COBALT_COMMON_LIBC_TIME_ICU_TIME_SUPPORT_H_
