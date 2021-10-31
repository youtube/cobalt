// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/elf_loader/elf_loader_impl.h"

#include <string>

#include "starboard/common/log.h"
#include "starboard/elf_loader/elf.h"
#include "starboard/elf_loader/elf_loader_constants.h"
#include "starboard/elf_loader/file_impl.h"
#include "starboard/elf_loader/log.h"
#include "starboard/elf_loader/lz4_file_impl.h"
#include "starboard/memory.h"
#include "starboard/string.h"

namespace starboard {
namespace elf_loader {

namespace {

bool EndsWith(const std::string& s, const std::string& suffix) {
  if (s.size() < suffix.size()) {
    return false;
  }
  return strcmp(s.c_str() + (s.size() - suffix.size()), suffix.c_str()) == 0;
}

}  // namespace

ElfLoaderImpl::ElfLoaderImpl() {
#if SB_API_VERSION < 12 || !(SB_API_VERSION >= 12 || SB_HAS(MMAP)) || \
    !SB_CAN(MAP_EXECUTABLE_MEMORY)
  SB_CHECK(false) << "The elf_loader requires SB_API_VERSION >= 12 with "
                     "executable memory map support!";
#endif
}

bool ElfLoaderImpl::Load(
    const char* name,
    const void* (*custom_get_extension)(const char* name)) {
  if (EndsWith(name, kCompressionSuffix)) {
    elf_file_.reset(new LZ4FileImpl());
    SB_LOG(INFO) << "Loading " << name << " using compression";
  } else {
    SB_LOG(INFO) << "Loading " << name;
    elf_file_.reset(new FileImpl());
  }
  elf_file_->Open(name);

  elf_header_loader_.reset(new ElfHeader());
  if (!elf_header_loader_->LoadElfHeader(elf_file_.get())) {
    SB_LOG(ERROR) << "Failed to load ELF header";
    return false;
  }

  SB_DLOG(INFO) << "Loaded ELF header";

  const auto* memory_mapped_file_extension =
      reinterpret_cast<const CobaltExtensionMemoryMappedFileApi*>(
          SbSystemGetExtension(kCobaltExtensionMemoryMappedFileName));
  if (memory_mapped_file_extension &&
      strcmp(memory_mapped_file_extension->name,
             kCobaltExtensionMemoryMappedFileName) == 0 &&
      memory_mapped_file_extension->version >= 1) {
    program_table_.reset(new ProgramTable(memory_mapped_file_extension));
  } else {
    program_table_.reset(new ProgramTable(nullptr));
  }

  program_table_->LoadProgramHeader(elf_header_loader_->GetHeader(),
                                    elf_file_.get());

  SB_DLOG(INFO) << "Loaded Program header";

  if (!program_table_->ReserveLoadMemory()) {
    SB_LOG(ERROR) << "Failed to reserve memory space";
    return false;
  }

  SB_DLOG(INFO) << "Reserved address space";

  if (!program_table_->LoadSegments(elf_file_.get())) {
    SB_LOG(ERROR) << "Failed to load segments";
    return false;
  }
  SB_DLOG(INFO) << "Loaded segments";

  Dyn* dynamic = NULL;
  size_t dynamic_count = 0;
  Word dynamic_flags = 0;
  program_table_->GetDynamicSection(&dynamic, &dynamic_count, &dynamic_flags);
  if (!dynamic) {
    SB_LOG(ERROR) << "No PT_DYNAMIC section!";
    return false;
  }
  dynamic_section_.reset(
      new DynamicSection(program_table_->GetBaseMemoryAddress(), dynamic,
                         dynamic_count, dynamic_flags));
  if (!dynamic_section_->InitDynamicSection()) {
    SB_LOG(ERROR) << "Failed to initialize dynamic section";
    return false;
  }
  SB_DLOG(INFO) << "Initialized dynamic section";

  exported_symbols_.reset(new ExportedSymbols(custom_get_extension));
  relocations_.reset(new Relocations(program_table_->GetBaseMemoryAddress(),
                                     dynamic_section_.get(),
                                     exported_symbols_.get()));
  if (!relocations_->InitRelocations()) {
    SB_LOG(ERROR) << "Failed to initialize relocations";
    return false;
  }
  if (relocations_->HasTextRelocations()) {
    SB_DLOG(INFO) << "HasTextRelocations";
    // Adjust the memory protection to its to allow modifications.
    if (program_table_->AdjustMemoryProtectionOfReadOnlySegments(
            kSbMemoryMapProtectWrite) < 0) {
      SB_LOG(ERROR) << "Unable to make segments writable";
      return false;
    }
  }
  SB_DLOG(INFO) << "Loaded relocations";
  if (!relocations_->ApplyAllRelocations()) {
    SB_LOG(ERROR) << "Failed to apply relocations";
    return false;
  }

  if (relocations_->HasTextRelocations()) {
#if SB_API_VERSION >= 12 || SB_HAS(MMAP)
    // Restores the memory protection to its original state.
    if (program_table_->AdjustMemoryProtectionOfReadOnlySegments(
            kSbMemoryMapProtectReserved) < 0) {
      SB_LOG(ERROR) << "Unable to restore segment protection";
      return false;
    }
#endif
  }

  SB_DLOG(INFO) << "Applied relocations";

  program_table_->PublishEvergreenInfo(name);
  SB_DLOG(INFO) << "Published Evergreen Info";

  SB_DLOG(INFO) << "Call constructors";
  dynamic_section_->CallConstructors();

  SB_DLOG(INFO) << "Finished loading";

  return true;
}
void* ElfLoaderImpl::LookupSymbol(const char* symbol) {
  const Sym* sym = dynamic_section_->LookupByName(symbol);
  void* address = NULL;
  if (sym) {
    address = reinterpret_cast<void*>(program_table_->GetBaseMemoryAddress() +
                                      sym->st_value);
  }
  return address;
}

ElfLoaderImpl::~ElfLoaderImpl() {
  dynamic_section_->CallDestructors();
}
}  // namespace elf_loader
}  // namespace starboard
