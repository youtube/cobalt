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

#include "starboard/common/scoped_ptr.h"
#include "starboard/elf_loader/elf.h"
#include "starboard/elf_loader/file_impl.h"
#include "starboard/string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace elf_loader {

namespace {

// TODO: implement
class RelocationsTest : public ::testing::Test {
 protected:
  RelocationsTest() {}
  ~RelocationsTest() {}

  scoped_ptr<Relocations> relocations_;
};

#if SB_IS(ARCH_X86) && SB_IS(64_BIT)
TEST_F(RelocationsTest, Initialize_X86_64) {
  char buff[1024] = "AAAAAAAAAAAAAAAAAAA";
  Addr load_bias = reinterpret_cast<Addr>(&buff);
  Dyn dynamic_table[10];
  Dyn entry1;
  entry1.d_tag = DT_REL;

  dynamic_table[0] = entry1;
  Dyn* dyn = dynamic_table;
  size_t dyn_count = 1;
  Word dyn_flags = 0;

  scoped_ptr<DynamicSection> dynamic_section(
      new DynamicSection(load_bias, dyn, dyn_count, dyn_flags));
  dynamic_section->InitDynamicSection();
  dynamic_section->InitDynamicSymbols();

  scoped_ptr<ExportedSymbols> exported_symbols(new ExportedSymbols());
  relocations_.reset(new Relocations(load_bias, dynamic_section.get(),
                                     exported_symbols.get()));
  Rela rela;
  rela.r_offset = 2;
  rela.r_info = R_X86_64_JMP_SLOT;
  rela.r_addend = 5;
  Addr sym_addr = 34;

  Addr target = rela.r_offset + load_bias;
  SB_LOG(INFO) << "target= " << reinterpret_cast<char*>(target);
  relocations_->ApplyResolvedReloc(&rela, sym_addr);
  EXPECT_EQ(39, *reinterpret_cast<Elf64_Sxword*>(buff + 2));
  SB_LOG(INFO) << "buffer= " << *reinterpret_cast<Elf64_Sxword*>(buff + 2);
}
#endif

}  // namespace
}  // namespace elf_loader
}  // namespace starboard
