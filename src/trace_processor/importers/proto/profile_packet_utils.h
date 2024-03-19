/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROFILE_PACKET_UTILS_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROFILE_PACKET_UTILS_H_
#include <optional>

#include "perfetto/ext/base/string_view.h"
#include "src/trace_processor/importers/proto/packet_sequence_state.h"
#include "src/trace_processor/importers/proto/stack_profile_tracker.h"

#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

class ProfilePacketUtils {
 public:
  static SequenceStackProfileTracker::SourceMapping MakeSourceMapping(
      const protos::pbzero::Mapping::Decoder& entry) {
    SequenceStackProfileTracker::SourceMapping src_mapping{};
    src_mapping.build_id = entry.build_id();
    src_mapping.exact_offset = entry.exact_offset();
    src_mapping.start_offset = entry.start_offset();
    src_mapping.start = entry.start();
    src_mapping.end = entry.end();
    src_mapping.load_bias = entry.load_bias();
    for (auto path_string_id_it = entry.path_string_ids(); path_string_id_it;
         ++path_string_id_it) {
      src_mapping.name_ids.emplace_back(*path_string_id_it);
    }
    return src_mapping;
  }

  static SequenceStackProfileTracker::SourceFrame MakeSourceFrame(
      const protos::pbzero::Frame::Decoder& entry) {
    SequenceStackProfileTracker::SourceFrame src_frame;
    src_frame.name_id = entry.function_name_id();
    src_frame.mapping_id = entry.mapping_id();
    src_frame.rel_pc = entry.rel_pc();
    return src_frame;
  }

  static SequenceStackProfileTracker::SourceCallstack MakeSourceCallstack(
      const protos::pbzero::Callstack::Decoder& entry) {
    SequenceStackProfileTracker::SourceCallstack src_callstack;
    for (auto frame_it = entry.frame_ids(); frame_it; ++frame_it)
      src_callstack.emplace_back(*frame_it);
    return src_callstack;
  }

  static const char* StringifyCpuMode(
      protos::pbzero::Profiling::CpuMode cpu_mode) {
    using protos::pbzero::Profiling;
    switch (cpu_mode) {
      case Profiling::MODE_UNKNOWN:
        return "unknown";
      case Profiling::MODE_KERNEL:
        return "kernel";
      case Profiling::MODE_USER:
        return "user";
      case Profiling::MODE_HYPERVISOR:
        return "hypervisor";
      case Profiling::MODE_GUEST_KERNEL:
        return "guest_kernel";
      case Profiling::MODE_GUEST_USER:
        return "guest_user";
    }
    return "unknown";  // switch should be complete, but gcc needs a hint
  }

  static const char* StringifyStackUnwindError(
      protos::pbzero::Profiling::StackUnwindError unwind_error) {
    using protos::pbzero::Profiling;
    switch (unwind_error) {
      case Profiling::UNWIND_ERROR_UNKNOWN:
        return "unknown";
      case Profiling::UNWIND_ERROR_NONE:
        return "none";  // should never see this serialized by traced_perf, the
                        // field should be unset instead
      case Profiling::UNWIND_ERROR_MEMORY_INVALID:
        return "memory_invalid";
      case Profiling::UNWIND_ERROR_UNWIND_INFO:
        return "unwind_info";
      case Profiling::UNWIND_ERROR_UNSUPPORTED:
        return "unsupported";
      case Profiling::UNWIND_ERROR_INVALID_MAP:
        return "invalid_map";
      case Profiling::UNWIND_ERROR_MAX_FRAMES_EXCEEDED:
        return "max_frames_exceeded";
      case Profiling::UNWIND_ERROR_REPEATED_FRAME:
        return "repeated_frame";
      case Profiling::UNWIND_ERROR_INVALID_ELF:
        return "invalid_elf";
      case Profiling::UNWIND_ERROR_SYSTEM_CALL:
        return "system_call";
      case Profiling::UNWIND_ERROR_THREAD_TIMEOUT:
        return "thread_timeout";
      case Profiling::UNWIND_ERROR_THREAD_DOES_NOT_EXIST:
        return "thread_does_not_exist";
      case Profiling::UNWIND_ERROR_BAD_ARCH:
        return "bad_arch";
      case Profiling::UNWIND_ERROR_MAPS_PARSE:
        return "maps_parse";
      case Profiling::UNWIND_ERROR_INVALID_PARAMETER:
        return "invalid_parameter";
      case Profiling::UNWIND_ERROR_PTRACE_CALL:
        return "ptrace_call";
    }
    return "unknown";  // switch should be complete, but gcc needs a hint
  }
};

class ProfilePacketInternLookup
    : public SequenceStackProfileTracker::InternLookup {
 public:
  explicit ProfilePacketInternLookup(PacketSequenceStateGeneration* seq_state)
      : seq_state_(seq_state) {}
  ~ProfilePacketInternLookup() override;

  std::optional<base::StringView> GetString(
      SequenceStackProfileTracker::SourceStringId iid,
      SequenceStackProfileTracker::InternedStringType type) const override {
    protos::pbzero::InternedString::Decoder* decoder = nullptr;
    switch (type) {
      case SequenceStackProfileTracker::InternedStringType::kBuildId:
        decoder = seq_state_->LookupInternedMessage<
            protos::pbzero::InternedData::kBuildIdsFieldNumber,
            protos::pbzero::InternedString>(iid);
        break;
      case SequenceStackProfileTracker::InternedStringType::kFunctionName:
        decoder = seq_state_->LookupInternedMessage<
            protos::pbzero::InternedData::kFunctionNamesFieldNumber,
            protos::pbzero::InternedString>(iid);
        break;
      case SequenceStackProfileTracker::InternedStringType::kMappingPath:
        decoder = seq_state_->LookupInternedMessage<
            protos::pbzero::InternedData::kMappingPathsFieldNumber,
            protos::pbzero::InternedString>(iid);
        break;
    }
    if (!decoder)
      return std::nullopt;
    return base::StringView(reinterpret_cast<const char*>(decoder->str().data),
                            decoder->str().size);
  }

  std::optional<SequenceStackProfileTracker::SourceMapping> GetMapping(
      SequenceStackProfileTracker::SourceMappingId iid) const override {
    auto* decoder = seq_state_->LookupInternedMessage<
        protos::pbzero::InternedData::kMappingsFieldNumber,
        protos::pbzero::Mapping>(iid);
    if (!decoder)
      return std::nullopt;
    return ProfilePacketUtils::MakeSourceMapping(*decoder);
  }

  std::optional<SequenceStackProfileTracker::SourceFrame> GetFrame(
      SequenceStackProfileTracker::SourceFrameId iid) const override {
    auto* decoder = seq_state_->LookupInternedMessage<
        protos::pbzero::InternedData::kFramesFieldNumber,
        protos::pbzero::Frame>(iid);
    if (!decoder)
      return std::nullopt;
    return ProfilePacketUtils::MakeSourceFrame(*decoder);
  }

  std::optional<SequenceStackProfileTracker::SourceCallstack> GetCallstack(
      SequenceStackProfileTracker::SourceCallstackId iid) const override {
    auto* interned_message_view = seq_state_->GetInternedMessageView(
        protos::pbzero::InternedData::kCallstacksFieldNumber, iid);
    if (!interned_message_view)
      return std::nullopt;
    protos::pbzero::Callstack::Decoder decoder(
        interned_message_view->message().data(),
        interned_message_view->message().length());
    return ProfilePacketUtils::MakeSourceCallstack(std::move(decoder));
  }

 private:
  PacketSequenceStateGeneration* seq_state_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_PROFILE_PACKET_UTILS_H_
