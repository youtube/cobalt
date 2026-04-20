// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_POSIX_EMU_INCLUDE_PTHREAD_H_
#define STARBOARD_ANDROID_SHARED_POSIX_EMU_INCLUDE_PTHREAD_H_

#include_next <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#if __ANDROID_API__ < 26
int pthread_getname_np(pthread_t thread, char* name, size_t len);
#endif  // __ANDROID_API__ < 26

#ifdef __cplusplus
}
#endif

#endif  // STARBOARD_ANDROID_SHARED_POSIX_EMU_INCLUDE_PTHREAD_H_
