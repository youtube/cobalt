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

<<<<<<< HEAD
#include <string.h>
#include <cstdint>

#include <optional>
#include <set>

#include "perfetto/base/flat_set.h"
#include "perfetto/ext/base/flat_hash_map.h"
#include "perfetto/ext/base/paged_memory.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/utils.h"
=======
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
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
#include "perfetto/ext/traced/data_source_types.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/protozero/message.h"
#include "perfetto/protozero/message_handle.h"
<<<<<<< HEAD
#include "src/traced/probes/ftrace/compact_sched.h"
#include "src/traced/probes/ftrace/ftrace_metadata.h"

#include "protos/perfetto/trace/trace_packet.pbzero.h"
=======
#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "src/kallsyms/kernel_symbol_map.h"
#include "src/kallsyms/lazy_kernel_symbolizer.h"
#include "src/traced/probes/ftrace/compact_sched.h"
#include "src/traced/probes/ftrace/ftrace_metadata.h"
#include "src/traced/probes/ftrace/proto_translation_table.h"
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

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
<<<<<<< HEAD
enum FtraceParseStatus : int32_t;
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
}  // namespace pbzero
}  // namespace protos

// Reads raw ftrace data for a cpu, parses it, and writes it into the perfetto
// tracing buffers.
class CpuReader {
 public:
<<<<<<< HEAD
  // Buffers used when parsing a chunk of ftrace data, allocated by
  // FtraceController and repeatedly reused by all CpuReaders:
  // * paged memory into which we read raw ftrace data.
  // * buffers to accumulate and emit scheduling data in a structure-of-arrays
  //   format (packed proto fields).
  class ParsingBuffers {
   public:
    void AllocateIfNeeded() {
      // PagedMemory stays valid as long as it was allocated once.
      if (!ftrace_data_.IsValid()) {
        ftrace_data_ = base::PagedMemory::Allocate(base::GetSysPageSize() *
                                                   kFtraceDataBufSizePages);
      }
      // Heap-allocated buffer gets freed and reallocated.
      if (!compact_sched_) {
        compact_sched_ = std::make_unique<CompactSchedBuffer>();
      }
    }

    void Release() {
      if (ftrace_data_.IsValid()) {
        ftrace_data_.AdviseDontNeed(ftrace_data_.Get(), ftrace_data_.size());
      }
      compact_sched_.reset();
    }

   private:
    friend class CpuReader;
    // When reading and parsing data for a particular cpu, we do it in batches
    // of this many pages. In other words, we'll read up to
    // |kFtraceDataBufSizePages| into memory, parse them, and then repeat if we
    // still haven't caught up to the writer.
    static constexpr size_t kFtraceDataBufSizePages = 32;

    uint8_t* ftrace_data_buf() const {
      return reinterpret_cast<uint8_t*>(ftrace_data_.Get());
    }
    size_t ftrace_data_buf_pages() const {
      PERFETTO_DCHECK(ftrace_data_.size() ==
                      base::GetSysPageSize() * kFtraceDataBufSizePages);
      return kFtraceDataBufSizePages;
    }
    CompactSchedBuffer* compact_sched_buf() const {
      return compact_sched_.get();
    }

    base::PagedMemory ftrace_data_;
    std::unique_ptr<CompactSchedBuffer> compact_sched_;
  };

  // Facilitates lazy proto writing - not every event in the kernel ring buffer
  // is serialised in the trace, so this class allows for trace packets to be
  // written only if there's at least one relevant event in the ring buffer
  // batch. Public for testing.
=======
  using FtraceEventBundle = protos::pbzero::FtraceEventBundle;

  // Helper class to generate `TracePacket`s when needed. Public for testing.
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  class Bundler {
   public:
    Bundler(TraceWriter* trace_writer,
            FtraceMetadata* metadata,
            LazyKernelSymbolizer* symbolizer,
            size_t cpu,
            const FtraceClockSnapshot* ftrace_clock_snapshot,
            protos::pbzero::FtraceClock ftrace_clock,
<<<<<<< HEAD
            CompactSchedBuffer* compact_sched_buf,
            bool compact_sched_enabled,
            uint64_t previous_bundle_end_ts,
            const base::FlatHashMap<uint32_t, std::vector<uint8_t>>*
                generic_pb_descriptors)
=======
            bool compact_sched_enabled)
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
        : trace_writer_(trace_writer),
          metadata_(metadata),
          symbolizer_(symbolizer),
          cpu_(cpu),
          ftrace_clock_snapshot_(ftrace_clock_snapshot),
          ftrace_clock_(ftrace_clock),
<<<<<<< HEAD
          compact_sched_enabled_(compact_sched_enabled),
          compact_sched_buf_(compact_sched_buf),
          initial_previous_bundle_end_ts_(previous_bundle_end_ts),
          generic_pb_descriptors_(generic_pb_descriptors) {
      if (compact_sched_enabled_)
        compact_sched_buf_->Reset();
    }
=======
          compact_sched_enabled_(compact_sched_enabled) {}
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

