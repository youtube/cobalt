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

#include "starboard/time_zone.h"

#include <time.h>

SbTimeZone SbTimeZoneGetCurrent() {
  // TODO: Using timezone assumes that tzset() has been called at some
  // point. That should happen as part of Starboard's main loop initialization,
  // but that doesn't exist yet.
  return timezone / 60;
}
