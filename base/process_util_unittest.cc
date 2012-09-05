// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _CRT_SECURE_NO_WARNINGS

#include <limits>

#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_LINUX)
#include <malloc.h>
#include <glib.h>
#include <sched.h>
#endif
#if defined(OS_POSIX)
#include <errno.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#endif
#if defined(OS_WIN)
#include <windows.h>
#endif
#if defined(OS_MACOSX)
#include <mach/vm_param.h>
#include <malloc/malloc.h>
#include "base/process_util_unittest_mac.h"
#endif

namespace {

#if defined(OS_WIN)
const wchar_t kProcessName[] = L"base_unittests.exe";
#else
const wchar_t kProcessName[] = L"base_unittests";
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
const char kShellPath[] = "/system/bin/sh";
const char kPosixShell[] = "sh";
#else
const char kShellPath[] = "/bin/sh";
const char kPosixShell[] = "bash";
#endif

const char kSignalFileSlow[] = "SlowChildProcess.die";
const char kSignalFileCrash[] = "CrashingChildProcess.die";
const char kSignalFileKill[] = "KilledChildProcess.die";

#if defined(OS_WIN)
const int kExpectedStillRunningExitCode = 0x102;
const int kExpectedKilledExitCode = 1;
#else
const int kExpectedStillRunningExitCode = 0;
#endif

// Sleeps until file filename is created.
void WaitToDie(const char* filename) {
  FILE* fp;
  do {
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
    fp = fopen(filename, "r");
  } while (!fp);
  fclose(fp);
}

// Signals children they should die now.
void SignalChildren(const char* filename) {
  FILE* fp = fopen(filename, "w");
  fclose(fp);
}

// Using a pipe to the child to wait for an event was considered, but
// there were cases in the past where pipes caused problems (other
// libraries closing the fds, child deadlocking). This is a simple
// case, so it's not worth the risk.  Using wait loops is discouraged
// in most instances.
base::TerminationStatus WaitForChildTermination(base::ProcessHandle handle,
                                                int* exit_code) {
  // Now we wait until the result is something other than STILL_RUNNING.
  base::TerminationStatus status = base::TERMINATION_STATUS_STILL_RUNNING;
  const base::TimeDelta kInterval = base::TimeDelta::FromMilliseconds(20);
  base::TimeDelta waited;
  do {
    status = base::GetTerminationStatus(handle, exit_code);
    base::PlatformThread::Sleep(kInterval);
    waited += kInterval;
  } while (status == base::TERMINATION_STATUS_STILL_RUNNING &&
// Waiting for more time for process termination on android devices.
#if defined(OS_ANDROID)
           waited < TestTimeouts::large_test_timeout());
#else
           waited < TestTimeouts::action_max_timeout());
#endif

  return status;
}

}  // namespace

class ProcessUtilTest : public base::MultiProcessTest {
 public:
#if defined(OS_POSIX)
  // Spawn a child process that counts how many file descriptors are open.
  int CountOpenFDsInChild();
#endif
  // Converts the filename to a platform specific filepath.
  // On Android files can not be created in arbitrary directories.
  static std::string GetSignalFilePath(const char* filename);
};

std::string ProcessUtilTest::GetSignalFilePath(const char* filename) {
#if !defined(OS_ANDROID)
  return filename;
#else
  FilePath tmp_dir;
  PathService::Get(base::DIR_CACHE, &tmp_dir);
  tmp_dir = tmp_dir.Append(filename);
  return tmp_dir.value();
#endif
}

MULTIPROCESS_TEST_MAIN(SimpleChildProcess) {
  return 0;
}

TEST_F(ProcessUtilTest, SpawnChild) {
  base::ProcessHandle handle = this->SpawnChild("SimpleChildProcess", false);
  ASSERT_NE(base::kNullProcessHandle, handle);
  EXPECT_TRUE(base::WaitForSingleProcess(
                  handle, TestTimeouts::action_max_timeout()));
  base::CloseProcessHandle(handle);
}

MULTIPROCESS_TEST_MAIN(SlowChildProcess) {
  WaitToDie(ProcessUtilTest::GetSignalFilePath(kSignalFileSlow).c_str());
  return 0;
}

TEST_F(ProcessUtilTest, KillSlowChild) {
  const std::string signal_file =
      ProcessUtilTest::GetSignalFilePath(kSignalFileSlow);
  remove(signal_file.c_str());
  base::ProcessHandle handle = this->SpawnChild("SlowChildProcess", false);
  ASSERT_NE(base::kNullProcessHandle, handle);
  SignalChildren(signal_file.c_str());
  EXPECT_TRUE(base::WaitForSingleProcess(
                  handle, TestTimeouts::action_max_timeout()));
  base::CloseProcessHandle(handle);
  remove(signal_file.c_str());
}

// Times out on Linux and Win, flakes on other platforms, http://crbug.com/95058
TEST_F(ProcessUtilTest, DISABLED_GetTerminationStatusExit) {
  const std::string signal_file =
      ProcessUtilTest::GetSignalFilePath(kSignalFileSlow);
  remove(signal_file.c_str());
  base::ProcessHandle handle = this->SpawnChild("SlowChildProcess", false);
  ASSERT_NE(base::kNullProcessHandle, handle);

  int exit_code = 42;
  EXPECT_EQ(base::TERMINATION_STATUS_STILL_RUNNING,
            base::GetTerminationStatus(handle, &exit_code));
  EXPECT_EQ(kExpectedStillRunningExitCode, exit_code);

  SignalChildren(signal_file.c_str());
  exit_code = 42;
  base::TerminationStatus status =
      WaitForChildTermination(handle, &exit_code);
  EXPECT_EQ(base::TERMINATION_STATUS_NORMAL_TERMINATION, status);
  EXPECT_EQ(0, exit_code);
  base::CloseProcessHandle(handle);
  remove(signal_file.c_str());
}

