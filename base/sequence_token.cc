// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequence_token.h"

#include "base/atomic_sequence_num.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

#if defined(STARBOARD)
#include "base/check_op.h"
#include "starboard/once.h"
#include "starboard/thread.h"
#include "base/logging.h"
#endif

namespace base {

namespace {

base::AtomicSequenceNumber g_sequence_token_generator;

base::AtomicSequenceNumber g_task_token_generator;

#if defined(STARBOARD)
ABSL_CONST_INIT SbOnceControl s_once_sequence_flag = SB_ONCE_INITIALIZER;
ABSL_CONST_INIT SbThreadLocalKey s_thread_local_sequence_key =
    kSbThreadLocalKeyInvalid;

void InitThreadLocalSequenceKey() {
  s_thread_local_sequence_key = SbThreadCreateLocalKey(NULL);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_sequence_key));
}

void EnsureThreadLocalSequenceKeyInited() {
  SbOnce(&s_once_sequence_flag, InitThreadLocalSequenceKey);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_sequence_key));
}

ABSL_CONST_INIT SbOnceControl s_once_set_sequence_flag = SB_ONCE_INITIALIZER;
ABSL_CONST_INIT SbThreadLocalKey s_thread_local_sequence_set_for_thread =
    kSbThreadLocalKeyInvalid;
void InitThreadLocalSequenceBoolKey() {
  s_thread_local_sequence_set_for_thread = SbThreadCreateLocalKey(NULL);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_sequence_set_for_thread));
}

void EnsureThreadLocalSequenceBoolKeyInited() {
  SbOnce(&s_once_set_sequence_flag, InitThreadLocalSequenceBoolKey);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_sequence_set_for_thread));
}

bool IsSequenceSetForThread() {
  void* set_for_thread =
      SbThreadGetLocalValue(s_thread_local_sequence_set_for_thread);
  return !!set_for_thread ? reinterpret_cast<intptr_t>(set_for_thread) != 0
                          : false;
}

ABSL_CONST_INIT SbOnceControl s_once_task_flag = SB_ONCE_INITIALIZER;
ABSL_CONST_INIT SbThreadLocalKey s_thread_local_task_key =
    kSbThreadLocalKeyInvalid;

void InitThreadLocalTaskKey() {
  s_thread_local_task_key = SbThreadCreateLocalKey(NULL);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_task_key));
}

void EnsureThreadLocalTaskKeyInited() {
  SbOnce(&s_once_task_flag, InitThreadLocalTaskKey);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_task_key));
}

ABSL_CONST_INIT SbOnceControl s_once_set_task_flag = SB_ONCE_INITIALIZER;
ABSL_CONST_INIT SbThreadLocalKey s_thread_local_task_set_for_thread =
    kSbThreadLocalKeyInvalid;
void InitThreadLocalTaskBoolKey() {
  s_thread_local_task_set_for_thread = SbThreadCreateLocalKey(NULL);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_task_set_for_thread));
}

void EnsureThreadLocalTaskBoolKeyInited() {
  SbOnce(&s_once_set_task_flag, InitThreadLocalTaskBoolKey);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_task_set_for_thread));
}

bool IsTaskSetForThread() {
  void* set_for_thread =
      SbThreadGetLocalValue(s_thread_local_task_set_for_thread);
  return !!set_for_thread ? reinterpret_cast<intptr_t>(set_for_thread) != 0
                          : false;
}
#else
ABSL_CONST_INIT thread_local SequenceToken current_sequence_token;
ABSL_CONST_INIT thread_local TaskToken current_task_token;
#endif

}  // namespace

bool SequenceToken::operator==(const SequenceToken& other) const {
  return token_ == other.token_ && IsValid();
}

bool SequenceToken::operator!=(const SequenceToken& other) const {
  return !(*this == other);
}

bool SequenceToken::IsValid() const {
  return token_ != kInvalidSequenceToken;
}

int SequenceToken::ToInternalValue() const {
  return token_;
}

