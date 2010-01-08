// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _CRT_SECURE_NO_WARNINGS

#include <limits>

#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/multiprocess_test.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_LINUX)
#include <dlfcn.h>
#include <errno.h>
#include <malloc.h>
#include <glib.h>
#endif
#if defined(OS_POSIX)
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#endif
#if defined(OS_WIN)
#include <windows.h>
#endif

namespace base {

class ProcessUtilTest : public MultiProcessTest {
#if defined(OS_POSIX)
 public:
  // Spawn a child process that counts how many file descriptors are open.
  int CountOpenFDsInChild();
#endif
};

MULTIPROCESS_TEST_MAIN(SimpleChildProcess) {
  return 0;
}

TEST_F(ProcessUtilTest, SpawnChild) {
  ProcessHandle handle = this->SpawnChild(L"SimpleChildProcess");

  ASSERT_NE(base::kNullProcessHandle, handle);
  EXPECT_TRUE(WaitForSingleProcess(handle, 5000));
  base::CloseProcessHandle(handle);
}

MULTIPROCESS_TEST_MAIN(SlowChildProcess) {
  // Sleep until file "SlowChildProcess.die" is created.
  FILE *fp;
  do {
    PlatformThread::Sleep(100);
    fp = fopen("SlowChildProcess.die", "r");
  } while (!fp);
  fclose(fp);
  remove("SlowChildProcess.die");
  exit(0);
  return 0;
}

TEST_F(ProcessUtilTest, KillSlowChild) {
  remove("SlowChildProcess.die");
  ProcessHandle handle = this->SpawnChild(L"SlowChildProcess");
  ASSERT_NE(base::kNullProcessHandle, handle);
  FILE *fp = fopen("SlowChildProcess.die", "w");
  fclose(fp);
  EXPECT_TRUE(base::WaitForSingleProcess(handle, 5000));
  base::CloseProcessHandle(handle);
}

// Ensure that the priority of a process is restored correctly after
// backgrounding and restoring.
// Note: a platform may not be willing or able to lower the priority of
// a process. The calls to SetProcessBackground should be noops then.
TEST_F(ProcessUtilTest, SetProcessBackgrounded) {
  ProcessHandle handle = this->SpawnChild(L"SimpleChildProcess");
  Process process(handle);
  int old_priority = process.GetPriority();
  process.SetProcessBackgrounded(true);
  process.SetProcessBackgrounded(false);
  int new_priority = process.GetPriority();
  EXPECT_EQ(old_priority, new_priority);
}

// TODO(estade): if possible, port these 2 tests.
#if defined(OS_WIN)
TEST_F(ProcessUtilTest, EnableLFH) {
  ASSERT_TRUE(EnableLowFragmentationHeap());
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
  ProcessMetrics* metrics =
      ProcessMetrics::CreateProcessMetrics(::GetCurrentProcess());
  ASSERT_TRUE(NULL != metrics);

  // Typical values here is ~1900 for total and ~1000 for largest. Obviously
  // it depends in what other tests have done to this process.
  FreeMBytes free_mem1 = {0};
  EXPECT_TRUE(metrics->CalculateFreeMemory(&free_mem1));
  EXPECT_LT(10u, free_mem1.total);
  EXPECT_LT(10u, free_mem1.largest);
  EXPECT_GT(2048u, free_mem1.total);
  EXPECT_GT(2048u, free_mem1.largest);
  EXPECT_GE(free_mem1.total, free_mem1.largest);
  EXPECT_TRUE(NULL != free_mem1.largest_ptr);

  // Allocate 20M and check again. It should have gone down.
  const int kAllocMB = 20;
  char* alloc = new char[kAllocMB * 1024 * 1024];
  EXPECT_TRUE(NULL != alloc);

  size_t expected_total = free_mem1.total - kAllocMB;
  size_t expected_largest = free_mem1.largest;

  FreeMBytes free_mem2 = {0};
  EXPECT_TRUE(metrics->CalculateFreeMemory(&free_mem2));
  EXPECT_GE(free_mem2.total, free_mem2.largest);
  EXPECT_GE(expected_total, free_mem2.total);
  EXPECT_GE(expected_largest, free_mem2.largest);
  EXPECT_TRUE(NULL != free_mem2.largest_ptr);

  delete[] alloc;
  delete metrics;
}

TEST_F(ProcessUtilTest, GetAppOutput) {
  // Let's create a decently long message.
  std::string message;
  for (int i = 0; i < 1025; i++) {  // 1025 so it does not end on a kilo-byte
                                    // boundary.
    message += "Hello!";
  }

  FilePath python_runtime;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &python_runtime));
  python_runtime = python_runtime.Append(FILE_PATH_LITERAL("third_party"))
                                 .Append(FILE_PATH_LITERAL("python_24"))
                                 .Append(FILE_PATH_LITERAL("python.exe"));

  CommandLine cmd_line(python_runtime);
  cmd_line.AppendLooseValue(L"-c");
  cmd_line.AppendLooseValue(L"\"import sys; sys.stdout.write('" +
      ASCIIToWide(message) + L"');\"");
  std::string output;
  ASSERT_TRUE(base::GetAppOutput(cmd_line, &output));
  EXPECT_EQ(message, output);

  // Let's make sure stderr is ignored.
  CommandLine other_cmd_line(python_runtime);
  other_cmd_line.AppendLooseValue(L"-c");
  other_cmd_line.AppendLooseValue(
      L"\"import sys; sys.stderr.write('Hello!');\"");
  output.clear();
  ASSERT_TRUE(base::GetAppOutput(other_cmd_line, &output));
  EXPECT_EQ("", output);
}
#endif  // defined(OS_WIN)

