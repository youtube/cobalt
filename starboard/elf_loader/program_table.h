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

#ifndef STARBOARD_ELF_LOADER_PROGRAM_TABLE_H_
#define STARBOARD_ELF_LOADER_PROGRAM_TABLE_H_

#include <vector>

#include "cobalt/extension/memory_mapped_file.h"
#include "starboard/elf_loader/elf.h"
#include "starboard/elf_loader/file.h"

namespace starboard {
namespace elf_loader {

// Loads the ELF's binary program table and memory maps
// the loadable segments.
//
// To properly initialize the program table and the segments
// the following calls are required:
//  1. LoadProgramHeader()
//  2. LoadSegments()
//
// After those calls the ProgramTable class is fully functional and
// the segments properly loaded.

class ProgramTable {
 public:
  explicit ProgramTable(
      const CobaltExtensionMemoryMappedFileApi* memory_mapped_file_extension);

  // Loads the program header.
  bool LoadProgramHeader(const Ehdr* elf_header, File* elf_file);

  // Loads the segments.
  bool LoadSegments(File* elf_file);

  // Retrieves the dynamic section table.
  void GetDynamicSection(Dyn** dynamic,
                         size_t* dynamic_count,
                         Word* dynamic_flags);

  // Adjusts the memory protection of read only segments.
  // This call is used to make text segments writable in order
  // to apply relocations. After the relocations are done the
  // protection is restored to its original read only state.
  int AdjustMemoryProtectionOfReadOnlySegments(int extra_prot_flags);

  // Reserves a contiguous block of memory, page aligned for mapping all
  // the segments of the binary.
  bool ReserveLoadMemory();

  // Retrieves the base load address for the binary.
  Addr GetBaseMemoryAddress();

  // Publish the memory mapping information of the library to
  // the EvergreenInfo API.
  void PublishEvergreenInfo(const char* file_path);

  ~ProgramTable();

 private:
  // Calculates the memory size of the binary.
  size_t GetLoadMemorySize();

 private:
  size_t phdr_num_;
  void* phdr_mmap_;
  Phdr* phdr_table_;
  Addr phdr_size_;

  std::vector<uint8_t> build_id_;

  // First page of reserved address space.
  void* load_start_;

  // Size in bytes of reserved address space.
  Addr load_size_;

  // The base memory address. All virtual addresses
  // from the ELF file are offsets from this address.
  Addr base_memory_address_;

  const CobaltExtensionMemoryMappedFileApi* memory_mapped_file_extension_;

  ProgramTable(const ProgramTable&) = delete;
  void operator=(const ProgramTable&) = delete;
};

}  // namespace elf_loader
}  // namespace starboard

#endif  // STARBOARD_ELF_LOADER_PROGRAM_TABLE_H_