#if defined(OS_WIN)
// TODO(cpu): figure out how to test this in other platforms.
TEST_F(ProcessUtilTest, GetProcId) {
  base::ProcessId id1 = base::GetProcId(GetCurrentProcess());
  EXPECT_NE(0ul, id1);
  base::ProcessHandle handle = this->SpawnChild("SimpleChildProcess", false);
  ASSERT_NE(base::kNullProcessHandle, handle);
  base::ProcessId id2 = base::GetProcId(handle);
  EXPECT_NE(0ul, id2);
  EXPECT_NE(id1, id2);
  base::CloseProcessHandle(handle);
}

TEST_F(ProcessUtilTest, GetModuleFromAddress) {
  // Since the unit tests are their own EXE, this should be
  // equivalent to the EXE's HINSTANCE.
  //
  // kExpectedKilledExitCode is a constant in this file and
  // therefore within the unit test EXE.
  EXPECT_EQ(::GetModuleHandle(NULL),
            base::GetModuleFromAddress(
                const_cast<int*>(&kExpectedKilledExitCode)));

  // Any address within the kernel32 module should return
  // kernel32's HMODULE.  Our only assumption here is that
  // kernel32 is larger than 4 bytes.
  HMODULE kernel32 = ::GetModuleHandle(L"kernel32.dll");
  HMODULE kernel32_from_address =
      base::GetModuleFromAddress(reinterpret_cast<DWORD*>(kernel32) + 1);
  EXPECT_EQ(kernel32, kernel32_from_address);
}
#endif

#if !defined(OS_MACOSX)
// This test is disabled on Mac, since it's flaky due to ReportCrash
// taking a variable amount of time to parse and load the debug and
// symbol data for this unit test's executable before firing the
// signal handler.
//
// TODO(gspencer): turn this test process into a very small program
// with no symbols (instead of using the multiprocess testing
// framework) to reduce the ReportCrash overhead.

MULTIPROCESS_TEST_MAIN(CrashingChildProcess) {
  WaitToDie(ProcessUtilTest::GetSignalFilePath(kSignalFileCrash).c_str());
#if defined(OS_POSIX)
  // Have to disable to signal handler for segv so we can get a crash
  // instead of an abnormal termination through the crash dump handler.
  ::signal(SIGSEGV, SIG_DFL);
#endif
  // Make this process have a segmentation fault.
  volatile int* oops = NULL;
  *oops = 0xDEAD;
  return 1;
}

// This test intentionally crashes, so we don't need to run it under
// AddressSanitizer.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_GetTerminationStatusCrash DISABLED_GetTerminationStatusCrash
#else
#define MAYBE_GetTerminationStatusCrash GetTerminationStatusCrash
#endif
TEST_F(ProcessUtilTest, MAYBE_GetTerminationStatusCrash) {
  const std::string signal_file =
    ProcessUtilTest::GetSignalFilePath(kSignalFileCrash);
  remove(signal_file.c_str());
  base::ProcessHandle handle = this->SpawnChild("CrashingChildProcess",
                                                false);
  ASSERT_NE(base::kNullProcessHandle, handle);

  int exit_code = 42;
  EXPECT_EQ(base::TERMINATION_STATUS_STILL_RUNNING,
            base::GetTerminationStatus(handle, &exit_code));
  EXPECT_EQ(kExpectedStillRunningExitCode, exit_code);

  SignalChildren(signal_file.c_str());
  exit_code = 42;
  base::TerminationStatus status =
      WaitForChildTermination(handle, &exit_code);
  EXPECT_EQ(base::TERMINATION_STATUS_PROCESS_CRASHED, status);

#if defined(OS_WIN)
  EXPECT_EQ(0xc0000005, exit_code);
#elif defined(OS_POSIX)
  int signaled = WIFSIGNALED(exit_code);
  EXPECT_NE(0, signaled);
  int signal = WTERMSIG(exit_code);
  EXPECT_EQ(SIGSEGV, signal);
#endif
  base::CloseProcessHandle(handle);

  // Reset signal handlers back to "normal".
  base::EnableInProcessStackDumping();
  remove(signal_file.c_str());
}
#endif  // !defined(OS_MACOSX)

MULTIPROCESS_TEST_MAIN(KilledChildProcess) {
  WaitToDie(ProcessUtilTest::GetSignalFilePath(kSignalFileKill).c_str());
#if defined(OS_WIN)
  // Kill ourselves.
  HANDLE handle = ::OpenProcess(PROCESS_ALL_ACCESS, 0, ::GetCurrentProcessId());
  ::TerminateProcess(handle, kExpectedKilledExitCode);
#elif defined(OS_POSIX)
  // Send a SIGKILL to this process, just like the OOM killer would.
  ::kill(getpid(), SIGKILL);
#endif
  return 1;
}

TEST_F(ProcessUtilTest, GetTerminationStatusKill) {
  const std::string signal_file =
    ProcessUtilTest::GetSignalFilePath(kSignalFileKill);
  remove(signal_file.c_str());
  base::ProcessHandle handle = this->SpawnChild("KilledChildProcess",
                                                false);
  ASSERT_NE(base::kNullProcessHandle, handle);

  int exit_code = 42;
  EXPECT_EQ(base::TERMINATION_STATUS_STILL_RUNNING,
            base::GetTerminationStatus(handle, &exit_code));
  EXPECT_EQ(kExpectedStillRunningExitCode, exit_code);

  SignalChildren(signal_file.c_str());
  exit_code = 42;
  base::TerminationStatus status =
      WaitForChildTermination(handle, &exit_code);
  EXPECT_EQ(base::TERMINATION_STATUS_PROCESS_WAS_KILLED, status);
#if defined(OS_WIN)
  EXPECT_EQ(kExpectedKilledExitCode, exit_code);
#elif defined(OS_POSIX)
  int signaled = WIFSIGNALED(exit_code);
  EXPECT_NE(0, signaled);
  int signal = WTERMSIG(exit_code);
  EXPECT_EQ(SIGKILL, signal);
#endif
  base::CloseProcessHandle(handle);
  remove(signal_file.c_str());
}