#if defined(OS_POSIX)
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
  HANDLE_EINTR(close(write_pipe));

  return 0;
}

int ProcessUtilTest::CountOpenFDsInChild() {
  int fds[2];
  if (pipe(fds) < 0)
    NOTREACHED();

  file_handle_mapping_vector fd_mapping_vec;
  fd_mapping_vec.push_back(std::pair<int,int>(fds[1], kChildPipe));
  ProcessHandle handle = this->SpawnChild(L"ProcessUtilsLeakFDChildProcess",
                                          fd_mapping_vec,
                                          false);
  CHECK(handle);
  HANDLE_EINTR(close(fds[1]));

  // Read number of open files in client process from pipe;
  int num_open_files = -1;
  ssize_t bytes_read =
      HANDLE_EINTR(read(fds[0], &num_open_files, sizeof(num_open_files)));
  CHECK(bytes_read == static_cast<ssize_t>(sizeof(num_open_files)));

  CHECK(WaitForSingleProcess(handle, 1000));
  base::CloseProcessHandle(handle);
  HANDLE_EINTR(close(fds[0]));

  return num_open_files;
}

TEST_F(ProcessUtilTest, FDRemapping) {
  int fds_before = CountOpenFDsInChild();

  // open some dummy fds to make sure they don't propogate over to the
  // child process.
  int dev_null = open("/dev/null", O_RDONLY);
  int sockets[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);

  int fds_after = CountOpenFDsInChild();

  ASSERT_EQ(fds_after, fds_before);

  HANDLE_EINTR(close(sockets[0]));
  HANDLE_EINTR(close(sockets[1]));
  HANDLE_EINTR(close(dev_null));
}

TEST_F(ProcessUtilTest, GetAppOutput) {
  std::string output;
  EXPECT_TRUE(GetAppOutput(CommandLine(FilePath("true")), &output));
  EXPECT_STREQ("", output.c_str());

  EXPECT_FALSE(GetAppOutput(CommandLine(FilePath("false")), &output));

  std::vector<std::string> argv;
  argv.push_back("/bin/echo");
  argv.push_back("-n");
  argv.push_back("foobar42");
  EXPECT_TRUE(GetAppOutput(CommandLine(argv), &output));
  EXPECT_STREQ("foobar42", output.c_str());
}

TEST_F(ProcessUtilTest, GetAppOutputRestricted) {
  // Unfortunately, since we can't rely on the path, we need to know where
  // everything is. So let's use /bin/sh, which is on every POSIX system, and
  // its built-ins.
  std::vector<std::string> argv;
  argv.push_back("/bin/sh");  // argv[0]
  argv.push_back("-c");       // argv[1]

  // On success, should set |output|. We use |/bin/sh -c 'exit 0'| instead of
  // |true| since the location of the latter may be |/bin| or |/usr/bin| (and we
  // need absolute paths).
  argv.push_back("exit 0");   // argv[2]; equivalent to "true"
  std::string output = "abc";
  EXPECT_TRUE(GetAppOutputRestricted(CommandLine(argv), &output, 100));
  EXPECT_STREQ("", output.c_str());

  // On failure, should not touch |output|. As above, but for |false|.
  argv[2] = "exit 1";  // equivalent to "false"
  output = "abc";
  EXPECT_FALSE(GetAppOutputRestricted(CommandLine(argv),
                                      &output, 100));
  EXPECT_STREQ("abc", output.c_str());

  // Amount of output exactly equal to space allowed.
  argv[2] = "echo 123456789";  // (the sh built-in doesn't take "-n")
  output.clear();
  EXPECT_TRUE(GetAppOutputRestricted(CommandLine(argv), &output, 10));
  EXPECT_STREQ("123456789\n", output.c_str());

  // Amount of output greater than space allowed.
  output.clear();
  EXPECT_TRUE(GetAppOutputRestricted(CommandLine(argv), &output, 5));
  EXPECT_STREQ("12345", output.c_str());

  // Amount of output less than space allowed.
  output.clear();
  EXPECT_TRUE(GetAppOutputRestricted(CommandLine(argv), &output, 15));
  EXPECT_STREQ("123456789\n", output.c_str());

  // Zero space allowed.
  output = "abc";
  EXPECT_TRUE(GetAppOutputRestricted(CommandLine(argv), &output, 0));
  EXPECT_STREQ("", output.c_str());
}

