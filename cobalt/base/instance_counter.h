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

#ifndef COBALT_BASE_INSTANCE_COUNTER_H_
#define COBALT_BASE_INSTANCE_COUNTER_H_

#include "base/logging.h"

#if defined(COBALT_BUILD_TYPE_GOLD)

#define DECLARE_INSTANCE_COUNTER(class_name)
#define ON_INSTANCE_CREATED(class_name)
#define ON_INSTANCE_RELEASED(class_name)

#else  // defined(COBALT_BUILD_TYPE_GOLD)

#define DECLARE_INSTANCE_COUNTER(class_name)      \
  namespace {                                     \
  SbAtomic32 s_##class_name##_instance_count = 0; \
  }

#define ON_INSTANCE_CREATED(class_name)                                       \
  {                                                                           \
    LOG(INFO) << "New instance of " << #class_name << " is created. We have " \
              << (SbAtomicNoBarrier_Increment(                                \
                     &s_##class_name##_instance_count, 1))                    \
              << " instances in total.";                                      \
  }

#define ON_INSTANCE_RELEASED(class_name)                                   \
  {                                                                        \
    LOG(INFO) << "Instance of " << #class_name << " is released. We have " \
              << (SbAtomicNoBarrier_Increment(                             \
                     &s_##class_name##_instance_count, -1))                \
              << " instances in total.";                                   \
  }
#endif  // defined(COBALT_BUILD_TYPE_GOLD)

#endif  // COBALT_BASE_INSTANCE_COUNTER_H_