// Ensure that the priority of a process is restored correctly after
// backgrounding and restoring.
// Note: a platform may not be willing or able to lower the priority of
// a process. The calls to SetProcessBackground should be noops then.
TEST_F(ProcessUtilTest, SetProcessBackgrounded) {
  base::ProcessHandle handle = this->SpawnChild("SimpleChildProcess", false);
  base::Process process(handle);
  int old_priority = process.GetPriority();
#if defined(OS_WIN)
  EXPECT_TRUE(process.SetProcessBackgrounded(true));
  EXPECT_TRUE(process.IsProcessBackgrounded());
  EXPECT_TRUE(process.SetProcessBackgrounded(false));
  EXPECT_FALSE(process.IsProcessBackgrounded());
#else
  process.SetProcessBackgrounded(true);
  process.SetProcessBackgrounded(false);
#endif
  int new_priority = process.GetPriority();
  EXPECT_EQ(old_priority, new_priority);
}

// Same as SetProcessBackgrounded but to this very process. It uses
// a different code path at least for Windows.
TEST_F(ProcessUtilTest, SetProcessBackgroundedSelf) {
  base::Process process(base::Process::Current().handle());
  int old_priority = process.GetPriority();
#if defined(OS_WIN)
  EXPECT_TRUE(process.SetProcessBackgrounded(true));
  EXPECT_TRUE(process.IsProcessBackgrounded());
  EXPECT_TRUE(process.SetProcessBackgrounded(false));
  EXPECT_FALSE(process.IsProcessBackgrounded());
#else
  process.SetProcessBackgrounded(true);
  process.SetProcessBackgrounded(false);
#endif
  int new_priority = process.GetPriority();
  EXPECT_EQ(old_priority, new_priority);
}

// TODO(estade): if possible, port these 2 tests.
#if defined(OS_WIN)
TEST_F(ProcessUtilTest, EnableLFH) {
  ASSERT_TRUE(base::EnableLowFragmentationHeap());
  if (IsDebuggerPresent()) {
    // Under these conditions, LFH can't be enabled. There's no point to test
    // anything.
    const char* no_debug_env = getenv("_NO_DEBUG_HEAP");
    if (!no_debug_env || strcmp(no_debug_env, "1"))
      return;
  }
  HANDLE heaps[1024] = { 0 };
  unsigned number_heaps = GetProcessHeaps(1024, heaps);
  EXPECT_GT(number_heaps, 0u);
  for (unsigned i = 0; i < number_heaps; ++i) {
    ULONG flag = 0;
    SIZE_T length;
    ASSERT_NE(0, HeapQueryInformation(heaps[i],
                                      HeapCompatibilityInformation,
                                      &flag,
                                      sizeof(flag),
                                      &length));
    // If flag is 0, the heap is a standard heap that does not support
    // look-asides. If flag is 1, the heap supports look-asides. If flag is 2,
    // the heap is a low-fragmentation heap (LFH). Note that look-asides are not
    // supported on the LFH.

    // We don't have any documented way of querying the HEAP_NO_SERIALIZE flag.
    EXPECT_LE(flag, 2u);
    EXPECT_NE(flag, 1u);
  }
}

TEST_F(ProcessUtilTest, CalcFreeMemory) {
  scoped_ptr<base::ProcessMetrics> metrics(
      base::ProcessMetrics::CreateProcessMetrics(::GetCurrentProcess()));
  ASSERT_TRUE(NULL != metrics.get());

  // Typical values here is ~1900 for total and ~1000 for largest. Obviously
  // it depends in what other tests have done to this process.
  base::FreeMBytes free_mem1 = {0};
  EXPECT_TRUE(metrics->CalculateFreeMemory(&free_mem1));
  EXPECT_LT(10u, free_mem1.total);
  EXPECT_LT(10u, free_mem1.largest);
  EXPECT_GT(2048u, free_mem1.total);
  EXPECT_GT(2048u, free_mem1.largest);
  EXPECT_GE(free_mem1.total, free_mem1.largest);
  EXPECT_TRUE(NULL != free_mem1.largest_ptr);

  // Allocate 20M and check again. It should have gone down.
  const int kAllocMB = 20;
  scoped_array<char> alloc(new char[kAllocMB * 1024 * 1024]);
  size_t expected_total = free_mem1.total - kAllocMB;
  size_t expected_largest = free_mem1.largest;

  base::FreeMBytes free_mem2 = {0};
  EXPECT_TRUE(metrics->CalculateFreeMemory(&free_mem2));
  EXPECT_GE(free_mem2.total, free_mem2.largest);
  EXPECT_GE(expected_total, free_mem2.total);
  EXPECT_GE(expected_largest, free_mem2.largest);
  EXPECT_TRUE(NULL != free_mem2.largest_ptr);
}

