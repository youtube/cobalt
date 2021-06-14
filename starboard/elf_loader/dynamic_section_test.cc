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

#include "starboard/common/scoped_ptr.h"
#include "starboard/string.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION >= 12 && (SB_API_VERSION >= 12 || SB_HAS(MMAP)) && \
    SB_CAN(MAP_EXECUTABLE_MEMORY)

namespace starboard {
namespace elf_loader {

namespace {

class DynamicSectionTest : public ::testing::Test {
 protected:
  DynamicSectionTest() {}
  ~DynamicSectionTest() {}
};

typedef struct DynamicSectionMemory {
  Dyn dynamic_table_[3];
  linker_function_t init_array_[2];
  linker_function_t fini_array_[2];
  Sym symbol_table_[2];
  char string_table_[128];
} DynamicSectionMemory;

bool gInitCalled = false;
bool gCtor1Called = false;
bool gCtor2Called = false;
bool gDtor1Called = false;
bool gDtor2Called = false;

void InitFunction() {
  gInitCalled = true;
}

void TestConstructor1() {
  gCtor1Called = true;
}

void TestConstructor2() {
  gCtor2Called = true;
}

void TestDestructor1() {
  gDtor1Called = true;
}

void TestDestructor2() {
  gDtor2Called = true;
}

TEST_F(DynamicSectionTest, Initialize) {
  Addr addr = 0x1234;
  Dyn dynamic_table[3];
  Word flags = 0;

  DynamicSection d(addr, dynamic_table, 3, flags);
  ASSERT_TRUE(d.InitDynamicSection());

  ASSERT_EQ(dynamic_table, d.GetDynamicTable());
  ASSERT_EQ(3, d.GetDynamicTableSize());
}

TEST_F(DynamicSectionTest, CallInit) {
  Word flags = 0;
  DynamicSectionMemory dynamic_section_memory;
  Addr base_addr = reinterpret_cast<Addr>(&dynamic_section_memory);
  dynamic_section_memory.dynamic_table_[0].d_tag = DT_INIT;
  dynamic_section_memory.dynamic_table_[0].d_un.d_ptr =
      reinterpret_cast<Addr>(InitFunction) - base_addr;

  DynamicSection d(base_addr, dynamic_section_memory.dynamic_table_, 1, flags);
  ASSERT_TRUE(d.InitDynamicSection());

  ASSERT_EQ(dynamic_section_memory.dynamic_table_, d.GetDynamicTable());
  ASSERT_EQ(1, d.GetDynamicTableSize());
  d.CallConstructors();
  ASSERT_TRUE(gInitCalled);
}

TEST_F(DynamicSectionTest, CallConstructors) {
  Word flags = 0;
  DynamicSectionMemory dynamic_section_memory;
  Addr base_addr = reinterpret_cast<Addr>(&dynamic_section_memory);

  dynamic_section_memory.init_array_[0] =
      reinterpret_cast<linker_function_t>(&TestConstructor1);
  dynamic_section_memory.init_array_[1] =
      reinterpret_cast<linker_function_t>(&TestConstructor2);

  dynamic_section_memory.dynamic_table_[0].d_tag = DT_INIT_ARRAY;
  dynamic_section_memory.dynamic_table_[0].d_un.d_ptr =
      reinterpret_cast<Addr>(dynamic_section_memory.init_array_) - base_addr;

  dynamic_section_memory.dynamic_table_[1].d_tag = DT_INIT_ARRAYSZ;
  dynamic_section_memory.dynamic_table_[1].d_un.d_val =
      sizeof(dynamic_section_memory.init_array_);

  DynamicSection d(base_addr, dynamic_section_memory.dynamic_table_, 2, flags);
  ASSERT_TRUE(d.InitDynamicSection());

  ASSERT_EQ(dynamic_section_memory.dynamic_table_, d.GetDynamicTable());
  ASSERT_EQ(2, d.GetDynamicTableSize());
  d.CallConstructors();
  ASSERT_TRUE(gCtor1Called);
  ASSERT_TRUE(gCtor2Called);
}

TEST_F(DynamicSectionTest, CallDestructors) {
  Word flags = 0;
  DynamicSectionMemory dynamic_section_memory;
  Addr base_addr = reinterpret_cast<Addr>(&dynamic_section_memory);

  dynamic_section_memory.fini_array_[0] =
      reinterpret_cast<linker_function_t>(&TestDestructor1);
  dynamic_section_memory.fini_array_[1] =
      reinterpret_cast<linker_function_t>(&TestDestructor2);

  dynamic_section_memory.dynamic_table_[0].d_tag = DT_FINI_ARRAY;
  dynamic_section_memory.dynamic_table_[0].d_un.d_ptr =
      reinterpret_cast<Addr>(dynamic_section_memory.fini_array_) - base_addr;

  dynamic_section_memory.dynamic_table_[1].d_tag = DT_FINI_ARRAYSZ;
  dynamic_section_memory.dynamic_table_[1].d_un.d_val =
      sizeof(dynamic_section_memory.fini_array_);

  DynamicSection d(base_addr, dynamic_section_memory.dynamic_table_, 2, flags);
  ASSERT_TRUE(d.InitDynamicSection());

  ASSERT_EQ(dynamic_section_memory.dynamic_table_, d.GetDynamicTable());
  ASSERT_EQ(2, d.GetDynamicTableSize());
  d.CallDestructors();
  ASSERT_TRUE(gDtor1Called);
  ASSERT_TRUE(gDtor2Called);
}

TEST_F(DynamicSectionTest, LookupById) {
  Word flags = 0;
  DynamicSectionMemory dynamic_section_memory;
  Addr base_addr = reinterpret_cast<Addr>(&dynamic_section_memory);

  dynamic_section_memory.dynamic_table_[0].d_tag = DT_SYMTAB;
  dynamic_section_memory.dynamic_table_[0].d_un.d_ptr =
      reinterpret_cast<Addr>(dynamic_section_memory.symbol_table_) - base_addr;

  DynamicSection d(base_addr, dynamic_section_memory.dynamic_table_, 1, flags);

  ASSERT_TRUE(d.InitDynamicSection());

  ASSERT_EQ(dynamic_section_memory.dynamic_table_, d.GetDynamicTable());
  ASSERT_EQ(1, d.GetDynamicTableSize());
  ASSERT_EQ(dynamic_section_memory.symbol_table_, d.LookupById(0));
  ASSERT_EQ(dynamic_section_memory.symbol_table_ + 1, d.LookupById(1));
}

TEST_F(DynamicSectionTest, LookupNameById) {
  Word flags = 0;
  DynamicSectionMemory dynamic_section_memory;
  Addr base_addr = reinterpret_cast<Addr>(&dynamic_section_memory);

  dynamic_section_memory.dynamic_table_[0].d_tag = DT_SYMTAB;
  dynamic_section_memory.dynamic_table_[0].d_un.d_ptr =
      reinterpret_cast<Addr>(dynamic_section_memory.symbol_table_) - base_addr;

  dynamic_section_memory.dynamic_table_[1].d_tag = DT_STRTAB;
  dynamic_section_memory.dynamic_table_[1].d_un.d_ptr =
      reinterpret_cast<Addr>(dynamic_section_memory.string_table_) - base_addr;

  memcpy(dynamic_section_memory.string_table_, "test1\x00test2\x00", 12);

  dynamic_section_memory.symbol_table_[0].st_name = 0;  // the offset of test1
  dynamic_section_memory.symbol_table_[1].st_name = 6;  // the offset of test2

  DynamicSection d(base_addr, dynamic_section_memory.dynamic_table_, 2, flags);

  ASSERT_TRUE(d.InitDynamicSection());

  ASSERT_EQ(dynamic_section_memory.dynamic_table_, d.GetDynamicTable());
  ASSERT_EQ(2, d.GetDynamicTableSize());

  ASSERT_EQ(0, strcmp("test1", d.LookupNameById(0)));
  ASSERT_EQ(0, strcmp("test2", d.LookupNameById(1)));
}

}  // namespace
}  // namespace elf_loader
}  // namespace starboard
#endif  // SB_API_VERSION >= 12 && (SB_API_VERSION >= 12
        // || SB_HAS(MMAP)) && SB_CAN(MAP_EXECUTABLE_MEMORY)
