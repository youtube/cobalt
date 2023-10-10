// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#include <limits.h>

#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

#if defined(STARBOARD)
#include "base/files/file_starboard.h"
#include "starboard/client_porting/eztime/eztime.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/system.h"
#include "starboard/time.h"
typedef SbFile FileHandle;
typedef SbMutex MutexHandle;
#else
#if defined(OS_WIN)
#include <io.h>
#include <windows.h>
typedef HANDLE FileHandle;
typedef HANDLE MutexHandle;
// Windows warns on using write().  It prefers _write().
#define write(fd, buf, count) _write(fd, buf, static_cast<unsigned int>(count))
// Windows doesn't define STDERR_FILENO.  Define it here.
#define STDERR_FILENO 2

#elif defined(OS_MACOSX)
// In MacOS 10.12 and iOS 10.0 and later ASL (Apple System Log) was deprecated
// in favor of OS_LOG (Unified Logging).
#include <AvailabilityMacros.h>
#if defined(OS_IOS)
#if !defined(__IPHONE_10_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_10_0
#define USE_ASL
#endif
#else  // !defined(OS_IOS)
#if !defined(MAC_OS_X_VERSION_10_12) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_12
#define USE_ASL
#endif
#endif  // defined(OS_IOS)

#if defined(USE_ASL)
#include <asl.h>
#else
#include <os/log.h>
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach-o/dyld.h>

#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#if defined(OS_NACL)
#include <sys/time.h>  // timespec doesn't seem to be in <time.h>
#endif
#include <time.h>
#endif

#if defined(OS_FUCHSIA)
#include <zircon/process.h>
#include <zircon/syscalls.h>
#endif

#if defined(OS_ANDROID)
#include <android/log.h>
#endif

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <errno.h>
#include <paths.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define MAX_PATH PATH_MAX
typedef FILE* FileHandle;
typedef pthread_mutex_t* MutexHandle;
#endif
#endif  // defined(STARBOARD)

#include <algorithm>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <ostream>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/stack.h"
#include "base/debug/activity_tracker.h"
#include "base/debug/alias.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/lazy_instance.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock_impl.h"
#include "base/threading/platform_thread.h"
#include "base/vlog.h"

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include "base/posix/safe_strerror.h"
#include "starboard/common/string.h"
#include "starboard/types.h"
#endif

namespace logging {

namespace {

VlogInfo* g_vlog_info = nullptr;
VlogInfo* g_vlog_info_prev = nullptr;

const char* const log_severity_names[] = {"INFO", "WARNING", "ERROR", "FATAL"};
static_assert(LOG_NUM_SEVERITIES == arraysize(log_severity_names),
              "Incorrect number of log_severity_names");

const char* log_severity_name(int severity) {
  if (severity >= 0 && severity < LOG_NUM_SEVERITIES)
    return log_severity_names[severity];
  return "UNKNOWN";
}

#if defined(OFFICIAL_BUILD)
int g_min_log_level = LOG_FATAL;
#else
int g_min_log_level = LOG_INFO;
#endif

LoggingDestination g_logging_destination = LOG_DEFAULT;

// For LOG_ERROR and above, always print to stderr.
const int kAlwaysPrintErrorLevel = LOG_ERROR;

// Which log file to use? This is initialized by InitLogging or
// will be lazily initialized to the default value when it is
// first needed.
#if defined(OS_WIN)
typedef std::wstring PathString;
#elif defined(OS_POSIX) || defined(OS_FUCHSIA) || defined(STARBOARD)
typedef std::string PathString;
#endif
PathString* g_log_file_name = nullptr;

// This file is lazily opened and the handle may be nullptr
FileHandle g_log_file = nullptr;

// What should be prepended to each message?
bool g_log_process_id = false;
#ifdef STARBOARD
// Cobalt has been used to logging thread ID for long and there is no
// strong reason to stop doing so.
bool g_log_thread_id = true;
#else
bool g_log_thread_id = false;
#endif
bool g_log_timestamp = true;
bool g_log_tickcount = false;
const char* g_log_prefix = nullptr;

// Should we pop up fatal debug messages in a dialog?
bool show_error_dialogs = false;

// An assert handler override specified by the client to be called instead of
// the debug message dialog and process termination. Assert handlers are stored
// in stack to allow overriding and restoring.
base::LazyInstance<base::stack<LogAssertHandlerFunction>>::Leaky
    log_assert_handler_stack = LAZY_INSTANCE_INITIALIZER;

// A log message handler that gets notified of every log message we process.
LogMessageHandlerFunction log_message_handler = nullptr;

// Helper functions to wrap platform differences.

int32_t CurrentProcessId() {
#if defined(STARBOARD)
  return 0;
#else
#if defined(OS_WIN)
  return GetCurrentProcessId();
#elif defined(OS_FUCHSIA)
  zx_info_handle_basic_t basic = {};
  zx_object_get_info(zx_process_self(), ZX_INFO_HANDLE_BASIC, &basic,
                     sizeof(basic), nullptr, nullptr);
  return basic.koid;
#elif defined(OS_POSIX)
  return getpid();
#endif
#endif
}

uint64_t TickCount() {
#if defined(STARBOARD)
  return static_cast<uint64_t>(SbTimeGetMonotonicNow());
#else
#if defined(OS_WIN)
  return GetTickCount();
#elif defined(OS_FUCHSIA)
  return zx_clock_get(ZX_CLOCK_MONOTONIC) /
         static_cast<zx_time_t>(base::Time::kNanosecondsPerMicrosecond);
#elif defined(OS_MACOSX)
  return mach_absolute_time();
#elif defined(OS_NACL)
  // NaCl sadly does not have _POSIX_TIMERS enabled in sys/features.h
  // So we have to use clock() for now.
  return clock();
#elif defined(OS_POSIX)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  uint64_t absolute_micro = static_cast<int64_t>(ts.tv_sec) * 1000000 +
                            static_cast<int64_t>(ts.tv_nsec) / 1000;

  return absolute_micro;
#endif
#endif
}

void DeleteFilePath(const PathString& log_name) {
#if defined(STARBOARD)
  SbFileDelete(log_name.c_str());
#else
#if defined(OS_WIN)
  DeleteFile(log_name.c_str());
#elif defined(OS_NACL)
  // Do nothing; unlink() isn't supported on NaCl.
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  unlink(log_name.c_str());
#else
#error Unsupported platform
#endif
#endif
}

PathString GetDefaultLogFile() {
#if defined(STARBOARD)
  // On Starboard, we politely ask for the log directory, like a civilized
  // platform.
  std::vector<char> path(kSbFileMaxPath + 1);
  SbSystemGetPath(kSbSystemPathDebugOutputDirectory, path.data(), path.size());
  PathString log_file = path.data();
  log_file += std::string(kSbFileSepString) + "debug.log";
  return log_file;
#else
#if defined(OS_WIN)
  // On Windows we use the same path as the exe.
  wchar_t module_name[MAX_PATH];
  GetModuleFileName(nullptr, module_name, MAX_PATH);

  PathString log_name = module_name;
  PathString::size_type last_backslash = log_name.rfind('\\', log_name.size());
  if (last_backslash != PathString::npos)
    log_name.erase(last_backslash + 1);
  log_name += L"debug.log";
  return log_name;
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  // On other platforms we just use the current directory.
  return PathString("debug.log");
#endif
#endif
}

// We don't need locks on Windows for atomically appending to files. The OS
// provides this functionality.
#if defined(OS_POSIX) || defined(OS_FUCHSIA) || defined(STARBOARD)
// This class acts as a wrapper for locking the logging files.
// LoggingLock::Init() should be called from the main thread before any logging
// is done. Then whenever logging, be sure to have a local LoggingLock
// instance on the stack. This will ensure that the lock is unlocked upon
// exiting the frame.
// LoggingLocks can not be nested.
class LoggingLock {
 public:
  LoggingLock() {
    LockLogging();
  }

