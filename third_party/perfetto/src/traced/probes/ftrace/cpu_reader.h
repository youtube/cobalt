/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef SRC_TRACED_PROBES_FTRACE_CPU_READER_H_
#define SRC_TRACED_PROBES_FTRACE_CPU_READER_H_

#include <stdint.h>
#include <string.h>

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <set>
#include <thread>

#include "perfetto/ext/base/paged_memory.h"
#include "perfetto/ext/base/pipe.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/thread_checker.h"
#include "perfetto/ext/traced/data_source_types.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/protozero/message.h"
#include "perfetto/protozero/message_handle.h"
#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "src/kallsyms/kernel_symbol_map.h"
#include "src/kallsyms/lazy_kernel_symbolizer.h"
#include "src/traced/probes/ftrace/compact_sched.h"
#include "src/traced/probes/ftrace/ftrace_metadata.h"
#include "src/traced/probes/ftrace/proto_translation_table.h"

namespace perfetto {

class FtraceDataSource;
class LazyKernelSymbolizer;
class ProtoTranslationTable;
struct FtraceClockSnapshot;
struct FtraceDataSourceConfig;

namespace protos {
namespace pbzero {
class FtraceEventBundle;
enum FtraceClock : int32_t;
}  // namespace pbzero
}  // namespace protos

// Reads raw ftrace data for a cpu, parses it, and writes it into the perfetto
// tracing buffers.
class CpuReader {
 public:
  using FtraceEventBundle = protos::pbzero::FtraceEventBundle;

  // Helper class to generate `TracePacket`s when needed. Public for testing.
  class Bundler {
   public:
    Bundler(TraceWriter* trace_writer,
            FtraceMetadata* metadata,
            LazyKernelSymbolizer* symbolizer,
            size_t cpu,
            const FtraceClockSnapshot* ftrace_clock_snapshot,
            protos::pbzero::FtraceClock ftrace_clock,
            bool compact_sched_enabled)
        : trace_writer_(trace_writer),
          metadata_(metadata),
          symbolizer_(symbolizer),
          cpu_(cpu),
          ftrace_clock_snapshot_(ftrace_clock_snapshot),
          ftrace_clock_(ftrace_clock),
          compact_sched_enabled_(compact_sched_enabled) {}

    ~Bundler() { FinalizeAndRunSymbolizer(); }

    protos::pbzero::FtraceEventBundle* GetOrCreateBundle() {
      if (!bundle_) {
        StartNewPacket(false);
      }
      return bundle_;
    }

    // Forces the creation of a new TracePacket.
    void StartNewPacket(bool lost_events);

    // This function is called after the contents of a FtraceBundle are written.
    void FinalizeAndRunSymbolizer();

    CompactSchedBuffer* compact_sched_buffer() {
      // FinalizeAndRunSymbolizer will only process the compact_sched_buffer_ if
      // there is an open bundle.
      GetOrCreateBundle();
      return &compact_sched_buffer_;
    }

   private:
    TraceWriter* const trace_writer_;         // Never nullptr.
    FtraceMetadata* const metadata_;          // Never nullptr.
    LazyKernelSymbolizer* const symbolizer_;  // Can be nullptr.
    const size_t cpu_;
    const FtraceClockSnapshot* const ftrace_clock_snapshot_;
    protos::pbzero::FtraceClock const ftrace_clock_;
    const bool compact_sched_enabled_;

    TraceWriter::TracePacketHandle packet_;
    protos::pbzero::FtraceEventBundle* bundle_ = nullptr;
    // Allocate the buffer for compact scheduler events (which will be unused if
    // the compact option isn't enabled).
    CompactSchedBuffer compact_sched_buffer_;
  };

  struct PageHeader {
    uint64_t timestamp;
    uint64_t size;
    bool lost_events;
  };

  CpuReader(size_t cpu,
            const ProtoTranslationTable* table,
            LazyKernelSymbolizer* symbolizer,
            const FtraceClockSnapshot*,
            base::ScopedFile trace_fd);
  ~CpuReader();

  // Reads and parses all ftrace data for this cpu (in batches), until we catch
  // up to the writer, or hit |max_pages|. Returns number of pages read.
  size_t ReadCycle(uint8_t* parsing_buf,
                   size_t parsing_buf_size_pages,
                   size_t max_pages,
                   const std::set<FtraceDataSource*>& started_data_sources);

