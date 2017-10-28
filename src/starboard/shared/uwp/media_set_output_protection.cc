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

#include "starboard/media.h"

#include "starboard/log.h"
#include "starboard/shared/uwp/application_uwp.h"
#include "starboard/time.h"

using starboard::shared::uwp::ApplicationUwp;

bool SbMediaSetOutputProtection(bool should_enable_dhcp) {
  bool is_hdcp_on = SbMediaIsOutputProtected();

  if (is_hdcp_on == should_enable_dhcp) {
    return true;
  }

  SB_LOG(INFO) << "Attempting to "
               << (should_enable_dhcp ? "enable" : "disable")
               << " output protection.  Current status: "
               << (is_hdcp_on ? "enabled" : "disabled");
  SbTimeMonotonic tick = SbTimeGetMonotonicNow();

  bool hdcp_success = false;
  if (should_enable_dhcp) {
    hdcp_success = ApplicationUwp::Get()->TurnOnHdcp();
  } else {
    hdcp_success = ApplicationUwp::Get()->TurnOffHdcp();
  }

  is_hdcp_on = (hdcp_success ? should_enable_dhcp : !should_enable_dhcp);

  SbTimeMonotonic tock = SbTimeGetMonotonicNow();
  SB_LOG(INFO) << "Output protection is "
               << (is_hdcp_on ? "enabled" : "disabled")
               << ".  Toggling HDCP took " << (tock - tick) / kSbTimeMillisecond
               << " milliseconds.";
  return hdcp_success;
}