SequenceToken SequenceToken::Create() {
  return SequenceToken(g_sequence_token_generator.GetNext());
}

SequenceToken SequenceToken::GetForCurrentThread() {
#if defined(STARBOARD)
  if (IsSequenceSetForThread()) {
    int token = static_cast<int>(reinterpret_cast<intptr_t>(
        SbThreadGetLocalValue(s_thread_local_sequence_key)));
    return SequenceToken(token);
  } else {
    return SequenceToken();
  }
#else
  return current_sequence_token;
#endif
}

bool TaskToken::operator==(const TaskToken& other) const {
  return token_ == other.token_ && IsValid();
}

bool TaskToken::operator!=(const TaskToken& other) const {
  return !(*this == other);
}

bool TaskToken::IsValid() const {
  return token_ != kInvalidTaskToken;
}

TaskToken TaskToken::Create() {
  return TaskToken(g_task_token_generator.GetNext());
}

TaskToken TaskToken::GetForCurrentThread() {
#if defined(STARBOARD)
  if (IsTaskSetForThread()) {
    int token = static_cast<int>(reinterpret_cast<intptr_t>(
        SbThreadGetLocalValue(s_thread_local_task_key)));
    return TaskToken(token);
  } else {
    return TaskToken();
  }
#else
  return current_task_token;
#endif
}

ScopedSetSequenceTokenForCurrentThread::ScopedSetSequenceTokenForCurrentThread(
    const SequenceToken& sequence_token)
#if defined(STARBOARD)
{
  EnsureThreadLocalSequenceKeyInited();
  auto sequence_reset_token = TaskToken::GetForCurrentThread();
  DCHECK(!sequence_reset_token.IsValid());
  scoped_sequence_reset_value_ = reinterpret_cast<void*>(
                            static_cast<intptr_t>(sequence_reset_token.GetToken()));
  SbThreadSetLocalValue(s_thread_local_sequence_key,
                        reinterpret_cast<void*>(
                            static_cast<intptr_t>(sequence_token.GetToken())));

  EnsureThreadLocalTaskKeyInited();
  auto task_reset_token = TaskToken::GetForCurrentThread();
  DCHECK(!task_reset_token.IsValid());
  scoped_task_reset_value_ = reinterpret_cast<void*>(
                            static_cast<intptr_t>(task_reset_token.GetToken()));
  SbThreadSetLocalValue(s_thread_local_task_key,
                        reinterpret_cast<void*>(static_cast<intptr_t>(
                            TaskToken::Create().GetToken())));

  EnsureThreadLocalSequenceBoolKeyInited();
  SbThreadSetLocalValue(s_thread_local_sequence_set_for_thread,
                        reinterpret_cast<void*>(static_cast<intptr_t>(true)));
  EnsureThreadLocalTaskBoolKeyInited();
  SbThreadSetLocalValue(s_thread_local_task_set_for_thread,
                        reinterpret_cast<void*>(static_cast<intptr_t>(true)));
}
#else
    // The lambdas here exist because invalid tokens don't compare equal, so
    // passing invalid sequence/task tokens as the third args to AutoReset
    // constructors doesn't work.
    : sequence_token_resetter_(&current_sequence_token,
                               [&sequence_token]() {
                                 DCHECK(!current_sequence_token.IsValid());
                                 return sequence_token;
                               }()),
      task_token_resetter_(&current_task_token, [] {
        DCHECK(!current_task_token.IsValid());
        return TaskToken::Create();
      }()) {
}
#endif

#if defined(STARBOARD)
ScopedSetSequenceTokenForCurrentThread::
    ~ScopedSetSequenceTokenForCurrentThread() {
  SbThreadSetLocalValue(s_thread_local_sequence_key,
                        scoped_sequence_reset_value_);
  SbThreadSetLocalValue(s_thread_local_task_key, scoped_task_reset_value_);
}
#else
ScopedSetSequenceTokenForCurrentThread::
    ~ScopedSetSequenceTokenForCurrentThread() = default;
#endif

}  // namespace base