TEST_F(ProcessUtilTest, GetAppOutput) {
  // Let's create a decently long message.
  std::string message;
  for (int i = 0; i < 1025; i++) {  // 1025 so it does not end on a kilo-byte
                                    // boundary.
    message += "Hello!";
  }
  // cmd.exe's echo always adds a \r\n to its output.
  std::string expected(message);
  expected += "\r\n";

  FilePath cmd(L"cmd.exe");
  CommandLine cmd_line(cmd);
  cmd_line.AppendArg("/c");
  cmd_line.AppendArg("echo " + message + "");
  std::string output;
  ASSERT_TRUE(base::GetAppOutput(cmd_line, &output));
  EXPECT_EQ(expected, output);

  // Let's make sure stderr is ignored.
  CommandLine other_cmd_line(cmd);
  other_cmd_line.AppendArg("/c");
  // http://msdn.microsoft.com/library/cc772622.aspx
  cmd_line.AppendArg("echo " + message + " >&2");
  output.clear();
  ASSERT_TRUE(base::GetAppOutput(other_cmd_line, &output));
  EXPECT_EQ("", output);
}

TEST_F(ProcessUtilTest, LaunchAsUser) {
  base::UserTokenHandle token;
  ASSERT_TRUE(OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token));
  std::wstring cmdline =
      this->MakeCmdLine("SimpleChildProcess", false).GetCommandLineString();
  base::LaunchOptions options;
  options.as_user = token;
  EXPECT_TRUE(base::LaunchProcess(cmdline, options, NULL));
}

#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)

// For the following Mac tests:
// Note that base::EnableTerminationOnHeapCorruption() is called as part of
// test suite setup and does not need to be done again, else mach_override
// will fail.

#if !defined(ADDRESS_SANITIZER)
// The following code tests the system implementation of malloc() thus no need
// to test it under AddressSanitizer.
TEST_F(ProcessUtilTest, MacMallocFailureDoesNotTerminate) {
  // Install the OOM killer.
  base::EnableTerminationOnOutOfMemory();

  // Test that ENOMEM doesn't crash via CrMallocErrorBreak two ways: the exit
  // code and lack of the error string. The number of bytes is one less than
  // MALLOC_ABSOLUTE_MAX_SIZE, more than which the system early-returns NULL and
  // does not call through malloc_error_break(). See the comment at
  // EnableTerminationOnOutOfMemory() for more information.
  void* buf = NULL;
  ASSERT_EXIT(
      buf = malloc(std::numeric_limits<size_t>::max() - (2 * PAGE_SIZE) - 1),
      testing::KilledBySignal(SIGTRAP),
      "\\*\\*\\* error: can't allocate region.*"
          "(Terminating process due to a potential for future heap "
          "corruption){0}");

  base::debug::Alias(buf);
}
#endif  // !defined(ADDRESS_SANITIZER)

TEST_F(ProcessUtilTest, MacTerminateOnHeapCorruption) {
  // Assert that freeing an unallocated pointer will crash the process.
  char buf[3];
  asm("" : "=r" (buf));  // Prevent clang from being too smart.
#if !defined(ADDRESS_SANITIZER)
  ASSERT_DEATH(free(buf), "being freed.*"
      "\\*\\*\\* set a breakpoint in malloc_error_break to debug.*"
      "Terminating process due to a potential for future heap corruption");
#else
  // AddressSanitizer replaces malloc() and prints a different error message on
  // heap corruption.
  ASSERT_DEATH(free(buf), "attempting free on address which "
      "was not malloc\\(\\)-ed");
#endif  // !defined(ADDRESS_SANITIZER)
}

#endif  // defined(OS_MACOSX)

#if defined(OS_POSIX)

namespace {

// Returns the maximum number of files that a process can have open.
// Returns 0 on error.
int GetMaxFilesOpenInProcess() {
  struct rlimit rlim;
  if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) {
    return 0;
  }

  // rlim_t is a uint64 - clip to maxint. We do this since FD #s are ints
  // which are all 32 bits on the supported platforms.
  rlim_t max_int = static_cast<rlim_t>(std::numeric_limits<int32>::max());
  if (rlim.rlim_cur > max_int) {
    return max_int;
  }

  return rlim.rlim_cur;
}

const int kChildPipe = 20;  // FD # for write end of pipe in child process.

}  // namespace

MULTIPROCESS_TEST_MAIN(ProcessUtilsLeakFDChildProcess) {
  // This child process counts the number of open FDs, it then writes that
  // number out to a pipe connected to the parent.
  int num_open_files = 0;
  int write_pipe = kChildPipe;
  int max_files = GetMaxFilesOpenInProcess();
  for (int i = STDERR_FILENO + 1; i < max_files; i++) {
    if (i != kChildPipe) {
      int fd;
      if ((fd = HANDLE_EINTR(dup(i))) != -1) {
        close(fd);
        num_open_files += 1;
      }
    }
  }

  int written = HANDLE_EINTR(write(write_pipe, &num_open_files,
                                   sizeof(num_open_files)));
  DCHECK_EQ(static_cast<size_t>(written), sizeof(num_open_files));
  int ret = HANDLE_EINTR(close(write_pipe));
  DPCHECK(ret == 0);

  return 0;
}

int ProcessUtilTest::CountOpenFDsInChild() {
  int fds[2];
  if (pipe(fds) < 0)
    NOTREACHED();

  base::FileHandleMappingVector fd_mapping_vec;
  fd_mapping_vec.push_back(std::pair<int, int>(fds[1], kChildPipe));
  base::ProcessHandle handle = this->SpawnChild(
      "ProcessUtilsLeakFDChildProcess", fd_mapping_vec, false);
  CHECK(handle);
  int ret = HANDLE_EINTR(close(fds[1]));
  DPCHECK(ret == 0);

  // Read number of open files in client process from pipe;
  int num_open_files = -1;
  ssize_t bytes_read =
      HANDLE_EINTR(read(fds[0], &num_open_files, sizeof(num_open_files)));
  CHECK_EQ(bytes_read, static_cast<ssize_t>(sizeof(num_open_files)));

  CHECK(base::WaitForSingleProcess(handle, base::TimeDelta::FromSeconds(1)));
  base::CloseProcessHandle(handle);
  ret = HANDLE_EINTR(close(fds[0]));
  DPCHECK(ret == 0);

  return num_open_files;
}

