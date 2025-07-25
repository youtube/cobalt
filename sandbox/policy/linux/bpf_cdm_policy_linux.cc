// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/linux/bpf_cdm_policy_linux.h"

#include <errno.h>

#include "build/build_config.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_parameters_restrictions.h"
#include "sandbox/linux/seccomp-bpf-helpers/syscall_sets.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/policy/linux/sandbox_linux.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::ResultExpr;

namespace sandbox {
namespace policy {

CdmProcessPolicy::CdmProcessPolicy() {}
CdmProcessPolicy::~CdmProcessPolicy() {}

ResultExpr CdmProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
    case __NR_ioctl:
      return RestrictIoctl();
    // Allow the system calls below.
    case __NR_fdatasync:
    case __NR_fsync:
    case __NR_ftruncate:
    case __NR_fallocate:
#if defined(__i386__) || defined(__x86_64__) || defined(__mips__) || \
    defined(__aarch64__)
    case __NR_getrlimit:
#endif
#if defined(__i386__) || defined(__arm__)
    case __NR_ugetrlimit:
#endif
    case __NR_mremap:  // https://crbug.com/546204
    case __NR_pwrite64:
    case __NR_sysinfo:
    case __NR_times:
    case __NR_uname:
      return Allow();
    case __NR_prlimit64:
      return RestrictPrlimitToGetrlimit(GetPolicyPid());
    default:
      // Default on the content baseline policy.
      return BPFBasePolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace policy
}  // namespace sandbox
