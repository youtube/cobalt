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

#ifndef NB_THREAD_LOCAL_BOOLEAN_H_
#define NB_THREAD_LOCAL_BOOLEAN_H_

#include "nb/thread_local_pointer.h"

namespace nb {

class ThreadLocalBoolean {
 public:
  ThreadLocalBoolean() : default_value_(false) {}
  explicit ThreadLocalBoolean(bool default_value)
      : default_value_(default_value) {}
  ~ThreadLocalBoolean() {}

  bool Get() const {
    bool val = tlp_.Get() != NULL;
    return val ^ default_value_;
  }

  void Set(bool val) {
    val = val ^ default_value_;
    tlp_.Set(val ? TruePointer() : FalsePointer());
  }

 private:
  static void* TruePointer() { return reinterpret_cast<void*>(0x1); }
  static void* FalsePointer() { return NULL; }
  ThreadLocalPointer<void> tlp_;
  const bool default_value_;

  ThreadLocalBoolean(const ThreadLocalBoolean&) = delete;
  void operator=(const ThreadLocalBoolean&) = delete;
};

}  // namespace nb.

#endif  // NB_THREAD_LOCAL_POINTER_H_