TEST_F(ProcessUtilTest, FDRemapping) {
  int fds_before = CountOpenFDsInChild();

  // open some dummy fds to make sure they don't propagate over to the
  // child process.
  int dev_null = open("/dev/null", O_RDONLY);
  int sockets[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);

  int fds_after = CountOpenFDsInChild();

  ASSERT_EQ(fds_after, fds_before);

  int ret;
  ret = HANDLE_EINTR(close(sockets[0]));
  DPCHECK(ret == 0);
  ret = HANDLE_EINTR(close(sockets[1]));
  DPCHECK(ret == 0);
  ret = HANDLE_EINTR(close(dev_null));
  DPCHECK(ret == 0);
}

namespace {

std::string TestLaunchProcess(const base::EnvironmentVector& env_changes,
                              const int clone_flags) {
  std::vector<std::string> args;
  base::FileHandleMappingVector fds_to_remap;

  args.push_back(kPosixShell);
  args.push_back("-c");
  args.push_back("echo $BASE_TEST");

  int fds[2];
  PCHECK(pipe(fds) == 0);

  fds_to_remap.push_back(std::make_pair(fds[1], 1));
  base::LaunchOptions options;
  options.wait = true;
  options.environ = &env_changes;
  options.fds_to_remap = &fds_to_remap;
#if defined(OS_LINUX)
  options.clone_flags = clone_flags;
#else
  CHECK_EQ(0, clone_flags);
#endif  // OS_LINUX
  EXPECT_TRUE(base::LaunchProcess(args, options, NULL));
  PCHECK(HANDLE_EINTR(close(fds[1])) == 0);

  char buf[512];
  const ssize_t n = HANDLE_EINTR(read(fds[0], buf, sizeof(buf)));
  PCHECK(n > 0);

  PCHECK(HANDLE_EINTR(close(fds[0])) == 0);

  return std::string(buf, n);
}

const char kLargeString[] =
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789";

}  // namespace

TEST_F(ProcessUtilTest, LaunchProcess) {
  base::EnvironmentVector env_changes;
  const int no_clone_flags = 0;

  env_changes.push_back(std::make_pair(std::string("BASE_TEST"),
                                       std::string("bar")));
  EXPECT_EQ("bar\n", TestLaunchProcess(env_changes, no_clone_flags));
  env_changes.clear();

  EXPECT_EQ(0, setenv("BASE_TEST", "testing", 1 /* override */));
  EXPECT_EQ("testing\n", TestLaunchProcess(env_changes, no_clone_flags));

  env_changes.push_back(std::make_pair(std::string("BASE_TEST"),
                                       std::string("")));
  EXPECT_EQ("\n", TestLaunchProcess(env_changes, no_clone_flags));

  env_changes[0].second = "foo";
  EXPECT_EQ("foo\n", TestLaunchProcess(env_changes, no_clone_flags));

  env_changes.clear();
  EXPECT_EQ(0, setenv("BASE_TEST", kLargeString, 1 /* override */));
  EXPECT_EQ(std::string(kLargeString) + "\n",
            TestLaunchProcess(env_changes, no_clone_flags));

  env_changes.push_back(std::make_pair(std::string("BASE_TEST"),
                                       std::string("wibble")));
  EXPECT_EQ("wibble\n", TestLaunchProcess(env_changes, no_clone_flags));

#if defined(OS_LINUX)
  // Test a non-trival value for clone_flags.
  // Don't test on Valgrind as it has limited support for clone().
  if (!RunningOnValgrind()) {
    EXPECT_EQ("wibble\n", TestLaunchProcess(env_changes, CLONE_FS | SIGCHLD));
  }
#endif
}

TEST_F(ProcessUtilTest, AlterEnvironment) {
  const char* const empty[] = { NULL };
  const char* const a2[] = { "A=2", NULL };
  base::EnvironmentVector changes;
  char** e;

  e = base::AlterEnvironment(changes, empty);
  EXPECT_TRUE(e[0] == NULL);
  delete[] e;

  changes.push_back(std::make_pair(std::string("A"), std::string("1")));
  e = base::AlterEnvironment(changes, empty);
  EXPECT_EQ(std::string("A=1"), e[0]);
  EXPECT_TRUE(e[1] == NULL);
  delete[] e;

  changes.clear();
  changes.push_back(std::make_pair(std::string("A"), std::string("")));
  e = base::AlterEnvironment(changes, empty);
  EXPECT_TRUE(e[0] == NULL);
  delete[] e;

  changes.clear();
  e = base::AlterEnvironment(changes, a2);
  EXPECT_EQ(std::string("A=2"), e[0]);
  EXPECT_TRUE(e[1] == NULL);
  delete[] e;

  changes.clear();
  changes.push_back(std::make_pair(std::string("A"), std::string("1")));
  e = base::AlterEnvironment(changes, a2);
  EXPECT_EQ(std::string("A=1"), e[0]);
  EXPECT_TRUE(e[1] == NULL);
  delete[] e;

  changes.clear();
  changes.push_back(std::make_pair(std::string("A"), std::string("")));
  e = base::AlterEnvironment(changes, a2);
  EXPECT_TRUE(e[0] == NULL);
  delete[] e;
}

