// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <dirent.h>
#include <malloc.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/threading/thread_restrictions.h"

namespace {

enum ParsingState {
  KEY_NAME,
  KEY_VALUE
};

const char kProcDir[] = "/proc";
const char kStatFile[] = "stat";

// Returns a FilePath to "/proc/pid".
FilePath GetProcPidDir(pid_t pid) {
  return FilePath(kProcDir).Append(base::IntToString(pid));
}

// Fields from /proc/<pid>/stat, 0-based. See man 5 proc.
// If the ordering ever changes, carefully review functions that use these
// values.
enum ProcStatsFields {
  VM_COMM           = 1,   // Filename of executable, without parentheses.
  VM_STATE          = 2,   // Letter indicating the state of the process.
  VM_PPID           = 3,   // PID of the parent.
  VM_PGRP           = 4,   // Process group id.
  VM_UTIME          = 13,  // Time scheduled in user mode in clock ticks.
  VM_STIME          = 14,  // Time scheduled in kernel mode in clock ticks.
  VM_VSIZE          = 22,  // Virtual memory size in bytes.
  VM_RSS            = 23,  // Resident Set Size in pages.
};

// Reads /proc/<pid>/stat into |buffer|. Returns true if successful.
bool ReadProcStats(pid_t pid, std::string* buffer) {
  // Synchronously reading files in /proc is safe.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  FilePath stat_file = GetProcPidDir(pid).Append(kStatFile);
  if (!file_util::ReadFileToString(stat_file, buffer)) {
    DLOG(WARNING) << "Failed to get process stats.";
    return false;
  }
  return true;
}

// Takes |stats_data| and populates |proc_stats| with the values split by
// spaces. Taking into account the 2nd field may, in itself, contain spaces.
// Returns true if successful.
bool ParseProcStats(const std::string& stats_data,
                    std::vector<std::string>* proc_stats) {
  // The stat file is formatted as:
  // pid (process name) data1 data2 .... dataN
  // Look for the closing paren by scanning backwards, to avoid being fooled by
  // processes with ')' in the name.
  size_t open_parens_idx = stats_data.find(" (");
  size_t close_parens_idx = stats_data.rfind(") ");
  if (open_parens_idx == std::string::npos ||
      close_parens_idx == std::string::npos ||
      open_parens_idx > close_parens_idx) {
    DLOG(WARNING) << "Failed to find matched parens in '" << stats_data << "'";
    NOTREACHED();
    return false;
  }
  open_parens_idx++;

  proc_stats->clear();
  // PID.
  proc_stats->push_back(stats_data.substr(0, open_parens_idx));
  // Process name without parentheses.
  proc_stats->push_back(
      stats_data.substr(open_parens_idx + 1,
                        close_parens_idx - (open_parens_idx + 1)));

  // Split the rest.
  std::vector<std::string> other_stats;
  base::SplitString(stats_data.substr(close_parens_idx + 2), ' ', &other_stats);
  for (size_t i = 0; i < other_stats.size(); ++i)
    proc_stats->push_back(other_stats[i]);
  return true;
}

// Reads the |field_num|th field from |proc_stats|. Returns 0 on failure.
// This version does not handle the first 3 values, since the first value is
// simply |pid|, and the next two values are strings.
int GetProcStatsFieldAsInt(const std::vector<std::string>& proc_stats,
                           ProcStatsFields field_num) {
  if (field_num < VM_PPID) {
    NOTREACHED();
    return 0;
  }

  if (proc_stats.size() > static_cast<size_t>(field_num)) {
    int value;
    if (base::StringToInt(proc_stats[field_num], &value))
      return value;
  }
  NOTREACHED();
  return 0;
}

// Convenience wrapper around GetProcStatsFieldAsInt(), ParseProcStats() and
// ReadProcStats(). See GetProcStatsFieldAsInt() for details.
int ReadProcStatsAndGetFieldAsInt(pid_t pid, ProcStatsFields field_num) {
  std::string stats_data;
  if (!ReadProcStats(pid, &stats_data))
    return 0;
  std::vector<std::string> proc_stats;
  if (!ParseProcStats(stats_data, &proc_stats))
    return 0;
  return GetProcStatsFieldAsInt(proc_stats, field_num);
}

// Reads the |field_num|th field from |proc_stats|.
// Returns an empty string on failure.
// This version only handles VM_COMM and VM_STATE, which are the only fields
// that are strings.
std::string GetProcStatsFieldAsString(
    const std::vector<std::string>& proc_stats,
    ProcStatsFields field_num) {
  if (field_num < VM_COMM || field_num > VM_STATE) {
    NOTREACHED();
    return "";
  }

  if (proc_stats.size() > static_cast<size_t>(field_num))
    return proc_stats[field_num];

  NOTREACHED();
  return 0;
}

// Reads /proc/<pid>/cmdline and populates |proc_cmd_line_args| with the command
// line arguments. Returns true if successful.
// Note: /proc/<pid>/cmdline contains command line arguments separated by single
// null characters. We tokenize it into a vector of strings using '\0' as a
// delimiter.
bool GetProcCmdline(pid_t pid, std::vector<std::string>* proc_cmd_line_args) {
  // Synchronously reading files in /proc is safe.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  FilePath cmd_line_file = GetProcPidDir(pid).Append("cmdline");
  std::string cmd_line;
  if (!file_util::ReadFileToString(cmd_line_file, &cmd_line))
    return false;
  std::string delimiters;
  delimiters.push_back('\0');
  Tokenize(cmd_line, delimiters, proc_cmd_line_args);
  return true;
}

// Take a /proc directory entry named |d_name|, and if it is the directory for
// a process, convert it to a pid_t.
// Returns 0 on failure.
// e.g. /proc/self/ will return 0, whereas /proc/1234 will return 1234.
pid_t ProcDirSlotToPid(const char* d_name) {
  int i;
  for (i = 0; i < NAME_MAX && d_name[i]; ++i) {
    if (!IsAsciiDigit(d_name[i])) {
      return 0;
    }
  }
  if (i == NAME_MAX)
    return 0;

  // Read the process's command line.
  pid_t pid;
  std::string pid_string(d_name);
  if (!base::StringToInt(pid_string, &pid)) {
    NOTREACHED();
    return 0;
  }
  return pid;
}

// Get the total CPU of a single process.  Return value is number of jiffies
// on success or -1 on error.
int GetProcessCPU(pid_t pid) {
  // Use /proc/<pid>/task to find all threads and parse their /stat file.
  FilePath task_path = GetProcPidDir(pid).Append("task");

  DIR* dir = opendir(task_path.value().c_str());
  if (!dir) {
    DPLOG(ERROR) << "opendir(" << task_path.value() << ")";
    return -1;
  }

  int total_cpu = 0;
  while (struct dirent* ent = readdir(dir)) {
    pid_t tid = ProcDirSlotToPid(ent->d_name);
    if (!tid)
      continue;

    // Synchronously reading files in /proc is safe.
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    std::string stat;
    FilePath stat_path = task_path.Append(ent->d_name).Append(kStatFile);
    if (file_util::ReadFileToString(stat_path, &stat)) {
      int cpu = base::ParseProcStatCPU(stat);
      if (cpu > 0)
        total_cpu += cpu;
    }
  }
  closedir(dir);

  return total_cpu;
}

// Read /proc/<pid>/status and returns the value for |field|, or 0 on failure.
// Only works for fields in the form of "Field: value kB".
int ReadProcStatusAndGetFieldAsInt(pid_t pid, const std::string& field) {
  FilePath stat_file = GetProcPidDir(pid).Append("status");
  std::string status;
  {
    // Synchronously reading files in /proc is safe.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (!file_util::ReadFileToString(stat_file, &status))
      return 0;
  }

  StringTokenizer tokenizer(status, ":\n");
  ParsingState state = KEY_NAME;
  base::StringPiece last_key_name;
  while (tokenizer.GetNext()) {
    switch (state) {
      case KEY_NAME:
        last_key_name = tokenizer.token_piece();
        state = KEY_VALUE;
        break;
      case KEY_VALUE:
        DCHECK(!last_key_name.empty());
        if (last_key_name == field) {
          std::string value_str;
          tokenizer.token_piece().CopyToString(&value_str);
          std::string value_str_trimmed;
          TrimWhitespaceASCII(value_str, TRIM_ALL, &value_str_trimmed);
          std::vector<std::string> split_value_str;
          base::SplitString(value_str_trimmed, ' ', &split_value_str);
          if (split_value_str.size() != 2 || split_value_str[1] != "kB") {
            NOTREACHED();
            return 0;
          }
          int value;
          if (!base::StringToInt(split_value_str[0], &value)) {
            NOTREACHED();
            return 0;
          }
          return value;
        }
        state = KEY_NAME;
        break;
    }
  }
  NOTREACHED();
  return 0;
}

}  // namespace