TEST_F(ProcessUtilTest, GetAppOutputRestrictedNoZombies) {
  std::vector<std::string> argv;
  argv.push_back("/bin/sh");  // argv[0]
  argv.push_back("-c");       // argv[1]
  argv.push_back("echo 123456789012345678901234567890");  // argv[2]

  // Run |GetAppOutputRestricted()| 300 (> default per-user processes on Mac OS
  // 10.5) times with an output buffer big enough to capture all output.
  for (int i = 0; i < 300; i++) {
    std::string output;
    EXPECT_TRUE(GetAppOutputRestricted(CommandLine(argv), &output, 100));
    EXPECT_STREQ("123456789012345678901234567890\n", output.c_str());
  }

  // Ditto, but with an output buffer too small to capture all output.
  for (int i = 0; i < 300; i++) {
    std::string output;
    EXPECT_TRUE(GetAppOutputRestricted(CommandLine(argv), &output, 10));
    EXPECT_STREQ("1234567890", output.c_str());
  }
}

#if defined(OS_LINUX)
TEST_F(ProcessUtilTest, GetParentProcessId) {
  base::ProcessId ppid = GetParentProcessId(GetCurrentProcId());
  EXPECT_EQ(ppid, getppid());
}

TEST_F(ProcessUtilTest, ParseProcStatCPU) {
  // /proc/self/stat for a process running "top".
  const char kTopStat[] = "960 (top) S 16230 960 16230 34818 960 "
      "4202496 471 0 0 0 "
      "12 16 0 0 "  // <- These are the goods.
      "20 0 1 0 121946157 15077376 314 18446744073709551615 4194304 "
      "4246868 140733983044336 18446744073709551615 140244213071219 "
      "0 0 0 138047495 0 0 0 17 1 0 0 0 0 0";
  EXPECT_EQ(12 + 16, ParseProcStatCPU(kTopStat));

  // cat /proc/self/stat on a random other machine I have.
  const char kSelfStat[] = "5364 (cat) R 5354 5364 5354 34819 5364 "
      "0 142 0 0 0 "
      "0 0 0 0 "  // <- No CPU, apparently.
      "16 0 1 0 1676099790 2957312 114 4294967295 134512640 134528148 "
      "3221224832 3221224344 3086339742 0 0 0 0 0 0 0 17 0 0 0";

  EXPECT_EQ(0, ParseProcStatCPU(kSelfStat));
}
#endif

#endif  // defined(OS_POSIX)

// TODO(vandebo) make this work on Windows and Mac too.
#if defined(OS_LINUX)

#if defined(LINUX_USE_TCMALLOC)
extern "C" {
int tc_set_new_mode(int mode);
}
#endif  // defined(LINUX_USE_TCMALLOC)

class OutOfMemoryTest : public testing::Test {
 public:
  OutOfMemoryTest()
      : value_(NULL),
        // Make test size as large as possible minus a few pages so
        // that alignment or other rounding doesn't make it wrap.
        test_size_(std::numeric_limits<std::size_t>::max() - 8192) {
  }

  virtual void SetUp() {
    // Must call EnableTerminationOnOutOfMemory() because that is called from
    // chrome's main function and therefore hasn't been called yet.
    EnableTerminationOnOutOfMemory();
#if defined(LINUX_USE_TCMALLOC)
    tc_set_new_mode(1);
  }

  virtual void TearDown() {
    tc_set_new_mode(0);
#endif  // defined(LINUX_USE_TCMALLOC)
  }

  void* value_;
  size_t test_size_;
};

TEST_F(OutOfMemoryTest, New) {
  ASSERT_DEATH(value_ = new char[test_size_], "");
}

TEST_F(OutOfMemoryTest, Malloc) {
  ASSERT_DEATH(value_ = malloc(test_size_), "");
}

TEST_F(OutOfMemoryTest, Realloc) {
  ASSERT_DEATH(value_ = realloc(NULL, test_size_), "");
}

TEST_F(OutOfMemoryTest, Calloc) {
  ASSERT_DEATH(value_ = calloc(1024, test_size_ / 1024L), "");
}

TEST_F(OutOfMemoryTest, Valloc) {
  ASSERT_DEATH(value_ = valloc(test_size_), "");
}

TEST_F(OutOfMemoryTest, Pvalloc) {
  ASSERT_DEATH(value_ = pvalloc(test_size_), "");
}

TEST_F(OutOfMemoryTest, Memalign) {
  ASSERT_DEATH(value_ = memalign(4, test_size_), "");
}

TEST_F(OutOfMemoryTest, ViaSharedLibraries) {
  // g_try_malloc is documented to return NULL on failure. (g_malloc is the
  // 'safe' default that crashes if allocation fails). However, since we have
  // hopefully overridden malloc, even g_try_malloc should fail. This tests
  // that the run-time symbol resolution is overriding malloc for shared
  // libraries as well as for our code.
  ASSERT_DEATH(value_ = g_try_malloc(test_size_), "");
}


TEST_F(OutOfMemoryTest, Posix_memalign) {
  // Grab the return value of posix_memalign to silence a compiler warning
  // about unused return values. We don't actually care about the return
  // value, since we're asserting death.
  ASSERT_DEATH(EXPECT_EQ(ENOMEM, posix_memalign(&value_, 8, test_size_)), "");
}

#endif  // defined(OS_LINUX)

}  // namespace base
