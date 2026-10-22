// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_AOSP_SHARED_CHROMIUM_SWITCHES_H_
#define STARBOARD_AOSP_SHARED_CHROMIUM_SWITCHES_H_

// Chromium switch names for the AOSP Starboard layer, which can't use
// Chromium's ::switches constants (it builds against //copied_base/base).

namespace starboard {

// Expanded from Chromium's header-only list, so values match ::switches.
namespace network_switches {

#define NETWORK_SWITCH(name, value) inline constexpr char name[] = value;
#include "components/network_session_configurator/common/network_switch_list.h"  // nogncheck
#undef NETWORK_SWITCH

}  // namespace network_switches

// Copied from base/base_switches.cc. Keep in sync.
namespace base_switches {

// ::switches::kDisableDevShmUsage is only defined for IS_LINUX, which is false
// in this toolchain.
inline constexpr char kDisableDevShmUsage[] = "disable-dev-shm-usage";

}  // namespace base_switches

}  // namespace starboard

#endif  // STARBOARD_AOSP_SHARED_CHROMIUM_SWITCHES_H_
