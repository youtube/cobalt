// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy_android.h"

#include <fcntl.h>
#include <linux/android/binder.h>
#include <linux/ashmem.h>
#include <linux/nbd.h>
#include <linux/userfaultfd.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"

namespace sandbox {
namespace {

BPF_TEST_C(BaselinePolicyAndroid, Getrusage, BaselinePolicyAndroid) {
  struct rusage usage{};

  errno = 0;
  BPF_ASSERT_EQ(0, getrusage(RUSAGE_SELF, &usage));

  errno = 0;
  BPF_ASSERT_EQ(0, getrusage(RUSAGE_THREAD, &usage));
}

BPF_TEST_C(BaselinePolicyAndroid, CanOpenProcCpuinfo, BaselinePolicyAndroid) {
  // This is required for |android_getCpuFeatures()|, which is used to enable
  // various fast paths in the renderer (for instance, in zlib).
  //
  // __NR_open is blocked in 64 bit mode, but as long as libc's open() redirects
  // open() to openat(), then this should work. Make sure this stays true.
  BPF_ASSERT_NE(-1, open("/proc/cpuinfo", O_RDONLY));
}

BPF_TEST_C(BaselinePolicyAndroid, Membarrier, BaselinePolicyAndroid) {
  // Should not crash.
  syscall(__NR_membarrier, 32 /* cmd */, 0 /* flags */);
}

BPF_TEST_C(BaselinePolicyAndroid,
           SchedGetAffinity_Blocked,
           BaselinePolicyAndroid) {
  cpu_set_t set{};
  errno = 0;
  BPF_ASSERT_EQ(-1, sched_getaffinity(0, sizeof(set), &set));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST_C(BaselinePolicyAndroid,
           SchedSetAffinity_Blocked,
           BaselinePolicyAndroid) {
  cpu_set_t set{};
  errno = 0;
  BPF_ASSERT_EQ(-1, sched_setaffinity(0, sizeof(set), &set));
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST_C(BaselinePolicyAndroid, Ioctl_Allowed, BaselinePolicyAndroid) {
  base::ScopedFD fd(HANDLE_EINTR(open("/dev/null", O_RDWR)));
  BPF_ASSERT(fd.is_valid());

  errno = 0;
  BPF_ASSERT_EQ(-1, ioctl(fd.get(), ASHMEM_SET_PROT_MASK));
  BPF_ASSERT_EQ(ENOTTY, errno);

  errno = 0;
  BPF_ASSERT_EQ(-1, ioctl(fd.get(), BINDER_WRITE_READ));
  BPF_ASSERT_EQ(ENOTTY, errno);
  // 32- and 64-bit constant values for BINDER_WRITE_READ.
  errno = 0;
  BPF_ASSERT_EQ(-1, ioctl(fd.get(), 0xc0186201));
  BPF_ASSERT_EQ(ENOTTY, errno);
  errno = 0;
  BPF_ASSERT_EQ(-1, ioctl(fd.get(), 0xc0306201));
  BPF_ASSERT_EQ(ENOTTY, errno);

  errno = 0;
  BPF_ASSERT_EQ(-1, ioctl(fd.get(), TCGETS));
  BPF_ASSERT_EQ(ENOTTY, errno);
}

BPF_TEST_C(BaselinePolicyAndroid, Ioctl_Blocked, BaselinePolicyAndroid) {
  base::ScopedFD fd(HANDLE_EINTR(open("/dev/null", O_RDWR)));
  BPF_ASSERT(fd.is_valid());

  errno = 0;
  BPF_ASSERT_EQ(-1, ioctl(fd.get(), NBD_CLEAR_SOCK));
  BPF_ASSERT_EQ(EINVAL, errno);
}

BPF_DEATH_TEST_C(BaselinePolicyAndroid,
                 UserfaultfdIoctl_BlockedDefault,
                 DEATH_SEGV_MESSAGE(GetIoctlErrorMessageContentForTests()),
                 BaselinePolicyAndroid) {
  base::ScopedFD fd(HANDLE_EINTR(open("/dev/null", O_RDWR)));
  BPF_ASSERT(fd.is_valid());
  ioctl(fd.get(), UFFDIO_WAKE);
}

class AllowUserfaultfdBaselinePolicyAndroid : public BaselinePolicyAndroid {
 public:
  AllowUserfaultfdBaselinePolicyAndroid()
      : BaselinePolicyAndroid(
            RuntimeOptions{.allow_userfaultfd_ioctls = true}) {}
};

BPF_TEST_C(BaselinePolicyAndroid,
           UserfaultfdIoctl_Allowed,
           AllowUserfaultfdBaselinePolicyAndroid) {
  base::ScopedFD fd(HANDLE_EINTR(open("/dev/null", O_RDWR)));
  BPF_ASSERT(fd.is_valid());

  errno = 0;
  BPF_ASSERT_EQ(-1, ioctl(fd.get(), UFFDIO_WAKE));
  BPF_ASSERT_EQ(ENOTTY, errno);
}

BPF_DEATH_TEST_C(BaselinePolicyAndroid,
                 UserfaultfdIoctl_BlockedSubset,
                 DEATH_SEGV_MESSAGE(GetIoctlErrorMessageContentForTests()),
                 BaselinePolicyAndroid) {
  base::ScopedFD fd(HANDLE_EINTR(open("/dev/null", O_RDWR)));
  BPF_ASSERT(fd.is_valid());
  ioctl(fd.get(), UFFDIO_API);
}

}  // namespace
}  // namespace sandbox
