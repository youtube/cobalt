// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
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

// Reads /proc/<pid>/stat and populates |proc_stats| with the values split by
// spaces. Returns true if successful.
bool GetProcStats(pid_t pid, std::vector<std::string>* proc_stats) {
  // Synchronously reading files in /proc is safe.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  FilePath stat_file("/proc");
  stat_file = stat_file.Append(base::IntToString(pid));
  stat_file = stat_file.Append("stat");
  std::string mem_stats;
  if (!file_util::ReadFileToString(stat_file, &mem_stats))
    return false;
  base::SplitString(mem_stats, ' ', proc_stats);
  return true;
}

// Reads /proc/<pid>/cmdline and populates |proc_cmd_line_args| with the command
// line arguments. Returns true if successful.
// Note: /proc/<pid>/cmdline contains command line arguments separated by single
// null characters. We tokenize it into a vector of strings using '\0' as a
// delimiter.
bool GetProcCmdline(pid_t pid, std::vector<std::string>* proc_cmd_line_args) {
  // Synchronously reading files in /proc is safe.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  FilePath cmd_line_file("/proc");
  cmd_line_file = cmd_line_file.Append(base::IntToString(pid));
  cmd_line_file = cmd_line_file.Append("cmdline");
  std::string cmd_line;
  if (!file_util::ReadFileToString(cmd_line_file, &cmd_line))
    return false;
  std::string delimiters;
  delimiters.push_back('\0');
  Tokenize(cmd_line, delimiters, proc_cmd_line_args);
  return true;
}

// Get the total CPU of a single process.  Return value is number of jiffies
// on success or -1 on error.
int GetProcessCPU(pid_t pid) {
  // Synchronously reading files in /proc is safe.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  // Use /proc/<pid>/task to find all threads and parse their /stat file.
  FilePath path = FilePath(StringPrintf("/proc/%d/task/", pid));

  DIR* dir = opendir(path.value().c_str());
  if (!dir) {
    PLOG(ERROR) << "opendir(" << path.value() << ")";
    return -1;
  }

  int total_cpu = 0;
  while (struct dirent* ent = readdir(dir)) {
    if (ent->d_name[0] == '.')
      continue;

    FilePath stat_path = path.AppendASCII(ent->d_name).AppendASCII("stat");
    std::string stat;
    if (file_util::ReadFileToString(stat_path, &stat)) {
      int cpu = base::ParseProcStatCPU(stat);
      if (cpu > 0)
        total_cpu += cpu;
    }
  }
  closedir(dir);

  return total_cpu;
}

}  // namespace

