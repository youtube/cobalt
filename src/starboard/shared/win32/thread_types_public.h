// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_THREAD_TYPES_PUBLIC_H_
#define STARBOARD_SHARED_WIN32_THREAD_TYPES_PUBLIC_H_

// --- SbConditionVariable ---

// Transparent Condition Variable handle.
// Note: replica of RTL_CONDITION_VARIABLE in um/winnt.h to avoid include.
typedef struct WindowsConditionVariable { void* ptr; } SbConditionVariable;

// Condition Variable static initializer.
// Note: replica of RTL_CONDITION_VARIABILE_INIT in um/winnt.h to avoid include.
#define SB_CONDITION_VARIABLE_INITIALIZER \
  { 0 }

// --- SbMutex ---

// Transparent Mutex handle.
// Note SRWLock's are used for SbMutex because:
// - Normal Windows CreateMutex() mutexes are very heavy-weight
// - Critical sections do not have a static initializer
// Note: replica of RTL_SRWLOCK in um/winnt.h to avoid include.
typedef struct WindowsSrwLock { void* ptr; } SbMutex;

// Mutex static initializer.
// Note: replica of RTL_SRWLOCK_INIT in um/winnt.h to avoid include.
#define SB_MUTEX_INITIALIZER \
  { 0 }

// --- SbOnce ---

// Transparent Once control handle.
typedef union WindowsRunOnce { void* ptr; } SbOnceControl;

// Once static initializer.
// Note: replica of RTL_RUN_ONCE_INIT in um/winnt.h to avoid include.
#define SB_ONCE_INITIALIZER \
  { 0 }

// --- SbThread ---

// Transparent pthread handle.
typedef void* SbThread;

// Well-defined constant value to mean "no thread handle."
#define kSbThreadInvalid (SbThread) NULL

#endif  // STARBOARD_SHARED_WIN32_THREAD_TYPES_PUBLIC_H_
