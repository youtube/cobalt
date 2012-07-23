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

void StackDumpSignalHandler(int signal) {
  LOG(ERROR) << "Received signal " << signal;
  NSArray *stack_symbols = [NSThread callStackSymbols];
  for (NSString* stack_symbol in stack_symbols) {
    fprintf(stderr, "\t%s\n", [stack_symbol UTF8String]);
  }
  _exit(1);
}

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

bool EnableInProcessStackDumping() {
  // When running in an application, our code typically expects SIGPIPE
  // to be ignored.  Therefore, when testing that same code, it should run
  // with SIGPIPE ignored as well.
  struct sigaction action;
  action.sa_handler = SIG_IGN;
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  bool success = (sigaction(SIGPIPE, &action, NULL) == 0);

  success &= (signal(SIGILL, &StackDumpSignalHandler) != SIG_ERR);
  success &= (signal(SIGABRT, &StackDumpSignalHandler) != SIG_ERR);
  success &= (signal(SIGFPE, &StackDumpSignalHandler) != SIG_ERR);
  success &= (signal(SIGBUS, &StackDumpSignalHandler) != SIG_ERR);
  success &= (signal(SIGSEGV, &StackDumpSignalHandler) != SIG_ERR);
  success &= (signal(SIGSYS, &StackDumpSignalHandler) != SIG_ERR);

  return success;
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
