// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_process_information.h"

#include "base/logging.h"
#include "base/win/scoped_handle.h"

namespace base {
namespace win {

namespace {

// Closes the provided handle if it is not NULL.
void CheckAndCloseHandle(HANDLE handle) {
  if (!handle)
    return;
  if (::CloseHandle(handle))
    return;
  CHECK(false);
}

// Duplicates source into target, returning true upon success. |target| is
// guaranteed to be untouched in case of failure. Succeeds with no side-effects
// if source is NULL.
bool CheckAndDuplicateHandle(HANDLE source, HANDLE* target) {
  if (!source)
    return true;

  HANDLE temp = NULL;
  if (!::DuplicateHandle(::GetCurrentProcess(), source,
                         ::GetCurrentProcess(), &temp, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
    DPLOG(ERROR) << "Failed to duplicate a handle.";
    return false;
  }
  *target = temp;
  return true;
}

}  // namespace

ScopedProcessInformation::ScopedProcessInformation()
    : process_information_() {
}

ScopedProcessInformation::~ScopedProcessInformation() {
  Close();
}

PROCESS_INFORMATION* ScopedProcessInformation::Receive() {
  DCHECK(!IsValid()) << "process_information_ must be NULL";
  return &process_information_;
}

bool ScopedProcessInformation::IsValid() const {
  return process_information_.hThread || process_information_.hProcess ||
    process_information_.dwProcessId || process_information_.dwThreadId;
}


void ScopedProcessInformation::Close() {
  CheckAndCloseHandle(process_information_.hThread);
  CheckAndCloseHandle(process_information_.hProcess);
  Reset();
}

void ScopedProcessInformation::Swap(ScopedProcessInformation* other) {
  DCHECK(other);
  PROCESS_INFORMATION temp = other->process_information_;
  other->process_information_ = process_information_;
  process_information_ = temp;
}

bool ScopedProcessInformation::DuplicateFrom(
    const ScopedProcessInformation& other) {
  DCHECK(!IsValid()) << "target ScopedProcessInformation must be NULL";
  DCHECK(other.IsValid()) << "source ScopedProcessInformation must be valid";

  ScopedHandle duplicate_process;
  ScopedHandle duplicate_thread;

  if (CheckAndDuplicateHandle(other.process_handle(),
                              duplicate_process.Receive()) &&
      CheckAndDuplicateHandle(other.thread_handle(),
                              duplicate_thread.Receive())) {
    process_information_.dwProcessId = other.process_id();
    process_information_.dwThreadId = other.thread_id();
    process_information_.hProcess = duplicate_process.Take();
    process_information_.hThread = duplicate_thread.Take();
    return true;
  }

  return false;
}

PROCESS_INFORMATION ScopedProcessInformation::Take() {
  PROCESS_INFORMATION process_information = process_information_;
  Reset();
  return process_information;
}

HANDLE ScopedProcessInformation::TakeProcessHandle() {
  HANDLE process = process_information_.hProcess;
  process_information_.hProcess = NULL;
  process_information_.dwProcessId = 0;
  return process;
}

HANDLE ScopedProcessInformation::TakeThreadHandle() {
  HANDLE thread = process_information_.hThread;
  process_information_.hThread = NULL;
  process_information_.dwThreadId = 0;
  return thread;
}

void ScopedProcessInformation::Reset() {
  process_information_.hThread = NULL;
  process_information_.hProcess = NULL;
  process_information_.dwProcessId = 0;
  process_information_.dwThreadId = 0;
}

}  // namespace win
}  // namespace base
