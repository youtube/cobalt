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

#include "starboard/elf_loader/dynamic_section.h"

#include "starboard/common/log.h"
#include "starboard/elf_loader/log.h"

namespace starboard {
namespace elf_loader {

DynamicSection::DynamicSection(Addr base_memory_address,
                               Dyn* dynamic,
                               size_t dynamic_count,
                               Word dynamic_flags)
    : base_memory_address_(base_memory_address),
      soname_(NULL),
      dynamic_(dynamic),
      dynamic_count_(dynamic_count),
      dynamic_flags_(dynamic_flags),
      has_DT_SYMBOLIC_(false),
      symbol_table_(NULL),
      string_table_(NULL),
      preinit_array_(NULL),
      preinit_array_count_(0),
      init_array_(NULL),
      init_array_count_(0),
      fini_array_(NULL),
      fini_array_count_(0),
      init_func_(NULL),
      fini_func_(NULL) {}

bool DynamicSection::InitDynamicSection() {
  SB_DLOG(INFO) << "Dynamic section count=" << dynamic_count_;
  for (int i = 0; i < dynamic_count_; i++) {
    Addr dyn_value = dynamic_[i].d_un.d_val;
    uintptr_t dyn_addr = base_memory_address_ + dynamic_[i].d_un.d_ptr;
    SB_DLOG(INFO) << "Dynamic tag=" << dynamic_[i].d_tag;
    switch (dynamic_[i].d_tag) {
      case DT_DEBUG:
        // TODO: implement.
        break;
      case DT_INIT:
        SB_DLOG(INFO) << "  DT_INIT addr=0x" << std::hex << dyn_addr;
        init_func_ = reinterpret_cast<linker_function_t>(dyn_addr);
        break;
      case DT_FINI:
        SB_DLOG(INFO) << "  DT_FINI addr=0x" << std::hex << dyn_addr;
        fini_func_ = reinterpret_cast<linker_function_t>(dyn_addr);
        break;
      case DT_INIT_ARRAY:
        SB_DLOG(INFO) << "  DT_INIT_ARRAY addr=0x" << std::hex << dyn_addr;
        init_array_ = reinterpret_cast<linker_function_t*>(dyn_addr);
        break;
      case DT_INIT_ARRAYSZ:
        init_array_count_ = dyn_value / sizeof(Addr);
        SB_DLOG(INFO) << "  DT_INIT_ARRAYSZ value=0x" << std::hex << dyn_value
                      << " count=" << std::dec << init_array_count_;
        break;
      case DT_FINI_ARRAY:
        SB_DLOG(INFO) << "  DT_FINI_ARRAY addr=0x" << std::hex << dyn_addr;
        fini_array_ = reinterpret_cast<linker_function_t*>(dyn_addr);
        break;
      case DT_FINI_ARRAYSZ:
        fini_array_count_ = dyn_value / sizeof(Addr);
        SB_DLOG(INFO) << "  DT_FINI_ARRAYSZ value=0x" << std::hex << dyn_value
                      << " count=" << fini_array_count_;
        break;
      case DT_PREINIT_ARRAY:
        SB_DLOG(INFO) << "  DT_PREINIT_ARRAY addr=0x" << std::hex << dyn_addr;
        preinit_array_ = reinterpret_cast<linker_function_t*>(dyn_addr);
        break;
      case DT_PREINIT_ARRAYSZ:
        preinit_array_count_ = dyn_value / sizeof(Addr);
        SB_DLOG(INFO) << "  DT_PREINIT_ARRAYSZ addr=" << dyn_addr
                      << " count=" << preinit_array_count_;
        break;
      case DT_SYMBOLIC:
        SB_DLOG(INFO) << "  DT_SYMBOLIC";
        has_DT_SYMBOLIC_ = true;
        break;
      case DT_FLAGS:
        if (dyn_value & DF_SYMBOLIC)
          has_DT_SYMBOLIC_ = true;
        break;
      case DT_SONAME:
        soname_ = string_table_ + dyn_value;
        break;
      case DT_HASH:
        SB_DLOG(INFO) << "  DT_HASH addr=0x" << std::hex << dyn_addr;
        elf_hash_.Init(dyn_addr);
        break;
      case DT_GNU_HASH:
        SB_DLOG(INFO) << "  DT_GNU_HASH addr=0x" << std::hex << dyn_addr;
        gnu_hash_.Init(dyn_addr);
        break;
      case DT_STRTAB:
        SB_DLOG(INFO) << "  DT_STRTAB addr=0x" << std::hex << dyn_addr;
        string_table_ = reinterpret_cast<const char*>(dyn_addr);
        break;
      case DT_SYMTAB:
        SB_DLOG(INFO) << "  DT_SYMTAB addr=0x" << std::hex << dyn_addr;
        symbol_table_ = reinterpret_cast<Sym*>(dyn_addr);
        break;
      default:
        break;
    }
  }
  return true;
}

const Dyn* DynamicSection::GetDynamicTable() {
  return dynamic_;
}

size_t DynamicSection::GetDynamicTableSize() {
  return dynamic_count_;
}

void DynamicSection::CallConstructors() {
  CallFunction(init_func_, "DT_INIT");
  for (size_t n = 0; n < init_array_count_; ++n) {
    CallFunction(init_array_[n], "DT_INIT_ARRAY");
  }
}

void DynamicSection::CallDestructors() {
  for (size_t n = fini_array_count_; n > 0; --n) {
    CallFunction(fini_array_[n - 1], "DT_FINI_ARRAY");
  }
  CallFunction(fini_func_, "DT_FINI");
}

void DynamicSection::CallFunction(linker_function_t func,
                                  const char* func_type) {
  uintptr_t func_address = reinterpret_cast<uintptr_t>(func);

  // On some platforms  the entries in the array can be 0 or -1,
  // and should  be ignored e.g. Android:
  // https://android.googlesource.com/platform/bionic/+/android-4.2_r1/linker/README.TXT
  if (func_address != 0 && func_address != uintptr_t(-1)) {
    func();
  }
}

const Sym* DynamicSection::LookupById(size_t symbol_id) const {
  // TODO: Calculated the symbol_table size and validation check.
  return &symbol_table_[symbol_id];
}

bool DynamicSection::IsWeakById(size_t symbol_id) const {
  // TODO: Calculated the symbol_table size and validation check.
  return ELF_ST_BIND(symbol_table_[symbol_id].st_info) == STB_WEAK;
}

const char* DynamicSection::LookupNameById(size_t symbol_id) const {
  const Sym* sym = LookupById(symbol_id);
  // TODO: Confirm that LookupById actually can return NULL.
  if (!sym)
    return NULL;
  return string_table_ + sym->st_name;
}

const Sym* DynamicSection::LookupByName(const char* symbol_name) const {
  const Sym* sym =
      gnu_hash_.IsValid()
          ? gnu_hash_.LookupByName(symbol_name, symbol_table_, string_table_)
          : elf_hash_.LookupByName(symbol_name, symbol_table_, string_table_);

  // Ignore undefined symbols or those that are not global or weak definitions.
  if (!sym || sym->st_shndx == SHN_UNDEF)
    return NULL;

  uint8_t info = ELF_ST_BIND(sym->st_info);
  if (info != STB_GLOBAL && info != STB_WEAK)
    return NULL;

  return sym;
}

}  // namespace elf_loader
}  // namespace starboard
