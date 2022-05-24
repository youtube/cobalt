// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/socket.h"

using starboard::android::shared::JniEnvExt;

namespace {

const size_t kDefaultPrefixLength = 8;
const int kIPv6AddressSize = 16;

bool CopySocketAddress(jbyteArray array, SbSocketAddress* out_address) {
  JniEnvExt* env = JniEnvExt::Get();
  if (array == nullptr) {
    return false;
  }
  if (out_address == nullptr) {
    SB_LOG(ERROR) << "SbSocketGetInterfaceAddress NULL out_address";
    return false;
  }
  jint size = env->GetArrayLength(array);
  if (size > sizeof(out_address->address)) {
    SB_LOG(ERROR) << "SbSocketGetInterfaceAddress address too long";
    return false;
  }
  out_address->type =
      (size == 4) ? kSbSocketAddressTypeIpv4 : kSbSocketAddressTypeIpv6;
  jbyte* bytes = env->GetByteArrayElements(array, NULL);
  SB_CHECK(bytes) << "GetByteArrayElements failed";
  memcpy(out_address->address, bytes, size);
  env->ReleaseByteArrayElements(array, bytes, JNI_ABORT);
  return true;
}
}  // namespace

bool SbSocketGetInterfaceAddress(const SbSocketAddress* const destination,
                                 SbSocketAddress* out_source_address,
                                 SbSocketAddress* out_netmask) {
  if (out_source_address == nullptr) {
    return false;
  }

  memset(out_source_address->address, 0, sizeof(out_source_address->address));

  out_source_address->port = 0;

  JniEnvExt* env = JniEnvExt::Get();

  jboolean want_ipv6 =
      (destination != nullptr && destination->type == kSbSocketAddressTypeIpv6);
  jobject pair = (jbyteArray)env->CallStarboardObjectMethodOrAbort(
      "getLocalInterfaceAddressAndNetmask", "(Z)Landroid/util/Pair;",
      want_ipv6);

  if (!pair) {
    SB_LOG(ERROR) << "Null value returned from JNI call to "
                     "getLocalInterfaceAddressAndNetmask.";
    return false;
  }

  jobject field;
  field = env->GetObjectFieldOrAbort(pair, "first", "Ljava/lang/Object;");
  if (!CopySocketAddress(static_cast<jbyteArray>(field), out_source_address)) {
    return false;
  }
  field = env->GetObjectFieldOrAbort(pair, "second", "Ljava/lang/Object;");
  if (out_netmask &&
      !CopySocketAddress(static_cast<jbyteArray>(field), out_netmask)) {
    return false;
  }

  return true;
}
