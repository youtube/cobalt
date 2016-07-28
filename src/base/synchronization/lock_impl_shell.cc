/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base/synchronization/lock_impl.h"

#include <errno.h>

#include "base/logging.h"

namespace base {
namespace internal {

LockImpl::LockImpl() {
  int rv = lb_shell_mutex_init(&os_lock_);
  DCHECK_EQ(rv, 0);
}

LockImpl::~LockImpl() {
  int rv = lb_shell_mutex_destroy(&os_lock_);
  DCHECK_EQ(rv, 0);
}

bool LockImpl::Try() {
  int rv = lb_shell_mutex_trylock(&os_lock_);
  DCHECK(rv == 0 || rv == EBUSY);
  return rv == 0;
}

void LockImpl::Lock() {
  int rv = lb_shell_mutex_lock(&os_lock_);
  DCHECK_EQ(rv, 0);
}

void LockImpl::Unlock() {
  int rv = lb_shell_mutex_unlock(&os_lock_);
  DCHECK_EQ(rv, 0);
}

}  // namespace internal
}  // namespace base