TEST_F(ProcessUtilTest, GetAppOutput) {
  std::string output;

#if defined(OS_ANDROID)
  std::vector<std::string> argv;
  argv.push_back("sh");  // Instead of /bin/sh, force path search to find it.
  argv.push_back("-c");

  argv.push_back("exit 0");
  EXPECT_TRUE(base::GetAppOutput(CommandLine(argv), &output));
  EXPECT_STREQ("", output.c_str());

  argv[2] = "exit 1";
  EXPECT_FALSE(base::GetAppOutput(CommandLine(argv), &output));
  EXPECT_STREQ("", output.c_str());

  argv[2] = "echo foobar42";
  EXPECT_TRUE(base::GetAppOutput(CommandLine(argv), &output));
  EXPECT_STREQ("foobar42\n", output.c_str());
#else
  EXPECT_TRUE(base::GetAppOutput(CommandLine(FilePath("true")), &output));
  EXPECT_STREQ("", output.c_str());

  EXPECT_FALSE(base::GetAppOutput(CommandLine(FilePath("false")), &output));

  std::vector<std::string> argv;
  argv.push_back("/bin/echo");
  argv.push_back("-n");
  argv.push_back("foobar42");
  EXPECT_TRUE(base::GetAppOutput(CommandLine(argv), &output));
  EXPECT_STREQ("foobar42", output.c_str());
#endif  // defined(OS_ANDROID)
}

TEST_F(ProcessUtilTest, GetAppOutputRestricted) {
  // Unfortunately, since we can't rely on the path, we need to know where
  // everything is. So let's use /bin/sh, which is on every POSIX system, and
  // its built-ins.
  std::vector<std::string> argv;
  argv.push_back(std::string(kShellPath));  // argv[0]
  argv.push_back("-c");  // argv[1]

  // On success, should set |output|. We use |/bin/sh -c 'exit 0'| instead of
  // |true| since the location of the latter may be |/bin| or |/usr/bin| (and we
  // need absolute paths).
  argv.push_back("exit 0");   // argv[2]; equivalent to "true"
  std::string output = "abc";
  EXPECT_TRUE(base::GetAppOutputRestricted(CommandLine(argv), &output, 100));
  EXPECT_STREQ("", output.c_str());

  argv[2] = "exit 1";  // equivalent to "false"
  output = "before";
  EXPECT_FALSE(base::GetAppOutputRestricted(CommandLine(argv),
                                            &output, 100));
  EXPECT_STREQ("", output.c_str());

  // Amount of output exactly equal to space allowed.
  argv[2] = "echo 123456789";  // (the sh built-in doesn't take "-n")
  output.clear();
  EXPECT_TRUE(base::GetAppOutputRestricted(CommandLine(argv), &output, 10));
  EXPECT_STREQ("123456789\n", output.c_str());

  // Amount of output greater than space allowed.
  output.clear();
  EXPECT_TRUE(base::GetAppOutputRestricted(CommandLine(argv), &output, 5));
  EXPECT_STREQ("12345", output.c_str());

  // Amount of output less than space allowed.
  output.clear();
  EXPECT_TRUE(base::GetAppOutputRestricted(CommandLine(argv), &output, 15));
  EXPECT_STREQ("123456789\n", output.c_str());

  // Zero space allowed.
  output = "abc";
  EXPECT_TRUE(base::GetAppOutputRestricted(CommandLine(argv), &output, 0));
  EXPECT_STREQ("", output.c_str());
}

#if !defined(OS_MACOSX) && !defined(OS_OPENBSD)
// TODO(benwells): GetAppOutputRestricted should terminate applications
// with SIGPIPE when we have enough output. http://crbug.com/88502
TEST_F(ProcessUtilTest, GetAppOutputRestrictedSIGPIPE) {
  std::vector<std::string> argv;
  std::string output;

  argv.push_back(std::string(kShellPath));  // argv[0]
  argv.push_back("-c");
#if defined(OS_ANDROID)
  argv.push_back("while echo 12345678901234567890; do :; done");
  EXPECT_TRUE(base::GetAppOutputRestricted(CommandLine(argv), &output, 10));
  EXPECT_STREQ("1234567890", output.c_str());
#else
  argv.push_back("yes");
  EXPECT_TRUE(base::GetAppOutputRestricted(CommandLine(argv), &output, 10));
  EXPECT_STREQ("y\ny\ny\ny\ny\n", output.c_str());
#endif
}
#endif

TEST_F(ProcessUtilTest, GetAppOutputRestrictedNoZombies) {
  std::vector<std::string> argv;

  argv.push_back(std::string(kShellPath));  // argv[0]
  argv.push_back("-c");  // argv[1]
  argv.push_back("echo 123456789012345678901234567890");  // argv[2]

  // Run |GetAppOutputRestricted()| 300 (> default per-user processes on Mac OS
  // 10.5) times with an output buffer big enough to capture all output.
  for (int i = 0; i < 300; i++) {
    std::string output;
    EXPECT_TRUE(base::GetAppOutputRestricted(CommandLine(argv), &output, 100));
    EXPECT_STREQ("123456789012345678901234567890\n", output.c_str());
  }

  // Ditto, but with an output buffer too small to capture all output.
  for (int i = 0; i < 300; i++) {
    std::string output;
    EXPECT_TRUE(base::GetAppOutputRestricted(CommandLine(argv), &output, 10));
    EXPECT_STREQ("1234567890", output.c_str());
  }
}

TEST_F(ProcessUtilTest, GetAppOutputWithExitCode) {
  // Test getting output from a successful application.
  std::vector<std::string> argv;
  std::string output;
  int exit_code;
  argv.push_back(std::string(kShellPath));  // argv[0]
  argv.push_back("-c");  // argv[1]
  argv.push_back("echo foo");  // argv[2];
  EXPECT_TRUE(base::GetAppOutputWithExitCode(CommandLine(argv), &output,
                                             &exit_code));
  EXPECT_STREQ("foo\n", output.c_str());
  EXPECT_EQ(exit_code, 0);

  // Test getting output from an application which fails with a specific exit
  // code.
  output.clear();
  argv[2] = "echo foo; exit 2";
  EXPECT_TRUE(base::GetAppOutputWithExitCode(CommandLine(argv), &output,
                                             &exit_code));
  EXPECT_STREQ("foo\n", output.c_str());
  EXPECT_EQ(exit_code, 2);
}