namespace base {

ProcessId GetParentProcessId(ProcessHandle process) {
  // Synchronously reading files in /proc is safe.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  FilePath stat_file("/proc");
  stat_file = stat_file.Append(base::IntToString(process));
  stat_file = stat_file.Append("status");
  std::string status;
  if (!file_util::ReadFileToString(stat_file, &status))
    return -1;

  StringTokenizer tokenizer(status, ":\n");
  ParsingState state = KEY_NAME;
  std::string last_key_name;
  while (tokenizer.GetNext()) {
    switch (state) {
      case KEY_NAME:
        last_key_name = tokenizer.token();
        state = KEY_VALUE;
        break;
      case KEY_VALUE:
        DCHECK(!last_key_name.empty());
        if (last_key_name == "PPid") {
          int ppid;
          base::StringToInt(tokenizer.token(), &ppid);
          return ppid;
        }
        state = KEY_NAME;
        break;
    }
  }
  NOTREACHED();
  return -1;
}

FilePath GetProcessExecutablePath(ProcessHandle process) {
  FilePath stat_file("/proc");
  stat_file = stat_file.Append(base::IntToString(process));
  stat_file = stat_file.Append("exe");
  FilePath exe_name;
  if (!file_util::ReadSymbolicLink(stat_file, &exe_name)) {
    // No such process.  Happens frequently in e.g. TerminateAllChromeProcesses
    return FilePath();
  }
  return exe_name;
}

ProcessIterator::ProcessIterator(const ProcessFilter* filter)
    : filter_(filter) {
  procfs_dir_ = opendir("/proc");
}

ProcessIterator::~ProcessIterator() {
  if (procfs_dir_) {
    closedir(procfs_dir_);
    procfs_dir_ = NULL;
  }
}

bool ProcessIterator::CheckForNextProcess() {
  // TODO(port): skip processes owned by different UID

  dirent* slot = 0;
  const char* openparen;
  const char* closeparen;
  std::vector<std::string> cmd_line_args;

  // Arbitrarily guess that there will never be more than 200 non-process
  // files in /proc.  Hardy has 53.
  int skipped = 0;
  const int kSkipLimit = 200;
  while (skipped < kSkipLimit) {
    slot = readdir(procfs_dir_);
    // all done looking through /proc?
    if (!slot)
      return false;

    // If not a process, keep looking for one.
    bool notprocess = false;
    int i;
    for (i = 0; i < NAME_MAX && slot->d_name[i]; ++i) {
       if (!isdigit(slot->d_name[i])) {
         notprocess = true;
         break;
       }
    }
    if (i == NAME_MAX || notprocess) {
      skipped++;
      continue;
    }

    // Read the process's command line.
    std::string pid_string(slot->d_name);
    int pid;
    if (StringToInt(pid_string, &pid) && !GetProcCmdline(pid, &cmd_line_args))
      return false;

    // Read the process's status.
    char buf[NAME_MAX + 12];
    sprintf(buf, "/proc/%s/stat", slot->d_name);
    FILE *fp = fopen(buf, "r");
    if (!fp)
      return false;
    const char* result = fgets(buf, sizeof(buf), fp);
    fclose(fp);
    if (!result)
      return false;

    // Parse the status.  It is formatted like this:
    // %d (%s) %c %d %d ...
    // pid (name) runstate ppid gid
    // To avoid being fooled by names containing a closing paren, scan
    // backwards.
    openparen = strchr(buf, '(');
    closeparen = strrchr(buf, ')');
    if (!openparen || !closeparen)
      return false;
    char runstate = closeparen[2];

    // Is the process in 'Zombie' state, i.e. dead but waiting to be reaped?
    // Allowed values: D R S T Z
    if (runstate != 'Z')
      break;

    // Nope, it's a zombie; somebody isn't cleaning up after their children.
    // (e.g. WaitForProcessesToExit doesn't clean up after dead children yet.)
    // There could be a lot of zombies, can't really decrement i here.
  }
  if (skipped >= kSkipLimit) {
    NOTREACHED();
    return false;
  }

  // This seems fragile.
  entry_.pid_ = atoi(slot->d_name);
  entry_.ppid_ = atoi(closeparen + 3);
  entry_.gid_ = atoi(strchr(closeparen + 4, ' '));

  entry_.cmd_line_args_.assign(cmd_line_args.begin(), cmd_line_args.end());

  // TODO(port): read pid's commandline's $0, like killall does.  Using the
  // short name between openparen and closeparen won't work for long names!
  int len = closeparen - openparen - 1;
  entry_.exe_file_.assign(openparen + 1, len);
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
  std::vector<std::string> proc_stats;
  if (!GetProcStats(process_, &proc_stats))
    LOG(WARNING) << "Failed to get process stats.";
  const size_t kVmSize = 22;
  if (proc_stats.size() > kVmSize) {
    int vm_size;
    base::StringToInt(proc_stats[kVmSize], &vm_size);
    return static_cast<size_t>(vm_size);
  }
  return 0;
}

// On linux, we return the high water mark of vsize.
size_t ProcessMetrics::GetPeakPagefileUsage() const {
  std::vector<std::string> proc_stats;
  if (!GetProcStats(process_, &proc_stats))
    LOG(WARNING) << "Failed to get process stats.";
  const size_t kVmPeak = 21;
  if (proc_stats.size() > kVmPeak) {
    int vm_peak;
    if (base::StringToInt(proc_stats[kVmPeak], &vm_peak))
      return vm_peak;
  }
  return 0;
}

// On linux, we return RSS.
size_t ProcessMetrics::GetWorkingSetSize() const {
  std::vector<std::string> proc_stats;
  if (!GetProcStats(process_, &proc_stats))
    LOG(WARNING) << "Failed to get process stats.";
  const size_t kVmRss = 23;
  if (proc_stats.size() > kVmRss) {
    int num_pages;
    if (base::StringToInt(proc_stats[kVmRss], &num_pages))
      return static_cast<size_t>(num_pages) * getpagesize();
  }
  return 0;
}

// On linux, we return the high water mark of RSS.
size_t ProcessMetrics::GetPeakWorkingSetSize() const {
  std::vector<std::string> proc_stats;
  if (!GetProcStats(process_, &proc_stats))
    LOG(WARNING) << "Failed to get process stats.";
  const size_t kVmHwm = 23;
  if (proc_stats.size() > kVmHwm) {
    int num_pages;
    base::StringToInt(proc_stats[kVmHwm], &num_pages);
    return static_cast<size_t>(num_pages) * getpagesize();
  }
  return 0;
}

bool ProcessMetrics::GetMemoryBytes(size_t* private_bytes,
                                    size_t* shared_bytes) {
  WorkingSetKBytes ws_usage;
  if (!GetWorkingSetKBytes(&ws_usage))
    return false;

  if (private_bytes)
    *private_bytes = ws_usage.priv << 10;

  if (shared_bytes)
    *shared_bytes = ws_usage.shared * 1024;

  return true;
}

// Private and Shared working set sizes are obtained from /proc/<pid>/smaps.
// When that's not available, use the values from /proc<pid>/statm as a
// close approximation.
// See http://www.pixelbeat.org/scripts/ps_mem.py
bool ProcessMetrics::GetWorkingSetKBytes(WorkingSetKBytes* ws_usage) const {
  // Synchronously reading files in /proc is safe.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  FilePath proc_dir = FilePath("/proc").Append(base::IntToString(process_));
  std::string smaps;
  int private_kb = 0;
  int pss_kb = 0;
  bool have_pss = false;
  bool ret;

  {
    FilePath smaps_file = proc_dir.Append("smaps");
    // Synchronously reading files in /proc is safe.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    ret = file_util::ReadFileToString(smaps_file, &smaps);
  }
  if (ret && smaps.length() > 0) {
    const std::string private_prefix = "Private_";
    const std::string pss_prefix = "Pss";
    StringTokenizer tokenizer(smaps, ":\n");
    StringPiece last_key_name;
    ParsingState state = KEY_NAME;
    while (tokenizer.GetNext()) {
      switch (state) {
        case KEY_NAME:
          last_key_name = tokenizer.token_piece();
          state = KEY_VALUE;
          break;
        case KEY_VALUE:
          if (last_key_name.empty()) {
            NOTREACHED();
            return false;
          }
          if (last_key_name.starts_with(private_prefix)) {
            int cur;
            base::StringToInt(tokenizer.token(), &cur);
            private_kb += cur;
          } else if (last_key_name.starts_with(pss_prefix)) {
            have_pss = true;
            int cur;
            base::StringToInt(tokenizer.token(), &cur);
            pss_kb += cur;
          }
          state = KEY_NAME;
          break;
      }
    }
  } else {
    // Try statm if smaps is empty because of the SUID sandbox.
    // First we need to get the page size though.
    int page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024;
    if (page_size_kb <= 0)
      return false;

    std::string statm;
    {
      FilePath statm_file = proc_dir.Append("statm");
      // Synchronously reading files in /proc is safe.
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      ret = file_util::ReadFileToString(statm_file, &statm);
    }
    if (!ret || statm.length() == 0)
      return false;

    std::vector<std::string> statm_vec;
    base::SplitString(statm, ' ', &statm_vec);
    if (statm_vec.size() != 7)
      return false;  // Not the format we expect.

    int statm1, statm2;
    base::StringToInt(statm_vec[1], &statm1);
    base::StringToInt(statm_vec[2], &statm2);
    private_kb = (statm1 - statm2) * page_size_kb;
  }
  ws_usage->priv = private_kb;
  // Sharable is not calculated, as it does not provide interesting data.
  ws_usage->shareable = 0;

  ws_usage->shared = 0;
  if (have_pss)
    ws_usage->shared = pss_kb;
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
  FilePath io_file("/proc");
  io_file = io_file.Append(base::IntToString(process_));
  io_file = io_file.Append("io");
  if (!file_util::ReadFileToString(io_file, &proc_io_contents))
    return false;

  (*io_counters).OtherOperationCount = 0;
  (*io_counters).OtherTransferCount = 0;

  StringTokenizer tokenizer(proc_io_contents, ": \n");
  ParsingState state = KEY_NAME;
  std::string last_key_name;
  while (tokenizer.GetNext()) {
    switch (state) {
      case KEY_NAME:
        last_key_name = tokenizer.token();
        state = KEY_VALUE;
        break;
      case KEY_VALUE:
        DCHECK(!last_key_name.empty());
        if (last_key_name == "syscr") {
          base::StringToInt64(tokenizer.token(),
              reinterpret_cast<int64*>(&(*io_counters).ReadOperationCount));
        } else if (last_key_name == "syscw") {
          base::StringToInt64(tokenizer.token(),
              reinterpret_cast<int64*>(&(*io_counters).WriteOperationCount));
        } else if (last_key_name == "rchar") {
          base::StringToInt64(tokenizer.token(),
              reinterpret_cast<int64*>(&(*io_counters).ReadTransferCount));
        } else if (last_key_name == "wchar") {
          base::StringToInt64(tokenizer.token(),
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
  // /proc/<pid>/stat contains the process name in parens.  In case the
  // process name itself contains parens, skip past them.
  std::string::size_type rparen = input.rfind(')');
  if (rparen == std::string::npos)
    return -1;

  // From here, we expect a bunch of space-separated fields, where the
  // 0-indexed 11th and 12th are utime and stime.  On two different machines
  // I found 42 and 39 fields, so let's just expect the ones we need.
  std::vector<std::string> fields;
  base::SplitString(input.substr(rparen + 2), ' ', &fields);
  if (fields.size() < 13)
    return -1;  // Output not in the format we expect.

  int fields11, fields12;
  base::StringToInt(fields[11], &fields11);
  base::StringToInt(fields[12], &fields12);
  return fields11 + fields12;
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
const size_t kMemCacheIndex = 10;

}  // namespace

size_t GetSystemCommitCharge() {
  // Synchronously reading files in /proc is safe.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  // Used memory is: total - free - buffers - caches
  FilePath meminfo_file("/proc/meminfo");
  std::string meminfo_data;
  if (!file_util::ReadFileToString(meminfo_file, &meminfo_data)) {
    LOG(WARNING) << "Failed to open /proc/meminfo.";
    return 0;
  }
  std::vector<std::string> meminfo_fields;
  SplitStringAlongWhitespace(meminfo_data, &meminfo_fields);

  if (meminfo_fields.size() < kMemCacheIndex) {
    LOG(WARNING) << "Failed to parse /proc/meminfo.  Only found " <<
      meminfo_fields.size() << " fields.";
    return 0;
  }

  DCHECK_EQ(meminfo_fields[kMemTotalIndex-1], "MemTotal:");
  DCHECK_EQ(meminfo_fields[kMemFreeIndex-1], "MemFree:");
  DCHECK_EQ(meminfo_fields[kMemBuffersIndex-1], "Buffers:");
  DCHECK_EQ(meminfo_fields[kMemCacheIndex-1], "Cached:");

  int mem_total, mem_free, mem_buffers, mem_cache;
  base::StringToInt(meminfo_fields[kMemTotalIndex], &mem_total);
  base::StringToInt(meminfo_fields[kMemFreeIndex], &mem_free);
  base::StringToInt(meminfo_fields[kMemBuffersIndex], &mem_buffers);
  base::StringToInt(meminfo_fields[kMemCacheIndex], &mem_cache);

  return mem_total - mem_free - mem_buffers - mem_cache;
}

namespace {

void OnNoMemorySize(size_t size) {
  if (size != 0)
    LOG(FATAL) << "Out of memory, size = " << size;
  LOG(FATAL) << "Out of memory.";
}

void OnNoMemory() {
  OnNoMemorySize(0);
}

}  // namespace

extern "C" {
#if !defined(USE_TCMALLOC)

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

void EnableTerminationOnOutOfMemory() {
  // Set the new-out of memory handler.
  std::set_new_handler(&OnNoMemory);
  // If we're using glibc's allocator, the above functions will override
  // malloc and friends and make them die on out of memory.
}

bool AdjustOOMScore(ProcessId process, int score) {
  if (score < 0 || score > 15)
    return false;

  FilePath oom_adj("/proc");
  oom_adj = oom_adj.Append(base::Int64ToString(process));
  oom_adj = oom_adj.AppendASCII("oom_adj");

  if (!file_util::PathExists(oom_adj))
    return false;

  std::string score_str = base::IntToString(score);
  return (static_cast<int>(score_str.length()) ==
          file_util::WriteFile(oom_adj, score_str.c_str(), score_str.length()));
}

}  // namespace base
