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

#include "cobalt/common/libc/time/icu_time_support.h"

#include <stdio.h>
#include <time.h>
#include <memory>
#include <optional>

#include "cobalt/common/icu_init/init.h"
#include "cobalt/common/libc/no_destructor.h"

#include "starboard/common/log.h"
#include "unicode/gregocal.h"
#include "unicode/timezone.h"
#include "unicode/tznames.h"
#include "unicode/unistr.h"

namespace cobalt {
namespace common {
namespace libc {
namespace time {

IcuTimeSupport* IcuTimeSupport::GetInstance() {
  static NoDestructor<IcuTimeSupport> instance;
  cobalt::common::icu_init::EnsureInitialized();
  return instance.get();
}

void IcuTimeSupport::GetPosixTimezoneGlobals(long& out_timezone,
                                             int& out_daylight,
                                             char** out_tzname) {
  state_.GetPosixTimezoneGlobals(out_timezone, out_daylight, out_tzname);
}

}  // namespace time
}  // namespace libc
}  // namespace common
}  // namespace cobalt