    ~Bundler() { FinalizeAndRunSymbolizer(); }

    protos::pbzero::FtraceEventBundle* GetOrCreateBundle() {
      if (!bundle_) {
<<<<<<< HEAD
        StartNewPacket(false, initial_previous_bundle_end_ts_);
=======
        StartNewPacket(false);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      }
      return bundle_;
    }

    // Forces the creation of a new TracePacket.
<<<<<<< HEAD
    void StartNewPacket(bool lost_events,
                        uint64_t previous_bundle_end_timestamp);
=======
    void StartNewPacket(bool lost_events);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

    // This function is called after the contents of a FtraceBundle are written.
    void FinalizeAndRunSymbolizer();

<<<<<<< HEAD
    CompactSchedBuffer* compact_sched_buf() {
      // FinalizeAndRunSymbolizer will only process the compact_sched_buf_ if
      // there is an open bundle.
      GetOrCreateBundle();
      return compact_sched_buf_;
    }

    base::FlatSet<uint32_t>* generic_descriptors_to_write() {
      return &generic_descriptors_to_write_;
    }

    void WriteGenericEventDescriptors();

=======
    CompactSchedBuffer* compact_sched_buffer() {
      // FinalizeAndRunSymbolizer will only process the compact_sched_buffer_ if
      // there is an open bundle.
      GetOrCreateBundle();
      return &compact_sched_buffer_;
    }

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
   private:
    TraceWriter* const trace_writer_;         // Never nullptr.
    FtraceMetadata* const metadata_;          // Never nullptr.
    LazyKernelSymbolizer* const symbolizer_;  // Can be nullptr.
    const size_t cpu_;
    const FtraceClockSnapshot* const ftrace_clock_snapshot_;
    protos::pbzero::FtraceClock const ftrace_clock_;
    const bool compact_sched_enabled_;
<<<<<<< HEAD
    CompactSchedBuffer* const compact_sched_buf_;
    uint64_t initial_previous_bundle_end_ts_;
    // Keyed by proto field id within |FtraceEvent|.
    base::FlatSet<uint32_t> generic_descriptors_to_write_;
    const base::FlatHashMap<uint32_t, std::vector<uint8_t>>*
        generic_pb_descriptors_;

    TraceWriter::TracePacketHandle packet_;
    protos::pbzero::FtraceEventBundle* bundle_ = nullptr;
=======

    TraceWriter::TracePacketHandle packet_;
    protos::pbzero::FtraceEventBundle* bundle_ = nullptr;
    // Allocate the buffer for compact scheduler events (which will be unused if
    // the compact option isn't enabled).
    CompactSchedBuffer compact_sched_buffer_;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  };

  struct PageHeader {
    uint64_t timestamp;
    uint64_t size;
    bool lost_events;
  };

  CpuReader(size_t cpu,
<<<<<<< HEAD
            base::ScopedFile trace_fd,
            const ProtoTranslationTable* table,
            LazyKernelSymbolizer* symbolizer,
            protos::pbzero::FtraceClock ftrace_clock,
            const FtraceClockSnapshot* ftrace_clock_snapshot);
  ~CpuReader();

  // move-only
  CpuReader(const CpuReader&) = delete;
  CpuReader& operator=(const CpuReader&) = delete;
  CpuReader(CpuReader&&) = default;
  CpuReader& operator=(CpuReader&&) = default;

  // Reads and parses all ftrace data for this cpu (in batches), until we catch
  // up to the writer, or hit |max_pages|. Returns number of pages read.
  size_t ReadCycle(ParsingBuffers* parsing_bufs,
                   size_t max_pages,
                   const std::set<FtraceDataSource*>& started_data_sources);

  // Niche version of ReadCycle for FrozenFtraceDataSource, assumes a stopped
  // tracefs instance. Don't add new callers.
  size_t ReadFrozen(
      ParsingBuffers* parsing_bufs,
      size_t max_pages,
      const FtraceDataSourceConfig* parsing_config,
      FtraceMetadata* metadata,
      base::FlatSet<protos::pbzero::FtraceParseStatus>* parse_errors,
      TraceWriter* trace_writer);

