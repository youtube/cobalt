// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
