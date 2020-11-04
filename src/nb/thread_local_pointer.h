/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef NB_THREAD_LOCAL_POINTER_H_
#define NB_THREAD_LOCAL_POINTER_H_

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/thread.h"

namespace nb {

template <typename Type>
class ThreadLocalPointer {
 public:
  ThreadLocalPointer() {
    slot_ = SbThreadCreateLocalKey(NULL);  // No destructor for pointer.
    SB_DCHECK(kSbThreadLocalKeyInvalid != slot_);
  }

  ~ThreadLocalPointer() { SbThreadDestroyLocalKey(slot_); }

  Type* Get() const {
    void* ptr = SbThreadGetLocalValue(slot_);
    Type* type_ptr = static_cast<Type*>(ptr);
    return type_ptr;
  }

  void Set(Type* ptr) {
    void* void_ptr = static_cast<void*>(ptr);
    SbThreadSetLocalValue(slot_, void_ptr);
  }

 private:
  SbThreadLocalKey slot_;
  ThreadLocalPointer<Type>(const ThreadLocalPointer<Type>&) = delete;
  void operator=(const ThreadLocalPointer<Type>&) = delete;
};

}  // namespace nb.

#endif  // NB_THREAD_LOCAL_POINTER_H_
