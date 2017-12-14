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

#include "net/base/net_util.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/socket.h"

using starboard::android::shared::JniEnvExt;

namespace {

const size_t kDefaultPrefixLength = 8;

}  // namespace

// Note: The following is an incorrect implementation for
// SbSocketGetInterfaceAddress.  Specifically, things missing are:
// We should see if the destination is NULL, or ANY address, or a regular
// unicast address.
// Also, currently, we assume netmask is 255.255.255.0 in IPv4, and
// has a prefix length of 64 for IPv6.
// For IPv6, there are a few rules about which IPs are valid (and prefered).
// E.g. globally routable IP addresses are prefered over ULAs.
bool SbSocketGetInterfaceAddress(const SbSocketAddress* const /*destination*/,
                                 SbSocketAddress* out_source_address,
                                 SbSocketAddress* out_netmask) {
  if (out_source_address == NULL) {
    return false;
  }

  SbMemorySet(out_source_address->address, 0,
              sizeof(out_source_address->address));
  out_source_address->port = 0;

  JniEnvExt* env = JniEnvExt::Get();

  jbyteArray s = (jbyteArray)env->CallStarboardObjectMethodOrAbort(
      "getLocalInterfaceAddress", "()[B");
  if (s == NULL) {
    return false;
  }

  jint sz = env->GetArrayLength(s);
  if (sz > sizeof(out_source_address->address)) {
    // This should never happen
    SB_LOG(ERROR) << "SbSocketGetInterfaceAddress address too big";
    return false;
  }
  switch (sz) {
    case 4:
      out_source_address->type = kSbSocketAddressTypeIpv4;
      if (out_netmask) {
        out_netmask->address[0] = 255;
        out_netmask->address[1] = 255;
        out_netmask->address[2] = 255;
        out_netmask->address[3] = 0;
        out_netmask->type = kSbSocketAddressTypeIpv4;
      }
      break;
    default:
      out_source_address->type = kSbSocketAddressTypeIpv6;
      if (out_netmask) {
        for (int i = 0; i < kDefaultPrefixLength; ++i) {
          out_netmask->address[i] = 0xff;
        }
        for (int i = kDefaultPrefixLength; i < net::kIPv6AddressSize; ++i) {
          out_netmask->address[i] = 0;
        }
        out_netmask->type = kSbSocketAddressTypeIpv6;
      }
      break;
  }

  jbyte* bytes = env->GetByteArrayElements(s, NULL);
  SB_CHECK(bytes) << "GetByteArrayElements failed";
  SbMemoryCopy(out_source_address->address, bytes, sz);
  env->ReleaseByteArrayElements(s, bytes, JNI_ABORT);

  return true;
}
