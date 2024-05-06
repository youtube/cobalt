/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef GLIMP_EGL_SCOPED_EGL_LOCK_H_
#define GLIMP_EGL_SCOPED_EGL_LOCK_H_

#include <pthread.h>

namespace glimp {
namespace egl {

// A helper class to enable easy locking of the glimp EGL global mutex.
class ScopedEGLLock {
 public:
  ScopedEGLLock() { pthread_mutex_lock(&mutex_); }
  ~ScopedEGLLock() { pthread_mutex_unlock(&mutex_); }

 private:
  static pthread_mutex_t mutex_;
};

}  // namespace egl
}  // namespace glimp

#endif  // GLIMP_EGL_SCOPED_EGL_LOCK_H_
