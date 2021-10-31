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

#ifndef STARBOARD_COMMON_LOCKED_PTR_H_
#define STARBOARD_COMMON_LOCKED_PTR_H_

#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/scoped_ptr.h"

namespace starboard {

// This class adds thread-safety to any class.  Note when using LockedPtr<T>,
// the caller should use the returned object directly as a pointer without
// holding on it.  This is enforced by making the ctors of Impl private.
template <typename Type>
class LockedPtr {
 public:
  template <typename ImplType>
  class Impl {
   public:
    ~Impl() {
      if (mutex_) {
        mutex_->Release();
      }
    }

    ImplType* operator->() {
      SB_DCHECK(ptr_);
      return ptr_;
    }

   private:
    friend class LockedPtr<Type>;

    Impl(Mutex* mutex, ImplType* ptr) : mutex_(mutex), ptr_(ptr) {
      SB_DCHECK(mutex);
    }

    // The copy ctor transfers the ownership.  So only the last one holds the
    // lock.  This is safe as long as the user won't hold a reference to the
    // returned object.
    Impl(Impl& that) {
      mutex_ = that.mutex_;
      ptr_ = that.ptr_;

      that.mutex_ = NULL;
      that.ptr_ = NULL;
    }

    Mutex* mutex_;
    ImplType* ptr_;
  };

  LockedPtr() : ptr_(NULL) {}
  explicit LockedPtr(scoped_ptr<Type> ptr) : ptr_(ptr) {
    SB_DCHECK(ptr != NULL);
  }

  // This function is solely used to set the pointer to a valid value when it is
  // not convenient to construct the object directly.  So it checks if the
  // existing |ptr_| is NULL.
  void set(scoped_ptr<Type> ptr) {
    starboard::ScopedLock scoped_lock(mutex_);
    SB_DCHECK(ptr_ == NULL);
    ptr_ = ptr.Pass();
  }

  void reset() {
    starboard::ScopedLock scoped_lock(mutex_);
    ptr_.reset();
  }

  bool is_valid() const {
    starboard::ScopedLock scoped_lock(mutex_);
    return ptr_ != NULL;
  }

  Impl<Type> operator->() {
    mutex_.Acquire();
    Impl<Type> impl(&mutex_, ptr_.get());
    return impl;
  }

  Impl<const Type> operator->() const {
    mutex_.Acquire();
    Impl<const Type> impl(&mutex_, ptr_.get());
    return impl;
  }

 private:
  mutable Mutex mutex_;
  scoped_ptr<Type> ptr_;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_LOCKED_PTR_H_
