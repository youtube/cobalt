// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/stack_trace.h"
#include "base/dir_reader_posix.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"

#if defined(OS_MACOSX)
#include <crt_externs.h>
#include <sys/event.h>
#define environ (*_NSGetEnviron())
#else
extern char** environ;
#endif

namespace base {

namespace {

int WaitpidWithTimeout(ProcessHandle handle, int64 wait_milliseconds,
                       bool* success) {
  // This POSIX version of this function only guarantees that we wait no less
  // than |wait_milliseconds| for the process to exit.  The child process may
  // exit sometime before the timeout has ended but we may still block for up
  // to 256 milliseconds after the fact.
  //
  // waitpid() has no direct support on POSIX for specifying a timeout, you can
  // either ask it to block indefinitely or return immediately (WNOHANG).
  // When a child process terminates a SIGCHLD signal is sent to the parent.
  // Catching this signal would involve installing a signal handler which may
  // affect other parts of the application and would be difficult to debug.
  //
  // Our strategy is to call waitpid() once up front to check if the process
  // has already exited, otherwise to loop for wait_milliseconds, sleeping for
  // at most 256 milliseconds each time using usleep() and then calling
  // waitpid().  The amount of time we sleep starts out at 1 milliseconds, and
  // we double it every 4 sleep cycles.
  //
  // usleep() is speced to exit if a signal is received for which a handler
  // has been installed.  This means that when a SIGCHLD is sent, it will exit
  // depending on behavior external to this function.
  //
  // This function is used primarily for unit tests, if we want to use it in
  // the application itself it would probably be best to examine other routes.
  int status = -1;
  pid_t ret_pid = HANDLE_EINTR(waitpid(handle, &status, WNOHANG));
  static const int64 kMaxSleepInMicroseconds = 1 << 18;  // ~256 milliseconds.
  int64 max_sleep_time_usecs = 1 << 10;  // ~1 milliseconds.
  int64 double_sleep_time = 0;

  // If the process hasn't exited yet, then sleep and try again.
  Time wakeup_time = Time::Now() +
      TimeDelta::FromMilliseconds(wait_milliseconds);
  while (ret_pid == 0) {
    Time now = Time::Now();
    if (now > wakeup_time)
      break;
    // Guaranteed to be non-negative!
    int64 sleep_time_usecs = (wakeup_time - now).InMicroseconds();
    // Sleep for a bit while we wait for the process to finish.
    if (sleep_time_usecs > max_sleep_time_usecs)
      sleep_time_usecs = max_sleep_time_usecs;

    // usleep() will return 0 and set errno to EINTR on receipt of a signal
    // such as SIGCHLD.
    usleep(sleep_time_usecs);
    ret_pid = HANDLE_EINTR(waitpid(handle, &status, WNOHANG));

    if ((max_sleep_time_usecs < kMaxSleepInMicroseconds) &&
        (double_sleep_time++ % 4 == 0)) {
      max_sleep_time_usecs *= 2;
    }
  }

  if (success)
    *success = (ret_pid != -1);

  return status;
}

void StackDumpSignalHandler(int signal, siginfo_t* info, ucontext_t* context) {
  LOG(ERROR) << "Received signal " << signal;
  debug::StackTrace().PrintBacktrace();

  // TODO(shess): Port to Linux.
#if defined(OS_MACOSX)
  // TODO(shess): Port to 64-bit.
#if ARCH_CPU_32_BITS
  char buf[1024];
  size_t len;

  // NOTE: Even |snprintf()| is not on the approved list for signal
  // handlers, but buffered I/O is definitely not on the list due to
  // potential for |malloc()|.
  len = static_cast<size_t>(
      snprintf(buf, sizeof(buf),
               "ax: %x, bx: %x, cx: %x, dx: %x\n",
               context->uc_mcontext->__ss.__eax,
               context->uc_mcontext->__ss.__ebx,
               context->uc_mcontext->__ss.__ecx,
               context->uc_mcontext->__ss.__edx));
  write(STDERR_FILENO, buf, std::min(len, sizeof(buf) - 1));

  len = static_cast<size_t>(
      snprintf(buf, sizeof(buf),
               "di: %x, si: %x, bp: %x, sp: %x, ss: %x, flags: %x\n",
               context->uc_mcontext->__ss.__edi,
               context->uc_mcontext->__ss.__esi,
               context->uc_mcontext->__ss.__ebp,
               context->uc_mcontext->__ss.__esp,
               context->uc_mcontext->__ss.__ss,
               context->uc_mcontext->__ss.__eflags));
  write(STDERR_FILENO, buf, std::min(len, sizeof(buf) - 1));

  len = static_cast<size_t>(
      snprintf(buf, sizeof(buf),
               "ip: %x, cs: %x, ds: %x, es: %x, fs: %x, gs: %x\n",
               context->uc_mcontext->__ss.__eip,
               context->uc_mcontext->__ss.__cs,
               context->uc_mcontext->__ss.__ds,
               context->uc_mcontext->__ss.__es,
               context->uc_mcontext->__ss.__fs,
               context->uc_mcontext->__ss.__gs));
  write(STDERR_FILENO, buf, std::min(len, sizeof(buf) - 1));
#endif  // ARCH_CPU_32_BITS
#endif  // defined(OS_MACOSX)
  _exit(1);
}

void ResetChildSignalHandlersToDefaults() {
  // The previous signal handlers are likely to be meaningless in the child's
  // context so we reset them to the defaults for now. http://crbug.com/44953
  // These signal handlers are setup in browser_main.cc:BrowserMain
  signal(SIGTERM, SIG_DFL);
  signal(SIGHUP, SIG_DFL);
  signal(SIGINT, SIG_DFL);
}

}  // anonymous namespace

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

bool OpenProcessHandleWithAccess(ProcessId pid,
                                 uint32 access_flags,
                                 ProcessHandle* handle) {
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
  static unsigned kMaxSleepMs = 1000;
  unsigned sleep_ms = 4;

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
      if (pid == -1) {
        if (errno == ECHILD) {
          // The wait may fail with ECHILD if another process also waited for
          // the same pid, causing the process state to get cleaned up.
          exited = true;
          break;
        }
        DPLOG(ERROR) << "Error waiting for process " << process_id;
      }

      usleep(sleep_ms * 1000);
      if (sleep_ms < kMaxSleepMs)
        sleep_ms *= 2;
    }