TEST_F(ProcessUtilTest, GetParentProcessId) {
  base::ProcessId ppid = base::GetParentProcessId(base::GetCurrentProcId());
  EXPECT_EQ(ppid, getppid());
}

#if defined(OS_LINUX) || defined(OS_ANDROID)
TEST_F(ProcessUtilTest, ParseProcStatCPU) {
  // /proc/self/stat for a process running "top".
  const char kTopStat[] = "960 (top) S 16230 960 16230 34818 960 "
      "4202496 471 0 0 0 "
      "12 16 0 0 "  // <- These are the goods.
      "20 0 1 0 121946157 15077376 314 18446744073709551615 4194304 "
      "4246868 140733983044336 18446744073709551615 140244213071219 "
      "0 0 0 138047495 0 0 0 17 1 0 0 0 0 0";
  EXPECT_EQ(12 + 16, base::ParseProcStatCPU(kTopStat));

  // cat /proc/self/stat on a random other machine I have.
  const char kSelfStat[] = "5364 (cat) R 5354 5364 5354 34819 5364 "
      "0 142 0 0 0 "
      "0 0 0 0 "  // <- No CPU, apparently.
      "16 0 1 0 1676099790 2957312 114 4294967295 134512640 134528148 "
      "3221224832 3221224344 3086339742 0 0 0 0 0 0 0 17 0 0 0";

  EXPECT_EQ(0, base::ParseProcStatCPU(kSelfStat));
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

// TODO(port): port those unit tests.
bool IsProcessDead(base::ProcessHandle child) {
  // waitpid() will actually reap the process which is exactly NOT what we
  // want to test for.  The good thing is that if it can't find the process
  // we'll get a nice value for errno which we can test for.
  const pid_t result = HANDLE_EINTR(waitpid(child, NULL, WNOHANG));
  return result == -1 && errno == ECHILD;
}

TEST_F(ProcessUtilTest, DelayedTermination) {
  base::ProcessHandle child_process =
      SpawnChild("process_util_test_never_die", false);
  ASSERT_TRUE(child_process);
  base::EnsureProcessTerminated(child_process);
  base::WaitForSingleProcess(child_process, base::TimeDelta::FromSeconds(5));

  // Check that process was really killed.
  EXPECT_TRUE(IsProcessDead(child_process));
  base::CloseProcessHandle(child_process);
}

MULTIPROCESS_TEST_MAIN(process_util_test_never_die) {
  while (1) {
    sleep(500);
  }
  return 0;
}

TEST_F(ProcessUtilTest, ImmediateTermination) {
  base::ProcessHandle child_process =
      SpawnChild("process_util_test_die_immediately", false);
  ASSERT_TRUE(child_process);
  // Give it time to die.
  sleep(2);
  base::EnsureProcessTerminated(child_process);

  // Check that process was really killed.
  EXPECT_TRUE(IsProcessDead(child_process));
  base::CloseProcessHandle(child_process);
}

MULTIPROCESS_TEST_MAIN(process_util_test_die_immediately) {
  return 0;
}

#endif  // defined(OS_POSIX)

// Android doesn't implement set_new_handler, so we can't use the
// OutOfMemoryTest cases.
// OpenBSD does not support these tests either.
// AddressSanitizer defines the malloc()/free()/etc. functions so that they
// don't crash if the program is out of memory, so the OOM tests aren't supposed
// to work.
// TODO(vandebo) make this work on Windows too.
#if !defined(OS_ANDROID) && !defined(OS_OPENBSD) && \
    !defined(OS_WIN) && !defined(ADDRESS_SANITIZER)

#if defined(USE_TCMALLOC)
extern "C" {
int tc_set_new_mode(int mode);
}
#endif  // defined(USE_TCMALLOC)

class OutOfMemoryDeathTest : public testing::Test {
 public:
  OutOfMemoryDeathTest()
      : value_(NULL),
        // Make test size as large as possible minus a few pages so
        // that alignment or other rounding doesn't make it wrap.
        test_size_(std::numeric_limits<std::size_t>::max() - 12 * 1024),
        signed_test_size_(std::numeric_limits<ssize_t>::max()) {
  }

#if defined(USE_TCMALLOC)
  virtual void SetUp() OVERRIDE {
    tc_set_new_mode(1);
  }

  virtual void TearDown() OVERRIDE {
    tc_set_new_mode(0);
  }
#endif  // defined(USE_TCMALLOC)

  void SetUpInDeathAssert() {
    // Must call EnableTerminationOnOutOfMemory() because that is called from
    // chrome's main function and therefore hasn't been called yet.
    // Since this call may result in another thread being created and death
    // tests shouldn't be started in a multithread environment, this call
    // should be done inside of the ASSERT_DEATH.
    base::EnableTerminationOnOutOfMemory();
  }

  void* value_;
  size_t test_size_;
  ssize_t signed_test_size_;
};

TEST_F(OutOfMemoryDeathTest, New) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = operator new(test_size_);
    }, "");
}

TEST_F(OutOfMemoryDeathTest, NewArray) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = new char[test_size_];
    }, "");
}

TEST_F(OutOfMemoryDeathTest, Malloc) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = malloc(test_size_);
    }, "");
}

TEST_F(OutOfMemoryDeathTest, Realloc) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = realloc(NULL, test_size_);
    }, "");
}

TEST_F(OutOfMemoryDeathTest, Calloc) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = calloc(1024, test_size_ / 1024L);
    }, "");
}

