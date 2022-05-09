// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

// Defines the pthread versions of Starboard synchronization primitives and the
// static initializers for those primitives.

#ifndef STARBOARD_SHARED_WIN32_TYPES_INTERNAL_H_
#define STARBOARD_SHARED_WIN32_TYPES_INTERNAL_H_

#define SB_WIN32_INTERNAL_MUTEX(mutex_ptr) \
  reinterpret_cast<PSRWLOCK>((mutex_ptr)->mutex_buffer)
#define SB_WIN32_INTERNAL_CONDITION(condition_ptr) \
  reinterpret_cast<PCONDITION_VARIABLE>((condition_ptr)->condition_buffer)
#define SB_WIN32_INTERNAL_ONCE(once_ptr) \
  reinterpret_cast<PINIT_ONCE>((once_ptr)->once_buffer)
#else
#define SB_WIN32_INTERNAL_MUTEX(mutex_ptr) reinterpret_cast<PSRWLOCK>(mutex_ptr)
#define SB_WIN32_INTERNAL_CONDITION(condition_ptr) \
  reinterpret_cast<PCONDITION_VARIABLE>(condition_ptr)
#define SB_WIN32_INTERNAL_ONCE(once_ptr) reinterpret_cast<PINIT_ONCE>(once_ptr)

#endif  // STARBOARD_SHARED_WIN32_TYPES_INTERNAL_H_
