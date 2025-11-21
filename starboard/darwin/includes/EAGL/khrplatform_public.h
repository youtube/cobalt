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

// The Khronos platform-specific types and definitions for Darwin.

// Other source files should never include this header directly, but should
// include the generic "KHR/khrplatform.h" instead.

#ifndef STARBOARD_DARWIN_INCLUDES_EAGL_KHRPLATFORM_PUBLIC_H_
#define STARBOARD_DARWIN_INCLUDES_EAGL_KHRPLATFORM_PUBLIC_H_

#include <stdint.h>
#include <unistd.h>

#define KHRONOS_APICALL
#define KHRONOS_APIENTRY
#define KHRONOS_APIATTRIBUTES

#define KHRONOS_SUPPORT_INT64 1
#define KHRONOS_SUPPORT_FLOAT 1

typedef int32_t khronos_int32_t;
typedef uint32_t khronos_uint32_t;
typedef int64_t khronos_int64_t;
typedef uint64_t khronos_uint64_t;

typedef int8_t khronos_int8_t;
typedef uint8_t khronos_uint8_t;
typedef int16_t khronos_int16_t;
typedef uint16_t khronos_uint16_t;

typedef intptr_t khronos_intptr_t;
typedef uintptr_t khronos_uintptr_t;
typedef ssize_t khronos_ssize_t;
typedef size_t khronos_usize_t;

#endif  // STARBOARD_DARWIN_INCLUDES_EAGL_KHRPLATFORM_PUBLIC_H_