=======
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

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
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
<<<<<<< HEAD
    // actual symbol addresses. We just need to match that against the symbol
=======
    // actual symbol addesses. We just need to match that against the symbol
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
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
<<<<<<< HEAD
  static protos::pbzero::FtraceParseStatus ParsePagePayload(
      const uint8_t* start_of_payload,
      const PageHeader* page_header,
      const ProtoTranslationTable* table,
      const FtraceDataSourceConfig* ds_config,
      Bundler* bundler,
      FtraceMetadata* metadata,
      uint64_t* bundle_end_timestamp);
=======
  static size_t ParsePagePayload(const uint8_t* start_of_payload,
                                 const PageHeader* page_header,
                                 const ProtoTranslationTable* table,
                                 const FtraceDataSourceConfig* ds_config,
                                 Bundler* bundler,
                                 FtraceMetadata* metadata);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

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
<<<<<<< HEAD
                         FtraceMetadata* metadata,
                         base::FlatSet<uint32_t>* generic_descriptors_to_write);
=======
                         FtraceMetadata* metadata);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

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
<<<<<<< HEAD
                            protozero::Message* message);
=======
                            protozero::Message* message,
                            FtraceMetadata* metadata);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  // Parse a sys_exit event according to the pre-validated expected format
  static bool ParseSysExit(const Event& info,
                           const uint8_t* start,
                           const uint8_t* end,
                           const FtraceDataSourceConfig* ds_config,
                           protozero::Message* message,
                           FtraceMetadata* metadata);

<<<<<<< HEAD
  static bool ParseGenericEventLegacy(const Event& info,
                                      const uint8_t* start,
                                      const uint8_t* end,
                                      const ProtoTranslationTable* table,
                                      protozero::Message* message,
                                      FtraceMetadata* metadata);

=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
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
<<<<<<< HEAD
  // Returns true if all pages were parsed correctly. In case of parsing
  // errors, they will be recorded in the FtraceEventBundle proto.
  //
  // public and static for testing
  static bool ProcessPagesForDataSource(
=======
  // Returns the number of correctly processed pages. If the return value is
  // equal to |pages_read|, there was no error. Otherwise, the return value
  // points to the first page that contains an error.
  //
  // public and static for testing
  static size_t ProcessPagesForDataSource(
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      TraceWriter* trace_writer,
      FtraceMetadata* metadata,
      size_t cpu,
      const FtraceDataSourceConfig* ds_config,
<<<<<<< HEAD
      base::FlatSet<protos::pbzero::FtraceParseStatus>* parse_errors,
      uint64_t* bundle_end_timestamp,
      const uint8_t* parsing_buf,
      size_t pages_read,
      CompactSchedBuffer* compact_sched_buf,
      const ProtoTranslationTable* table,
      LazyKernelSymbolizer* symbolizer,
      const FtraceClockSnapshot* ftrace_clock_snapshot,
      protos::pbzero::FtraceClock ftrace_clock);

  // For FtraceController, which manages poll callbacks on per-cpu buffer fds.
  int RawBufferFd() const { return trace_fd_.get(); }

 private:
=======
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

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  // Reads at most |max_pages| of ftrace data, parses it, and writes it
  // into |started_data_sources|. Returns number of pages read.
  // See comment on ftrace_controller.cc:kMaxParsingWorkingSetPages for
  // rationale behind the batching.
  size_t ReadAndProcessBatch(
      uint8_t* parsing_buf,
      size_t max_pages,
      bool first_batch_in_cycle,
<<<<<<< HEAD
      CompactSchedBuffer* compact_sched_buf,
      const std::set<FtraceDataSource*>& started_data_sources);

  size_t cpu_;
  const ProtoTranslationTable* table_;
  LazyKernelSymbolizer* symbolizer_;
  base::ScopedFile trace_fd_;
  protos::pbzero::FtraceClock ftrace_clock_{};
  const FtraceClockSnapshot* ftrace_clock_snapshot_;
=======
      const std::set<FtraceDataSource*>& started_data_sources);

  const size_t cpu_;
  const ProtoTranslationTable* const table_;
  LazyKernelSymbolizer* const symbolizer_;
  const FtraceClockSnapshot* const ftrace_clock_snapshot_;
  base::ScopedFile trace_fd_;
  protos::pbzero::FtraceClock ftrace_clock_{};
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_CPU_READER_H_
