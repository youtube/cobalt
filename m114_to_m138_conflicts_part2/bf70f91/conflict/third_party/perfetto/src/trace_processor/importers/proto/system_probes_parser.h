/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_SYSTEM_PROBES_PARSER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_SYSTEM_PROBES_PARSER_H_

<<<<<<< HEAD
#include <cstddef>
#include <cstdint>
#include <vector>

#include "perfetto/protozero/field.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto::trace_processor {
=======
#include <array>
#include <set>
#include <vector>

#include "perfetto/protozero/field.h"
#include "protos/perfetto/trace/sys_stats/sys_stats.pbzero.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

class TraceProcessorContext;

class SystemProbesParser {
 public:
  using ConstBytes = protozero::ConstBytes;
  using ConstChars = protozero::ConstChars;

  explicit SystemProbesParser(TraceProcessorContext*);

  void ParseProcessTree(ConstBytes);
<<<<<<< HEAD
  void ParseProcessStats(int64_t ts, ConstBytes);
=======
  void ParseProcessStats(int64_t timestamp, ConstBytes);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  void ParseSysStats(int64_t ts, ConstBytes);
  void ParseSystemInfo(ConstBytes);
  void ParseCpuInfo(ConstBytes);

 private:
  void ParseThreadStats(int64_t timestamp, uint32_t pid, ConstBytes);
  void ParseDiskStats(int64_t ts, ConstBytes blob);
  void ParseProcessFds(int64_t ts, uint32_t pid, ConstBytes);
<<<<<<< HEAD
  void ParseCpuIdleStats(int64_t ts, ConstBytes);
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  TraceProcessorContext* const context_;

  const StringId utid_name_id_;
<<<<<<< HEAD
  const StringId is_kthread_id_;

  // Arm CPU identifier string IDs
  const StringId arm_cpu_implementer;
  const StringId arm_cpu_architecture;
  const StringId arm_cpu_variant;
  const StringId arm_cpu_part;
  const StringId arm_cpu_revision;

  std::vector<const char*> meminfo_strs_;
  std::vector<const char*> vmstat_strs_;

=======
  const StringId num_forks_name_id_;
  const StringId num_irq_total_name_id_;
  const StringId num_softirq_total_name_id_;
  const StringId num_irq_name_id_;
  const StringId num_softirq_name_id_;
  const StringId cpu_times_user_ns_id_;
  const StringId cpu_times_user_nice_ns_id_;
  const StringId cpu_times_system_mode_ns_id_;
  const StringId cpu_times_idle_ns_id_;
  const StringId cpu_times_io_wait_ns_id_;
  const StringId cpu_times_irq_ns_id_;
  const StringId cpu_times_softirq_ns_id_;
  const StringId oom_score_adj_id_;
  const StringId cpu_freq_id_;
  std::vector<StringId> meminfo_strs_id_;
  std::vector<StringId> vmstat_strs_id_;

  // Maps a proto field number for memcounters in ProcessStats::Process to
  // their StringId. Keep kProcStatsProcessSize equal to 1 + max proto field
  // id of ProcessStats::Process. Also update the value in
  // ChromeSystemProbesParser.
  static constexpr size_t kProcStatsProcessSize = 15;
  std::array<StringId, kProcStatsProcessSize> proc_stats_process_names_{};

  uint64_t ms_per_tick_ = 0;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  uint32_t page_size_ = 0;

  int64_t prev_read_amount = -1;
  int64_t prev_write_amount = -1;
  int64_t prev_discard_amount = -1;
  int64_t prev_flush_count = -1;
  int64_t prev_read_time = -1;
  int64_t prev_write_time = -1;
  int64_t prev_discard_time = -1;
  int64_t prev_flush_time = -1;
};

<<<<<<< HEAD
}  // namespace perfetto::trace_processor
=======
}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_SYSTEM_PROBES_PARSER_H_
