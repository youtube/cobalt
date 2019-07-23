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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKMUTEX_STARBOARD_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKMUTEX_STARBOARD_H_

#include "starboard/common/mutex.h"
#include "starboard/thread.h"

// A Starboard-based implementation of mutex, with support for static
// initialization without initializer/finalizer, which can cause race
// conditions at application exit.

// A SkBaseMutex is a POD structure that can be directly initialized
// at declaration time with SK_DECLARE_STATIC/GLOBAL_MUTEX. This avoids the
// generation of a static initializer/finalizer.
struct SkBaseMutex {
  void acquire() {
    SbMutexAcquire(&mutex_);
    SetAcquired();
  }

  void release() {
    SetReleased();
    SbMutexRelease(&mutex_);
  }

  void assertHeld() { AssertAcquired(); }

  SbMutex mutex_;

#ifdef SK_DEBUG
  void Init() {
    holding_thread_ = kSbThreadInvalid;
    initialized_ = true;
  }

  void SetAcquired() {
    if (!initialized_) {
      Init();
    }
    SB_DCHECK(holding_thread_ == kSbThreadInvalid);
    holding_thread_ = SbThreadGetCurrent();
  }

  void SetReleased() {
    SB_DCHECK(holding_thread_ == SbThreadGetCurrent());
    holding_thread_ = kSbThreadInvalid;
  }

  void AssertAcquired() const {
    SkASSERT(initialized_ && holding_thread_ != kSbThreadInvalid);
  }

  // |initialized_| will be default-initialized to |false| by the POD-style
  // initializer.
  bool initialized_;
  SbThread holding_thread_;
#else
  void Init() {}
  void SetAcquired() {}
  void SetReleased() {}
  void AssertAcquired() const {}
#endif  // SK_DEBUG
};

// A mutex that requires to be initialized through normal C++ construction,
// i.e. when it's a member of another class, or allocated on the heap.
class SkMutex : public SkBaseMutex {
 public:
  SkMutex() {
    Init();
    SbMutexCreate(&mutex_);
  }
  ~SkMutex() { SbMutexDestroy(&mutex_); }
};

#define SK_BASE_MUTEX_INIT \
  { SB_MUTEX_INITIALIZER }

// Using POD-style initialization prevents the generation of a static
// initializer.
//
// Without magic statics there are no thread safety guarantees on initialization
// of local statics (even POD). As a result, it is illegal to use
// SK_DECLARE_STATIC_MUTEX in a function.
//
// Because SkBaseMutex is not a primitive, a static SkBaseMutex cannot be
// initialized in a class with this macro.
#define SK_DECLARE_STATIC_MUTEX(name) \
  namespace {}                        \
  static SkBaseMutex name = SK_BASE_MUTEX_INIT

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKMUTEX_STARBOARD_H_
