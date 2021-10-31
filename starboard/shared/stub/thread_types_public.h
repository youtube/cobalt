// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// Includes threading primitive types and initializers.

#ifndef STARBOARD_SHARED_STUB_THREAD_TYPES_PUBLIC_H_
#define STARBOARD_SHARED_STUB_THREAD_TYPES_PUBLIC_H_

// --- SbConditionVariable ---

// Transparent Condition Variable handle.
typedef void* SbConditionVariable;

// Condition Variable static initializer.
#define SB_CONDITION_VARIABLE_INITIALIZER NULL

// --- SbMutex ---

// Transparent Mutex handle.
typedef void* SbMutex;

// Mutex static initializer.
#define SB_MUTEX_INITIALIZER NULL

// --- SbOnce ---

// Transparent Once control handle.
typedef void* SbOnceControl;

// Once static initializer.
#define SB_ONCE_INITIALIZER NULL

// --- SbThread ---

// Transparent pthread handle.
typedef void* SbThread;

// Well-defined constant value to mean "no thread handle."
#define kSbThreadInvalid (SbThread)-1

#endif  // STARBOARD_SHARED_STUB_THREAD_TYPES_PUBLIC_H_