namespace base {

#if defined(USE_LINUX_BREAKPAD)
size_t g_oom_size = 0U;
#endif

ProcessId GetParentProcessId(ProcessHandle process) {
  ProcessId pid = ReadProcStatsAndGetFieldAsInt(process, VM_PPID);
  if (pid)
    return pid;
  return -1;
}

FilePath GetProcessExecutablePath(ProcessHandle process) {
  FilePath stat_file = GetProcPidDir(process).Append("exe");
  FilePath exe_name;
  if (!file_util::ReadSymbolicLink(stat_file, &exe_name)) {
    // No such process.  Happens frequently in e.g. TerminateAllChromeProcesses
    return FilePath();
  }
  return exe_name;
}

ProcessIterator::ProcessIterator(const ProcessFilter* filter)
    : filter_(filter) {
  procfs_dir_ = opendir(kProcDir);
}

ProcessIterator::~ProcessIterator() {
  if (procfs_dir_) {
    closedir(procfs_dir_);
    procfs_dir_ = NULL;
  }
}

bool ProcessIterator::CheckForNextProcess() {
  // TODO(port): skip processes owned by different UID

  pid_t pid = kNullProcessId;
  std::vector<std::string> cmd_line_args;
  std::string stats_data;
  std::vector<std::string> proc_stats;

  // Arbitrarily guess that there will never be more than 200 non-process
  // files in /proc.  Hardy has 53 and Lucid has 61.
  int skipped = 0;
  const int kSkipLimit = 200;
  while (skipped < kSkipLimit) {
    dirent* slot = readdir(procfs_dir_);
    // all done looking through /proc?
    if (!slot)
      return false;

    // If not a process, keep looking for one.
    pid = ProcDirSlotToPid(slot->d_name);
    if (!pid) {
      skipped++;
      continue;
    }

    if (!GetProcCmdline(pid, &cmd_line_args))
      continue;

    if (!ReadProcStats(pid, &stats_data))
      continue;
    if (!ParseProcStats(stats_data, &proc_stats))
      continue;

    std::string runstate = GetProcStatsFieldAsString(proc_stats, VM_STATE);
    if (runstate.size() != 1) {
      NOTREACHED();
      continue;
    }

    // Is the process in 'Zombie' state, i.e. dead but waiting to be reaped?
    // Allowed values: D R S T Z
    if (runstate[0] != 'Z')
      break;

    // Nope, it's a zombie; somebody isn't cleaning up after their children.
    // (e.g. WaitForProcessesToExit doesn't clean up after dead children yet.)
    // There could be a lot of zombies, can't really decrement i here.
  }
  if (skipped >= kSkipLimit) {
    NOTREACHED();
    return false;
  }

  entry_.pid_ = pid;
  entry_.ppid_ = GetProcStatsFieldAsInt(proc_stats, VM_PPID);
  entry_.gid_ = GetProcStatsFieldAsInt(proc_stats, VM_PGRP);
  entry_.cmd_line_args_.assign(cmd_line_args.begin(), cmd_line_args.end());

  // TODO(port): read pid's commandline's $0, like killall does.  Using the
  // short name between openparen and closeparen won't work for long names!
  entry_.exe_file_ = GetProcStatsFieldAsString(proc_stats, VM_COMM);
  return true;
}

bool NamedProcessIterator::IncludeEntry() {
  if (executable_name_ != entry().exe_file())
    return false;
  return ProcessIterator::IncludeEntry();
}


// static
ProcessMetrics* ProcessMetrics::CreateProcessMetrics(ProcessHandle process) {
  return new ProcessMetrics(process);
}

// On linux, we return vsize.
size_t ProcessMetrics::GetPagefileUsage() const {
  return ReadProcStatsAndGetFieldAsInt(process_, VM_VSIZE);
}

// On linux, we return the high water mark of vsize.
size_t ProcessMetrics::GetPeakPagefileUsage() const {
  return ReadProcStatusAndGetFieldAsInt(process_, "VmPeak") * 1024;
}

// On linux, we return RSS.
size_t ProcessMetrics::GetWorkingSetSize() const {
  return ReadProcStatsAndGetFieldAsInt(process_, VM_RSS) * getpagesize();
}

// On linux, we return the high water mark of RSS.
size_t ProcessMetrics::GetPeakWorkingSetSize() const {
  return ReadProcStatusAndGetFieldAsInt(process_, "VmHWM") * 1024;
}

bool ProcessMetrics::GetMemoryBytes(size_t* private_bytes,
                                    size_t* shared_bytes) {
  WorkingSetKBytes ws_usage;
  if (!GetWorkingSetKBytes(&ws_usage))
    return false;

  if (private_bytes)
    *private_bytes = ws_usage.priv * 1024;

  if (shared_bytes)
    *shared_bytes = ws_usage.shared * 1024;

  return true;
}

// Private and Shared working set sizes are obtained from /proc/<pid>/statm.
bool ProcessMetrics::GetWorkingSetKBytes(WorkingSetKBytes* ws_usage) const {
  // Use statm instead of smaps because smaps is:
  // a) Large and slow to parse.
  // b) Unavailable in the SUID sandbox.

  // First we need to get the page size, since everything is measured in pages.
  // For details, see: man 5 proc.
  const int page_size_kb = getpagesize() / 1024;
  if (page_size_kb <= 0)
    return false;

  std::string statm;
  {
    FilePath statm_file = GetProcPidDir(process_).Append("statm");
    // Synchronously reading files in /proc is safe.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    bool ret = file_util::ReadFileToString(statm_file, &statm);
    if (!ret || statm.length() == 0)
      return false;
  }

  std::vector<std::string> statm_vec;
  base::SplitString(statm, ' ', &statm_vec);
  if (statm_vec.size() != 7)
    return false;  // Not the format we expect.

  int statm_rss, statm_shared;
  base::StringToInt(statm_vec[1], &statm_rss);
  base::StringToInt(statm_vec[2], &statm_shared);

  ws_usage->priv = (statm_rss - statm_shared) * page_size_kb;
  ws_usage->shared = statm_shared * page_size_kb;

  // Sharable is not calculated, as it does not provide interesting data.
  ws_usage->shareable = 0;

  return true;
}

double ProcessMetrics::GetCPUUsage() {
  // This queries the /proc-specific scaling factor which is
  // conceptually the system hertz.  To dump this value on another
  // system, try
  //   od -t dL /proc/self/auxv
  // and look for the number after 17 in the output; mine is
  //   0000040          17         100           3   134512692
  // which means the answer is 100.
  // It may be the case that this value is always 100.
  static const int kHertz = sysconf(_SC_CLK_TCK);

  struct timeval now;
  int retval = gettimeofday(&now, NULL);
  if (retval)
    return 0;
  int64 time = TimeValToMicroseconds(now);

  if (last_time_ == 0) {
    // First call, just set the last values.
    last_time_ = time;
    last_cpu_ = GetProcessCPU(process_);
    return 0;
  }

  int64 time_delta = time - last_time_;
  DCHECK_NE(time_delta, 0);
  if (time_delta == 0)
    return 0;

  int cpu = GetProcessCPU(process_);

  // We have the number of jiffies in the time period.  Convert to percentage.
  // Note this means we will go *over* 100 in the case where multiple threads
  // are together adding to more than one CPU's worth.
  int percentage = 100 * (cpu - last_cpu_) /
      (kHertz * TimeDelta::FromMicroseconds(time_delta).InSecondsF());

  last_time_ = time;
  last_cpu_ = cpu;

  return percentage;
}

// To have /proc/self/io file you must enable CONFIG_TASK_IO_ACCOUNTING
// in your kernel configuration.
bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) const {
  // Synchronously reading files in /proc is safe.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  std::string proc_io_contents;
  FilePath io_file = GetProcPidDir(process_).Append("io");
  if (!file_util::ReadFileToString(io_file, &proc_io_contents))
    return false;

  (*io_counters).OtherOperationCount = 0;
  (*io_counters).OtherTransferCount = 0;

  StringTokenizer tokenizer(proc_io_contents, ": \n");
  ParsingState state = KEY_NAME;
  StringPiece last_key_name;
  while (tokenizer.GetNext()) {
    switch (state) {
      case KEY_NAME:
        last_key_name = tokenizer.token_piece();
        state = KEY_VALUE;
        break;
      case KEY_VALUE:
        DCHECK(!last_key_name.empty());
        if (last_key_name == "syscr") {
          base::StringToInt64(tokenizer.token_piece(),
              reinterpret_cast<int64*>(&(*io_counters).ReadOperationCount));
        } else if (last_key_name == "syscw") {
          base::StringToInt64(tokenizer.token_piece(),
              reinterpret_cast<int64*>(&(*io_counters).WriteOperationCount));
        } else if (last_key_name == "rchar") {
          base::StringToInt64(tokenizer.token_piece(),
              reinterpret_cast<int64*>(&(*io_counters).ReadTransferCount));
        } else if (last_key_name == "wchar") {
          base::StringToInt64(tokenizer.token_piece(),
              reinterpret_cast<int64*>(&(*io_counters).WriteTransferCount));
        }
        state = KEY_NAME;
        break;
    }
  }
  return true;
}