  template <typename T>
  static bool ReadAndAdvance(const uint8_t** ptr, const uint8_t* end, T* out) {
    if (*ptr > end - sizeof(T))
      return false;
    memcpy(reinterpret_cast<void*>(out), reinterpret_cast<const void*>(*ptr),
           sizeof(T));
    *ptr += sizeof(T);
    return true;
  }

  // Caller must do the bounds check:
  // [start + offset, start + offset + sizeof(T))
  // Returns the raw value not the varint.
  template <typename T>
  static T ReadIntoVarInt(const uint8_t* start,
                          uint32_t field_id,
                          protozero::Message* out) {
    T t;
    memcpy(&t, reinterpret_cast<const void*>(start), sizeof(T));
    out->AppendVarInt<T>(field_id, t);
    return t;
  }

  template <typename T>
  static void ReadInode(const uint8_t* start,
                        uint32_t field_id,
                        protozero::Message* out,
                        FtraceMetadata* metadata) {
    T t = ReadIntoVarInt<T>(start, field_id, out);
    metadata->AddInode(static_cast<Inode>(t));
  }

  template <typename T>
  static void ReadDevId(const uint8_t* start,
                        uint32_t field_id,
                        protozero::Message* out,
                        FtraceMetadata* metadata) {
    T t;
    memcpy(&t, reinterpret_cast<const void*>(start), sizeof(T));
    BlockDeviceID dev_id = TranslateBlockDeviceIDToUserspace<T>(t);
    out->AppendVarInt<BlockDeviceID>(field_id, dev_id);
    metadata->AddDevice(dev_id);
  }

  template <typename T>
  static void ReadSymbolAddr(const uint8_t* start,
                             uint32_t field_id,
                             protozero::Message* out,
                             FtraceMetadata* metadata) {
    // ReadSymbolAddr is a bit special. In order to not disclose KASLR layout
    // via traces, we put in the trace only a mangled address (which really is
    // the insertion order into metadata.kernel_addrs). We don't care about the
    // actual symbol addesses. We just need to match that against the symbol
    // name in the names in the FtraceEventBundle.KernelSymbols.
    T full_addr;
    memcpy(&full_addr, reinterpret_cast<const void*>(start), sizeof(T));
    uint32_t interned_index = metadata->AddSymbolAddr(full_addr);
    out->AppendVarInt(field_id, interned_index);
  }

  static void ReadPid(const uint8_t* start,
                      uint32_t field_id,
                      protozero::Message* out,
                      FtraceMetadata* metadata) {
    int32_t pid = ReadIntoVarInt<int32_t>(start, field_id, out);
    metadata->AddPid(pid);
  }

  static void ReadCommonPid(const uint8_t* start,
                            uint32_t field_id,
                            protozero::Message* out,
                            FtraceMetadata* metadata) {
    int32_t pid = ReadIntoVarInt<int32_t>(start, field_id, out);
    metadata->AddCommonPid(pid);
  }

  // Internally the kernel stores device ids in a different layout to that
  // exposed to userspace via stat etc. There's no userspace function to convert
  // between the formats so we have to do it ourselves.
  template <typename T>
  static BlockDeviceID TranslateBlockDeviceIDToUserspace(T kernel_dev) {
    // Provided search index s_dev from
    // https://github.com/torvalds/linux/blob/v4.12/include/linux/fs.h#L404
    // Convert to user space id using
    // https://github.com/torvalds/linux/blob/v4.12/include/linux/kdev_t.h#L10
    // TODO(azappone): see if this is the same on all platforms
    uint64_t maj = static_cast<uint64_t>(kernel_dev) >> 20;
    uint64_t min = static_cast<uint64_t>(kernel_dev) & ((1U << 20) - 1);
    return static_cast<BlockDeviceID>(  // From makedev()
        ((maj & 0xfffff000ULL) << 32) | ((maj & 0xfffULL) << 8) |
        ((min & 0xffffff00ULL) << 12) | ((min & 0xffULL)));
  }

  // Returns a parsed representation of the given raw ftrace page's header.
  static std::optional<CpuReader::PageHeader> ParsePageHeader(
      const uint8_t** ptr,
      uint16_t page_header_size_len);

