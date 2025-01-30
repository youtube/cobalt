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

#include "base/synchronization/lock_impl.h"

#include "base/check_op.h"

#if SB_API_VERSION < 16
#include "starboard/mutex.h"
#endif  // SB_API_VERSION < 16

namespace base {
namespace internal {

LockImpl::LockImpl() {
#if SB_API_VERSION < 16
  bool result = SbMutexCreate(&native_handle_);
  DCHECK(result);
#else
  int result = pthread_mutex_init(&native_handle_, nullptr);
  DCHECK_EQ(result, 0);
#endif  // SB_API_VERSION < 16
}

LockImpl::~LockImpl() {
#if SB_API_VERSION < 16
  bool result = SbMutexDestroy(&native_handle_);
  DCHECK(result);
#else
  int result = pthread_mutex_destroy(&native_handle_);
  DCHECK_EQ(result, 0);
#endif  // SB_API_VERSION < 16
}

void LockImpl::LockInternal() {
#if SB_API_VERSION < 16
  SbMutexResult result = SbMutexAcquire(&native_handle_);
  DCHECK_NE(kSbMutexDestroyed, result);
#else
  int result = pthread_mutex_lock(&native_handle_);
  DCHECK_EQ(result, 0);
#endif  // SB_API_VERSION < 16
}

}  // namespace internal
}  // namespace base
