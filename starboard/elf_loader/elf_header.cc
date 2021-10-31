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

#include "starboard/elf_loader/elf_header.h"

#include "starboard/common/log.h"
#include "starboard/elf_loader/log.h"
#include "starboard/memory.h"

namespace starboard {
namespace elf_loader {

ElfHeader::ElfHeader() {
  elf_header_.reset(new Ehdr());
}

bool ElfHeader::LoadElfHeader(File* file) {
  SB_DLOG(INFO) << "LoadElfHeader";
  if (!file->ReadFromOffset(0, reinterpret_cast<char*>(elf_header_.get()),
                            sizeof(Ehdr))) {
    SB_LOG(ERROR) << "Failed to read file";
    return false;
  }

  if (memcmp(elf_header_->e_ident, ELFMAG, SELFMAG) != 0) {
    SB_LOG(ERROR) << "Bad ELF magic: " << elf_header_->e_ident;
    return false;
  }

  if (elf_header_->e_ident[EI_CLASS] != ELF_CLASS_VALUE) {
    SB_LOG(ERROR) << "Not a " << ELF_BITS
                  << "-bit class: " << elf_header_->e_ident[EI_CLASS];
    return false;
  }
  if (elf_header_->e_ident[EI_DATA] != ELFDATA2LSB) {
    SB_LOG(ERROR) << "Not little-endian class" << elf_header_->e_ident[EI_DATA];
    return false;
  }

  if (elf_header_->e_type != ET_DYN) {
    SB_LOG(ERROR) << "Not a shared library type:" << std::hex
                  << elf_header_->e_type;
    return false;
  }

  if (elf_header_->e_version != EV_CURRENT) {
    SB_LOG(ERROR) << "Unexpected ELF version: " << elf_header_->e_version;
    return false;
  }

  if (elf_header_->e_machine != ELF_MACHINE) {
    SB_LOG(ERROR) << "Unexpected ELF machine type: " << std::hex
                  << elf_header_->e_machine;
    return false;
  }

  return true;
}

const Ehdr* ElfHeader::GetHeader() {
  return elf_header_.get();
}

}  // namespace elf_loader
}  // namespace starboard