ProcessMetrics::ProcessMetrics(ProcessHandle process)
    : process_(process),
      last_time_(0),
      last_system_time_(0),
      last_cpu_(0) {
  processor_count_ = base::SysInfo::NumberOfProcessors();
}


// Exposed for testing.
int ParseProcStatCPU(const std::string& input) {
  std::vector<std::string> proc_stats;
  if (!ParseProcStats(input, &proc_stats))
    return -1;

  if (proc_stats.size() <= VM_STIME)
    return -1;
  int utime = GetProcStatsFieldAsInt(proc_stats, VM_UTIME);
  int stime = GetProcStatsFieldAsInt(proc_stats, VM_STIME);
  return utime + stime;
}

namespace {

// The format of /proc/meminfo is:
//
// MemTotal:      8235324 kB
// MemFree:       1628304 kB
// Buffers:        429596 kB
// Cached:        4728232 kB
// ...
const size_t kMemTotalIndex = 1;
const size_t kMemFreeIndex = 4;
const size_t kMemBuffersIndex = 7;
const size_t kMemCachedIndex = 10;
const size_t kMemActiveAnonIndex = 22;
const size_t kMemInactiveAnonIndex = 25;
const size_t kMemActiveFileIndex = 28;
const size_t kMemInactiveFileIndex = 31;

}  // namespace