TEST_F(OutOfMemoryDeathTest, Valloc) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = valloc(test_size_);
    }, "");
}

#if defined(OS_LINUX)
TEST_F(OutOfMemoryDeathTest, Pvalloc) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = pvalloc(test_size_);
    }, "");
}

TEST_F(OutOfMemoryDeathTest, Memalign) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = memalign(4, test_size_);
    }, "");
}

TEST_F(OutOfMemoryDeathTest, ViaSharedLibraries) {
  // g_try_malloc is documented to return NULL on failure. (g_malloc is the
  // 'safe' default that crashes if allocation fails). However, since we have
  // hopefully overridden malloc, even g_try_malloc should fail. This tests
  // that the run-time symbol resolution is overriding malloc for shared
  // libraries as well as for our code.
  ASSERT_DEATH({
      SetUpInDeathAssert();
      value_ = g_try_malloc(test_size_);
    }, "");
}
#endif  // OS_LINUX

// Android doesn't implement posix_memalign().
#if defined(OS_POSIX) && !defined(OS_ANDROID)
TEST_F(OutOfMemoryDeathTest, Posix_memalign) {
  typedef int (*memalign_t)(void **, size_t, size_t);
#if defined(OS_MACOSX)
  // posix_memalign only exists on >= 10.6. Use dlsym to grab it at runtime
  // because it may not be present in the SDK used for compilation.
  memalign_t memalign =
      reinterpret_cast<memalign_t>(dlsym(RTLD_DEFAULT, "posix_memalign"));
#else
  memalign_t memalign = posix_memalign;
#endif  // defined(OS_MACOSX)
  if (memalign) {
    // Grab the return value of posix_memalign to silence a compiler warning
    // about unused return values. We don't actually care about the return
    // value, since we're asserting death.
    ASSERT_DEATH({
        SetUpInDeathAssert();
        EXPECT_EQ(ENOMEM, memalign(&value_, 8, test_size_));
      }, "");
  }
}
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID)

#if defined(OS_MACOSX)

// Purgeable zone tests (if it exists)

TEST_F(OutOfMemoryDeathTest, MallocPurgeable) {
  malloc_zone_t* zone = base::GetPurgeableZone();
  if (zone)
    ASSERT_DEATH({
        SetUpInDeathAssert();
        value_ = malloc_zone_malloc(zone, test_size_);
      }, "");
}

TEST_F(OutOfMemoryDeathTest, ReallocPurgeable) {
  malloc_zone_t* zone = base::GetPurgeableZone();
  if (zone)
    ASSERT_DEATH({
        SetUpInDeathAssert();
        value_ = malloc_zone_realloc(zone, NULL, test_size_);
      }, "");
}

TEST_F(OutOfMemoryDeathTest, CallocPurgeable) {
  malloc_zone_t* zone = base::GetPurgeableZone();
  if (zone)
    ASSERT_DEATH({
        SetUpInDeathAssert();
        value_ = malloc_zone_calloc(zone, 1024, test_size_ / 1024L);
      }, "");
}

TEST_F(OutOfMemoryDeathTest, VallocPurgeable) {
  malloc_zone_t* zone = base::GetPurgeableZone();
  if (zone)
    ASSERT_DEATH({
        SetUpInDeathAssert();
        value_ = malloc_zone_valloc(zone, test_size_);
      }, "");
}

TEST_F(OutOfMemoryDeathTest, PosixMemalignPurgeable) {
  malloc_zone_t* zone = base::GetPurgeableZone();

  typedef void* (*zone_memalign_t)(malloc_zone_t*, size_t, size_t);
  // malloc_zone_memalign only exists on >= 10.6. Use dlsym to grab it at
  // runtime because it may not be present in the SDK used for compilation.
  zone_memalign_t zone_memalign =
      reinterpret_cast<zone_memalign_t>(
        dlsym(RTLD_DEFAULT, "malloc_zone_memalign"));

  if (zone && zone_memalign) {
    ASSERT_DEATH({
        SetUpInDeathAssert();
        value_ = zone_memalign(zone, 8, test_size_);
      }, "");
  }
}

// Since these allocation functions take a signed size, it's possible that
// calling them just once won't be enough to exhaust memory. In the 32-bit
// environment, it's likely that these allocation attempts will fail because
// not enough contiguous address space is available. In the 64-bit environment,
// it's likely that they'll fail because they would require a preposterous
// amount of (virtual) memory.

TEST_F(OutOfMemoryDeathTest, CFAllocatorSystemDefault) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      while ((value_ =
              base::AllocateViaCFAllocatorSystemDefault(signed_test_size_))) {}
    }, "");
}

TEST_F(OutOfMemoryDeathTest, CFAllocatorMalloc) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      while ((value_ =
              base::AllocateViaCFAllocatorMalloc(signed_test_size_))) {}
    }, "");
}

TEST_F(OutOfMemoryDeathTest, CFAllocatorMallocZone) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      while ((value_ =
              base::AllocateViaCFAllocatorMallocZone(signed_test_size_))) {}
    }, "");
}

#if !defined(ARCH_CPU_64_BITS)

// See process_util_unittest_mac.mm for an explanation of why this test isn't
// run in the 64-bit environment.

TEST_F(OutOfMemoryDeathTest, PsychoticallyBigObjCObject) {
  ASSERT_DEATH({
      SetUpInDeathAssert();
      while ((value_ = base::AllocatePsychoticallyBigObjCObject())) {}
    }, "");
}

#endif  // !ARCH_CPU_64_BITS
#endif  // OS_MACOSX

#endif  // !defined(OS_ANDROID) && !defined(OS_OPENBSD) &&
        // !defined(OS_WIN) && !defined(ADDRESS_SANITIZER)
