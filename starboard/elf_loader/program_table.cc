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

#include "starboard/elf_loader/program_table.h"

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/elf_loader/evergreen_info.h"
#include "starboard/elf_loader/log.h"
#include "starboard/memory.h"
#include "starboard/string.h"

#define MAYBE_MAP_FLAG(x, from, to) (((x) & (from)) ? (to) : 0)

#if SB_CAN(MAP_EXECUTABLE_MEMORY)
#define PFLAGS_TO_PROT(x)                               \
  (MAYBE_MAP_FLAG((x), PF_X, kSbMemoryMapProtectExec) | \
   MAYBE_MAP_FLAG((x), PF_R, kSbMemoryMapProtectRead) | \
   MAYBE_MAP_FLAG((x), PF_W, kSbMemoryMapProtectWrite))
#endif

#define MAP_FAILED ((void*)-1)

namespace starboard {
namespace elf_loader {

ProgramTable::ProgramTable(
    const CobaltExtensionMemoryMappedFileApi* memory_mapped_file_extension)
    : phdr_num_(0),
      phdr_mmap_(NULL),
      phdr_table_(NULL),
      phdr_size_(0),
      load_start_(NULL),
      load_size_(0),
      base_memory_address_(0),
      memory_mapped_file_extension_(memory_mapped_file_extension) {}

bool ProgramTable::LoadProgramHeader(const Ehdr* elf_header, File* elf_file) {
  if (!elf_header) {
    SB_LOG(ERROR) << "Ehdr is required";
    return false;
  }
  if (!elf_file) {
    SB_LOG(ERROR) << "File is required";
    return false;
  }
  phdr_num_ = elf_header->e_phnum;

  SB_DLOG(INFO) << "Program Header count=" << phdr_num_;
  // Like the kernel, only accept program header tables smaller than 64 KB.
  if (phdr_num_ < 1 || phdr_num_ > 65536 / elf_header->e_phentsize) {
    SB_LOG(ERROR) << "Invalid program header count: " << phdr_num_;
    return false;
  }

  SB_DLOG(INFO) << "elf_header->e_phoff=" << elf_header->e_phoff;
  SB_DLOG(INFO) << "elf_header->e_phnum=" << elf_header->e_phnum;

  Addr page_min = PAGE_START(elf_header->e_phoff);
  Addr page_max =
      PAGE_END(elf_header->e_phoff + (phdr_num_ * elf_header->e_phentsize));
  Addr page_offset = PAGE_OFFSET(elf_header->e_phoff);

  SB_DLOG(INFO) << "page_min=" << page_min;
  SB_DLOG(INFO) << "page_max=" << page_max;

  phdr_size_ = page_max - page_min;

  SB_DLOG(INFO) << "page_max - page_min=" << page_max - page_min;

  if (memory_mapped_file_extension_) {
    SB_DLOG(INFO) << "Memory mapped file for the program header";
    phdr_mmap_ = memory_mapped_file_extension_->MemoryMapFile(
        NULL, elf_file->GetName().c_str(), kSbMemoryMapProtectRead, page_min,
        phdr_size_);
    if (!phdr_mmap_) {
      SB_LOG(ERROR) << "Failed to memory map the program header";
      return false;
    }

    SB_DLOG(INFO) << "Allocated address=" << phdr_mmap_;
  } else {
#if SB_API_VERSION >= 12 || SB_HAS(MMAP)
    phdr_mmap_ =
        SbMemoryMap(phdr_size_, kSbMemoryMapProtectWrite, "program_header");
    if (!phdr_mmap_) {
      SB_LOG(ERROR) << "Failed to allocate memory";
      return false;
    }

    SB_DLOG(INFO) << "Allocated address=" << phdr_mmap_;
#else
    SB_CHECK(false);
#endif
    if (!elf_file->ReadFromOffset(page_min, reinterpret_cast<char*>(phdr_mmap_),
                                  phdr_size_)) {
      SB_LOG(ERROR) << "Failed to read program header from file offset: "
                    << page_min;
      return false;
    }
#if SB_API_VERSION >= 12 || SB_HAS(MMAP)
    bool mp_result =
        SbMemoryProtect(phdr_mmap_, phdr_size_, kSbMemoryMapProtectRead);
    SB_DLOG(INFO) << "mp_result=" << mp_result;
    if (!mp_result) {
      SB_LOG(ERROR) << "Failed to protect program header";
      return false;
    }
#else
    SB_CHECK(false);
#endif
  }

  phdr_table_ = reinterpret_cast<Phdr*>(reinterpret_cast<char*>(phdr_mmap_) +
                                        page_offset);
  SB_DLOG(INFO) << "phdr_table_=" << phdr_table_;
  return true;
}

static bool ElfClassBuildIDNoteIdentifier(const void* section,
                                          size_t length,
                                          std::vector<uint8_t>& identifier) {
  const void* section_end = reinterpret_cast<const char*>(section) + length;
  const Nhdr* note_header = reinterpret_cast<const Nhdr*>(section);
  while (reinterpret_cast<const void*>(note_header) < section_end) {
    if (note_header->n_type == NT_GNU_BUILD_ID)
      break;
    note_header = reinterpret_cast<const Nhdr*>(
        reinterpret_cast<const char*>(note_header) + sizeof(Nhdr) +
        NOTE_PADDING(note_header->n_namesz) +
        NOTE_PADDING(note_header->n_descsz));
  }
  if (reinterpret_cast<const void*>(note_header) >= section_end ||
      note_header->n_descsz == 0) {
    return false;
  }

  const uint8_t* build_id = reinterpret_cast<const uint8_t*>(note_header) +
                            sizeof(Nhdr) + NOTE_PADDING(note_header->n_namesz);
  identifier.insert(identifier.end(), build_id,
                    build_id + note_header->n_descsz);

  return true;
}

bool ProgramTable::LoadSegments(File* elf_file) {
  for (size_t i = 0; i < phdr_num_; ++i) {
    const Phdr* phdr = &phdr_table_[i];

    if (phdr->p_type == PT_NOTE) {
      if (!ElfClassBuildIDNoteIdentifier(
              reinterpret_cast<const void*>(phdr->p_vaddr +
                                            base_memory_address_),
              phdr->p_memsz, build_id_)) {
        SB_LOG(INFO) << "Could not get build id";
      }
      continue;
    }
    if (phdr->p_type != PT_LOAD) {
      continue;
    }

    // Segment byte addresses in memory.
    Addr seg_start = phdr->p_vaddr + base_memory_address_;
    Addr seg_end = seg_start + phdr->p_memsz;

    // Segment page addresses in memory.
    Addr seg_page_start = PAGE_START(seg_start);
    Addr seg_page_end = PAGE_END(seg_end);

    // File offsets.
    Addr seg_file_end = seg_start + phdr->p_filesz;
    Addr file_start = phdr->p_offset;
    Addr file_end = file_start + phdr->p_filesz;

    SB_DLOG(INFO) << " phdr->p_offset=" << phdr->p_offset
                  << " phdr->p_filesz=" << phdr->p_filesz;

    Addr file_page_start = PAGE_START(file_start);
    Addr file_length = file_end - file_page_start;

    SB_DLOG(INFO) << "Mapping segment: "
                  << " file_page_start=" << file_page_start
                  << " file_length=" << file_length << " seg_page_start=0x"
                  << std::hex << seg_page_start;

#if (SB_API_VERSION >= 12 || SB_HAS(MMAP)) && SB_CAN(MAP_EXECUTABLE_MEMORY)
    if (file_length != 0) {
      const int prot_flags = PFLAGS_TO_PROT(phdr->p_flags);
      SB_DLOG(INFO) << "segment prot_flags=" << std::hex << prot_flags;

      void* seg_addr = reinterpret_cast<void*>(seg_page_start);
      bool mp_ret = false;
      if (memory_mapped_file_extension_) {
        SB_DLOG(INFO) << "Using Memory Mapped File for Loading the Segment";
        void* p = memory_mapped_file_extension_->MemoryMapFile(
            seg_addr, elf_file->GetName().c_str(), kSbMemoryMapProtectRead,
            file_page_start, file_length);
        if (!p) {
          SB_LOG(ERROR) << "Failed to memory map file: " << elf_file->GetName();
          return false;
        }
      } else {
        SB_DLOG(INFO) << "Not using Memory Mapped Files";
        mp_ret =
            SbMemoryProtect(seg_addr, file_length, kSbMemoryMapProtectWrite);
        SB_DLOG(INFO) << "segment vaddress=" << seg_addr;

        if (!mp_ret) {
          SB_LOG(ERROR) << "Failed to unprotect segment";
          return false;
        }
        if (!elf_file->ReadFromOffset(file_page_start,
                                      reinterpret_cast<char*>(seg_addr),
                                      file_length)) {
          SB_DLOG(INFO) << "Failed to read segment from file offset: "
                        << file_page_start;
          return false;
        }
      }
      mp_ret = SbMemoryProtect(seg_addr, file_length, prot_flags);
      SB_DLOG(INFO) << "mp_ret=" << mp_ret;
      if (!mp_ret) {
        SB_LOG(ERROR) << "Failed to protect segment";
        return false;
      }
      if (!seg_addr) {
        SB_LOG(ERROR) << "Could not map segment " << i;
        return false;
      }
    }
#else
    SB_CHECK(false);
#endif

    // if the segment is writable, and does not end on a page boundary,
    // zero-fill it until the page limit.
    if ((phdr->p_flags & PF_W) != 0 && PAGE_OFFSET(seg_file_end) > 0) {
      memset(reinterpret_cast<void*>(seg_file_end), 0,
             PAGE_SIZE - PAGE_OFFSET(seg_file_end));
    }

    seg_file_end = PAGE_END(seg_file_end);

    // seg_file_end is now the first page address after the file
    // content. If seg_page_end is larger, we need to zero anything
    // between them. This is done by using a private anonymous
    // map for all extra pages.
    if (seg_page_end > seg_file_end) {
#if (SB_API_VERSION >= 12 || SB_HAS(MMAP)) && SB_CAN(MAP_EXECUTABLE_MEMORY)
      bool mprotect_fix = SbMemoryProtect(reinterpret_cast<void*>(seg_file_end),
                                          seg_page_end - seg_file_end,
                                          kSbMemoryMapProtectWrite);
      SB_DLOG(INFO) << "mprotect_fix=" << mprotect_fix;
      if (!mprotect_fix) {
        SB_LOG(ERROR) << "Failed to unprotect end of segment";
        return false;
      }
#else
      SB_CHECK(false);
#endif

      memset(reinterpret_cast<void*>(seg_file_end), 0,
             seg_page_end - seg_file_end);
#if (SB_API_VERSION >= 12 || SB_HAS(MMAP)) && SB_CAN(MAP_EXECUTABLE_MEMORY)
      SbMemoryProtect(reinterpret_cast<void*>(seg_file_end),
                      seg_page_end - seg_file_end,
                      PFLAGS_TO_PROT(phdr->p_flags));
      SB_DLOG(INFO) << "mprotect_fix=" << mprotect_fix;
      if (!mprotect_fix) {
        SB_LOG(ERROR) << "Failed to protect end of segment";
        return false;
      }
#else
      SB_CHECK(false);
#endif
    }
  }
  return true;
}

size_t ProgramTable::GetLoadMemorySize() {
  Addr min_vaddr = ~static_cast<Addr>(0);
  Addr max_vaddr = 0x00000000U;

  bool found_pt_load = false;
  for (size_t i = 0; i < phdr_num_; ++i) {
    const Phdr* phdr = &phdr_table_[i];

    if (phdr->p_type != PT_LOAD) {
      SB_DLOG(INFO) << "GetLoadMemorySize: ignoring segment with type: "
                    << phdr->p_type;
      continue;
    }
    found_pt_load = true;

    if (phdr->p_vaddr < min_vaddr) {
      SB_DLOG(INFO) << "p_vaddr=" << std::hex << phdr->p_vaddr;
      min_vaddr = phdr->p_vaddr;
    }

    if (phdr->p_vaddr + phdr->p_memsz > max_vaddr) {
      max_vaddr = phdr->p_vaddr + phdr->p_memsz;
      SB_DLOG(INFO) << "phdr->p_vaddr=" << phdr->p_vaddr
                    << " phdr->p_memsz=" << phdr->p_memsz;
      SB_DLOG(INFO) << " max_vaddr=0x" << std::hex << max_vaddr;
    }
  }
  if (!found_pt_load) {
    min_vaddr = 0x00000000U;
  }

  min_vaddr = PAGE_START(min_vaddr);
  max_vaddr = PAGE_END(max_vaddr);

  return max_vaddr - min_vaddr;
}

void ProgramTable::GetDynamicSection(Dyn** dynamic,
                                     size_t* dynamic_count,
                                     Word* dynamic_flags) {
  const Phdr* phdr = phdr_table_;
  const Phdr* phdr_limit = phdr + phdr_num_;

  for (phdr = phdr_table_; phdr < phdr_limit; phdr++) {
    if (phdr->p_type != PT_DYNAMIC) {
      SB_DLOG(INFO) << "Ignore section with type: " << phdr->p_type;
      continue;
    }

    SB_DLOG(INFO) << "Reading at vaddr: " << phdr->p_vaddr;
    *dynamic = reinterpret_cast<Dyn*>(base_memory_address_ + phdr->p_vaddr);
    if (dynamic_count) {
      *dynamic_count = static_cast<size_t>((phdr->p_memsz / sizeof(Dyn)));
    }
    if (dynamic_flags) {
      *dynamic_flags = phdr->p_flags;
    }
    return;
  }
  *dynamic = NULL;
  if (dynamic_count) {
    *dynamic_count = 0;
  }
}

int ProgramTable::AdjustMemoryProtectionOfReadOnlySegments(
    int extra_prot_flags) {
  const Phdr* phdr = phdr_table_;
  const Phdr* phdr_limit = phdr + phdr_num_;

  for (; phdr < phdr_limit; phdr++) {
    if (phdr->p_type != PT_LOAD || (phdr->p_flags & PF_W) != 0)
      continue;

    Addr seg_page_start = PAGE_START(phdr->p_vaddr) + base_memory_address_;
    Addr seg_page_end =
        PAGE_END(phdr->p_vaddr + phdr->p_memsz) + base_memory_address_;
#if (SB_API_VERSION >= 12 || SB_HAS(MMAP)) && SB_CAN(MAP_EXECUTABLE_MEMORY)
    int ret = SbMemoryProtect(reinterpret_cast<void*>(seg_page_start),
                              seg_page_end - seg_page_start,
                              PFLAGS_TO_PROT(phdr->p_flags) | extra_prot_flags);
    if (ret < 0) {
      return -1;
    }
#else
    SB_CHECK(false);
#endif
  }
  return 0;
}

bool ProgramTable::ReserveLoadMemory() {
  load_size_ = GetLoadMemorySize();
  if (load_size_ == 0) {
    SB_LOG(ERROR) << "No loadable segments";
    return false;
  }

  SB_DLOG(INFO) << "Load size=" << load_size_;

#if (SB_API_VERSION >= 12 || SB_HAS(MMAP))
  load_start_ =
      SbMemoryMap(load_size_, kSbMemoryMapProtectReserved, "reserved_mem");
  if (load_start_ == MAP_FAILED) {
    SB_LOG(ERROR) << "Could not reserve " << load_size_
                  << " bytes of address space";
    return false;
  }
#else
  SB_CHECK(false);
#endif
  base_memory_address_ = reinterpret_cast<Addr>(load_start_);
  SB_LOG(INFO) << "Load start=" << std::hex << load_start_
               << " base_memory_address=0x" << base_memory_address_;
  return true;
}

void ProgramTable::PublishEvergreenInfo(const char* file_path) {
  EvergreenInfo evergreen_info;
  memset(&evergreen_info, 0, sizeof(EvergreenInfo));
  starboard::strlcpy(evergreen_info.file_path_buf, file_path,
                     EVERGREEN_FILE_PATH_MAX_SIZE);
  evergreen_info.base_address = base_memory_address_;
  evergreen_info.load_size = load_size_;
  evergreen_info.phdr_table = (uint64_t)phdr_table_;
  evergreen_info.phdr_table_num = phdr_num_;

  std::vector<char> tmp(build_id_.begin(), build_id_.end());
  tmp.push_back('\0');
  starboard::strlcpy(evergreen_info.build_id, tmp.data(),
                     EVERGREEN_BUILD_ID_MAX_SIZE);
  evergreen_info.build_id_length = build_id_.size();

  SetEvergreenInfo(&evergreen_info);
}

Addr ProgramTable::GetBaseMemoryAddress() {
  return base_memory_address_;
}

ProgramTable::~ProgramTable() {
  SetEvergreenInfo(NULL);
#if SB_API_VERSION >= 12 || SB_HAS(MMAP)
  if (load_start_) {
    SbMemoryUnmap(load_start_, load_size_);
  }
  if (phdr_mmap_) {
    SbMemoryUnmap(phdr_mmap_, phdr_size_);
  }
#else
  SB_CHECK(false);
#endif
}

}  // namespace elf_loader
}  // namespace starboard