SystemMemoryInfoKB::SystemMemoryInfoKB()
    : total(0),
      free(0),
      buffers(0),
      cached(0),
      active_anon(0),
      inactive_anon(0),
      active_file(0),
      inactive_file(0),
      shmem(0) {
}

bool GetSystemMemoryInfo(SystemMemoryInfoKB* meminfo) {
  // Synchronously reading files in /proc is safe.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  // Used memory is: total - free - buffers - caches
  FilePath meminfo_file("/proc/meminfo");
  std::string meminfo_data;
  if (!file_util::ReadFileToString(meminfo_file, &meminfo_data)) {
    DLOG(WARNING) << "Failed to open " << meminfo_file.value();
    return false;
  }
  std::vector<std::string> meminfo_fields;
  SplitStringAlongWhitespace(meminfo_data, &meminfo_fields);

  if (meminfo_fields.size() < kMemCachedIndex) {
    DLOG(WARNING) << "Failed to parse " << meminfo_file.value()
                  << ".  Only found " << meminfo_fields.size() << " fields.";
    return false;
  }

  DCHECK_EQ(meminfo_fields[kMemTotalIndex-1], "MemTotal:");
  DCHECK_EQ(meminfo_fields[kMemFreeIndex-1], "MemFree:");
  DCHECK_EQ(meminfo_fields[kMemBuffersIndex-1], "Buffers:");
  DCHECK_EQ(meminfo_fields[kMemCachedIndex-1], "Cached:");
  DCHECK_EQ(meminfo_fields[kMemActiveAnonIndex-1], "Active(anon):");
  DCHECK_EQ(meminfo_fields[kMemInactiveAnonIndex-1], "Inactive(anon):");
  DCHECK_EQ(meminfo_fields[kMemActiveFileIndex-1], "Active(file):");
  DCHECK_EQ(meminfo_fields[kMemInactiveFileIndex-1], "Inactive(file):");

  base::StringToInt(meminfo_fields[kMemTotalIndex], &meminfo->total);
  base::StringToInt(meminfo_fields[kMemFreeIndex], &meminfo->free);
  base::StringToInt(meminfo_fields[kMemBuffersIndex], &meminfo->buffers);
  base::StringToInt(meminfo_fields[kMemCachedIndex], &meminfo->cached);
  base::StringToInt(meminfo_fields[kMemActiveAnonIndex], &meminfo->active_anon);
  base::StringToInt(meminfo_fields[kMemInactiveAnonIndex],
                    &meminfo->inactive_anon);
  base::StringToInt(meminfo_fields[kMemActiveFileIndex], &meminfo->active_file);
  base::StringToInt(meminfo_fields[kMemInactiveFileIndex],
                    &meminfo->inactive_file);
#if defined(OS_CHROMEOS)
  // Chrome OS has a tweaked kernel that allows us to query Shmem, which is
  // usually video memory otherwise invisible to the OS.  Unfortunately, the
  // meminfo format varies on different hardware so we have to search for the
  // string.  It always appears after "Cached:".
  for (size_t i = kMemCachedIndex+2; i < meminfo_fields.size(); i += 3) {
    if (meminfo_fields[i] == "Shmem:") {
      base::StringToInt(meminfo_fields[i+1], &meminfo->shmem);
      break;
    }
  }
#endif
  return true;
}