  ~LoggingLock() {
    UnlockLogging();
  }

  static void Init(LogLockingState lock_log, const PathChar* new_log_file) {
    if (initialized)
      return;
    lock_log_file = lock_log;

    if (lock_log_file != LOCK_LOG_FILE)
      log_lock = new base::internal::LockImpl();

    initialized = true;
  }

 private:
  static void LockLogging() {
    if (lock_log_file == LOCK_LOG_FILE) {
#if defined(STARBOARD)
      SbMutexAcquire(&log_mutex);
#else
      pthread_mutex_lock(&log_mutex);
#endif
    } else {
      // use the lock
      log_lock->Lock();
    }
  }

  static void UnlockLogging() {
    if (lock_log_file == LOCK_LOG_FILE) {
#if defined(STARBOARD)
      SbMutexRelease(&log_mutex);
#else
      pthread_mutex_unlock(&log_mutex);
#endif
    } else {
      log_lock->Unlock();
    }
  }

  // The lock is used if log file locking is false. It helps us avoid problems
  // with multiple threads writing to the log file at the same time.  Use
  // LockImpl directly instead of using Lock, because Lock makes logging calls.
  static base::internal::LockImpl* log_lock;

  // When we don't use a lock, we are using a global mutex. We need to do this
  // because LockFileEx is not thread safe.
#if defined(STARBOARD)
  static SbMutex log_mutex;
#else
  static pthread_mutex_t log_mutex;
#endif

