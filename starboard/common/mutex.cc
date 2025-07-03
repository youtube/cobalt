// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/mutex.h"
#include "starboard/common/log.h"

namespace starboard {

Mutex::Mutex() : mutex_() {
  pthread_mutex_init(&mutex_, nullptr);
}

Mutex::~Mutex() {
  pthread_mutex_destroy(&mutex_);
}

void Mutex::Acquire() const {
  pthread_mutex_lock(&mutex_);
}

bool Mutex::AcquireTry() const {
  return pthread_mutex_trylock(&mutex_) == 0;
}

void Mutex::Release() const {
  pthread_mutex_unlock(&mutex_);
}

pthread_mutex_t* Mutex::mutex() const {
  return &mutex_;
}

ScopedLock::ScopedLock(const Mutex& mutex) : mutex_(mutex) {
  mutex_.Acquire();
}

ScopedLock::~ScopedLock() {
  mutex_.Release();
}

}  // namespace starboard
