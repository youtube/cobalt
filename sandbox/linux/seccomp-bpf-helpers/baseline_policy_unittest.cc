// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/synchronization/lock_impl.h"
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sched.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <tuple>

#include "base/allocator/partition_alloc_features.h"
#include "base/clang_profiling_buildflags.h"
#include "base/features.h"
#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy.h"
#include "sandbox/linux/seccomp-bpf-helpers/sigsys_handlers.h"
#include "sandbox/linux/seccomp-bpf/bpf_tests.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/services/thread_helpers.h"
#include "sandbox/linux/system_headers/linux_futex.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/linux/system_headers/linux_time.h"
#include "sandbox/linux/tests/test_utils.h"
#include "sandbox/linux/tests/unit_tests.h"

#if !defined(SO_PEEK_OFF)
#define SO_PEEK_OFF 42
#endif

namespace sandbox {

namespace {

// This also tests that read(), write(), fstat(), and fstatat(.., "", ..,
// AT_EMPTY_PATH) are allowed.
void TestPipeOrSocketPair(base::ScopedFD read_end, base::ScopedFD write_end) {
  BPF_ASSERT_LE(0, read_end.get());
  BPF_ASSERT_LE(0, write_end.get());
  struct stat stat_buf;
  int sys_ret = fstat(read_end.get(), &stat_buf);
  BPF_ASSERT_EQ(0, sys_ret);
  BPF_ASSERT(S_ISFIFO(stat_buf.st_mode) || S_ISSOCK(stat_buf.st_mode));

  sys_ret = fstatat(read_end.get(), "", &stat_buf, AT_EMPTY_PATH);
  BPF_ASSERT_EQ(0, sys_ret);
  BPF_ASSERT(S_ISFIFO(stat_buf.st_mode) || S_ISSOCK(stat_buf.st_mode));

  // Make sure fstatat with anything other than an empty string is denied.
  sys_ret = fstatat(read_end.get(), "/", &stat_buf, AT_EMPTY_PATH);
  BPF_ASSERT_EQ(sys_ret, -1);
  BPF_ASSERT_EQ(EPERM, errno);

  // Make sure fstatat without AT_EMPTY_PATH is denied.
  sys_ret = fstatat(read_end.get(), "", &stat_buf, 0);
  BPF_ASSERT_EQ(sys_ret, -1);
  BPF_ASSERT_EQ(EPERM, errno);

  const ssize_t kTestTransferSize = 4;
  static const char kTestString[kTestTransferSize] = {'T', 'E', 'S', 'T'};
  ssize_t transfered = 0;

  transfered =
      HANDLE_EINTR(write(write_end.get(), kTestString, kTestTransferSize));
  BPF_ASSERT_EQ(kTestTransferSize, transfered);
  char read_buf[kTestTransferSize + 1] = {};
  transfered = HANDLE_EINTR(read(read_end.get(), read_buf, sizeof(read_buf)));
  BPF_ASSERT_EQ(kTestTransferSize, transfered);
  BPF_ASSERT_EQ(0, memcmp(kTestString, read_buf, kTestTransferSize));
}

// Test that a few easy-to-test system calls are allowed.
BPF_TEST_C(BaselinePolicy, BaselinePolicyBasicAllowed, BaselinePolicy) {
  BPF_ASSERT_EQ(0, sched_yield());

  int pipefd[2];
  int sys_ret = pipe(pipefd);
  BPF_ASSERT_EQ(0, sys_ret);
  TestPipeOrSocketPair(base::ScopedFD(pipefd[0]), base::ScopedFD(pipefd[1]));

  BPF_ASSERT_LE(1, getpid());
  BPF_ASSERT_LE(0, getuid());
}

BPF_TEST_C(BaselinePolicy, FchmodErrno, BaselinePolicy) {
  int ret = fchmod(-1, 07777);
  BPF_ASSERT_EQ(-1, ret);
  // Without the sandbox, this would EBADF instead.
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST_C(BaselinePolicy, ForkErrno, BaselinePolicy) {
  errno = 0;
  pid_t pid = fork();
  const int fork_errno = errno;
  TestUtils::HandlePostForkReturn(pid);

  BPF_ASSERT_EQ(-1, pid);
  BPF_ASSERT_EQ(EPERM, fork_errno);
}

pid_t ForkX86Glibc() {
  static pid_t ptid;
  return sys_clone(CLONE_PARENT_SETTID | SIGCHLD, nullptr, &ptid, nullptr,
                   nullptr);
}

BPF_TEST_C(BaselinePolicy, ForkX86Eperm, BaselinePolicy) {
  errno = 0;
  pid_t pid = ForkX86Glibc();
  const int fork_errno = errno;
  TestUtils::HandlePostForkReturn(pid);

  BPF_ASSERT_EQ(-1, pid);
  BPF_ASSERT_EQ(EPERM, fork_errno);
}

pid_t ForkARMGlibc() {
  static pid_t ctid;
  return sys_clone(CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | SIGCHLD, nullptr,
                   nullptr, &ctid, nullptr);
}

BPF_TEST_C(BaselinePolicy, ForkArmEperm, BaselinePolicy) {
  errno = 0;
  pid_t pid = ForkARMGlibc();
  const int fork_errno = errno;
  TestUtils::HandlePostForkReturn(pid);

  BPF_ASSERT_EQ(-1, pid);
  BPF_ASSERT_EQ(EPERM, fork_errno);
}

// system() calls into vfork() on old Android builds and returns when vfork is
// blocked. This causes undefined behavior on x86 Android builds on versions
// prior to Q, which causes the stack to get corrupted, so this test cannot be
// made to pass.
#if BUILDFLAG(IS_ANDROID) && defined(__i386__)
#define MAYBE_SystemEperm DISABLED_SystemEperm
#else
#define MAYBE_SystemEperm SystemEperm
#endif
BPF_TEST_C(BaselinePolicy, MAYBE_SystemEperm, BaselinePolicy) {
  errno = 0;
  int ret_val = system("echo SHOULD NEVER RUN");
  // glibc >= 2.33 changed the ret code: 127 is now expected on bits 15-8
  // previously it was simply -1, so check for not zero
  BPF_ASSERT_NE(0, ret_val);
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST_C(BaselinePolicy, CloneVforkEperm, BaselinePolicy) {
  errno = 0;
  // Allocate a couple pages for the child's stack even though the child should
  // never start.
  constexpr size_t kStackSize = 4096 * 4;
  void* child_stack = mmap(nullptr, kStackSize, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
  BPF_ASSERT_NE(child_stack, nullptr);
  pid_t pid = syscall(__NR_clone, CLONE_VM | CLONE_VFORK | SIGCHLD,
                      static_cast<char*>(child_stack) + kStackSize, nullptr,
                      nullptr, nullptr);
  const int clone_errno = errno;
  TestUtils::HandlePostForkReturn(pid);

  munmap(child_stack, kStackSize);

  BPF_ASSERT_EQ(-1, pid);
  BPF_ASSERT_EQ(EPERM, clone_errno);
}

BPF_TEST_C(BaselinePolicy, CreateThread, BaselinePolicy) {
  base::Thread thread("sandbox_tests");
  BPF_ASSERT(thread.Start());
}

// Rseq should be enabled if it exists (i.e. shouldn't receive EPERM).
#if !BUILDFLAG(IS_ANDROID)
BPF_TEST_C(BaselinePolicy, RseqEnabled, BaselinePolicy) {
  errno = 0;
  int res = syscall(__NR_rseq, 0, 0, 0, 0);

  BPF_ASSERT_EQ(-1, res);
  // The parameters above are invalid so the system call should fail with
  // EINVAL, or ENOSYS if the kernel is too old to recognize the system call.
  BPF_ASSERT(EINVAL == errno || ENOSYS == errno);
}
#endif  // !BUILDFLAG(IS_ANDROID)

BPF_DEATH_TEST_C(BaselinePolicy,
                 DisallowedCloneFlagCrashes,
                 DEATH_SEGV_MESSAGE(GetCloneErrorMessageContentForTests()),
                 BaselinePolicy) {
  pid_t pid = sys_clone(CLONE_THREAD | SIGCHLD);
  TestUtils::HandlePostForkReturn(pid);
}

BPF_DEATH_TEST_C(BaselinePolicy,
                 DisallowedKillCrashes,
                 DEATH_SEGV_MESSAGE(GetKillErrorMessageContentForTests()),
                 BaselinePolicy) {
  BPF_ASSERT_NE(1, getpid());
  kill(1, 0);
  _exit(0);
}

BPF_TEST_C(BaselinePolicy, CanKillSelf, BaselinePolicy) {
  int sys_ret = kill(getpid(), 0);
  BPF_ASSERT_EQ(0, sys_ret);
}

BPF_TEST_C(BaselinePolicy, Socketpair, BaselinePolicy) {
  int sv[2];
  int sys_ret = socketpair(AF_UNIX, SOCK_DGRAM, 0, sv);
  BPF_ASSERT_EQ(0, sys_ret);
  TestPipeOrSocketPair(base::ScopedFD(sv[0]), base::ScopedFD(sv[1]));

  sys_ret = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv);
  BPF_ASSERT_EQ(0, sys_ret);
  TestPipeOrSocketPair(base::ScopedFD(sv[0]), base::ScopedFD(sv[1]));
}

#if !defined(GRND_NONBLOCK)
#define GRND_NONBLOCK 1
#endif

#if !defined(GRND_INSECURE)
#define GRND_INSECURE 4
#endif

BPF_TEST_C(BaselinePolicy, GetRandom, BaselinePolicy) {
  char buf[1];

  // Many systems do not yet support getrandom(2) so ENOSYS is a valid result
  // here.
  errno = 0;
  int ret = HANDLE_EINTR(syscall(__NR_getrandom, buf, sizeof(buf), 0));
  BPF_ASSERT((ret == -1 && errno == ENOSYS) || ret == 1);
  errno = 0;
  ret = HANDLE_EINTR(syscall(__NR_getrandom, buf, sizeof(buf), GRND_NONBLOCK));
  BPF_ASSERT((ret == -1 && (errno == ENOSYS || errno == EAGAIN)) || ret == 1);

  // GRND_INSECURE is not supported on versions of Android < 12, and Android
  // returns EINVAL in that case.
  errno = 0;
  ret = HANDLE_EINTR(syscall(__NR_getrandom, buf, sizeof(buf), GRND_INSECURE));
  BPF_ASSERT(
      (ret == -1 && (errno == ENOSYS || errno == EAGAIN || errno == EINVAL)) ||
      ret == 1);
}

// Not all architectures can restrict the domain for socketpair().
#if defined(__x86_64__) || defined(__arm__) || defined(__aarch64__)
BPF_DEATH_TEST_C(BaselinePolicy,
                 SocketpairWrongDomain,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 BaselinePolicy) {
  int sv[2];
  std::ignore = socketpair(AF_INET, SOCK_STREAM, 0, sv);
  _exit(1);
}
#endif  // defined(__x86_64__) || defined(__arm__) || defined(__aarch64__)

BPF_TEST_C(BaselinePolicy, EPERM_open, BaselinePolicy) {
  errno = 0;
  int sys_ret = open("/proc/cpuinfo", O_RDONLY);
  BPF_ASSERT_EQ(-1, sys_ret);
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST_C(BaselinePolicy, EPERM_access, BaselinePolicy) {
  errno = 0;
  int sys_ret = access("/proc/cpuinfo", R_OK);
  BPF_ASSERT_EQ(-1, sys_ret);
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_TEST_C(BaselinePolicy, EPERM_getcwd, BaselinePolicy) {
  errno = 0;
  char buf[1024];
  char* cwd = getcwd(buf, sizeof(buf));
  BPF_ASSERT_EQ(nullptr, cwd);
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_DEATH_TEST_C(BaselinePolicy,
                 SIGSYS_InvalidSyscall,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 BaselinePolicy) {
  Syscall::InvalidCall();
}

// A failing test using this macro could be problematic since we perform
// system calls by passing "0" as every argument.
// The kernel could SIGSEGV the process or the system call itself could reboot
// the machine. Some thoughts have been given when hand-picking the system
// calls below to limit any potential side effects outside of the current
// process.
#define TEST_BASELINE_SIGSYS(sysno)                                      \
  BPF_DEATH_TEST_C(BaselinePolicy,                                       \
                   SIGSYS_##sysno,                                       \
                   DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()), \
                   BaselinePolicy) {                                     \
    syscall(sysno, 0, 0, 0, 0, 0, 0);                                    \
    _exit(1);                                                            \
  }

TEST_BASELINE_SIGSYS(__NR_acct)
TEST_BASELINE_SIGSYS(__NR_chroot)
TEST_BASELINE_SIGSYS(__NR_fanotify_init)
TEST_BASELINE_SIGSYS(__NR_fgetxattr)
TEST_BASELINE_SIGSYS(__NR_getcpu)
TEST_BASELINE_SIGSYS(__NR_getitimer)
TEST_BASELINE_SIGSYS(__NR_init_module)
TEST_BASELINE_SIGSYS(__NR_io_cancel)
TEST_BASELINE_SIGSYS(__NR_keyctl)
TEST_BASELINE_SIGSYS(__NR_mq_open)
TEST_BASELINE_SIGSYS(__NR_ptrace)
TEST_BASELINE_SIGSYS(__NR_sched_setaffinity)
TEST_BASELINE_SIGSYS(__NR_setpgid)
TEST_BASELINE_SIGSYS(__NR_swapon)
TEST_BASELINE_SIGSYS(__NR_sysinfo)
TEST_BASELINE_SIGSYS(__NR_syslog)
TEST_BASELINE_SIGSYS(__NR_timer_create)

#if !defined(__aarch64__)
TEST_BASELINE_SIGSYS(__NR_inotify_init)
TEST_BASELINE_SIGSYS(__NR_vserver)
#endif

#if defined(LIBC_GLIBC) && !BUILDFLAG(IS_CHROMEOS)
BPF_TEST_C(BaselinePolicy, FutexEINVAL, BaselinePolicy) {
  int ops[] = {
      FUTEX_CMP_REQUEUE_PI, FUTEX_CMP_REQUEUE_PI_PRIVATE,
      FUTEX_UNLOCK_PI_PRIVATE,
  };

  for (int op : ops) {
    BPF_ASSERT_EQ(-1, syscall(__NR_futex, nullptr, op, 0, nullptr, nullptr, 0));
    BPF_ASSERT_EQ(EINVAL, errno);
  }
}
#else

#define TEST_BASELINE_PI_FUTEX(op)                            \
  _TEST_BASELINE_PI_FUTEX(BaselinePolicy, Futex_##op) {       \
    syscall(__NR_futex, nullptr, op, 0, nullptr, nullptr, 0); \
    _exit(1);                                                 \
  }

#if BUILDFLAG(ENABLE_MUTEX_PRIORITY_INHERITANCE)
// PI futexes are only allowed by the sandbox on kernels >= 6.1 and if the
// kUsePriorityInheritanceMutex is enabled. In order to test this,
// |_TEST_BASELINE_PI_FUTEX| generates a test which has two parts:
//  - The first part of the test enables the feature and performs the futex
//    syscall in a child process with the provided futex operation. Then it
//    asserts that the syscall succeed only if the kernel version is at
//    least 6.1.
//  - The second part of the test disables the feature and performs the futex
//    syscall in a child process with the provided futex operation. Then it
//    asserts that the syscall always crashes the process.
#define _TEST_BASELINE_PI_FUTEX(test_case_name, test_name)                  \
  void BPF_TEST_PI_FUTEX_##test_name();                                     \
  TEST(test_case_name, DISABLE_ON_TSAN(test_name)) {                        \
    __TEST_BASELINE_PI_FUTEX(BPF_TEST_PI_FUTEX_##test_name,                 \
                             base::features::kUsePriorityInheritanceMutex,  \
                             true);                                         \
    __TEST_BASELINE_PI_FUTEX(BPF_TEST_PI_FUTEX_##test_name,                 \
                             base::features::kUsePriorityInheritanceMutex,  \
                             false);                                        \
    __TEST_BASELINE_PI_FUTEX(                                               \
        BPF_TEST_PI_FUTEX_##test_name,                                      \
        base::features::kPartitionAllocUsePriorityInheritanceLocks, true);  \
    __TEST_BASELINE_PI_FUTEX(                                               \
        BPF_TEST_PI_FUTEX_##test_name,                                      \
        base::features::kPartitionAllocUsePriorityInheritanceLocks, false); \
  }                                                                         \
  void BPF_TEST_PI_FUTEX_##test_name()

#define __TEST_BASELINE_PI_FUTEX(test_name, feature, enable)              \
  {                                                                       \
    base::test::ScopedFeatureList feature_;                               \
    feature_.InitWithFeatureState(feature, enable);                       \
    sandbox::SandboxBPFTestRunner bpf_test_runner(                        \
        new BPFTesterSimpleDelegate<BaselinePolicy>(test_name));          \
    sandbox::UnitTests::RunTestInProcess(                                 \
        &bpf_test_runner, PIFutexDeath,                                   \
        static_cast<const void*>(GetFutexErrorMessageContentForTests())); \
  }

void PIFutexDeath(int status, const std::string& msg, const void* aux) {
  if (base::KernelSupportsPriorityInheritanceFutex() &&
      (base::FeatureList::IsEnabled(
           base::features::kUsePriorityInheritanceMutex) ||
       base::FeatureList::IsEnabled(
           base::features::kPartitionAllocUsePriorityInheritanceLocks))) {
    sandbox::UnitTests::DeathSuccess(status, msg, nullptr);
  } else {
    sandbox::UnitTests::DeathSEGVMessage(status, msg, aux);
  }
}
#else
#define _TEST_BASELINE_PI_FUTEX(test_case_name, test_name)                    \
  BPF_DEATH_TEST_C(BaselinePolicy, test_name,                                 \
                   DEATH_SEGV_MESSAGE(GetFutexErrorMessageContentForTests()), \
                   BaselinePolicy)
#endif

TEST_BASELINE_PI_FUTEX(FUTEX_LOCK_PI)
TEST_BASELINE_PI_FUTEX(FUTEX_LOCK_PI2)
TEST_BASELINE_PI_FUTEX(FUTEX_TRYLOCK_PI)
TEST_BASELINE_PI_FUTEX(FUTEX_WAIT_REQUEUE_PI)
TEST_BASELINE_PI_FUTEX(FUTEX_CMP_REQUEUE_PI)
TEST_BASELINE_PI_FUTEX(FUTEX_UNLOCK_PI_PRIVATE)
#endif

BPF_TEST_C(BaselinePolicy, PrctlDumpable, BaselinePolicy) {
  const int is_dumpable = prctl(PR_GET_DUMPABLE, 0, 0, 0, 0);
  BPF_ASSERT(is_dumpable == 1 || is_dumpable == 0);
  const int prctl_ret = prctl(PR_SET_DUMPABLE, is_dumpable, 0, 0, 0, 0);
  BPF_ASSERT_EQ(0, prctl_ret);
}

// Workaround incomplete Android headers.
#if !defined(PR_CAPBSET_READ)
#define PR_CAPBSET_READ 23
#endif

#if !BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
BPF_DEATH_TEST_C(BaselinePolicy,
                 PrctlSigsys,
                 DEATH_SEGV_MESSAGE(GetPrctlErrorMessageContentForTests()),
                 BaselinePolicy) {
  prctl(PR_CAPBSET_READ, 0, 0, 0, 0);
  _exit(1);
}
#endif

BPF_TEST_C(BaselinePolicy, GetOrSetPriority, BaselinePolicy) {
  errno = 0;
  const int original_prio = getpriority(PRIO_PROCESS, 0);
  // Check errno instead of the return value since this system call can return
  // -1 as a valid value.
  BPF_ASSERT_EQ(0, errno);

  errno = 0;
  int rc = getpriority(PRIO_PROCESS, getpid());
  BPF_ASSERT_EQ(0, errno);

  rc = getpriority(PRIO_PROCESS, getpid() + 1);
  BPF_ASSERT_EQ(-1, rc);
  BPF_ASSERT_EQ(EPERM, errno);

  rc = setpriority(PRIO_PROCESS, 0, original_prio);
  BPF_ASSERT_EQ(0, rc);

  rc = setpriority(PRIO_PROCESS, getpid(), original_prio);
  BPF_ASSERT_EQ(0, rc);

  errno = 0;
  rc = setpriority(PRIO_PROCESS, getpid() + 1, original_prio);
  BPF_ASSERT_EQ(-1, rc);
  BPF_ASSERT_EQ(EPERM, errno);
}

BPF_DEATH_TEST_C(BaselinePolicy,
                 GetPrioritySigsys,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 BaselinePolicy) {
  getpriority(PRIO_USER, 0);
  _exit(1);
}

BPF_DEATH_TEST_C(BaselinePolicy,
                 ClockGettimeWithDisallowedClockCrashes,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 BaselinePolicy) {
  struct timespec ts;
  syscall(SYS_clock_gettime, (~0) | CLOCKFD, &ts);
}

BPF_DEATH_TEST_C(BaselinePolicy,
                 ClockNanosleepWithDisallowedClockCrashes,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 BaselinePolicy) {
  struct timespec ts;
  struct timespec out_ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 0;
  syscall(SYS_clock_nanosleep, (~0) | CLOCKFD, 0, &ts, &out_ts);
}

#if !defined(GRND_RANDOM)
#define GRND_RANDOM 2
#endif

BPF_DEATH_TEST_C(BaselinePolicy,
                 GetRandomOfDevRandomCrashes,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 BaselinePolicy) {
  syscall(__NR_getrandom, nullptr, 0, GRND_RANDOM);
}

#if !defined(__i386__)
BPF_DEATH_TEST_C(BaselinePolicy,
                 GetSockOptWrongLevelSigsys,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 BaselinePolicy) {
  int fds[2];
  PCHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
  int id;
  socklen_t peek_off_size = sizeof(id);
  getsockopt(fds[0], IPPROTO_TCP, SO_PEEK_OFF, &id, &peek_off_size);
}

BPF_DEATH_TEST_C(BaselinePolicy,
                 GetSockOptWrongOptionSigsys,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 BaselinePolicy) {
  int fds[2];
  PCHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
  int id;
  socklen_t peek_off_size = sizeof(id);
  getsockopt(fds[0], SOL_SOCKET, SO_DEBUG, &id, &peek_off_size);
}

BPF_DEATH_TEST_C(BaselinePolicy,
                 SetSockOptWrongLevelSigsys,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 BaselinePolicy) {
  int fds[2];
  PCHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
  int id;
  setsockopt(fds[0], IPPROTO_TCP, SO_PEEK_OFF, &id, sizeof(id));
}

BPF_DEATH_TEST_C(BaselinePolicy,
                 SetSockOptWrongOptionSigsys,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 BaselinePolicy) {
  int fds[2];
  PCHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
  int id;
  setsockopt(fds[0], SOL_SOCKET, SO_DEBUG, &id, sizeof(id));
}

BPF_DEATH_TEST_C(BaselinePolicy,
                 SendFlagsFiltered,
                 DEATH_SUCCESS(),
                 BaselinePolicy) {
  int fds[2];
  PCHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

  char buf[1] = {'x'};
// Check allowlisted MSG_DONTWAIT with send. Newer platforms don't have send()
// anymore.
#if defined(__NR_send)
  PCHECK(syscall(__NR_send, fds[0], &buf, 1, MSG_DONTWAIT) != -1);
#endif  //  defined(__NR_send)

  // Check allowlisted MSG_DONTWAIT with sendto
  PCHECK(syscall(__NR_sendto, fds[0], &buf, 1, MSG_DONTWAIT, nullptr,
                 nullptr) != -1);

  // Check allowlisted MSG_DONTWAIT with sendmsg
  struct msghdr msg = {};
  PCHECK(syscall(__NR_sendmsg, fds[0], &msg, MSG_DONTWAIT) != -1);
}

#if defined(__NR_send)
BPF_DEATH_TEST_C(BaselinePolicy,
                 SendFlagsFilteredMSG_OOB,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 BaselinePolicy) {
  int fds[2];
  PCHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

  char buf[1];
  // Specifically disallow MSG_OOB
  syscall(__NR_send, fds[0], &buf, 1, MSG_OOB);
}
#endif  //  defined(__NR_send)

BPF_DEATH_TEST_C(BaselinePolicy,
                 SendfromFlagsFilteredMSG_OOB,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 BaselinePolicy) {
  int fds[2];
  PCHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

  char buf[1];
  // Specifically disallow MSG_OOB
  syscall(__NR_sendto, fds[0], &buf, 1, MSG_OOB, nullptr, nullptr);
}

BPF_DEATH_TEST_C(BaselinePolicy,
                 SendmsgFlagsFilteredMSG_OOB,
                 DEATH_SEGV_MESSAGE(GetErrorMessageContentForTests()),
                 BaselinePolicy) {
  int fds[2];
  PCHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
  // Specifically disallow MSG_OOB
  struct msghdr msg = {};
  syscall(__NR_sendmsg, fds[0], &msg, MSG_DONTWAIT | MSG_OOB);
}

#endif  // !defined(i386)

}  // namespace

}  // namespace sandbox