  static bool initialized;
  static LogLockingState lock_log_file;
};

// static
bool LoggingLock::initialized = false;
// static
base::internal::LockImpl* LoggingLock::log_lock = nullptr;
// static
LogLockingState LoggingLock::lock_log_file = LOCK_LOG_FILE;

#if defined(STARBOARD)
SbMutex LoggingLock::log_mutex = SB_MUTEX_INITIALIZER;
#else
pthread_mutex_t LoggingLock::log_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#endif  // OS_POSIX || OS_FUCHSIA

// Called by logging functions to ensure that |g_log_file| is initialized
// and can be used for writing. Returns false if the file could not be
// initialized. |g_log_file| will be nullptr in this case.
bool InitializeLogFileHandle() {
  if (g_log_file)
    return true;

  if (!g_log_file_name) {
    // Nobody has called InitLogging to specify a debug log file, so here we
    // initialize the log file name to a default.
    g_log_file_name = new PathString(GetDefaultLogFile());
  }

#if defined(STARBOARD)
  // This seems to get called a lot with an empty filename, at least in
  // base_unittests.
  if (g_log_file_name->empty()) {
    return false;
  }
#endif

  if ((g_logging_destination & LOG_TO_FILE) != 0) {
#if defined(STARBOARD)
    g_log_file = SbFileOpen(g_log_file_name->c_str(),
                            kSbFileOpenAlways | kSbFileWrite, NULL, NULL);
    if (!SbFileIsValid(g_log_file))
      return false;

    SbFileSeek(g_log_file, kSbFileFromEnd, 0);
#else
#if defined(OS_WIN)
    // The FILE_APPEND_DATA access mask ensures that the file is atomically
    // appended to across accesses from multiple threads.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa364399(v=vs.85).aspx
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa363858(v=vs.85).aspx
    g_log_file = CreateFile(g_log_file_name->c_str(), FILE_APPEND_DATA,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_log_file == INVALID_HANDLE_VALUE || g_log_file == nullptr) {
      // We are intentionally not using FilePath or FileUtil here to reduce the
      // dependencies of the logging implementation. For e.g. FilePath and
      // FileUtil depend on shell32 and user32.dll. This is not acceptable for
      // some consumers of base logging like chrome_elf, etc.
      // Please don't change the code below to use FilePath.
      // try the current directory
      wchar_t system_buffer[MAX_PATH];
      system_buffer[0] = 0;
      DWORD len = ::GetCurrentDirectory(arraysize(system_buffer),
                                        system_buffer);
      if (len == 0 || len > arraysize(system_buffer))
        return false;

      *g_log_file_name = system_buffer;
      // Append a trailing backslash if needed.
      if (g_log_file_name->back() != L'\\')
        *g_log_file_name += L"\\";
      *g_log_file_name += L"debug.log";

      g_log_file = CreateFile(g_log_file_name->c_str(), FILE_APPEND_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (g_log_file == INVALID_HANDLE_VALUE || g_log_file == nullptr) {
        g_log_file = nullptr;
        return false;
      }
    }
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
    g_log_file = fopen(g_log_file_name->c_str(), "a");
    if (g_log_file == nullptr)
      return false;
#else
#error Unsupported platform
#endif
#endif
  }

  return true;
}

void CloseFile(FileHandle log) {
#if defined(STARBOARD)
  SbFileClose(log);
#else
#if defined(OS_WIN)
  CloseHandle(log);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  fclose(log);
#else
#error Unsupported platform
#endif
#endif
}

void CloseLogFileUnlocked() {
  if (!g_log_file)
    return;

  CloseFile(g_log_file);
  g_log_file = nullptr;
}

#if defined(STARBOARD)
SbLogPriority LogLevelToStarboardLogPriority(int level) {
  switch (level) {
    case LOG_INFO:
      return kSbLogPriorityInfo;
    case LOG_WARNING:
      return kSbLogPriorityWarning;
    case LOG_ERROR:
      return kSbLogPriorityError;
    case LOG_FATAL:
      return kSbLogPriorityFatal;
    case LOG_VERBOSE:
    default:
      if (level <= LOG_VERBOSE) {
        // Verbose level can be any negative integer, sanity check its range to
        // filter out potential errors.
        DCHECK_GE(level, -256);
        return kSbLogPriorityInfo;
      }
      NOTREACHED() << "Unrecognized log level.";
      return kSbLogPriorityInfo;
  }
}
#endif  // defined(STARBOARD)

}  // namespace

#if DCHECK_IS_CONFIGURABLE
// In DCHECK-enabled Chrome builds, allow the meaning of LOG_DCHECK to be
// determined at run-time. We default it to INFO, to avoid it triggering
// crashes before the run-time has explicitly chosen the behaviour.
BASE_EXPORT logging::LogSeverity LOG_DCHECK = LOG_INFO;
#endif  // DCHECK_IS_CONFIGURABLE

// This is never instantiated, it's just used for EAT_STREAM_PARAMETERS to have
// an object of the correct type on the LHS of the unused part of the ternary
// operator.
std::ostream* g_swallow_stream;

LoggingSettings::LoggingSettings()
    : logging_dest(LOG_DEFAULT),
      log_file(nullptr),
      lock_log(LOCK_LOG_FILE),
      delete_old(APPEND_TO_OLD_LOG_FILE) {}

bool BaseInitLoggingImpl(const LoggingSettings& settings) {
#if defined(OS_NACL)
  // Can log only to the system debug log.
  CHECK_EQ(settings.logging_dest & ~LOG_TO_SYSTEM_DEBUG_LOG, 0);
#endif
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Don't bother initializing |g_vlog_info| unless we use one of the
  // vlog switches.
  if (command_line->HasSwitch(switches::kV) ||
      command_line->HasSwitch(switches::kVModule)) {
    // NOTE: If |g_vlog_info| has already been initialized, it might be in use
    // by another thread. Don't delete the old VLogInfo, just create a second
    // one. We keep track of both to avoid memory leak warnings.
    CHECK(!g_vlog_info_prev);
    g_vlog_info_prev = g_vlog_info;

    g_vlog_info =
        new VlogInfo(command_line->GetSwitchValueASCII(switches::kV),
                     command_line->GetSwitchValueASCII(switches::kVModule),
                     &g_min_log_level);
  }

  g_logging_destination = settings.logging_dest;

  // ignore file options unless logging to file is set.
  if ((g_logging_destination & LOG_TO_FILE) == 0)
    return true;

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  LoggingLock::Init(settings.lock_log, settings.log_file);
  LoggingLock logging_lock;
#endif

  // Calling InitLogging twice or after some log call has already opened the
  // default log file will re-initialize to the new options.
  CloseLogFileUnlocked();

  if (!g_log_file_name)
    g_log_file_name = new PathString();
  *g_log_file_name = settings.log_file;
  if (settings.delete_old == DELETE_OLD_LOG_FILE)
    DeleteFilePath(*g_log_file_name);

  return InitializeLogFileHandle();
}

void SetMinLogLevel(int level) {
  g_min_log_level = std::min(LOG_FATAL, level);
}

#if defined(OFFICIAL_BUILD) && !SB_IS(MODULAR)
int GetMinLogLevel() {
  return LOG_NUM_SEVERITIES;
}

bool ShouldCreateLogMessage(int severity) {
  return false;
}

int GetVlogVerbosity() {
  return LOG_INFO - GetMinLogLevel();
}

int GetVlogLevelHelper(const char* file, size_t N) {
  return GetVlogVerbosity();
}

void SetLogItems(bool enable_process_id,
                 bool enable_thread_id,
                 bool enable_timestamp,
                 bool enable_tickcount) {}

void SetLogPrefix(const char* prefix) {}

#else  // defined(OFFICIAL_BUILD) && !SB_IS(MODULAR)

int GetMinLogLevel() {
  return g_min_log_level;
}

bool ShouldCreateLogMessage(int severity) {
  if (severity < g_min_log_level)
    return false;

  // Return true here unless we know ~LogMessage won't do anything. Note that
  // ~LogMessage writes to stderr if severity_ >= kAlwaysPrintErrorLevel, even
  // when g_logging_destination is LOG_NONE.
  return g_logging_destination != LOG_NONE || log_message_handler ||
         severity >= kAlwaysPrintErrorLevel;
}

int GetVlogVerbosity() {
  return std::max(-1, LOG_INFO - GetMinLogLevel());
}

int GetVlogLevelHelper(const char* file, size_t N) {
  DCHECK_GT(N, 0U);
  // Note: |g_vlog_info| may change on a different thread during startup
  // (but will always be valid or nullptr).
  VlogInfo* vlog_info = g_vlog_info;
  return vlog_info ?
      vlog_info->GetVlogLevel(base::StringPiece(file, N - 1)) :
      GetVlogVerbosity();
}

void SetLogItems(bool enable_process_id, bool enable_thread_id,
                 bool enable_timestamp, bool enable_tickcount) {
  g_log_process_id = enable_process_id;
  g_log_thread_id = enable_thread_id;
  g_log_timestamp = enable_timestamp;
  g_log_tickcount = enable_tickcount;
}

void SetLogPrefix(const char* prefix) {
  DCHECK(!prefix ||
         base::ContainsOnlyChars(prefix, "abcdefghijklmnopqrstuvwxyz"));
  g_log_prefix = prefix;
}
#endif  // defined(OFFICIAL_BUILD) && !SB_IS(MODULAR)

void SetShowErrorDialogs(bool enable_dialogs) {
  show_error_dialogs = enable_dialogs;
}

ScopedLogAssertHandler::ScopedLogAssertHandler(
    LogAssertHandlerFunction handler) {
  log_assert_handler_stack.Get().push(std::move(handler));
}

ScopedLogAssertHandler::~ScopedLogAssertHandler() {
  log_assert_handler_stack.Get().pop();
}

void SetLogMessageHandler(LogMessageHandlerFunction handler) {
  log_message_handler = handler;
}

LogMessageHandlerFunction GetLogMessageHandler() {
  return log_message_handler;
}

// Explicit instantiations for commonly used comparisons.
template std::string* MakeCheckOpString<int, int>(
    const int&, const int&, const char* names);
template std::string* MakeCheckOpString<unsigned long, unsigned long>(
    const unsigned long&, const unsigned long&, const char* names);
template std::string* MakeCheckOpString<unsigned long, unsigned int>(
    const unsigned long&, const unsigned int&, const char* names);
template std::string* MakeCheckOpString<unsigned int, unsigned long>(
    const unsigned int&, const unsigned long&, const char* names);
template std::string* MakeCheckOpString<std::string, std::string>(
    const std::string&, const std::string&, const char* name);

void MakeCheckOpValueString(std::ostream* os, std::nullptr_t p) {
  (*os) << "nullptr";
}

#if !defined(NDEBUG)
// Displays a message box to the user with the error message in it.
// Used for fatal messages, where we close the app simultaneously.
// This is for developers only; we don't use this in circumstances
// (like release builds) where users could see it, since users don't
// understand these messages anyway.
void DisplayDebugMessageInDialog(const std::string& str) {
  if (str.empty())
    return;

  if (!show_error_dialogs)
    return;

#if defined(OS_WIN)
  // We intentionally don't implement a dialog on other platforms.
  // You can just look at stderr.
  MessageBoxW(nullptr, base::UTF8ToUTF16(str).c_str(), L"Fatal error",
              MB_OK | MB_ICONHAND | MB_TOPMOST);
#endif  // defined(OS_WIN)
}
#endif  // !defined(NDEBUG)

LogMessage::LogMessage(const char* file, int line, LogSeverity severity)
    : severity_(severity), file_(file), line_(line) {
  Init(file, line);
}

LogMessage::LogMessage(const char* file, int line, const char* condition)
    : severity_(LOG_FATAL), file_(file), line_(line) {
  Init(file, line);
  stream_ << "Check failed: " << condition << ". ";
}

LogMessage::LogMessage(const char* file, int line, std::string* result)
    : severity_(LOG_FATAL), file_(file), line_(line) {
  Init(file, line);
  stream_ << "Check failed: " << *result;
  delete result;
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity,
                       std::string* result)
    : severity_(severity), file_(file), line_(line) {
  Init(file, line);
  stream_ << "Check failed: " << *result;
  delete result;
}

LogMessage::~LogMessage() {
  size_t stack_start = stream_.tellp();
#if !defined(OFFICIAL_BUILD) && !defined(OS_NACL) && !defined(__UCLIBC__) && \
    !defined(OS_AIX)
  if (severity_ == LOG_FATAL && !base::debug::BeingDebugged()) {
    // Include a stack trace on a fatal, unless a debugger is attached.
    base::debug::StackTrace trace;
    stream_ << std::endl;  // Newline to separate from log message.
    trace.OutputToStream(&stream_);
  }
#endif
  stream_ << std::endl;
  std::string str_newline(stream_.str());
  TRACE_EVENT_INSTANT1("log", "LogMessage", TRACE_EVENT_SCOPE_THREAD, "message",
                       str_newline);

  // Give any log message handler first dibs on the message.
  if (log_message_handler &&
      log_message_handler(severity_, file_, line_,
                          message_start_, str_newline)) {
    // The handler took care of it, no further processing.
    return;
  }

  if ((g_logging_destination & LOG_TO_SYSTEM_DEBUG_LOG) != 0) {
#if defined(STARBOARD)
    SbLog(LogLevelToStarboardLogPriority(severity_), str_newline.c_str());
#else
#if defined(OS_WIN)
    OutputDebugStringA(str_newline.c_str());
#elif defined(OS_MACOSX)
    // In LOG_TO_SYSTEM_DEBUG_LOG mode, log messages are always written to
    // stderr. If stderr is /dev/null, also log via ASL (Apple System Log) or
    // its successor OS_LOG. If there's something weird about stderr, assume
    // that log messages are going nowhere and log via ASL/OS_LOG too.
    // Messages logged via ASL/OS_LOG show up in Console.app.
    //
    // Programs started by launchd, as UI applications normally are, have had
    // stderr connected to /dev/null since OS X 10.8. Prior to that, stderr was
    // a pipe to launchd, which logged what it received (see log_redirect_fd in
    // 10.7.5 launchd-392.39/launchd/src/launchd_core_logic.c).
    //
    // Another alternative would be to determine whether stderr is a pipe to
    // launchd and avoid logging via ASL only in that case. See 10.7.5
    // CF-635.21/CFUtilities.c also_do_stderr(). This would result in logging to
    // both stderr and ASL/OS_LOG even in tests, where it's undesirable to log
    // to the system log at all.
    //
    // Note that the ASL client by default discards messages whose levels are
    // below ASL_LEVEL_NOTICE. It's possible to change that with
    // asl_set_filter(), but this is pointless because syslogd normally applies
    // the same filter.
    const bool log_to_system = []() {
      struct stat stderr_stat;
      if (fstat(fileno(stderr), &stderr_stat) == -1) {
        return true;
      }
      if (!S_ISCHR(stderr_stat.st_mode)) {
        return false;
      }

      struct stat dev_null_stat;
      if (stat(_PATH_DEVNULL, &dev_null_stat) == -1) {
        return true;
      }

      return !S_ISCHR(dev_null_stat.st_mode) ||
             stderr_stat.st_rdev == dev_null_stat.st_rdev;
    }();

    if (log_to_system) {
      // Log roughly the same way that CFLog() and NSLog() would. See 10.10.5
      // CF-1153.18/CFUtilities.c __CFLogCString().
      CFBundleRef main_bundle = CFBundleGetMainBundle();
      CFStringRef main_bundle_id_cf =
          main_bundle ? CFBundleGetIdentifier(main_bundle) : nullptr;
      std::string main_bundle_id =
          main_bundle_id_cf ? base::SysCFStringRefToUTF8(main_bundle_id_cf)
                            : std::string("");
#if defined(USE_ASL)
      // The facility is set to the main bundle ID if available. Otherwise,
      // "com.apple.console" is used.
      const class ASLClient {
       public:
        explicit ASLClient(const std::string& facility)
            : client_(asl_open(nullptr, facility.c_str(), ASL_OPT_NO_DELAY)) {}
        ~ASLClient() { asl_close(client_); }

        aslclient get() const { return client_; }

       private:
        aslclient client_;
        DISALLOW_COPY_AND_ASSIGN(ASLClient);
      } asl_client(main_bundle_id.empty() ? main_bundle_id
                                          : "com.apple.console");

      const class ASLMessage {
       public:
        ASLMessage() : message_(asl_new(ASL_TYPE_MSG)) {}
        ~ASLMessage() { asl_free(message_); }

        aslmsg get() const { return message_; }

       private:
        aslmsg message_;
        DISALLOW_COPY_AND_ASSIGN(ASLMessage);
      } asl_message;

      // By default, messages are only readable by the admin group. Explicitly
      // make them readable by the user generating the messages.
      char euid_string[12];
      snprintf(euid_string, arraysize(euid_string), "%d", geteuid());
      asl_set(asl_message.get(), ASL_KEY_READ_UID, euid_string);

      // Map Chrome log severities to ASL log levels.
      const char* const asl_level_string = [](LogSeverity severity) {
        // ASL_LEVEL_* are ints, but ASL needs equivalent strings. This
        // non-obvious two-step macro trick achieves what's needed.
        // https://gcc.gnu.org/onlinedocs/cpp/Stringification.html
#define ASL_LEVEL_STR(level) ASL_LEVEL_STR_X(level)
#define ASL_LEVEL_STR_X(level) #level
        switch (severity) {
          case LOG_INFO:
            return ASL_LEVEL_STR(ASL_LEVEL_INFO);
          case LOG_WARNING:
            return ASL_LEVEL_STR(ASL_LEVEL_WARNING);
          case LOG_ERROR:
            return ASL_LEVEL_STR(ASL_LEVEL_ERR);
          case LOG_FATAL:
            return ASL_LEVEL_STR(ASL_LEVEL_CRIT);
          default:
            return severity < 0 ? ASL_LEVEL_STR(ASL_LEVEL_DEBUG)
                                : ASL_LEVEL_STR(ASL_LEVEL_NOTICE);
        }
#undef ASL_LEVEL_STR
#undef ASL_LEVEL_STR_X
      }(severity_);
      asl_set(asl_message.get(), ASL_KEY_LEVEL, asl_level_string);

      asl_set(asl_message.get(), ASL_KEY_MSG, str_newline.c_str());

      asl_send(asl_client.get(), asl_message.get());
#else   // !defined(USE_ASL)
      const class OSLog {
       public:
        explicit OSLog(const char* subsystem)
            : os_log_(subsystem ? os_log_create(subsystem, "chromium_logging")
                                : OS_LOG_DEFAULT) {}
        ~OSLog() {
          if (os_log_ != OS_LOG_DEFAULT) {
            os_release(os_log_);
          }
        }
        os_log_t get() const { return os_log_; }

       private:
        os_log_t os_log_;
        DISALLOW_COPY_AND_ASSIGN(OSLog);
      } log(main_bundle_id.empty() ? nullptr : main_bundle_id.c_str());
      const os_log_type_t os_log_type = [](LogSeverity severity) {
        switch (severity) {
          case LOG_INFO:
            return OS_LOG_TYPE_INFO;
          case LOG_WARNING:
            return OS_LOG_TYPE_DEFAULT;
          case LOG_ERROR:
            return OS_LOG_TYPE_ERROR;
          case LOG_FATAL:
            return OS_LOG_TYPE_FAULT;
          default:
            return severity < 0 ? OS_LOG_TYPE_DEBUG : OS_LOG_TYPE_DEFAULT;
        }
      }(severity_);
      os_log_with_type(log.get(), os_log_type, "%{public}s",
                       str_newline.c_str());
#endif  // defined(USE_ASL)
    }
#elif defined(OS_ANDROID)
    android_LogPriority priority =
        (severity_ < 0) ? ANDROID_LOG_VERBOSE : ANDROID_LOG_UNKNOWN;
    switch (severity_) {
      case LOG_INFO:
        priority = ANDROID_LOG_INFO;
        break;
      case LOG_WARNING:
        priority = ANDROID_LOG_WARN;
        break;
      case LOG_ERROR:
        priority = ANDROID_LOG_ERROR;
        break;
      case LOG_FATAL:
        priority = ANDROID_LOG_FATAL;
        break;
    }
    __android_log_write(priority, "chromium", str_newline.c_str());
#endif
    ignore_result(fwrite(str_newline.data(), str_newline.size(), 1, stderr));
    fflush(stderr);
#endif
  } else if (severity_ >= kAlwaysPrintErrorLevel) {
#if defined(STARBOARD)
    SbLog(LogLevelToStarboardLogPriority(severity_), str_newline.c_str());
#else
    // When we're only outputting to a log file, above a certain log level, we
    // should still output to stderr so that we can better detect and diagnose
    // problems with unit tests, especially on the buildbots.
    ignore_result(fwrite(str_newline.data(), str_newline.size(), 1, stderr));
    fflush(stderr);
#endif
  }

  // write to log file
  if ((g_logging_destination & LOG_TO_FILE) != 0) {
    // We can have multiple threads and/or processes, so try to prevent them
    // from clobbering each other's writes.
    // If the client app did not call InitLogging, and the lock has not
    // been created do it now. We do this on demand, but if two threads try
    // to do this at the same time, there will be a race condition to create
    // the lock. This is why InitLogging should be called from the main
    // thread at the beginning of execution.
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
    LoggingLock::Init(LOCK_LOG_FILE, nullptr);
    LoggingLock logging_lock;
#endif
    if (InitializeLogFileHandle()) {
#if defined(STARBOARD)
      SbFileSeek(g_log_file, kSbFileFromEnd, 0);
      int written = 0;
      while (written < str_newline.length()) {
        int result = SbFileWrite(g_log_file, &(str_newline.c_str()[written]),
                                 str_newline.length() - written);
        base::RecordFileWriteStat(result);
        if (result < 0) {
          break;
        }
        written += result;
      }
#else
#if defined(OS_WIN)
      DWORD num_written;
      WriteFile(g_log_file,
                static_cast<const void*>(str_newline.c_str()),
                static_cast<DWORD>(str_newline.length()),
                &num_written,
                nullptr);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
      ignore_result(fwrite(
          str_newline.data(), str_newline.size(), 1, g_log_file));
      fflush(g_log_file);
#else
#error Unsupported platform
#endif
#endif
    }
  }

  if (severity_ == LOG_FATAL) {
#if !defined(STARBOARD)
    // Write the log message to the global activity tracker, if running.
    base::debug::GlobalActivityTracker* tracker =
        base::debug::GlobalActivityTracker::Get();
    if (tracker)
      tracker->RecordLogMessage(str_newline);
#endif  // !defined(STARBOARD)

    // Ensure the first characters of the string are on the stack so they
    // are contained in minidumps for diagnostic purposes.
    DEBUG_ALIAS_FOR_CSTR(str_stack, str_newline.c_str(), 1024);

    if (log_assert_handler_stack.IsCreated() &&
        !log_assert_handler_stack.Get().empty()) {
      LogAssertHandlerFunction log_assert_handler =
          log_assert_handler_stack.Get().top();

      if (log_assert_handler) {
        log_assert_handler.Run(
            file_, line_,
            base::StringPiece(str_newline.c_str() + message_start_,
                              stack_start - message_start_),
            base::StringPiece(str_newline.c_str() + stack_start));
      }
    } else {
      // Don't use the string with the newline, get a fresh version to send to
      // the debug message process. We also don't display assertions to the
      // user in release mode. The enduser can't do anything with this
      // information, and displaying message boxes when the application is
      // hosed can cause additional problems.
#ifndef NDEBUG
      if (!base::debug::BeingDebugged()) {
        // Displaying a dialog is unnecessary when debugging and can complicate
        // debugging.
        DisplayDebugMessageInDialog(stream_.str());
      }
#endif
      // Crash the process to generate a dump.
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
      IMMEDIATE_CRASH();
#else
      base::debug::BreakDebugger();
#endif
    }
  }
}

// writes the common header info to the stream
void LogMessage::Init(const char* file, int line) {
  base::StringPiece filename(file);
  size_t last_slash_pos = filename.find_last_of("\\/");
  if (last_slash_pos != base::StringPiece::npos)
    filename.remove_prefix(last_slash_pos + 1);

  // TODO(darin): It might be nice if the columns were fixed width.

  stream_ <<  '[';
  if (g_log_prefix)
    stream_ << g_log_prefix << ':';
#ifndef STARBOARD
  // Cobalt and cobalt unittests are both single thread applciation.
  if (g_log_process_id)
    stream_ << CurrentProcessId() << ':';
#endif
  if (g_log_thread_id)
    stream_ << base::PlatformThread::GetName() << '/' << base::PlatformThread::CurrentId() << ":";
  if (g_log_timestamp) {
#if defined(STARBOARD)
    EzTimeValue time_value;
    EzTimeValueGetNow(&time_value, NULL);
    struct EzTimeExploded local_time = {0};
    EzTimeTExplodeLocal(&(time_value.tv_sec), &local_time);
    struct EzTimeExploded* tm_time = &local_time;
    stream_ << std::setfill('0')
            << std::setw(2) << 1 + tm_time->tm_mon
            << std::setw(2) << tm_time->tm_mday
            << '/'
            << std::setw(2) << tm_time->tm_hour
            << std::setw(2) << tm_time->tm_min
            << std::setw(2) << tm_time->tm_sec
            << '.' << std::setw(6) << time_value.tv_usec
            << ':';
#else
#if defined(OS_WIN)
    SYSTEMTIME local_time;
    GetLocalTime(&local_time);
    stream_ << std::setfill('0') << std::setw(2) << local_time.wMonth
            << std::setw(2) << local_time.wDay << '/' << std::setw(2)
            << local_time.wHour << std::setw(2) << local_time.wMinute
            << std::setw(2) << local_time.wSecond << '.' << std::setw(3)
            << local_time.wMilliseconds << ':';
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
    timeval tv;
    gettimeofday(&tv, nullptr);
    time_t t = tv.tv_sec;
    struct tm local_time;
    localtime_r(&t, &local_time);
    struct tm* tm_time = &local_time;
    stream_ << std::setfill('0') << std::setw(2) << 1 + tm_time->tm_mon
            << std::setw(2) << tm_time->tm_mday << '/' << std::setw(2)
            << tm_time->tm_hour << std::setw(2) << tm_time->tm_min
            << std::setw(2) << tm_time->tm_sec << '.' << std::setw(6)
            << tv.tv_usec << ':';
#else
#error Unsupported platform
#endif
#endif
  }
#ifndef STARBOARD
  // Cobalt does not want tickcounts.
  if (g_log_tickcount)
    stream_ << TickCount() << ':';
#endif
  if (severity_ >= 0)
    stream_ << log_severity_name(severity_);
  else
    stream_ << "VERBOSE" << -severity_;

  stream_ << ":" << filename << "(" << line << ")] ";

  message_start_ = stream_.str().length();
}

#if defined(OS_WIN)
// This has already been defined in the header, but defining it again as DWORD
// ensures that the type used in the header is equivalent to DWORD. If not,
// the redefinition is a compile error.
typedef DWORD SystemErrorCode;
#endif

SystemErrorCode GetLastSystemErrorCode() {
#if defined(STARBOARD)
  return SbSystemGetLastError();
#else
#if defined(OS_WIN)
  return ::GetLastError();
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  return errno;
#endif
#endif
}

#if defined(STARBOARD)
BASE_EXPORT std::string SystemErrorCodeToString(SystemErrorCode error_code) {
  const int kErrorMessageBufferSize = 256;
  char msgbuf[kErrorMessageBufferSize];

  if (SbSystemGetErrorString(error_code, msgbuf, kErrorMessageBufferSize) > 0) {
    // Messages returned by system end with line breaks.
    return base::CollapseWhitespaceASCII(msgbuf, true) +
           base::StringPrintf(" (%d)", error_code);
  } else {
    return base::StringPrintf("Error (%d) while retrieving error. (%d)",
                              GetLastSystemErrorCode(), error_code);
  }
}
#else
BASE_EXPORT std::string SystemErrorCodeToString(SystemErrorCode error_code) {
#if defined(OS_WIN)
  const int kErrorMessageBufferSize = 256;
  char msgbuf[kErrorMessageBufferSize];
  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD len = FormatMessageA(flags, nullptr, error_code, 0, msgbuf,
                             arraysize(msgbuf), nullptr);
  if (len) {
    // Messages returned by system end with line breaks.
    return base::CollapseWhitespaceASCII(msgbuf, true) +
           base::StringPrintf(" (0x%lX)", error_code);
  }
  return base::StringPrintf("Error (0x%lX) while retrieving error. (0x%lX)",
                            GetLastError(), error_code);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  return base::safe_strerror(error_code) +
         base::StringPrintf(" (%d)", error_code);
#endif  // defined(OS_WIN)
}
#endif  // defined(STARBOARD)

#if defined(STARBOARD)
StarboardErrorLogMessage::StarboardErrorLogMessage(const char* file,
                                                   int line,
                                                   LogSeverity severity,
                                                   SystemErrorCode err)
    : err_(err), log_message_(file, line, severity) {}

StarboardErrorLogMessage::~StarboardErrorLogMessage() {
  stream() << ": " << SystemErrorCodeToString(err_);
  // We're about to crash (CHECK). Put |err_| on the stack (by placing it in a
  // field) and use Alias in hopes that it makes it into crash dumps.
  SystemErrorCode last_error = err_;
  base::debug::Alias(&last_error);
}
#else
#if defined(OS_WIN)
Win32ErrorLogMessage::Win32ErrorLogMessage(const char* file,
                                           int line,
                                           LogSeverity severity,
                                           SystemErrorCode err)
    : err_(err),
      log_message_(file, line, severity) {
}

Win32ErrorLogMessage::~Win32ErrorLogMessage() {
  stream() << ": " << SystemErrorCodeToString(err_);
  // We're about to crash (CHECK). Put |err_| on the stack (by placing it in a
  // field) and use Alias in hopes that it makes it into crash dumps.
  DWORD last_error = err_;
  base::debug::Alias(&last_error);
}
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
ErrnoLogMessage::ErrnoLogMessage(const char* file,
                                 int line,
                                 LogSeverity severity,
                                 SystemErrorCode err)
    : err_(err),
      log_message_(file, line, severity) {
}

ErrnoLogMessage::~ErrnoLogMessage() {
  stream() << ": " << SystemErrorCodeToString(err_);
  // We're about to crash (CHECK). Put |err_| on the stack (by placing it in a
  // field) and use Alias in hopes that it makes it into crash dumps.
  int last_error = err_;
  base::debug::Alias(&last_error);
}
#endif  // defined(OS_WIN)
#endif  // defined(STARBOARD)

void CloseLogFile() {
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  LoggingLock logging_lock;
#endif
  CloseLogFileUnlocked();
}

void RawLog(int level, const char* message) {
  if (level >= g_min_log_level && message) {
#if defined(STARBOARD)
    SbLogRaw(message);
    const size_t message_len = strlen(message);
    if (message_len > 0 && message[message_len - 1] != '\n') {
      SbLogRaw("\n");
    }
#else
    size_t bytes_written = 0;
    const size_t message_len = strlen(message);
    int rv;
    while (bytes_written < message_len) {
      rv = HANDLE_EINTR(
          write(STDERR_FILENO, message + bytes_written,
                message_len - bytes_written));
      if (rv < 0) {
        // Give up, nothing we can do now.
        break;
      }
      bytes_written += rv;
    }

    if (message_len > 0 && message[message_len - 1] != '\n') {
      do {
        rv = HANDLE_EINTR(write(STDERR_FILENO, "\n", 1));
        if (rv < 0) {
          // Give up, nothing we can do now.
          break;
        }
      } while (rv != 1);
    }
#endif  // defined(STARBOARD)
  }

  if (level == LOG_FATAL)
    base::debug::BreakDebugger();
}

// This was defined at the beginning of this file.
#undef write

#if defined(OS_WIN)
bool IsLoggingToFileEnabled() {
  return g_logging_destination & LOG_TO_FILE;
}

std::wstring GetLogFileFullPath() {
  if (g_log_file_name)
    return *g_log_file_name;
  return std::wstring();
}
#endif

BASE_EXPORT void LogErrorNotReached(const char* file, int line) {
  LogMessage(file, line, LOG_ERROR).stream()
      << "NOTREACHED() hit.";
}

}  // namespace logging

#if !defined(STARBOARD)  // This is implemented already in Starboard.
std::ostream& std::operator<<(std::ostream& out, const wchar_t* wstr) {
  return out << (wstr ? base::WideToUTF8(wstr) : std::string());
}
#endif  // !defined(STARBOARD)