  // Parse the payload of a raw ftrace page, and write the events as protos
  // into the provided bundle (and/or compact buffer).
  // |table| contains the mix of compile time (e.g. proto field ids) and
  // run time (e.g. field offset and size) information necessary to do this.
  // The table is initialized once at start time by the ftrace controller
  // which passes it to the CpuReader which passes it here.
  // The caller is responsible for validating that the page_header->size stays
  // within the current page.
  static size_t ParsePagePayload(const uint8_t* start_of_payload,
                                 const PageHeader* page_header,
                                 const ProtoTranslationTable* table,
                                 const FtraceDataSourceConfig* ds_config,
                                 Bundler* bundler,
                                 FtraceMetadata* metadata);

  // Parse a single raw ftrace event beginning at |start| and ending at |end|
  // and write it into the provided bundle as a proto.
  // |table| contains the mix of compile time (e.g. proto field ids) and
  // run time (e.g. field offset and size) information necessary to do this.
  // The table is initialized once at start time by the ftrace controller
  // which passes it to the CpuReader which passes it to ParsePage which
  // passes it here.
  static bool ParseEvent(uint16_t ftrace_event_id,
                         const uint8_t* start,
                         const uint8_t* end,
                         const ProtoTranslationTable* table,
                         const FtraceDataSourceConfig* ds_config,
                         protozero::Message* message,
                         FtraceMetadata* metadata);

  static bool ParseField(const Field& field,
                         const uint8_t* start,
                         const uint8_t* end,
                         const ProtoTranslationTable* table,
                         protozero::Message* message,
                         FtraceMetadata* metadata);

  // Parse a sys_enter event according to the pre-validated expected format
  static bool ParseSysEnter(const Event& info,
                            const uint8_t* start,
                            const uint8_t* end,
                            protozero::Message* message,
                            FtraceMetadata* metadata);

  // Parse a sys_exit event according to the pre-validated expected format
  static bool ParseSysExit(const Event& info,
                           const uint8_t* start,
                           const uint8_t* end,
                           const FtraceDataSourceConfig* ds_config,
                           protozero::Message* message,
                           FtraceMetadata* metadata);

  // Parse a sched_switch event according to pre-validated format, and buffer
  // the individual fields in the given compact encoding batch.
  static void ParseSchedSwitchCompact(const uint8_t* start,
                                      uint64_t timestamp,
                                      const CompactSchedSwitchFormat* format,
                                      CompactSchedBuffer* compact_buf,
                                      FtraceMetadata* metadata);

  // Parse a sched_waking event according to pre-validated format, and buffer
  // the individual fields in the given compact encoding batch.
  static void ParseSchedWakingCompact(const uint8_t* start,
                                      uint64_t timestamp,
                                      const CompactSchedWakingFormat* format,
                                      CompactSchedBuffer* compact_buf,
                                      FtraceMetadata* metadata);

  // Parses & encodes the given range of contiguous tracing pages. Called by
  // |ReadAndProcessBatch| for each active data source.
  //
  // Returns the number of correctly processed pages. If the return value is
  // equal to |pages_read|, there was no error. Otherwise, the return value
  // points to the first page that contains an error.
  //
  // public and static for testing
  static size_t ProcessPagesForDataSource(
      TraceWriter* trace_writer,
      FtraceMetadata* metadata,
      size_t cpu,
      const FtraceDataSourceConfig* ds_config,
      const uint8_t* parsing_buf,
      const size_t pages_read,
      const ProtoTranslationTable* table,
      LazyKernelSymbolizer* symbolizer,
      const FtraceClockSnapshot*,
      protos::pbzero::FtraceClock);

  void set_ftrace_clock(protos::pbzero::FtraceClock clock) {
    ftrace_clock_ = clock;
  }

 private:
  CpuReader(const CpuReader&) = delete;
  CpuReader& operator=(const CpuReader&) = delete;

  // Reads at most |max_pages| of ftrace data, parses it, and writes it
  // into |started_data_sources|. Returns number of pages read.
  // See comment on ftrace_controller.cc:kMaxParsingWorkingSetPages for
  // rationale behind the batching.
  size_t ReadAndProcessBatch(
      uint8_t* parsing_buf,
      size_t max_pages,
      bool first_batch_in_cycle,
      const std::set<FtraceDataSource*>& started_data_sources);

  const size_t cpu_;
  const ProtoTranslationTable* const table_;
  LazyKernelSymbolizer* const symbolizer_;
  const FtraceClockSnapshot* const ftrace_clock_snapshot_;
  base::ScopedFile trace_fd_;
  protos::pbzero::FtraceClock ftrace_clock_{};
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_CPU_READER_H_
