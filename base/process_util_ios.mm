// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#import <Foundation/Foundation.h>
#include <mach/task.h>
#include <stdio.h>

#include "base/logging.h"

// This is just enough of a shim to let the support needed by test_support
// link.  In general, process_util isn't valid on iOS.

namespace base {

namespace {

bool GetTaskInfo(task_basic_info_64* task_info_data) {
  mach_msg_type_number_t count = TASK_BASIC_INFO_64_COUNT;
  kern_return_t kr = task_info(mach_task_self(),
                               TASK_BASIC_INFO_64,
                               reinterpret_cast<task_info_t>(task_info_data),
                               &count);
  return kr == KERN_SUCCESS;
}

}  // namespace

ProcessId GetCurrentProcId() {
  return getpid();
}

ProcessHandle GetCurrentProcessHandle() {
  return GetCurrentProcId();
}

void EnableTerminationOnHeapCorruption() {
  // On iOS, there nothing to do AFAIK.
}

void EnableTerminationOnOutOfMemory() {
  // iOS provides this for free!
}

void RaiseProcessToHighPriority() {
  // Impossible on iOS. Do nothing.
}

ProcessMetrics::ProcessMetrics(ProcessHandle process) {}

ProcessMetrics::~ProcessMetrics() {}

// static
ProcessMetrics* ProcessMetrics::CreateProcessMetrics(ProcessHandle process) {
  return new ProcessMetrics(process);
}

size_t ProcessMetrics::GetWorkingSetSize() const {
  task_basic_info_64 task_info_data;
  if (!GetTaskInfo(&task_info_data))
    return 0;
  return task_info_data.resident_size;
}

}  // namespace base
