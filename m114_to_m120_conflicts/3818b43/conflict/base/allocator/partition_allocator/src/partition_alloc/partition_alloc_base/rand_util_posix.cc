// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/rand_util.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sstream>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/check.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/files/file_util.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/no_destructor.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/posix/eintr_wrapper.h"
#include "build/build_config.h"

<<<<<<< HEAD:base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/rand_util_posix.cc
#if BUILDFLAG(IS_MAC)
=======
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
#include "third_party/lss/linux_syscall_support.h"
#elif BUILDFLAG(IS_MAC)
>>>>>>> 2a45082b24a (Add BUILDFLAG IS_COBALT_HERMETIC_BUILD (#4966)):base/allocator/partition_allocator/partition_alloc_base/rand_util_posix.cc
// TODO(crbug.com/995996): Waiting for this header to appear in the iOS SDK.
// (See below.)
#include <sys/random.h>
#endif

namespace {

#if BUILDFLAG(IS_AIX)
// AIX has no 64-bit support for O_CLOEXEC.
static constexpr int kOpenFlags = O_RDONLY;
#else
static constexpr int kOpenFlags = O_RDONLY | O_CLOEXEC;
#endif

// We keep the file descriptor for /dev/urandom around so we don't need to
// reopen it (which is expensive), and since we may not even be able to reopen
// it if we are later put in a sandbox. This class wraps the file descriptor so
// we can use a static-local variable to handle opening it on the first access.
class URandomFd {
 public:
  URandomFd() : fd_(PA_HANDLE_EINTR(open("/dev/urandom", kOpenFlags))) {
    PA_BASE_CHECK(fd_ >= 0) << "Cannot open /dev/urandom";
  }

  ~URandomFd() { close(fd_); }

  int fd() const { return fd_; }

 private:
  const int fd_;
};

int GetUrandomFD() {
  static partition_alloc::internal::base::NoDestructor<URandomFd> urandom_fd;
  return urandom_fd->fd();
}

}  // namespace

namespace partition_alloc::internal::base {

// NOTE: In an ideal future, all implementations of this function will just
// wrap BoringSSL's `RAND_bytes`. TODO(crbug.com/995996): Figure out the
// build/test/performance issues with dcheng's CL
// (https://chromium-review.googlesource.com/c/chromium/src/+/1545096) and land
// it or some form of it.
void RandBytes(void* output, size_t output_length) {
<<<<<<< HEAD:base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/rand_util_posix.cc
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && !defined(IS_COBALT_HERMETIC_BUILD)
  // Use `syscall(__NR_getrandom...` to avoid a dependency on
  // `third_party/linux_syscall_support.h`.
  //
  // Here in PartitionAlloc, we don't need to look before we leap
  // because we know that both Linux and CrOS only support kernels
  // that do have this syscall defined. This diverges from upstream
  // `//base` behavior both here and below.
  const ssize_t r =
      PA_HANDLE_EINTR(syscall(__NR_getrandom, output, output_length, 0));
=======
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  // We have to call `getrandom` via Linux Syscall Support, rather than through
  // the libc wrapper, because we might not have an up-to-date libc (e.g. on
  // some bots).
  const ssize_t r = PA_HANDLE_EINTR(sys_getrandom(output, output_length, 0));
>>>>>>> 2a45082b24a (Add BUILDFLAG IS_COBALT_HERMETIC_BUILD (#4966)):base/allocator/partition_allocator/partition_alloc_base/rand_util_posix.cc

  // Return success only on total success. In case errno == ENOSYS (or any other
  // error), we'll fall through to reading from urandom below.
  if (output_length == static_cast<size_t>(r)) {
    PA_MSAN_UNPOISON(output, output_length);
    return;
  }
#elif BUILDFLAG(IS_MAC)
  // TODO(crbug.com/995996): Enable this on iOS too, when sys/random.h arrives
  // in its SDK.
  if (getentropy(output, output_length) == 0) {
    return;
  }
#endif
  // If getrandom(2) above returned with an error and the /dev/urandom fallback
  // took place on Linux/ChromeOS bots, they would fail with a CHECK in
  // nacl_helper. The latter assumes that the number of open file descriptors
  // must be constant. The nacl_helper knows about the FD from
  // //base/rand_utils, but is not aware of the urandom_fd from this file (see
  // CheckForExpectedNumberOfOpenFds).
  //
  // *  On `linux_chromium_asan_rel_ng` in
  //    `ContentBrowserTest.RendererCrashCallStack`:
  //    ```
  //    [FATAL:rand_util_posix.cc(45)] Check failed: fd_ >= 0. Cannot open
  //    /dev/urandom
  //    ```
  // *  On `linux-lacros-rel` in
  //    `NaClBrowserTestGLibc.CrashInCallback`:
  //    ```
  //    2023-07-03T11:31:13.115755Z FATAL nacl_helper:
  //    [nacl_sandbox_linux.cc(178)] Check failed: expected_num_fds ==
  //    sandbox::ProcUtil::CountOpenFds(proc_fd_.get()) (6 vs. 7)
  //    ```
  const int urandom_fd = GetUrandomFD();
  const bool success =
      ReadFromFD(urandom_fd, static_cast<char*>(output), output_length);
  PA_BASE_CHECK(success);
}

}  // namespace partition_alloc::internal::base
