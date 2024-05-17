// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_PTHREAD_THREAD_SAMPLER_INTERNAL_H_
#define STARBOARD_SHARED_PTHREAD_THREAD_SAMPLER_INTERNAL_H_

#include "starboard/thread.h"

class SbThreadSamplerPrivate {
 public:
  explicit SbThreadSamplerPrivate(SbThread thread);
  ~SbThreadSamplerPrivate();

  SbThreadContext Freeze();
  bool Thaw();

  SbThread thread() { return thread_; }

 private:
  SbThread thread_;
};

#endif  // STARBOARD_SHARED_PTHREAD_THREAD_SAMPLER_INTERNAL_H_