    // If we're waiting and the child hasn't died by now, force it
    // with a SIGKILL.
    if (!exited)
      result = kill(process_id, SIGKILL) == 0;
  }

  if (!result)
    DPLOG(ERROR) << "Unable to terminate process " << process_id;

  return result;
}

bool KillProcessGroup(ProcessHandle process_group_id) {
  bool result = kill(-1 * process_group_id, SIGKILL) == 0;
  if (!result)
    PLOG(ERROR) << "Unable to terminate process group " << process_group_id;
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

#if defined(OS_LINUX)
  static const rlim_t kSystemDefaultMaxFds = 8192;
  static const char kFDDir[] = "/proc/self/fd";
#elif defined(OS_MACOSX)
  static const rlim_t kSystemDefaultMaxFds = 256;
  static const char kFDDir[] = "/dev/fd";
#elif defined(OS_SOLARIS)
  static const rlim_t kSystemDefaultMaxFds = 8192;
  static const char kFDDir[] = "/dev/fd";
#elif defined(OS_FREEBSD)
  static const rlim_t kSystemDefaultMaxFds = 8192;
  static const char kFDDir[] = "/dev/fd";
#elif defined(OS_OPENBSD)
  static const rlim_t kSystemDefaultMaxFds = 256;
  static const char kFDDir[] = "/dev/fd";
#endif

void CloseSuperfluousFds(const base::InjectiveMultimap& saved_mapping) {
  // DANGER: no calls to malloc are allowed from now on:
  // http://crbug.com/36678

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

  DirReaderPosix fd_dir(kFDDir);

  if (!fd_dir.IsValid()) {
    // Fallback case: Try every possible fd.
    for (rlim_t i = 0; i < max_fds; ++i) {
      const int fd = static_cast<int>(i);
      if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
        continue;
      InjectiveMultimap::const_iterator j;
      for (j = saved_mapping.begin(); j != saved_mapping.end(); j++) {
        if (fd == j->dest)
          break;
      }
      if (j != saved_mapping.end())
        continue;

      // Since we're just trying to close anything we can find,
      // ignore any error return values of close().
      int unused ALLOW_UNUSED = HANDLE_EINTR(close(fd));
    }
    return;
  }

  const int dir_fd = fd_dir.fd();

  for ( ; fd_dir.Next(); ) {
    // Skip . and .. entries.
    if (fd_dir.name()[0] == '.')
      continue;

    char *endptr;
    errno = 0;
    const long int fd = strtol(fd_dir.name(), &endptr, 10);
    if (fd_dir.name()[0] == 0 || *endptr || fd < 0 || errno)
      continue;
    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
      continue;
    InjectiveMultimap::const_iterator i;
    for (i = saved_mapping.begin(); i != saved_mapping.end(); i++) {
      if (fd == i->dest)
        break;
    }
    if (i != saved_mapping.end())
      continue;
    if (fd == dir_fd)
      continue;

    // When running under Valgrind, Valgrind opens several FDs for its
    // own use and will complain if we try to close them.  All of
    // these FDs are >= |max_fds|, so we can check against that here
    // before closing.  See https://bugs.kde.org/show_bug.cgi?id=191758
    if (fd < static_cast<int>(max_fds)) {
      int ret = HANDLE_EINTR(close(fd));
      DPCHECK(ret == 0);
    }
  }
}

char** AlterEnvironment(const environment_vector& changes,
                        const char* const* const env) {
  unsigned count = 0;
  unsigned size = 0;

  // First assume that all of the current environment will be included.
  for (unsigned i = 0; env[i]; i++) {
    const char *const pair = env[i];
    count++;
    size += strlen(pair) + 1 /* terminating NUL */;
  }

  for (environment_vector::const_iterator
       j = changes.begin(); j != changes.end(); j++) {
    bool found = false;
    const char *pair;

    for (unsigned i = 0; env[i]; i++) {
      pair = env[i];
      const char *const equals = strchr(pair, '=');
      if (!equals)
        continue;
      const unsigned keylen = equals - pair;
      if (keylen == j->first.size() &&
          memcmp(pair, j->first.data(), keylen) == 0) {
        found = true;
        break;
      }
    }

    // if found, we'll either be deleting or replacing this element.
    if (found) {
      count--;
      size -= strlen(pair) + 1;
      if (j->second.size())
        found = false;
    }

    // if !found, then we have a new element to add.
    if (!found && j->second.size() > 0) {
      count++;
      size += j->first.size() + 1 /* '=' */ + j->second.size() + 1 /* NUL */;
    }
  }

  count++;  // for the final NULL
  uint8_t *buffer = new uint8_t[sizeof(char*) * count + size];
  char **const ret = reinterpret_cast<char**>(buffer);
  unsigned k = 0;
  char *scratch = reinterpret_cast<char*>(buffer + sizeof(char*) * count);

  for (unsigned i = 0; env[i]; i++) {
    const char *const pair = env[i];
    const char *const equals = strchr(pair, '=');
    if (!equals) {
      const unsigned len = strlen(pair);
      ret[k++] = scratch;
      memcpy(scratch, pair, len + 1);
      scratch += len + 1;
      continue;
    }
    const unsigned keylen = equals - pair;
    bool handled = false;
    for (environment_vector::const_iterator
         j = changes.begin(); j != changes.end(); j++) {
      if (j->first.size() == keylen &&
          memcmp(j->first.data(), pair, keylen) == 0) {
        if (!j->second.empty()) {
          ret[k++] = scratch;
          memcpy(scratch, pair, keylen + 1);
          scratch += keylen + 1;
          memcpy(scratch, j->second.c_str(), j->second.size() + 1);
          scratch += j->second.size() + 1;
        }
        handled = true;
        break;
      }
    }

    if (!handled) {
      const unsigned len = strlen(pair);
      ret[k++] = scratch;
      memcpy(scratch, pair, len + 1);
      scratch += len + 1;
    }
  }

  // Now handle new elements
  for (environment_vector::const_iterator
       j = changes.begin(); j != changes.end(); j++) {
    if (j->second.empty())
      continue;

    bool found = false;
    for (unsigned i = 0; env[i]; i++) {
      const char *const pair = env[i];
      const char *const equals = strchr(pair, '=');
      if (!equals)
        continue;
      const unsigned keylen = equals - pair;
      if (keylen == j->first.size() &&
          memcmp(pair, j->first.data(), keylen) == 0) {
        found = true;
        break;
      }
    }

    if (!found) {
      ret[k++] = scratch;
      memcpy(scratch, j->first.data(), j->first.size());
      scratch += j->first.size();
      *scratch++ = '=';
      memcpy(scratch, j->second.c_str(), j->second.size() + 1);
      scratch += j->second.size() + 1;
     }
  }

  ret[k] = NULL;
  return ret;
}

bool LaunchAppImpl(
    const std::vector<std::string>& argv,
    const environment_vector& env_changes,
    const file_handle_mapping_vector& fds_to_remap,
    bool wait,
    ProcessHandle* process_handle,
    bool start_new_process_group) {
  pid_t pid;
  InjectiveMultimap fd_shuffle1, fd_shuffle2;
  fd_shuffle1.reserve(fds_to_remap.size());
  fd_shuffle2.reserve(fds_to_remap.size());
  scoped_array<char*> argv_cstr(new char*[argv.size() + 1]);
  scoped_array<char*> new_environ(AlterEnvironment(env_changes, environ));

  pid = fork();
  if (pid < 0) {
    PLOG(ERROR) << "fork";
    return false;
  }
  if (pid == 0) {
    // Child process

    if (start_new_process_group) {
      // Instead of inheriting the process group ID of the parent, the child
      // starts off a new process group with pgid equal to its process ID.
      if (setpgid(0, 0) < 0) {
        PLOG(ERROR) << "setpgid";
        return false;
      }
    }
#if defined(OS_MACOSX)
    RestoreDefaultExceptionHandler();
#endif

    ResetChildSignalHandlersToDefaults();

#if 0
    // When debugging it can be helpful to check that we really aren't making
    // any hidden calls to malloc.
    void *malloc_thunk =
        reinterpret_cast<void*>(reinterpret_cast<intptr_t>(malloc) & ~4095);
    mprotect(malloc_thunk, 4096, PROT_READ | PROT_WRITE | PROT_EXEC);
    memset(reinterpret_cast<void*>(malloc), 0xff, 8);
#endif

    // DANGER: no calls to malloc are allowed from now on:
    // http://crbug.com/36678

    for (file_handle_mapping_vector::const_iterator
        it = fds_to_remap.begin(); it != fds_to_remap.end(); ++it) {
      fd_shuffle1.push_back(InjectionArc(it->first, it->second, false));
      fd_shuffle2.push_back(InjectionArc(it->first, it->second, false));
    }

    environ = new_environ.get();

    // Obscure fork() rule: in the child, if you don't end up doing exec*(),
    // you call _exit() instead of exit(). This is because _exit() does not
    // call any previously-registered (in the parent) exit handlers, which
    // might do things like block waiting for threads that don't even exist
    // in the child.

    // fd_shuffle1 is mutated by this call because it cannot malloc.
    if (!ShuffleFileDescriptors(&fd_shuffle1))
      _exit(127);

    CloseSuperfluousFds(fd_shuffle2);

    for (size_t i = 0; i < argv.size(); i++)
      argv_cstr[i] = const_cast<char*>(argv[i].c_str());
    argv_cstr[argv.size()] = NULL;
    execvp(argv_cstr[0], argv_cstr.get());
    RAW_LOG(ERROR, "LaunchApp: failed to execvp:");
    RAW_LOG(ERROR, argv_cstr[0]);
    _exit(127);
  } else {
    // Parent process
    if (wait) {
      // While this isn't strictly disk IO, waiting for another process to
      // finish is the sort of thing ThreadRestrictions is trying to prevent.
      base::ThreadRestrictions::AssertIOAllowed();
      pid_t ret = HANDLE_EINTR(waitpid(pid, 0, 0));
      DPCHECK(ret > 0);
    }

    if (process_handle)
      *process_handle = pid;
  }

  return true;
}

bool LaunchApp(
    const std::vector<std::string>& argv,
    const environment_vector& env_changes,
    const file_handle_mapping_vector& fds_to_remap,
    bool wait,
    ProcessHandle* process_handle) {
  return LaunchAppImpl(argv, env_changes, fds_to_remap,
                       wait, process_handle, false);
}

bool LaunchAppInNewProcessGroup(
    const std::vector<std::string>& argv,
    const environment_vector& env_changes,
    const file_handle_mapping_vector& fds_to_remap,
    bool wait,
    ProcessHandle* process_handle) {
  return LaunchAppImpl(argv, env_changes, fds_to_remap, wait,
                       process_handle, true);
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

  sig_t handler = reinterpret_cast<sig_t>(&StackDumpSignalHandler);
  success &= (signal(SIGILL, handler) != SIG_ERR);
  success &= (signal(SIGABRT, handler) != SIG_ERR);
  success &= (signal(SIGFPE, handler) != SIG_ERR);
  success &= (signal(SIGBUS, handler) != SIG_ERR);
  success &= (signal(SIGSEGV, handler) != SIG_ERR);
  success &= (signal(SIGSYS, handler) != SIG_ERR);

  return success;
}

void RaiseProcessToHighPriority() {
  // On POSIX, we don't actually do anything here.  We could try to nice() or
  // setpriority() or sched_getscheduler, but these all require extra rights.
}

TerminationStatus GetTerminationStatus(ProcessHandle handle, int* exit_code) {
  int status = 0;
  const pid_t result = HANDLE_EINTR(waitpid(handle, &status, WNOHANG));
  if (result == -1) {
    PLOG(ERROR) << "waitpid(" << handle << ")";
    if (exit_code)
      *exit_code = 0;
    return TERMINATION_STATUS_NORMAL_TERMINATION;
  } else if (result == 0) {
    // the child hasn't exited yet.
    if (exit_code)
      *exit_code = 0;
    return TERMINATION_STATUS_STILL_RUNNING;
  }

  if (exit_code)
    *exit_code = status;

  if (WIFSIGNALED(status)) {
    switch (WTERMSIG(status)) {
      case SIGABRT:
      case SIGBUS:
      case SIGFPE:
      case SIGILL:
      case SIGSEGV:
        return TERMINATION_STATUS_PROCESS_CRASHED;
      case SIGINT:
      case SIGKILL:
      case SIGTERM:
        return TERMINATION_STATUS_PROCESS_WAS_KILLED;
      default:
        break;
    }
  }

  if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
    return TERMINATION_STATUS_ABNORMAL_TERMINATION;

  return TERMINATION_STATUS_NORMAL_TERMINATION;
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

bool WaitForExitCodeWithTimeout(ProcessHandle handle, int* exit_code,
                                int64 timeout_milliseconds) {
  bool waitpid_success = false;
  int status = WaitpidWithTimeout(handle, timeout_milliseconds,
                                  &waitpid_success);
  if (status == -1)
    return false;
  if (!waitpid_success)
    return false;
  if (!WIFEXITED(status))
    return false;
  if (WIFSIGNALED(status)) {
    *exit_code = -1;
    return true;
  }
  *exit_code = WEXITSTATUS(status);
  return true;
}

#if defined(OS_MACOSX)
// Using kqueue on Mac so that we can wait on non-child processes.
// We can't use kqueues on child processes because we need to reap
// our own children using wait.
static bool WaitForSingleNonChildProcess(ProcessHandle handle,
                                         int64 wait_milliseconds) {
  int kq = kqueue();
  if (kq == -1) {
    PLOG(ERROR) << "kqueue";
    return false;
  }

  struct kevent change = { 0 };
  EV_SET(&change, handle, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);

  struct timespec spec;
  struct timespec *spec_ptr;
  if (wait_milliseconds != base::kNoTimeout) {
    time_t sec = static_cast<time_t>(wait_milliseconds / 1000);
    wait_milliseconds = wait_milliseconds - (sec * 1000);
    spec.tv_sec = sec;
    spec.tv_nsec = wait_milliseconds * 1000000L;
    spec_ptr = &spec;
  } else {
    spec_ptr = NULL;
  }

  while(true) {
    struct kevent event = { 0 };
    int event_count = HANDLE_EINTR(kevent(kq, &change, 1, &event, 1, spec_ptr));
    if (close(kq) != 0) {
      PLOG(ERROR) << "close";
    }
    if (event_count < 0) {
      PLOG(ERROR) << "kevent";
      return false;
    } else if (event_count == 0) {
      if (wait_milliseconds != base::kNoTimeout) {
        // Timed out.
        return false;
      }
    } else if ((event_count == 1) &&
               (handle == static_cast<pid_t>(event.ident)) &&
               (event.filter == EVFILT_PROC)) {
      if (event.fflags == NOTE_EXIT) {
        return true;
      } else if (event.flags == EV_ERROR) {
        LOG(ERROR) << "kevent error " << event.data;
        return false;
      } else {
        NOTREACHED();
        return false;
      }
    } else {
      NOTREACHED();
      return false;
    }
  }
}
#endif  // OS_MACOSX

bool WaitForSingleProcess(ProcessHandle handle, int64 wait_milliseconds) {
  ProcessHandle parent_pid = GetParentProcessId(handle);
  ProcessHandle our_pid = Process::Current().handle();
  if (parent_pid != our_pid) {
#if defined(OS_MACOSX)
    // On Mac we can wait on non child processes.
    return WaitForSingleNonChildProcess(handle, wait_milliseconds);
#else
    // Currently on Linux we can't handle non child processes.
    NOTIMPLEMENTED();
#endif  // OS_MACOSX
  }
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
  static const int kMicrosecondsPerSecond = 1000000;
  int64 ret = tv.tv_sec;  // Avoid (int * int) integer overflow.
  ret *= kMicrosecondsPerSecond;
  ret += tv.tv_usec;
  return ret;
}

// Executes the application specified by |cl| and wait for it to exit. Stores
// the output (stdout) in |output|. If |do_search_path| is set, it searches the
// path for the application; in that case, |envp| must be null, and it will use
// the current environment. If |do_search_path| is false, |cl| should fully
// specify the path of the application, and |envp| will be used as the
// environment. Redirects stderr to /dev/null. Returns true on success
// (application launched and exited cleanly, with exit code indicating success).
static bool GetAppOutputInternal(const CommandLine& cl, char* const envp[],
                                 std::string* output, size_t max_output,
                                 bool do_search_path) {
  // Doing a blocking wait for another command to finish counts as IO.
  base::ThreadRestrictions::AssertIOAllowed();

  int pipe_fd[2];
  pid_t pid;
  InjectiveMultimap fd_shuffle1, fd_shuffle2;
  const std::vector<std::string>& argv = cl.argv();
  scoped_array<char*> argv_cstr(new char*[argv.size() + 1]);

  fd_shuffle1.reserve(3);
  fd_shuffle2.reserve(3);

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
        // DANGER: no calls to malloc are allowed from now on:
        // http://crbug.com/36678

        // Obscure fork() rule: in the child, if you don't end up doing exec*(),
        // you call _exit() instead of exit(). This is because _exit() does not
        // call any previously-registered (in the parent) exit handlers, which
        // might do things like block waiting for threads that don't even exist
        // in the child.
        int dev_null = open("/dev/null", O_WRONLY);
        if (dev_null < 0)
          _exit(127);

        fd_shuffle1.push_back(InjectionArc(pipe_fd[1], STDOUT_FILENO, true));
        fd_shuffle1.push_back(InjectionArc(dev_null, STDERR_FILENO, true));
        fd_shuffle1.push_back(InjectionArc(dev_null, STDIN_FILENO, true));
        // Adding another element here? Remeber to increase the argument to
        // reserve(), above.

        std::copy(fd_shuffle1.begin(), fd_shuffle1.end(),
                  std::back_inserter(fd_shuffle2));

        if (!ShuffleFileDescriptors(&fd_shuffle1))
          _exit(127);

        CloseSuperfluousFds(fd_shuffle2);

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

        output->clear();
        char buffer[256];
        size_t output_buf_left = max_output;
        ssize_t bytes_read = 1;  // A lie to properly handle |max_output == 0|
                                 // case in the logic below.

        while (output_buf_left > 0) {
          bytes_read = HANDLE_EINTR(read(pipe_fd[0], buffer,
                                    std::min(output_buf_left, sizeof(buffer))));
          if (bytes_read <= 0)
            break;
          output->append(buffer, bytes_read);
          output_buf_left -= static_cast<size_t>(bytes_read);
        }
        close(pipe_fd[0]);

        // Always wait for exit code (even if we know we'll declare success).
        int exit_code = EXIT_FAILURE;
        bool success = WaitForExitCode(pid, &exit_code);

        // If we stopped because we read as much as we wanted, we always declare
        // success (because the child may exit due to |SIGPIPE|).
        if (output_buf_left || bytes_read <= 0) {
          if (!success || exit_code != EXIT_SUCCESS)
            return false;
        }

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

bool WaitForProcessesToExit(const FilePath::StringType& executable_name,
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
    base::PlatformThread::Sleep(100);
  } while ((base::Time::Now() - end_time) > base::TimeDelta());

  return result;
}

bool CleanupProcesses(const FilePath::StringType& executable_name,
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
