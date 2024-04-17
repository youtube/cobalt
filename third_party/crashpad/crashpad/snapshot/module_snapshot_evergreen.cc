// Copyright 2020 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "snapshot/module_snapshot_evergreen.h"

#include <endian.h>

#include <algorithm>
#include <utility>

#include "base/files/file_path.h"
#include "snapshot/crashpad_types/image_annotation_reader.h"
#include "snapshot/memory_snapshot_generic.h"
#include "util/misc/elf_note_types.h"

namespace crashpad {
namespace internal {

ModuleSnapshotEvergreen::ModuleSnapshotEvergreen(
    const std::string& name,
    ModuleSnapshot::ModuleType type,
    uint64_t address,
    uint64_t size,
    std::vector<uint8_t> build_id)
    : ModuleSnapshot(),
      name_(name),
      crashpad_info_(),
      type_(type),
      initialized_(),
      streams_(),
      address_(address),
      size_(size),
      build_id_(std::move(build_id)) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  INITIALIZATION_STATE_SET_VALID(initialized_);
}

ModuleSnapshotEvergreen::~ModuleSnapshotEvergreen() = default;

bool ModuleSnapshotEvergreen::Initialize() {
  return true;
}

bool ModuleSnapshotEvergreen::GetCrashpadOptions(
    CrashpadInfoClientOptions* options) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (!crashpad_info_) {
    return false;
  }

  options->crashpad_handler_behavior =
      crashpad_info_->CrashpadHandlerBehavior();
  options->system_crash_reporter_forwarding =
      crashpad_info_->SystemCrashReporterForwarding();
  options->gather_indirectly_referenced_memory =
      crashpad_info_->GatherIndirectlyReferencedMemory();
  options->indirectly_referenced_memory_cap =
      crashpad_info_->IndirectlyReferencedMemoryCap();
  return true;
}

std::string ModuleSnapshotEvergreen::Name() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return name_;
}

uint64_t ModuleSnapshotEvergreen::Address() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return address_;
}

uint64_t ModuleSnapshotEvergreen::Size() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return size_;
}

time_t ModuleSnapshotEvergreen::Timestamp() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return 0;
}

void ModuleSnapshotEvergreen::FileVersion(uint16_t* version_0,
                                          uint16_t* version_1,
                                          uint16_t* version_2,
                                          uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *version_0 = 0;
  *version_1 = 0;
  *version_2 = 0;
  *version_3 = 0;
}

void ModuleSnapshotEvergreen::SourceVersion(uint16_t* version_0,
                                            uint16_t* version_1,
                                            uint16_t* version_2,
                                            uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *version_0 = 0;
  *version_1 = 0;
  *version_2 = 0;
  *version_3 = 0;
}

ModuleSnapshot::ModuleType ModuleSnapshotEvergreen::GetModuleType() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return type_;
}

void ModuleSnapshotEvergreen::UUIDAndAge(crashpad::UUID* uuid,
                                         uint32_t* age) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *age = 0;

  // TODO: Add build id
  // auto build_id = BuildID();
  // build_id.insert(
  //     build_id.end(), 16 - std::min(build_id.size(), size_t{16}), '\0');
  // uuid->InitializeFromBytes(build_id.data());

  // TODO(scottmg): https://crashpad.chromium.org/bug/229. These are
  // endian-swapped to match FileID::ConvertIdentifierToUUIDString() in
  // Breakpad. This is necessary as this identifier is used for symbol lookup.
  // uuid->data_1 = htobe32(uuid->data_1);
  // uuid->data_2 = htobe16(uuid->data_2);
  // uuid->data_3 = htobe16(uuid->data_3);
}

std::string ModuleSnapshotEvergreen::DebugFileName() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return base::FilePath(Name()).BaseName().value();
}

std::vector<uint8_t> ModuleSnapshotEvergreen::BuildID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return build_id_;
}

std::vector<std::string> ModuleSnapshotEvergreen::AnnotationsVector() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<std::string>();
}

std::map<std::string, std::string>
ModuleSnapshotEvergreen::AnnotationsSimpleMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::map<std::string, std::string>();
}

std::vector<AnnotationSnapshot> ModuleSnapshotEvergreen::AnnotationObjects()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<AnnotationSnapshot>();
}

std::set<CheckedRange<uint64_t>> ModuleSnapshotEvergreen::ExtraMemoryRanges()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::set<CheckedRange<uint64_t>>();
}

std::vector<const UserMinidumpStream*>
ModuleSnapshotEvergreen::CustomMinidumpStreams() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const UserMinidumpStream*>();
}

}  // namespace internal
}  // namespace crashpad
