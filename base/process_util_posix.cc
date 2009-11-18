// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <limits>
#include <set>

#include "base/debug_util.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/sys_info.h"
#include "base/time.h"
#include "base/waitable_event.h"

const int kMicrosecondsPerSecond = 1000000;

namespace base {

namespace {

int WaitpidWithTimeout(ProcessHandle handle, int64 wait_milliseconds,
                       bool* success) {
  // This POSIX version of this function only guarantees that we wait no less
  // than |wait_milliseconds| for the proces to exit.  The child process may
  // exit sometime before the timeout has ended but we may still block for
  // up to 0.25 seconds after the fact.
  //
  // waitpid() has no direct support on POSIX for specifying a timeout, you can
  // either ask it to block indefinitely or return immediately (WNOHANG).
  // When a child process terminates a SIGCHLD signal is sent to the parent.
  // Catching this signal would involve installing a signal handler which may
  // affect other parts of the application and would be difficult to debug.
  //
  // Our strategy is to call waitpid() once up front to check if the process
  // has already exited, otherwise to loop for wait_milliseconds, sleeping for
  // at most 0.25 secs each time using usleep() and then calling waitpid().
  //
  // usleep() is speced to exit if a signal is received for which a handler
  // has been installed.  This means that when a SIGCHLD is sent, it will exit
  // depending on behavior external to this function.
  //
  // This function is used primarily for unit tests, if we want to use it in
  // the application itself it would probably be best to examine other routes.
  int status = -1;
  pid_t ret_pid = HANDLE_EINTR(waitpid(handle, &status, WNOHANG));
  static const int64 kQuarterSecondInMicroseconds = kMicrosecondsPerSecond / 4;

  // If the process hasn't exited yet, then sleep and try again.
  Time wakeup_time = Time::Now() + TimeDelta::FromMilliseconds(
      wait_milliseconds);
  while (ret_pid == 0) {
    Time now = Time::Now();
    if (now > wakeup_time)
      break;
    // Guaranteed to be non-negative!
    int64 sleep_time_usecs = (wakeup_time - now).InMicroseconds();
    // Don't sleep for more than 0.25 secs at a time.
    if (sleep_time_usecs > kQuarterSecondInMicroseconds) {
      sleep_time_usecs = kQuarterSecondInMicroseconds;
    }

    // usleep() will return 0 and set errno to EINTR on receipt of a signal
    // such as SIGCHLD.
    usleep(sleep_time_usecs);
    ret_pid = HANDLE_EINTR(waitpid(handle, &status, WNOHANG));
  }

  if (success)
    *success = (ret_pid != -1);

  return status;
}

void StackDumpSignalHandler(int signal) {
  StackTrace().PrintBacktrace();
  _exit(1);
}

}  // namespace

ProcessId GetCurrentProcId() {
  return getpid();
}

ProcessHandle GetCurrentProcessHandle() {
  return GetCurrentProcId();
}

bool OpenProcessHandle(ProcessId pid, ProcessHandle* handle) {
  // On Posix platforms, process handles are the same as PIDs, so we
  // don't need to do anything.
  *handle = pid;
  return true;
}

bool OpenPrivilegedProcessHandle(ProcessId pid, ProcessHandle* handle) {
  // On POSIX permissions are checked for each operation on process,
  // not when opening a "handle".
  return OpenProcessHandle(pid, handle);
}

void CloseProcessHandle(ProcessHandle process) {
  // See OpenProcessHandle, nothing to do.
  return;
}

ProcessId GetProcId(ProcessHandle process) {
  return process;
}

// Attempts to kill the process identified by the given process
// entry structure.  Ignores specified exit_code; posix can't force that.
// Returns true if this is successful, false otherwise.
bool KillProcess(ProcessHandle process_id, int exit_code, bool wait) {
  DCHECK_GT(process_id, 1) << " tried to kill invalid process_id";
  if (process_id <= 1)
    return false;

  bool result = kill(process_id, SIGTERM) == 0;

  if (result && wait) {
    int tries = 60;
    // The process may not end immediately due to pending I/O
    bool exited = false;
    while (tries-- > 0) {
      pid_t pid = HANDLE_EINTR(waitpid(process_id, NULL, WNOHANG));
      if (pid == process_id) {
        exited = true;
        break;
      }

      sleep(1);
    }

    if (!exited) {
      result = kill(process_id, SIGKILL) == 0;
    }
  }

  if (!result) {
    DLOG(ERROR) << "Unable to terminate process " << process_id << "; error "
                << errno;
  }

  return result;
}

// A class to handle auto-closing of DIR*'s.
class ScopedDIRClose {
 public:
  inline void operator()(DIR* x) const {
    if (x) {
      closedir(x);
    }
  }
};
typedef scoped_ptr_malloc<DIR, ScopedDIRClose> ScopedDIR;

void CloseSuperfluousFds(const base::InjectiveMultimap& saved_mapping) {
#if defined(OS_LINUX)
  static const rlim_t kSystemDefaultMaxFds = 8192;
  static const char fd_dir[] = "/proc/self/fd";
#elif defined(OS_MACOSX)
  static const rlim_t kSystemDefaultMaxFds = 256;
  static const char fd_dir[] = "/dev/fd";
#elif defined(OS_FREEBSD)
  static const rlim_t kSystemDefaultMaxFds = 8192;
  static const char fd_dir[] = "/dev/fd";
#endif
  std::set<int> saved_fds;

  // Get the maximum number of FDs possible.
  struct rlimit nofile;
  rlim_t max_fds;
  if (getrlimit(RLIMIT_NOFILE, &nofile)) {
    // getrlimit failed. Take a best guess.
    max_fds = kSystemDefaultMaxFds;
    DLOG(ERROR) << "getrlimit(RLIMIT_NOFILE) failed: " << errno;
  } else {
    max_fds = nofile.rlim_cur;
  }

  if (max_fds > INT_MAX)
    max_fds = INT_MAX;

  // Don't close stdin, stdout and stderr
  saved_fds.insert(STDIN_FILENO);
  saved_fds.insert(STDOUT_FILENO);
  saved_fds.insert(STDERR_FILENO);

  for (base::InjectiveMultimap::const_iterator
       i = saved_mapping.begin(); i != saved_mapping.end(); ++i) {
    saved_fds.insert(i->dest);
  }

  ScopedDIR dir_closer(opendir(fd_dir));
  DIR *dir = dir_closer.get();
  if (NULL == dir) {
    DLOG(ERROR) << "Unable to open " << fd_dir;

    // Fallback case: Try every possible fd.
    for (rlim_t i = 0; i < max_fds; ++i) {
      const int fd = static_cast<int>(i);
      if (saved_fds.find(fd) != saved_fds.end())
        continue;

      HANDLE_EINTR(close(fd));
    }
    return;
  }
  int dir_fd = dirfd(dir);

  struct dirent *ent;
  while ((ent = readdir(dir))) {
    // Skip . and .. entries.
    if (ent->d_name[0] == '.')
      continue;

    char *endptr;
    errno = 0;
    const long int fd = strtol(ent->d_name, &endptr, 10);
    if (ent->d_name[0] == 0 || *endptr || fd < 0 || errno)
      continue;
    if (saved_fds.find(fd) != saved_fds.end())
      continue;
    if (fd == dir_fd)
      continue;

    // When running under Valgrind, Valgrind opens several FDs for its
    // own use and will complain if we try to close them.  All of
    // these FDs are >= |max_fds|, so we can check against that here
    // before closing.  See https://bugs.kde.org/show_bug.cgi?id=191758
    if (fd < static_cast<int>(max_fds))
      HANDLE_EINTR(close(fd));
  }
}

// Sets all file descriptors to close on exec except for stdin, stdout
// and stderr.
// TODO(agl): Remove this function. It's fundamentally broken for multithreaded
// apps.
void SetAllFDsToCloseOnExec() {
#if defined(OS_LINUX)
  const char fd_dir[] = "/proc/self/fd";
#elif defined(OS_MACOSX) || defined(OS_FREEBSD)
  const char fd_dir[] = "/dev/fd";
#endif
  ScopedDIR dir_closer(opendir(fd_dir));
  DIR *dir = dir_closer.get();
  if (NULL == dir) {
    DLOG(ERROR) << "Unable to open " << fd_dir;
    return;
  }

  struct dirent *ent;
  while ((ent = readdir(dir))) {
    // Skip . and .. entries.
    if (ent->d_name[0] == '.')
      continue;
    int i = atoi(ent->d_name);
    // We don't close stdin, stdout or stderr.
    if (i <= STDERR_FILENO)
      continue;

    int flags = fcntl(i, F_GETFD);
    if ((flags == -1) || (fcntl(i, F_SETFD, flags | FD_CLOEXEC) == -1)) {
      DLOG(ERROR) << "fcntl failure.";
    }
  }
}

bool LaunchApp(const std::vector<std::string>& argv,
               const environment_vector& environ,
               const file_handle_mapping_vector& fds_to_remap,
               bool wait, ProcessHandle* process_handle) {
  pid_t pid = fork();
  if (pid < 0)
    return false;

  if (pid == 0) {
    // Child process
#if defined(OS_MACOSX)
    RestoreDefaultExceptionHandler();
#endif

    InjectiveMultimap fd_shuffle;
    for (file_handle_mapping_vector::const_iterator
        it = fds_to_remap.begin(); it != fds_to_remap.end(); ++it) {
      fd_shuffle.push_back(InjectionArc(it->first, it->second, false));
    }

    for (environment_vector::const_iterator it = environ.begin();
         it != environ.end(); ++it) {
      if (it->first) {
        if (it->second) {
          setenv(it->first, it->second, 1);
        } else {
          unsetenv(it->first);
        }
      }
    }

    // Obscure fork() rule: in the child, if you don't end up doing exec*(),
    // you call _exit() instead of exit(). This is because _exit() does not
    // call any previously-registered (in the parent) exit handlers, which
    // might do things like block waiting for threads that don't even exist
    // in the child.
    if (!ShuffleFileDescriptors(fd_shuffle))
      _exit(127);

    // If we are using the SUID sandbox, it sets a magic environment variable
    // ("SBX_D"), so we remove that variable from the environment here on the
    // off chance that it's already set.
    unsetenv("SBX_D");

    CloseSuperfluousFds(fd_shuffle);

    scoped_array<char*> argv_cstr(new char*[argv.size() + 1]);
    for (size_t i = 0; i < argv.size(); i++)
      argv_cstr[i] = const_cast<char*>(argv[i].c_str());
    argv_cstr[argv.size()] = NULL;
    execvp(argv_cstr[0], argv_cstr.get());
    PLOG(ERROR) << "LaunchApp: execvp(" << argv_cstr[0] << ") failed";
    _exit(127);
  } else {
    // Parent process
    if (wait)
      HANDLE_EINTR(waitpid(pid, 0, 0));

    if (process_handle)
      *process_handle = pid;
  }

  return true;
}

bool LaunchApp(const std::vector<std::string>& argv,
               const file_handle_mapping_vector& fds_to_remap,
               bool wait, ProcessHandle* process_handle) {
  base::environment_vector no_env;
  return LaunchApp(argv, no_env, fds_to_remap, wait, process_handle);
}

bool LaunchApp(const CommandLine& cl,
               bool wait, bool start_hidden,
               ProcessHandle* process_handle) {
  file_handle_mapping_vector no_files;
  return LaunchApp(cl.argv(), no_files, wait, process_handle);
}

ProcessMetrics::ProcessMetrics(ProcessHandle process) : process_(process),
                                                        last_time_(0),
                                                        last_system_time_(0)
#if defined(OS_LINUX)
                                                      , last_cpu_(0)
#endif
{
  processor_count_ = base::SysInfo::NumberOfProcessors();
}

// static
ProcessMetrics* ProcessMetrics::CreateProcessMetrics(ProcessHandle process) {
  return new ProcessMetrics(process);
}

ProcessMetrics::~ProcessMetrics() { }

void EnableTerminationOnHeapCorruption() {
  // On POSIX, there nothing to do AFAIK.
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

  // TODO(phajdan.jr): Catch other crashy signals, like SIGABRT.
  success &= (signal(SIGSEGV, &StackDumpSignalHandler) != SIG_ERR);
  success &= (signal(SIGILL, &StackDumpSignalHandler) != SIG_ERR);
  success &= (signal(SIGBUS, &StackDumpSignalHandler) != SIG_ERR);
  success &= (signal(SIGFPE, &StackDumpSignalHandler) != SIG_ERR);
  return success;
}

void AttachToConsole() {
  // On POSIX, there nothing to do AFAIK. Maybe create a new console if none
  // exist?
}

void RaiseProcessToHighPriority() {
  // On POSIX, we don't actually do anything here.  We could try to nice() or
  // setpriority() or sched_getscheduler, but these all require extra rights.
}

bool DidProcessCrash(bool* child_exited, ProcessHandle handle) {
  int status;
  const pid_t result = HANDLE_EINTR(waitpid(handle, &status, WNOHANG));
  if (result == -1) {
    PLOG(ERROR) << "waitpid(" << handle << ")";
    if (child_exited)
      *child_exited = false;
    return false;
  } else if (result == 0) {
    // the child hasn't exited yet.
    if (child_exited)
      *child_exited = false;
    return false;
  }

  if (child_exited)
    *child_exited = true;

  if (WIFSIGNALED(status)) {
    switch(WTERMSIG(status)) {
      case SIGSEGV:
      case SIGILL:
      case SIGABRT:
      case SIGFPE:
        return true;
      default:
        return false;
    }
  }

  if (WIFEXITED(status))
    return WEXITSTATUS(status) != 0;

  return false;
}

bool WaitForExitCode(ProcessHandle handle, int* exit_code) {
  int status;
  if (HANDLE_EINTR(waitpid(handle, &status, 0)) == -1) {
    NOTREACHED();
    return false;
  }

  if (WIFEXITED(status)) {
    *exit_code = WEXITSTATUS(status);
    return true;
  }

  // If it didn't exit cleanly, it must have been signaled.
  DCHECK(WIFSIGNALED(status));
  return false;
}

bool WaitForSingleProcess(ProcessHandle handle, int64 wait_milliseconds) {
  bool waitpid_success;
  int status;
  if (wait_milliseconds == base::kNoTimeout)
    waitpid_success = (HANDLE_EINTR(waitpid(handle, &status, 0)) != -1);
  else
    status = WaitpidWithTimeout(handle, wait_milliseconds, &waitpid_success);
  if (status != -1) {
    DCHECK(waitpid_success);
    return WIFEXITED(status);
  } else {
    return false;
  }
}

bool CrashAwareSleep(ProcessHandle handle, int64 wait_milliseconds) {
  bool waitpid_success;
  int status = WaitpidWithTimeout(handle, wait_milliseconds, &waitpid_success);
  if (status != -1) {
    DCHECK(waitpid_success);
    return !(WIFEXITED(status) || WIFSIGNALED(status));
  } else {
    // If waitpid returned with an error, then the process doesn't exist
    // (which most probably means it didn't exist before our call).
    return waitpid_success;
  }
}

int64 TimeValToMicroseconds(const struct timeval& tv) {
  return tv.tv_sec * kMicrosecondsPerSecond + tv.tv_usec;
}

#if defined(OS_MACOSX)
// TODO(port): this function only returns the *current* CPU's usage;
// we want to return this->process_'s CPU usage.
int ProcessMetrics::GetCPUUsage() {
  struct timeval now;
  struct rusage usage;

  int retval = gettimeofday(&now, NULL);
  if (retval)
    return 0;
  retval = getrusage(RUSAGE_SELF, &usage);
  if (retval)
    return 0;

  int64 system_time = (TimeValToMicroseconds(usage.ru_stime) +
                       TimeValToMicroseconds(usage.ru_utime)) /
                        processor_count_;
  int64 time = TimeValToMicroseconds(now);

  if ((last_system_time_ == 0) || (last_time_ == 0)) {
    // First call, just set the last values.
    last_system_time_ = system_time;
    last_time_ = time;
    return 0;
  }

  int64 system_time_delta = system_time - last_system_time_;
  int64 time_delta = time - last_time_;
  DCHECK(time_delta != 0);
  if (time_delta == 0)
    return 0;

  // We add time_delta / 2 so the result is rounded.
  int cpu = static_cast<int>((system_time_delta * 100 + time_delta / 2) /
                             time_delta);

  last_system_time_ = system_time;
  last_time_ = time;

  return cpu;
}
#endif

// Executes the application specified by |cl| and wait for it to exit. Stores
// the output (stdout) in |output|. If |do_search_path| is set, it searches the
// path for the application; in that case, |envp| must be null, and it will use
// the current environment. If |do_search_path| is false, |cl| should fully
// specify the path of the application, and |envp| will be used as the
// environment. Redirects stderr to /dev/null. Returns true on success
// (application launched and exited cleanly, with exit code indicating success).
// |output| is modified only when the function finished successfully.
static bool GetAppOutputInternal(const CommandLine& cl, char* const envp[],
                                 std::string* output, size_t max_output,
                                 bool do_search_path) {
  int pipe_fd[2];
  pid_t pid;

  // Either |do_search_path| should be false or |envp| should be null, but not
  // both.
  DCHECK(!do_search_path ^ !envp);

  if (pipe(pipe_fd) < 0)
    return false;

  switch (pid = fork()) {
    case -1:  // error
      close(pipe_fd[0]);
      close(pipe_fd[1]);
      return false;
    case 0:  // child
      {
#if defined(OS_MACOSX)
        RestoreDefaultExceptionHandler();
#endif

        // Obscure fork() rule: in the child, if you don't end up doing exec*(),
        // you call _exit() instead of exit(). This is because _exit() does not
        // call any previously-registered (in the parent) exit handlers, which
        // might do things like block waiting for threads that don't even exist
        // in the child.
        int dev_null = open("/dev/null", O_WRONLY);
        if (dev_null < 0)
          _exit(127);

        InjectiveMultimap fd_shuffle;
        fd_shuffle.push_back(InjectionArc(pipe_fd[1], STDOUT_FILENO, true));
        fd_shuffle.push_back(InjectionArc(dev_null, STDERR_FILENO, true));
        fd_shuffle.push_back(InjectionArc(dev_null, STDIN_FILENO, true));

        if (!ShuffleFileDescriptors(fd_shuffle))
          _exit(127);

        CloseSuperfluousFds(fd_shuffle);

        const std::vector<std::string> argv = cl.argv();
        scoped_array<char*> argv_cstr(new char*[argv.size() + 1]);
        for (size_t i = 0; i < argv.size(); i++)
          argv_cstr[i] = const_cast<char*>(argv[i].c_str());
        argv_cstr[argv.size()] = NULL;
        if (do_search_path)
          execvp(argv_cstr[0], argv_cstr.get());
        else
          execve(argv_cstr[0], argv_cstr.get(), envp);
        _exit(127);
      }
    default:  // parent
      {
        // Close our writing end of pipe now. Otherwise later read would not
        // be able to detect end of child's output (in theory we could still
        // write to the pipe).
        close(pipe_fd[1]);

        char buffer[256];
        std::string buf_output;
        size_t output_left = max_output;
        ssize_t bytes_read = 1;  // A lie to properly handle |max_output == 0|
                                 // case in the logic below.

        while (output_left > 0) {
          bytes_read = HANDLE_EINTR(read(pipe_fd[0], buffer,
                                    std::min(output_left, sizeof(buffer))));
          if (bytes_read <= 0)
            break;
          buf_output.append(buffer, bytes_read);
          output_left -= static_cast<size_t>(bytes_read);
        }
        close(pipe_fd[0]);

        // If we stopped because we read as much as we wanted, we always declare
        // success (because the child may exit due to |SIGPIPE|).
        if (output_left || bytes_read <= 0) {
          int exit_code = EXIT_FAILURE;
          bool success = WaitForExitCode(pid, &exit_code);
          if (!success || exit_code != EXIT_SUCCESS)
            return false;
        }

        output->swap(buf_output);
        return true;
      }
  }
}

bool GetAppOutput(const CommandLine& cl, std::string* output) {
  // Run |execve()| with the current environment and store "unlimited" data.
  return GetAppOutputInternal(cl, NULL, output,
                              std::numeric_limits<std::size_t>::max(), true);
}

// TODO(viettrungluu): Conceivably, we should have a timeout as well, so we
// don't hang if what we're calling hangs.
bool GetAppOutputRestricted(const CommandLine& cl,
                            std::string* output, size_t max_output) {
  // Run |execve()| with the empty environment.
  char* const empty_environ = NULL;
  return GetAppOutputInternal(cl, &empty_environ, output, max_output, false);
}

int GetProcessCount(const std::wstring& executable_name,
                    const ProcessFilter* filter) {
  int count = 0;

  NamedProcessIterator iter(executable_name, filter);
  while (iter.NextProcessEntry())
    ++count;
  return count;
}

bool KillProcesses(const std::wstring& executable_name, int exit_code,
                   const ProcessFilter* filter) {
  bool result = true;
  const ProcessEntry* entry;

  NamedProcessIterator iter(executable_name, filter);
  while ((entry = iter.NextProcessEntry()) != NULL)
    result = KillProcess((*entry).pid, exit_code, true) && result;

  return result;
}

bool WaitForProcessesToExit(const std::wstring& executable_name,
                            int64 wait_milliseconds,
                            const ProcessFilter* filter) {
  bool result = false;

  // TODO(port): This is inefficient, but works if there are multiple procs.
  // TODO(port): use waitpid to avoid leaving zombies around

  base::Time end_time = base::Time::Now() +
      base::TimeDelta::FromMilliseconds(wait_milliseconds);
  do {
    NamedProcessIterator iter(executable_name, filter);
    if (!iter.NextProcessEntry()) {
      result = true;
      break;
    }
    PlatformThread::Sleep(100);
  } while ((base::Time::Now() - end_time) > base::TimeDelta());

  return result;
}

bool CleanupProcesses(const std::wstring& executable_name,
                      int64 wait_milliseconds,
                      int exit_code,
                      const ProcessFilter* filter) {
  bool exited_cleanly =
      WaitForProcessesToExit(executable_name, wait_milliseconds,
                             filter);
  if (!exited_cleanly)
    KillProcesses(executable_name, exit_code, filter);
  return exited_cleanly;
}

}  // namespace base
