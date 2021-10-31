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

#ifndef COBALT_BASE_REF_COUNTED_LOCK_H_
#define COBALT_BASE_REF_COUNTED_LOCK_H_

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"

namespace base {

//
// A non-thread-safe ref-counted wrapper around a lock.
//
class RefCountedLock : public base::RefCounted<RefCountedLock> {
 public:
  base::Lock& GetLock() { return lock_; }

 private:
  base::Lock lock_;
};

//
// A thread-safe ref-counted wrapper around a lock.
//
class RefCountedThreadSafeLock
    : public base::RefCountedThreadSafe<RefCountedThreadSafeLock> {
 public:
  base::Lock& GetLock() { return lock_; }

 private:
  base::Lock lock_;
};

}  // namespace base

#endif  // COBALT_BASE_REF_COUNTED_LOCK_H_