size_t GetSystemCommitCharge() {
  SystemMemoryInfoKB meminfo;
  if (!GetSystemMemoryInfo(&meminfo))
    return 0;
  return meminfo.total - meminfo.free - meminfo.buffers - meminfo.cached;
}

namespace {

void OnNoMemorySize(size_t size) {
#if defined(USE_LINUX_BREAKPAD)
  g_oom_size = size;
#endif

  if (size != 0)
    LOG(FATAL) << "Out of memory, size = " << size;
  LOG(FATAL) << "Out of memory.";
}

void OnNoMemory() {
  OnNoMemorySize(0);
}

}  // namespace

extern "C" {
#if !defined(USE_TCMALLOC) && !defined(ADDRESS_SANITIZER) && \
    !defined(OS_ANDROID)

extern "C" {
void* __libc_malloc(size_t size);
void* __libc_realloc(void* ptr, size_t size);
void* __libc_calloc(size_t nmemb, size_t size);
void* __libc_valloc(size_t size);
void* __libc_pvalloc(size_t size);
void* __libc_memalign(size_t alignment, size_t size);
}  // extern "C"

// Overriding the system memory allocation functions:
//
// For security reasons, we want malloc failures to be fatal. Too much code
// doesn't check for a NULL return value from malloc and unconditionally uses
// the resulting pointer. If the first offset that they try to access is
// attacker controlled, then the attacker can direct the code to access any
// part of memory.
//
// Thus, we define all the standard malloc functions here and mark them as
// visibility 'default'. This means that they replace the malloc functions for
// all Chromium code and also for all code in shared libraries. There are tests
// for this in process_util_unittest.cc.
//
// If we are using tcmalloc, then the problem is moot since tcmalloc handles
// this for us. Thus this code is in a !defined(USE_TCMALLOC) block.
//
// If we are testing the binary with AddressSanitizer, we should not
// redefine malloc and let AddressSanitizer do it instead.
//
// We call the real libc functions in this code by using __libc_malloc etc.
// Previously we tried using dlsym(RTLD_NEXT, ...) but that failed depending on
// the link order. Since ld.so needs calloc during symbol resolution, it
// defines its own versions of several of these functions in dl-minimal.c.
// Depending on the runtime library order, dlsym ended up giving us those
// functions and bad things happened. See crbug.com/31809
//
// This means that any code which calls __libc_* gets the raw libc versions of
// these functions.

#define DIE_ON_OOM_1(function_name) \
  void* function_name(size_t) __attribute__ ((visibility("default"))); \
  \
  void* function_name(size_t size) { \
    void* ret = __libc_##function_name(size); \
    if (ret == NULL && size != 0) \
      OnNoMemorySize(size); \
    return ret; \
  }

#define DIE_ON_OOM_2(function_name, arg1_type) \
  void* function_name(arg1_type, size_t) \
      __attribute__ ((visibility("default"))); \
  \
  void* function_name(arg1_type arg1, size_t size) { \
    void* ret = __libc_##function_name(arg1, size); \
    if (ret == NULL && size != 0) \
      OnNoMemorySize(size); \
    return ret; \
  }

DIE_ON_OOM_1(malloc)
DIE_ON_OOM_1(valloc)
DIE_ON_OOM_1(pvalloc)

DIE_ON_OOM_2(calloc, size_t)
DIE_ON_OOM_2(realloc, void*)
DIE_ON_OOM_2(memalign, size_t)

// posix_memalign has a unique signature and doesn't have a __libc_ variant.
int posix_memalign(void** ptr, size_t alignment, size_t size)
    __attribute__ ((visibility("default")));

int posix_memalign(void** ptr, size_t alignment, size_t size) {
  // This will use the safe version of memalign, above.
  *ptr = memalign(alignment, size);
  return 0;
}

#endif  // !defined(USE_TCMALLOC)
}  // extern C

