// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_JOB_H_
#define SANDBOX_WIN_SRC_JOB_H_

#include <stddef.h>

#include "base/win/scoped_handle.h"

namespace sandbox {
enum class JobLevel;

// Handles the creation of job objects based on a security profile.
// Sample usage:
//   Job job;
//   job.Init(JobLevel::kLockdown, 0, 0);
class Job {
 public:
  Job();

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  ~Job();

  // Initializes and creates the job object. The security of the job is based
  // on the security_level parameter.
  // If the chosen profile has too many ui restrictions, you can disable some
  // by specifying them in the ui_exceptions parameters.
  // If the function succeeds, the return value is ERROR_SUCCESS. If the
  // function fails, the return value is the win32 error code corresponding to
  // the error.
  DWORD Init(JobLevel security_level, DWORD ui_exceptions, size_t memory_limit);

  // True if the job has been initialized and has a valid handle.
  bool IsValid();

  // If the object is not yet initialized, returns nullptr.
  HANDLE GetHandle();

  // Updates the active process limit.
  DWORD SetActiveProcessLimit(DWORD processes);

 private:
  // Handle to the job referenced by the object.
  base::win::ScopedHandle job_handle_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_JOB_H_
