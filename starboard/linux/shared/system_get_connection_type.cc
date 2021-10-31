// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/system.h"

#include "starboard/linux/shared/routes.h"

using starboard::shared::Routes;

SbSystemConnectionType SbSystemGetConnectionType() {
  Routes routes;

  if (!routes.RequestDump()) {
    return kSbSystemConnectionTypeUnknown;
  }

  // Find the top priority route and return if the corresponding route uses a
  // wireless interface.
  int priority = INT_MAX;
  bool is_wireless = false;
  while (auto route = routes.GetNextRoute()) {
    if (Routes::IsDefaultRoute(*route)) {
      if (route->priority < priority) {
        priority = route->priority;
        is_wireless = Routes::IsWirelessInterface(*route);
      }
    }
  }
  if (priority != INT_MAX)
    return is_wireless ? kSbSystemConnectionTypeWireless
                       : kSbSystemConnectionTypeWired;
  return kSbSystemConnectionTypeUnknown;
}
