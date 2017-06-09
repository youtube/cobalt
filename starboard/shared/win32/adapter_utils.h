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

#ifndef STARBOARD_SHARED_WIN32_ADAPTER_UTILS_H_
#define STARBOARD_SHARED_WIN32_ADAPTER_UTILS_H_

#include <winsock2.h>

#include <ifdef.h>
#include <iphlpapi.h>

#include <memory>

#include "starboard/socket.h"

namespace starboard {
namespace shared {
namespace win32 {

// Returns all of the results for wi32's
bool GetAdapters(const SbSocketAddressType address_type,
                 std::unique_ptr<char[]>* adapter_info);

// Returns true if a IP_ADAPTER_ADDRESSES IfType is a
// non-loopback Ethernet interface.
inline bool IsIfTypeEthernet(DWORD iftype) {
  switch (iftype) {
    case IF_TYPE_ETHERNET_CSMACD:
    case IF_TYPE_IEEE80211:
      return true;
    case IF_TYPE_SOFTWARE_LOOPBACK:
    default:
      return false;
  }
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_ADAPTER_UTILS_H_
