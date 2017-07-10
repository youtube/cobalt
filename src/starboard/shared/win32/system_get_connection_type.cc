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

#include "starboard/system.h"

#include <winsock2.h>

#include <ifdef.h>
#include <iphlpapi.h>

#include "starboard/shared/win32/adapter_utils.h"

namespace sbwin32 = ::starboard::shared::win32;

namespace {
// Return the connection type of the first "up" ethernet interface,
// or unknown if none found.
SbSystemConnectionType FindConnectionType(PIP_ADAPTER_ADDRESSES adapter) {
  for (; adapter != nullptr; adapter = adapter->Next) {
    if ((adapter->OperStatus != IfOperStatusUp) ||
        !sbwin32::IsIfTypeEthernet(adapter->IfType)) {
      continue;
    }
    if (adapter->IfType == IF_TYPE_IEEE80211) {
      return kSbSystemConnectionTypeWireless;
    }
    return kSbSystemConnectionTypeWired;
  }
  return kSbSystemConnectionTypeUnknown;
}
}  // namespace

SbSystemConnectionType SbSystemGetConnectionType() {
  std::unique_ptr<char[]> buffer;
  if (!sbwin32::GetAdapters(kSbSocketAddressTypeIpv4, &buffer)) {
      return kSbSystemConnectionTypeUnknown;
  }
  SbSystemConnectionType result = FindConnectionType(
      reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.get()));
  if (result != kSbSystemConnectionTypeUnknown) {
    return result;
  }
  if (!sbwin32::GetAdapters(kSbSocketAddressTypeIpv6, &buffer)) {
      return kSbSystemConnectionTypeUnknown;
  }
  return FindConnectionType(
      reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.get()));
}
