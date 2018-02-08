// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "base/threading/thread_local_storage.h"

#include "base/logging.h"

namespace base {

namespace internal {

// static
bool PlatformThreadLocalStorage::AllocTLS(TLSKey* key) {
  *key = SbThreadCreateLocalKey(
      base::internal::PlatformThreadLocalStorage::OnThreadExit);
  if (!SbThreadIsValidLocalKey(*key)) {
    NOTREACHED();
    return false;
  }

  return true;
}

// static
void PlatformThreadLocalStorage::FreeTLS(TLSKey key) {
  SbThreadDestroyLocalKey(key);
}

// static
void PlatformThreadLocalStorage::SetTLSValue(TLSKey key, void* value) {
  if (!SbThreadSetLocalValue(key, value)) {
    NOTREACHED();
  }
}

}  // namespace internal

}  // namespace base
