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
#include "starboard/common/log.h"
#include "starboard/elf_loader/elf.h"
#include "starboard/elf_loader/file_impl.h"
#include "starboard/memory.h"
#include "starboard/string.h"

namespace starboard {
namespace elf_loader {

ElfLoaderImpl::ElfLoaderImpl() {
#if SB_API_VERSION < 12 ||                                           \
    !(SB_API_VERSION >= SB_MMAP_REQUIRED_VERSION || SB_HAS(MMAP)) || \
    !SB_CAN(MAP_EXECUTABLE_MEMORY)
  SB_CHECK(false) << "The elf_loader requires SB_API_VERSION >= 12 with "
                     "executable memory map support!";
#endif
}

bool ElfLoaderImpl::Load(const char* name) {
  SB_LOG(INFO) << "Loading: " << name;
  elf_file_.reset(new FileImpl());
  elf_file_->Open(name);

  elf_header_loader_.reset(new ElfHeader());
  if (!elf_header_loader_->LoadElfHeader(elf_file_.get())) {
    SB_LOG(ERROR) << "Failed to loaded ELF header";
    return false;
  }

  SB_LOG(INFO) << "Loaded ELF header";

  program_table_.reset(new ProgramTable());
  program_table_->LoadProgramHeader(elf_header_loader_->GetHeader(),
                                    elf_file_.get());

  SB_LOG(INFO) << "Loaded Program header";

  if (!program_table_->ReserveLoadMemory()) {
    SB_LOG(ERROR) << "Failed to reserve memory space";
    return false;
  }

  SB_LOG(INFO) << "Reserved address space";

  if (!program_table_->LoadSegments(elf_file_.get())) {
    SB_LOG(ERROR) << "Failed to load segments";
    return false;
  }
  SB_LOG(INFO) << "Loaded segments";

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
  SB_LOG(INFO) << "Initialized dynamic section";
  if (!dynamic_section_->InitDynamicSymbols()) {
    SB_LOG(ERROR) << "Failed to load dynamic symbols";
    return false;
  }
  SB_LOG(INFO) << "Initialized dynamic symbols";

  exported_symbols_.reset(new ExportedSymbols());
  relocations_.reset(new Relocations(program_table_->GetBaseMemoryAddress(),
                                     dynamic_section_.get(),
                                     exported_symbols_.get()));
  if (!relocations_->InitRelocations()) {
    SB_LOG(ERROR) << "Failed to initialize relocations";
    return false;
  }
  if (relocations_->HasTextRelocations()) {
    SB_LOG(INFO) << "HasTextRelocations";
    // Adjust the memory protection to its to allow modifications.
    if (program_table_->AdjustMemoryProtectionOfReadOnlySegments(
            kSbMemoryMapProtectWrite) < 0) {
      SB_LOG(ERROR) << "Unable to make segments writable";
      return false;
    }
  }
  SB_LOG(INFO) << "Loaded relocations";
  if (!relocations_->ApplyAllRelocations()) {
    SB_LOG(ERROR) << "Failed to apply relocations";
    return false;
  }

  if (relocations_->HasTextRelocations()) {
    // Restores the memory protection to its original state.
#if SB_API_VERSION >= 10 && \
    (SB_API_VERSION >= SB_MMAP_REQUIRED_VERSION || SB_HAS(MMAP))
    if (program_table_->AdjustMemoryProtectionOfReadOnlySegments(
            kSbMemoryMapProtectReserved) < 0) {
      SB_LOG(ERROR) << "Unable to restore segment protection";
      return false;
    }
#else
    SB_CHECK(false);
#endif
  }

  SB_LOG(INFO) << "Applied relocations";

  SB_LOG(INFO) << "Call constructors";
  dynamic_section_->CallConstructors();

  SB_LOG(INFO) << "Finished loading";

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
