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

#include "base/notreached.h"

namespace base {

namespace internal {

// static
bool PlatformThreadLocalStorage::AllocTLS(TLSKey* key) {
  int res = pthread_key_create(key, base::internal::PlatformThreadLocalStorage::OnThreadExit);
  if (res != 0) {
    NOTREACHED();
    return false;
  }

  return true;
}

// static
void PlatformThreadLocalStorage::FreeTLS(TLSKey key) {
  pthread_key_delete(key);
}

// static
void PlatformThreadLocalStorage::SetTLSValue(TLSKey key, void* value) {
  if (pthread_setspecific(key, value) != 0) {
    NOTREACHED();
  }
}

}  // namespace internal

}  // namespace base
