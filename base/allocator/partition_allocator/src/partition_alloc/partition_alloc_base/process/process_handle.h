// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_PROCESS_PROCESS_HANDLE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_PROCESS_PROCESS_HANDLE_H_

#include <stdint.h>
#include <sys/types.h>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/component_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/win/windows_types.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <zircon/types.h>
#endif

namespace partition_alloc::internal::base {

// ProcessHandle is a platform specific type which represents the underlying OS
// handle to a process.
// ProcessId is a number which identifies the process in the OS.
#if BUILDFLAG(IS_WIN)
typedef DWORD ProcessId;
const ProcessId kNullProcessId = 0;
#elif BUILDFLAG(IS_FUCHSIA)
typedef zx_koid_t ProcessId;
const ProcessId kNullProcessId = ZX_KOID_INVALID;
#elif BUILDFLAG(IS_POSIX)
// On POSIX, our ProcessHandle will just be the PID.
typedef pid_t ProcessId;
const ProcessId kNullProcessId = 0;
#endif  // BUILDFLAG(IS_WIN)

// Returns the id of the current process.
// Note that on some platforms, this is not guaranteed to be unique across
// processes (use GetUniqueIdForProcess if uniqueness is required).
PA_COMPONENT_EXPORT(PARTITION_ALLOC) ProcessId GetCurrentProcId();

}  // namespace partition_alloc::internal::base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_PROCESS_PROCESS_HANDLE_H_