void EnableTerminationOnHeapCorruption() {
  // On Linux, there nothing to do AFAIK.
}

void EnableTerminationOnOutOfMemory() {
#if defined(OS_ANDROID)
  // Android doesn't support setting a new handler.
  DLOG(WARNING) << "Not feasible.";
#else
  // Set the new-out of memory handler.
  std::set_new_handler(&OnNoMemory);
  // If we're using glibc's allocator, the above functions will override
  // malloc and friends and make them die on out of memory.
#endif
}

// NOTE: This is not the only version of this function in the source:
// the setuid sandbox (in process_util_linux.c, in the sandbox source)
// also has its own C version.
bool AdjustOOMScore(ProcessId process, int score) {
  if (score < 0 || score > kMaxOomScore)
    return false;

  FilePath oom_path(GetProcPidDir(process));

  // Attempt to write the newer oom_score_adj file first.
  FilePath oom_file = oom_path.AppendASCII("oom_score_adj");
  if (file_util::PathExists(oom_file)) {
    std::string score_str = base::IntToString(score);
    DVLOG(1) << "Adjusting oom_score_adj of " << process << " to "
             << score_str;
    int score_len = static_cast<int>(score_str.length());
    return (score_len == file_util::WriteFile(oom_file,
                                              score_str.c_str(),
                                              score_len));
  }

  // If the oom_score_adj file doesn't exist, then we write the old
  // style file and translate the oom_adj score to the range 0-15.
  oom_file = oom_path.AppendASCII("oom_adj");
  if (file_util::PathExists(oom_file)) {
    // Max score for the old oom_adj range.  Used for conversion of new
    // values to old values.
    const int kMaxOldOomScore = 15;

    int converted_score = score * kMaxOldOomScore / kMaxOomScore;
    std::string score_str = base::IntToString(converted_score);
    DVLOG(1) << "Adjusting oom_adj of " << process << " to " << score_str;
    int score_len = static_cast<int>(score_str.length());
    return (score_len == file_util::WriteFile(oom_file,
                                              score_str.c_str(),
                                              score_len));
  }

  return false;
}

}  // namespace base
