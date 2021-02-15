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

#include "starboard/elf_loader/relocations.h"

#include "starboard/common/log.h"
#include "starboard/elf_loader/log.h"

namespace starboard {
namespace elf_loader {

Relocations::Relocations(Addr base_memory_address,
                         DynamicSection* dynamic_section,
                         ExportedSymbols* exported_symbols)
    : base_memory_address_(base_memory_address),
      dynamic_section_(dynamic_section),
      plt_relocations_(0),
      plt_relocations_size_(0),
      plt_got_(NULL),
      relocations_(0),
      relocations_size_(0),
      has_text_relocations_(false),
      has_symbolic_(false),
      exported_symbols_(exported_symbols) {}

bool Relocations::HasTextRelocations() {
  return has_text_relocations_;
}

bool Relocations::InitRelocations() {
  SB_DLOG(INFO) << "InitRelocations: dynamic_count="
                << dynamic_section_->GetDynamicTableSize();
  const Dyn* dynamic = dynamic_section_->GetDynamicTable();
  for (int i = 0; i < dynamic_section_->GetDynamicTableSize(); i++) {
    Addr dyn_value = dynamic[i].d_un.d_val;
    uintptr_t dyn_addr = base_memory_address_ + dynamic[i].d_un.d_ptr;
    Addr tag = dynamic[i].d_tag;
    SB_DLOG(INFO) << "InitRelocations: tag=" << tag;
    switch (tag) {
#if defined(USE_RELA)
      case DT_REL:
      case DT_RELSZ:
#else
      case DT_RELA:
      case DT_RELASZ:
#endif
        SB_LOG(ERROR) << "unsupported relocation type";
        return false;
      case DT_PLTREL:
        SB_DLOG(INFO) << "  DT_PLTREL value=" << dyn_value;
#if defined(USE_RELA)
        if (dyn_value != DT_RELA) {
          SB_LOG(ERROR) << "unsupported DT_PLTREL  expected DT_RELA";
          return false;
        }
#else
        if (dyn_value != DT_REL) {
          SB_LOG(ERROR) << "unsupported DT_PLTREL expected DT_REL";
          return false;
        }
#endif
        break;
      case DT_JMPREL:
        SB_DLOG(INFO) << "  DT_JMPREL addr=0x" << std::hex
                      << (dyn_addr - base_memory_address_);
        plt_relocations_ = dyn_addr;
        break;
      case DT_PLTRELSZ:
        plt_relocations_size_ = dyn_value;
        SB_DLOG(INFO) << "  DT_PLTRELSZ size=" << dyn_value;
        break;
#if defined(USE_RELA)
      case DT_RELA:
#else
      case DT_REL:
#endif
        SB_DLOG(INFO) << "  " << ((tag == DT_RELA) ? "DT_RELA" : "DT_REL")
                      << " addr=" << std::hex
                      << (dyn_addr - base_memory_address_);
        if (relocations_) {
          SB_LOG(ERROR)
              << "Unsupported DT_RELA/DT_REL combination in dynamic section";
          return false;
        }
        relocations_ = dyn_addr;
        break;
#if defined(USE_RELA)
      case DT_RELASZ:
#else
      case DT_RELSZ:
#endif
        SB_DLOG(INFO) << "  " << ((tag == DT_RELASZ) ? "DT_RELASZ" : "DT_RELSZ")
                      << " size=" << dyn_value;
        relocations_size_ = dyn_value;
        break;
      case DT_RELR:
      case DT_RELRSZ:
        SB_LOG(ERROR) << "  DT_RELRSZ NOT IMPLEMENTED";
        break;
      case DT_RELRENT:
      case DT_PLTGOT:
        SB_DLOG(INFO) << "DT_PLTGOT addr=0x" << std::hex
                      << (dyn_addr - base_memory_address_);
        plt_got_ = reinterpret_cast<Addr*>(dyn_addr);
        break;
      case DT_TEXTREL:
        SB_DLOG(INFO) << "  DT_TEXTREL";
        has_text_relocations_ = true;
        break;
      case DT_SYMBOLIC:
        SB_DLOG(INFO) << "  DT_SYMBOLIC";
        has_symbolic_ = true;
        break;
      case DT_FLAGS:
        if (dyn_value & DF_TEXTREL)
          has_text_relocations_ = true;
        if (dyn_value & DF_SYMBOLIC)
          has_symbolic_ = true;

        SB_DLOG(INFO) << "  DT_FLAGS has_text_relocations="
                      << has_text_relocations_
                      << " has_symbolic=" << has_symbolic_;

        break;
      default:
        break;
    }
  }

  return true;
}

bool Relocations::ApplyAllRelocations() {
  SB_DLOG(INFO) << "Applying regular relocations";
  if (!ApplyRelocations(reinterpret_cast<rel_t*>(relocations_),
                        relocations_size_ / sizeof(rel_t))) {
    SB_LOG(ERROR) << "regular relocations failed";
    return false;
  }

  SB_DLOG(INFO) << "Applying PLT relocations";
  if (!ApplyRelocations(reinterpret_cast<rel_t*>(plt_relocations_),
                        plt_relocations_size_ / sizeof(rel_t))) {
    SB_LOG(ERROR) << "PLT relocations failed";
    return false;
  }
  return true;
}

bool Relocations::ApplyRelocations(const rel_t* rel, size_t rel_count) {
  SB_DLOG(INFO) << "rel=" << std::hex << rel << std::dec
                << " rel_count=" << rel_count;

  if (!rel)
    return true;

  for (size_t rel_n = 0; rel_n < rel_count; rel++, rel_n++) {
    SB_DLOG(INFO) << "  Relocation " << rel_n + 1 << " of " << rel_count;

    if (!ApplyRelocation(rel))
      return false;
  }

  return true;
}

bool Relocations::ApplyRelocation(const rel_t* rel) {
  const Word rel_type = ELF_R_TYPE(rel->r_info);
  const Word rel_symbol = ELF_R_SYM(rel->r_info);

  Addr sym_addr = 0;
  Addr reloc = static_cast<Addr>(rel->r_offset + base_memory_address_);
  SB_DLOG(INFO) << "  offset=0x" << std::hex << rel->r_offset
                << " type=" << std::dec << rel_type << " reloc=0x" << std::hex
                << reloc << " symbol=" << rel_symbol;

  if (rel_type == 0)
    return true;

  if (rel_symbol != 0) {
    if (!ResolveSymbol(rel_type, rel_symbol, reloc, &sym_addr)) {
      SB_LOG(ERROR) << "Failed to resolve symbol: " << rel_symbol;
      return false;
    }
  }

  return ApplyResolvedReloc(rel, sym_addr);
}

#if defined(USE_RELA)
bool Relocations::ApplyResolvedReloc(const Rela* rela, Addr sym_addr) {
  const Word rela_type = ELF_R_TYPE(rela->r_info);
  const Sword addend = rela->r_addend;
  const Addr reloc = static_cast<Addr>(rela->r_offset + base_memory_address_);

  SB_DLOG(INFO) << "  rela reloc=0x" << std::hex << reloc << " offset=0x"
                << rela->r_offset << " type=" << std::dec << rela_type
                << " addend=0x" << std::hex << addend;
  Addr* target = reinterpret_cast<Addr*>(reloc);
  switch (rela_type) {
#if SB_IS(ARCH_ARM64)
    case R_AARCH64_JUMP_SLOT:
      SB_DLOG(INFO) << "  R_AARCH64_JUMP_SLOT target=" << std::hex << target
                    << " addr=" << (sym_addr + addend);
      *target = sym_addr + addend;
      break;

    case R_AARCH64_GLOB_DAT:
      SB_DLOG(INFO) << " R_AARCH64_GLOB_DAT target=" << std::hex << target
                    << " addr=" << (sym_addr + addend);
      *target = sym_addr + addend;
      break;

    case R_AARCH64_ABS64:
      SB_DLOG(INFO) << "  R_AARCH64_ABS64 target=" << std::hex << target << " "
                    << *target << " addr=" << sym_addr + addend;
      *target += sym_addr + addend;
      break;

    case R_AARCH64_RELATIVE:
      SB_DLOG(INFO) << "  R_AARCH64_RELATIVE target=" << std::hex << target
                    << " " << *target
                    << " bias=" << base_memory_address_ + addend;
      *target = base_memory_address_ + addend;
      break;
#endif

#if SB_IS(ARCH_X64)
    case R_X86_64_JMP_SLOT:
      SB_DLOG(INFO) << "  R_X86_64_JMP_SLOT target=" << std::hex << target
                    << " addr=" << (sym_addr + addend);
      *target = sym_addr + addend;
      break;

    case R_X86_64_GLOB_DAT:
      SB_DLOG(INFO) << "  R_X86_64_GLOB_DAT target=" << std::hex << target
                    << " addr=" << (sym_addr + addend);

      *target = sym_addr + addend;
      break;

    case R_X86_64_RELATIVE:
      SB_DLOG(INFO) << "  R_X86_64_RELATIVE target=" << std::hex << target
                    << " " << *target
                    << " bias=" << base_memory_address_ + addend;
      *target = base_memory_address_ + addend;
      break;

    case R_X86_64_64:
      *target = sym_addr + addend;
      break;

    case R_X86_64_PC32:
      *target = sym_addr + (addend - reloc);
      break;
#endif

    default:
      SB_LOG(ERROR) << "Invalid relocation type: " << rela_type;
      return false;
  }

  return true;
}
#else
bool Relocations::ApplyResolvedReloc(const Rel* rel, Addr sym_addr) {
  const Word rel_type = ELF_R_TYPE(rel->r_info);
  const Addr reloc = static_cast<Addr>(rel->r_offset + base_memory_address_);

  SB_DLOG(INFO) << "  rel reloc=0x" << std::hex << reloc << " offset=0x"
                << rel->r_offset << " type=" << std::dec << rel_type;

  Addr* target = reinterpret_cast<Addr*>(reloc);
  switch (rel_type) {
#if SB_IS(ARCH_ARM)
    case R_ARM_JUMP_SLOT:
      SB_DLOG(INFO) << "  R_ARM_JUMP_SLOT target=" << std::hex << target
                    << " addr=" << sym_addr;
      *target = sym_addr;
      break;

    case R_ARM_GLOB_DAT:
      SB_DLOG(INFO) << "  R_ARM_GLOB_DAT target=" << std::hex << target
                    << " addr=" << sym_addr;
      *target = sym_addr;
      break;

    case R_ARM_ABS32:
      SB_DLOG(INFO) << "  R_ARM_ABS32 target=" << std::hex << target << " "
                    << *target << " addr=" << sym_addr;
      *target += sym_addr;
      break;

    case R_ARM_REL32:
      SB_DLOG(INFO) << "  R_ARM_REL32 target=" << std::hex << target << " "
                    << *target << " addr=" << sym_addr
                    << " offset=" << rel->r_offset;
      *target += sym_addr - rel->r_offset;
      break;

    case R_ARM_RELATIVE:
      SB_DLOG(INFO) << "  RR_ARM_RELATIVE target=" << std::hex << target << " "
                    << *target << " bias=" << base_memory_address_;
      *target += base_memory_address_;
      break;
#endif

#if SB_IS(ARCH_X86)
    case R_386_JMP_SLOT:
      SB_DLOG(INFO) << "  R_386_JMP_SLOT target=" << std::hex << target
                    << " addr=" << sym_addr;

      *target = sym_addr;
      break;

    case R_386_GLOB_DAT:
      SB_DLOG(INFO) << "  R_386_GLOB_DAT target=" << std::hex << target
                    << " addr=" << sym_addr;
      *target = sym_addr;

      break;

    case R_386_RELATIVE:
      SB_DLOG(INFO) << "  R_386_RELATIVE target=" << std::hex << target << " "
                    << *target << " bias=" << base_memory_address_;

      *target += base_memory_address_;
      break;

    case R_386_32:
      SB_DLOG(INFO) << "  R_386_32 target=" << std::hex << target << " "
                    << *target << " addr=" << sym_addr;
      *target += sym_addr;
      break;

    case R_386_PC32:
      SB_DLOG(INFO) << "  R_386_PC32 target=" << std::hex << target << " "
                    << *target << " addr=" << sym_addr << " reloc=" << reloc;
      *target += (sym_addr - reloc);
      break;
#endif

    default:
      SB_LOG(ERROR) << "Invalid relocation type: " << rel_type;
      return false;
  }

  return true;
}
#endif

RelocationType Relocations::GetRelocationType(Word r_type) {
  switch (r_type) {
#if SB_IS(ARCH_ARM)
    case R_ARM_JUMP_SLOT:
    case R_ARM_GLOB_DAT:
    case R_ARM_ABS32:
      return RELOCATION_TYPE_ABSOLUTE;

    case R_ARM_REL32:
    case R_ARM_RELATIVE:
      return RELOCATION_TYPE_RELATIVE;

    case R_ARM_COPY:
      return RELOCATION_TYPE_COPY;
#endif

#if SB_IS(ARCH_ARM64)
    case R_AARCH64_JUMP_SLOT:
    case R_AARCH64_GLOB_DAT:
    case R_AARCH64_ABS64:
      return RELOCATION_TYPE_ABSOLUTE;

    case R_AARCH64_RELATIVE:
      return RELOCATION_TYPE_RELATIVE;

    case R_AARCH64_COPY:
      return RELOCATION_TYPE_COPY;
#endif

#if SB_IS(ARCH_X86)
    case R_386_JMP_SLOT:
    case R_386_GLOB_DAT:
    case R_386_32:
      return RELOCATION_TYPE_ABSOLUTE;

    case R_386_RELATIVE:
      return RELOCATION_TYPE_RELATIVE;

    case R_386_PC32:
      return RELOCATION_TYPE_PC_RELATIVE;
#endif

#if SB_IS(ARCH_X64)
    case R_X86_64_JMP_SLOT:
    case R_X86_64_GLOB_DAT:
    case R_X86_64_64:
      return RELOCATION_TYPE_ABSOLUTE;

    case R_X86_64_RELATIVE:
      return RELOCATION_TYPE_RELATIVE;

    case R_X86_64_PC32:
      return RELOCATION_TYPE_PC_RELATIVE;
#endif
    default:
      return RELOCATION_TYPE_UNKNOWN;
  }
}

bool Relocations::ResolveSymbol(Word rel_type,
                                Word rel_symbol,
                                Addr reloc,
                                Addr* sym_addr) {
  const char* sym_name = dynamic_section_->LookupNameById(rel_symbol);
  SB_DLOG(INFO) << "Resolve: " << sym_name;
  const void* address = NULL;

  const Sym* sym = dynamic_section_->LookupByName(sym_name);
  if (sym) {
    address = reinterpret_cast<void*>(base_memory_address_ + sym->st_value);
  } else {
    address = exported_symbols_->Lookup(sym_name);
  }

  SB_DLOG(INFO) << "Resolve: address=0x" << std::hex << address;

  if (address) {
    // The symbol was found, so compute its address.
    *sym_addr = reinterpret_cast<Addr>(address);
    return true;
  }

  // The symbol was not found. Normally this is an error except
  // if this is a weak reference.
  if (!dynamic_section_->IsWeakById(rel_symbol)) {
    SB_LOG(ERROR) << "Could not find symbol: " << sym_name;
    return false;
  }

  // IHI0044C AAELF 4.5.1.1:
  // Libraries are not searched to resolve weak references.
  // It is not an error for a weak reference to remain
  // unsatisfied.
  //
  // During linking, the value of an undefined weak reference is:
  // - Zero if the relocation type is absolute
  // - The address of the place if the relocation is pc-relative
  // - The address of nominal base address if the relocation
  //   type is base-relative.
  RelocationType r = GetRelocationType(rel_type);
  if (r == RELOCATION_TYPE_ABSOLUTE || r == RELOCATION_TYPE_RELATIVE) {
    *sym_addr = 0;
    return true;
  }

  if (r == RELOCATION_TYPE_PC_RELATIVE) {
    *sym_addr = reloc;
    return true;
  }

  SB_LOG(ERROR) << "Invalid weak relocation type (" << r
                << ") for unknown symbol '" << sym_name << "'";
  return false;
}
}  // namespace elf_loader
}  // namespace starboard
