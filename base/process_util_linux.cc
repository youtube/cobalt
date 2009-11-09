// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"

namespace {

enum ParsingState {
  KEY_NAME,
  KEY_VALUE
};

// Reads /proc/<pid>/stat and populates |proc_stats| with the values split by
// spaces.
void GetProcStats(pid_t pid, std::vector<std::string>* proc_stats) {
  FilePath stat_file("/proc");
  stat_file = stat_file.Append(IntToString(pid));
  stat_file = stat_file.Append("stat");
  std::string mem_stats;
  if (!file_util::ReadFileToString(stat_file, &mem_stats))
    return;
  SplitString(mem_stats, ' ', proc_stats);
}

}  // namespace

namespace base {

ProcessId GetParentProcessId(ProcessHandle process) {
  FilePath stat_file("/proc");
  stat_file = stat_file.Append(IntToString(process));
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
          pid_t ppid = StringToInt(tokenizer.token());
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
  stat_file = stat_file.Append(IntToString(process));
  stat_file = stat_file.Append("exe");
  char exename[2048];
  ssize_t len = readlink(stat_file.value().c_str(), exename, sizeof(exename));
  if (len < 1) {
    // No such process.  Happens frequently in e.g. TerminateAllChromeProcesses
    return FilePath();
  }
  return FilePath(std::string(exename, len));
}

NamedProcessIterator::NamedProcessIterator(const std::wstring& executable_name,
                                           const ProcessFilter* filter)
    : executable_name_(executable_name), filter_(filter) {
  procfs_dir_ = opendir("/proc");
}

NamedProcessIterator::~NamedProcessIterator() {
  if (procfs_dir_) {
    closedir(procfs_dir_);
    procfs_dir_ = NULL;
  }
}

const ProcessEntry* NamedProcessIterator::NextProcessEntry() {
  bool result = false;
  do {
    result = CheckForNextProcess();
  } while (result && !IncludeEntry());

  if (result)
    return &entry_;

  return NULL;
}

bool NamedProcessIterator::CheckForNextProcess() {
  // TODO(port): skip processes owned by different UID

  dirent* slot = 0;
  const char* openparen;
  const char* closeparen;

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
    // %d (%s) %c %d ...
    // pid (name) runstate ppid
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

  entry_.pid = atoi(slot->d_name);
  entry_.ppid = atoi(closeparen + 3);

  // TODO(port): read pid's commandline's $0, like killall does.  Using the
  // short name between openparen and closeparen won't work for long names!
  int len = closeparen - openparen - 1;
  if (len > NAME_MAX)
    len = NAME_MAX;
  memcpy(entry_.szExeFile, openparen + 1, len);
  entry_.szExeFile[len] = 0;

  return true;
}

bool NamedProcessIterator::IncludeEntry() {
  // TODO(port): make this also work for non-ASCII filenames
  if (WideToASCII(executable_name_) != entry_.szExeFile)
    return false;
  if (!filter_)
    return true;
  return filter_->Includes(entry_.pid, entry_.ppid);
}

// On linux, we return vsize.
size_t ProcessMetrics::GetPagefileUsage() const {
  std::vector<std::string> proc_stats;
  GetProcStats(process_, &proc_stats);
  const size_t kVmSize = 22;
  if (proc_stats.size() > kVmSize)
    return static_cast<size_t>(StringToInt(proc_stats[kVmSize]));
  return 0;
}

// On linux, we return the high water mark of vsize.
size_t ProcessMetrics::GetPeakPagefileUsage() const {
  std::vector<std::string> proc_stats;
  GetProcStats(process_, &proc_stats);
  const size_t kVmPeak = 21;
  if (proc_stats.size() > kVmPeak)
    return static_cast<size_t>(StringToInt(proc_stats[kVmPeak]));
  return 0;
}

// On linux, we return RSS.
size_t ProcessMetrics::GetWorkingSetSize() const {
  std::vector<std::string> proc_stats;
  GetProcStats(process_, &proc_stats);
  const size_t kVmRss = 23;
  if (proc_stats.size() > kVmRss) {
    size_t num_pages = static_cast<size_t>(StringToInt(proc_stats[kVmRss]));
    return num_pages * getpagesize();
  }
  return 0;
}

// On linux, we return the high water mark of RSS.
size_t ProcessMetrics::GetPeakWorkingSetSize() const {
  std::vector<std::string> proc_stats;
  GetProcStats(process_, &proc_stats);
  const size_t kVmHwm = 23;
  if (proc_stats.size() > kVmHwm) {
    size_t num_pages = static_cast<size_t>(StringToInt(proc_stats[kVmHwm]));
    return num_pages * getpagesize();
  }
  return 0;
}

size_t ProcessMetrics::GetPrivateBytes() const {
  WorkingSetKBytes ws_usage;
  GetWorkingSetKBytes(&ws_usage);
  return ws_usage.priv << 10;
}

// Private and Shared working set sizes are obtained from /proc/<pid>/smaps.
// When that's not available, use the values from /proc<pid>/statm as a
// close approximation.
// See http://www.pixelbeat.org/scripts/ps_mem.py
bool ProcessMetrics::GetWorkingSetKBytes(WorkingSetKBytes* ws_usage) const {
  FilePath stat_file =
    FilePath("/proc").Append(IntToString(process_)).Append("smaps");
  std::string smaps;
  int private_kb = 0;
  int pss_kb = 0;
  bool have_pss = false;
  if (file_util::ReadFileToString(stat_file, &smaps) && smaps.length() > 0) {
    StringTokenizer tokenizer(smaps, ":\n");
    ParsingState state = KEY_NAME;
    std::string last_key_name;
    while (tokenizer.GetNext()) {
      switch (state) {
        case KEY_NAME:
          last_key_name = tokenizer.token();
          state = KEY_VALUE;
          break;
        case KEY_VALUE:
          if (last_key_name.empty()) {
            NOTREACHED();
            return false;
          }
          if (StartsWithASCII(last_key_name, "Private_", 1)) {
            private_kb += StringToInt(tokenizer.token());
          } else if (StartsWithASCII(last_key_name, "Pss", 1)) {
            have_pss = true;
            pss_kb += StringToInt(tokenizer.token());
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

    stat_file =
        FilePath("/proc").Append(IntToString(process_)).Append("statm");
    std::string statm;
    if (!file_util::ReadFileToString(stat_file, &statm) || statm.length() == 0)
      return false;

    std::vector<std::string> statm_vec;
    SplitString(statm, ' ', &statm_vec);
    if (statm_vec.size() != 7)
      return false;  // Not the format we expect.
    private_kb = StringToInt(statm_vec[1]) - StringToInt(statm_vec[2]);
    private_kb *= page_size_kb;
  }
  ws_usage->priv = private_kb;
  // Sharable is not calculated, as it does not provide interesting data.
  ws_usage->shareable = 0;

  ws_usage->shared = 0;
  if (have_pss)
    ws_usage->shared = pss_kb;
  return true;
}

// To have /proc/self/io file you must enable CONFIG_TASK_IO_ACCOUNTING
// in your kernel configuration.
bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) const {
  std::string proc_io_contents;
  FilePath io_file("/proc");
  io_file = io_file.Append(IntToString(process_));
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
          (*io_counters).ReadOperationCount = StringToInt64(tokenizer.token());
        } else if (last_key_name == "syscw") {
          (*io_counters).WriteOperationCount = StringToInt64(tokenizer.token());
        } else if (last_key_name == "rchar") {
          (*io_counters).ReadTransferCount = StringToInt64(tokenizer.token());
        } else if (last_key_name == "wchar") {
          (*io_counters).WriteTransferCount = StringToInt64(tokenizer.token());
        }
        state = KEY_NAME;
        break;
    }
  }
  return true;
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
  SplitString(input.substr(rparen + 2), ' ', &fields);
  if (fields.size() < 13)
    return -1;  // Output not in the format we expect.

