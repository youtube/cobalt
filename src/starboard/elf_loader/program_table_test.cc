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

#include <string>
#include <vector>

#include "starboard/common/scoped_ptr.h"
#include "starboard/elf_loader/file.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION >= 12 && (SB_API_VERSION >= 12 || SB_HAS(MMAP)) && \
    SB_CAN(MAP_EXECUTABLE_MEMORY)
namespace starboard {
namespace elf_loader {

namespace {

class DummyFile : public File {
 public:
  typedef struct FileChunk {
    FileChunk(int file_offset, const char* buffer, int size)
        : file_offset_(file_offset), buffer_(buffer), size_(size) {}
    int file_offset_;
    const char* buffer_;
    int size_;
  } FileChunk;

  explicit DummyFile(const std::vector<FileChunk>& file_chunks)
      : file_chunks_(file_chunks), read_index_(0) {}

  bool Open(const char* name) {
    name_ = name;
    return true;
  }

  bool ReadFromOffset(int64_t offset, char* buffer, int size) {
    SB_LOG(INFO) << "ReadFromOffset offset=" << offset << " size=" << size
                 << " read_index_=" << read_index_;
    if (read_index_ >= file_chunks_.size()) {
      SB_LOG(INFO) << "ReadFromOffset EOF";
      return false;
    }
    const FileChunk& file_chunk = file_chunks_[read_index_++];
    if (offset != file_chunk.file_offset_) {
      SB_LOG(ERROR) << "ReadFromOffset: Invalid offset " << offset
                    << " expected " << file_chunk.file_offset_;
      return false;
    }
    if (size > file_chunk.size_) {
      SB_LOG(ERROR) << "ReadFromOffset: Invalid size " << size << " expected < "
                    << file_chunk.size_;
      return false;
    }
    memcpy(buffer, file_chunk.buffer_, size);
    return true;
  }
  void Close() {}

  const std::string& GetName() { return name_; }

 private:
  int file_offset_;
  const char* buffer_;
  int size_;
  std::vector<FileChunk> file_chunks_;
  int read_index_;
  std::string name_;
};

class ProgramTableTest : public ::testing::Test {
 protected:
  ProgramTableTest() { program_table_.reset(new ProgramTable(nullptr)); }
  ~ProgramTableTest() {}

  void HelperMethod() {}

 protected:
  scoped_ptr<ProgramTable> program_table_;
};

TEST_F(ProgramTableTest, LoadSegments) {
  // File structure
  // [Phdr1]
  // [Phdr2]
  // [200, 300) segment for phdr1
  // [250, 300) dynamic section in segment for phdr1
  // [400, 500) segment for phdr2
  Ehdr ehdr;
  ehdr.e_phnum = 3;
  ehdr.e_phoff = 0;
  ehdr.e_phentsize = sizeof(Phdr);

  Phdr ent1;
  Phdr ent2;
  Phdr ent3;
  memset(&ent1, 0, sizeof(Phdr));
  memset(&ent2, 0, sizeof(Phdr));
  memset(&ent3, 0, sizeof(Phdr));

  ent1.p_type = PT_LOAD;
  ent1.p_vaddr = 0;
  ent1.p_memsz = 2 * PAGE_SIZE;
  ent1.p_offset = 200;
  ent1.p_filesz = 100;
  ent1.p_flags = kSbMemoryMapProtectRead;

  ent2.p_type = PT_LOAD;
  ent2.p_vaddr = 2 * PAGE_SIZE;
  ent2.p_memsz = 3 * PAGE_SIZE;
  ent2.p_offset = 400;
  ent2.p_filesz = 100;
  ent1.p_flags = kSbMemoryMapProtectRead | kSbMemoryMapProtectExec;

  ent3.p_type = PT_DYNAMIC;
  ent3.p_vaddr = 250;
  ent3.p_memsz = 3 * sizeof(Dyn);
  ent3.p_offset = 250;
  ent3.p_filesz = 5 * sizeof(Dyn);
  ent3.p_flags = 0x42;

  Phdr program_table_data[3];
  program_table_data[0] = ent1;
  program_table_data[1] = ent2;
  program_table_data[2] = ent3;

  Dyn dynamic_table_data[3];
  dynamic_table_data[0].d_tag = DT_DEBUG;
  dynamic_table_data[1].d_tag = DT_DEBUG;
  dynamic_table_data[2].d_tag = DT_DEBUG;

  char program_table_page[PAGE_SIZE];
  memset(program_table_page, 0, sizeof(program_table_page));
  memcpy(program_table_page, program_table_data, sizeof(program_table_data));

  char segment_file_data1[2 * PAGE_SIZE];
  char segment_file_data2[3 * PAGE_SIZE];

  memcpy(segment_file_data1 + 250, dynamic_table_data,
         sizeof(dynamic_table_data));

  std::vector<DummyFile::FileChunk> file_chunks;
  file_chunks.push_back(
      DummyFile::FileChunk(0, program_table_page, sizeof(program_table_page)));
  file_chunks.push_back(
      DummyFile::FileChunk(0, segment_file_data1, sizeof(segment_file_data1)));
  file_chunks.push_back(
      DummyFile::FileChunk(0, segment_file_data2, sizeof(segment_file_data2)));

  DummyFile file(file_chunks);

  EXPECT_TRUE(program_table_->LoadProgramHeader(&ehdr, &file));

  EXPECT_EQ(program_table_->GetBaseMemoryAddress(), 0);

  EXPECT_TRUE(program_table_->ReserveLoadMemory());

  EXPECT_NE(program_table_->GetBaseMemoryAddress(), 0);

  EXPECT_TRUE(program_table_->LoadSegments(&file));

  Dyn* dynamic = NULL;
  size_t dynamic_count = 0;
  Word dynamic_flags = 0;

  program_table_->GetDynamicSection(&dynamic, &dynamic_count, &dynamic_flags);
  Dyn* expected_dyn = reinterpret_cast<Dyn*>(
      program_table_->GetBaseMemoryAddress() + ent3.p_vaddr);
  EXPECT_TRUE(dynamic != NULL);
  EXPECT_EQ(dynamic[0].d_tag, DT_DEBUG);
  EXPECT_EQ(dynamic[1].d_tag, DT_DEBUG);
  EXPECT_EQ(dynamic[2].d_tag, DT_DEBUG);
  EXPECT_EQ(dynamic_count, 3);
  EXPECT_EQ(dynamic_flags, 0x42);
}

}  // namespace
}  // namespace elf_loader
}  // namespace starboard
#endif  // SB_API_VERSION >= 12 && (SB_API_VERSION >= 12
        // || SB_HAS(MMAP)) && SB_CAN(MAP_EXECUTABLE_MEMORY)
