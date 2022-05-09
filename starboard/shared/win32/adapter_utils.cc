// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/win32/adapter_utils.h"

#include <winsock2.h>

#include <iphlpapi.h>

#include <memory>

#include "starboard/common/log.h"
#include "starboard/common/socket.h"
#include "starboard/shared/win32/socket_internal.h"

namespace {
const ULONG kDefaultAdapterInfoBufferSizeInBytes = 16 * 1024;
}  // namespace

namespace starboard {
namespace shared {
namespace win32 {

bool GetAdapters(const SbSocketAddressType address_type,
                 std::unique_ptr<char[]>* adapter_info) {
  SB_DCHECK(adapter_info);

  ULONG family = 0;
  int address_length_bytes = 0;

  switch (address_type) {
    case kSbSocketAddressTypeIpv4:
      family = AF_INET;
      address_length_bytes = kAddressLengthIpv4;
      break;
    case kSbSocketAddressTypeIpv6:
      family = AF_INET6;
      address_length_bytes = kAddressLengthIpv6;
      break;
    default:
      SB_NOTREACHED() << "Invalid address type: " << address_type;
      return false;
  }

  ULONG adapter_addresses_number_bytes = kDefaultAdapterInfoBufferSizeInBytes;

  for (int try_count = 0; try_count != 2; ++try_count) {
    // Using auto for return value here, since different versions of windows use
    // slightly different datatypes.  These differences do not matter to us, but
    // the compiler might warn on them.
    adapter_info->reset(new char[adapter_addresses_number_bytes]);
    PIP_ADAPTER_ADDRESSES adapter_addresses =
        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapter_info->get());

    // Note: If |GetAdapterAddresses| deems that buffer supplied is not enough,
    // it will return the recommended number of bytes in
    // |adapter_addresses_number_bytes|.
    auto retval = GetAdaptersAddresses(
        family, GAA_FLAG_SKIP_FRIENDLY_NAME | GAA_FLAG_SKIP_DNS_SERVER, nullptr,
        adapter_addresses, &adapter_addresses_number_bytes);

    if (retval == ERROR_SUCCESS) {
      return true;
    }
    if (retval != ERROR_BUFFER_OVERFLOW) {
      // Only retry with more memory if the error says so.
      break;
    }
    SB_LOG(ERROR) << "GetAdapterAddresses() failed with error code " << retval;
  }
  return false;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