  return StringToInt(fields[11]) + StringToInt(fields[12]);
}

// Get the total CPU of a single process.  Return value is number of jiffies
// on success or -1 on error.
static int GetProcessCPU(pid_t pid) {
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
      int cpu = ParseProcStatCPU(stat);
      if (cpu > 0)
        total_cpu += cpu;
    }
  }
  closedir(dir);

  return total_cpu;
}

int ProcessMetrics::GetCPUUsage() {
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
  // Used memory is: total - free - buffers - caches
  FilePath meminfo_file("/proc/meminfo");
  std::string meminfo_data;
  if (!file_util::ReadFileToString(meminfo_file, &meminfo_data))
    LOG(ERROR) << "Failed to open /proc/meminfo.";
    return 0;
  std::vector<std::string> meminfo_fields;
  SplitStringAlongWhitespace(meminfo_data, &meminfo_fields);

  if (meminfo_fields.size() < kMemCacheIndex) {
    LOG(ERROR) << "Failed to parse /proc/meminfo.  Only found " <<
      meminfo_fields.size() << " fields.";
    return 0;
  }

  DCHECK_EQ(meminfo_fields[kMemTotalIndex-1], "MemTotal:");
  DCHECK_EQ(meminfo_fields[kMemFreeIndex-1], "MemFree:");
  DCHECK_EQ(meminfo_fields[kMemBuffersIndex-1], "Buffers:");
  DCHECK_EQ(meminfo_fields[kMemCacheIndex-1], "Cached:");

  size_t result_in_kb;
  result_in_kb = StringToInt(meminfo_fields[kMemTotalIndex]);
  result_in_kb -= StringToInt(meminfo_fields[kMemFreeIndex]);
  result_in_kb -= StringToInt(meminfo_fields[kMemBuffersIndex]);
  result_in_kb -= StringToInt(meminfo_fields[kMemCacheIndex]);

  return result_in_kb;
}

}  // namespace base
