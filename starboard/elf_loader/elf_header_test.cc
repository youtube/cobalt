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

#include <string>

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
  DummyFile(const char* buffer, int size) : buffer_(buffer), size_(size) {}

  bool Open(const char* name) {
    name_ = name;
    return true;
  }
  bool ReadFromOffset(int64_t offset, char* buffer, int size) {
    SB_LOG(INFO) << "ReadFromOffset";
    if (offset != 0) {
      SB_LOG(ERROR) << "ReadFromOffset: Invalid offset " << offset;
      return false;
    }
    if (size < size_) {
      SB_LOG(ERROR) << "ReadFromOffset: Invalid size " << size;
      return false;
    }
    memcpy(buffer, buffer_, size);
    return true;
  }
  void Close() {}

  const std::string& GetName() { return name_; }

 private:
  const char* buffer_;
  int size_;
  std::string name_;
};

class ElfHeaderTest : public ::testing::Test {
 protected:
  ElfHeaderTest() {
    elf_header_.reset(new ElfHeader());
    memset(reinterpret_cast<char*>(&ehdr_data_), 0, sizeof(ehdr_data_));
    ehdr_data_.e_machine = ELF_MACHINE;
    ehdr_data_.e_ident[0] = 0x7F;
    ehdr_data_.e_ident[1] = 'E';
    ehdr_data_.e_ident[2] = 'L';
    ehdr_data_.e_ident[3] = 'F';
    ehdr_data_.e_ident[EI_CLASS] = ELF_CLASS_VALUE;
    ehdr_data_.e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr_data_.e_type = ET_DYN;
    ehdr_data_.e_version = EV_CURRENT;
    ehdr_data_.e_machine = ELF_MACHINE;

    dummy_file_.reset(new DummyFile(reinterpret_cast<const char*>(&ehdr_data_),
                                    sizeof(ehdr_data_)));
  }
  ~ElfHeaderTest() {}

  scoped_ptr<ElfHeader> elf_header_;
  Ehdr ehdr_data_;
  scoped_ptr<DummyFile> dummy_file_;
};

TEST_F(ElfHeaderTest, Initialize) {
  EXPECT_TRUE(elf_header_->LoadElfHeader(dummy_file_.get()));
}

TEST_F(ElfHeaderTest, NegativeBadImage) {
  ehdr_data_.e_ident[1] = 'F';
  dummy_file_.reset(new DummyFile(reinterpret_cast<const char*>(&ehdr_data_),
                                  sizeof(ehdr_data_)));
  EXPECT_FALSE(elf_header_->LoadElfHeader(dummy_file_.get()));
}

TEST_F(ElfHeaderTest, NegativeBadClass) {
  ehdr_data_.e_ident[EI_CLASS] = 0;
  dummy_file_.reset(new DummyFile(reinterpret_cast<const char*>(&ehdr_data_),
                                  sizeof(ehdr_data_)));
  EXPECT_FALSE(elf_header_->LoadElfHeader(dummy_file_.get()));
}

TEST_F(ElfHeaderTest, NegativeWrongGulliverLilliput) {
  ehdr_data_.e_type = 2;
  dummy_file_.reset(new DummyFile(reinterpret_cast<const char*>(&ehdr_data_),
                                  sizeof(ehdr_data_)));
  EXPECT_FALSE(elf_header_->LoadElfHeader(dummy_file_.get()));
}

TEST_F(ElfHeaderTest, NegativeBadType) {
  ehdr_data_.e_type = 2;
  dummy_file_.reset(new DummyFile(reinterpret_cast<const char*>(&ehdr_data_),
                                  sizeof(ehdr_data_)));
  EXPECT_FALSE(elf_header_->LoadElfHeader(dummy_file_.get()));
}

TEST_F(ElfHeaderTest, NegativeBadVersion) {
  ehdr_data_.e_version = 2;
  dummy_file_.reset(new DummyFile(reinterpret_cast<const char*>(&ehdr_data_),
                                  sizeof(ehdr_data_)));
  EXPECT_FALSE(elf_header_->LoadElfHeader(dummy_file_.get()));
}

TEST_F(ElfHeaderTest, NegativeBadMachine) {
  ehdr_data_.e_machine = 0;
  dummy_file_.reset(new DummyFile(reinterpret_cast<const char*>(&ehdr_data_),
                                  sizeof(ehdr_data_)));
  EXPECT_FALSE(elf_header_->LoadElfHeader(dummy_file_.get()));
}
}  // namespace
}  // namespace elf_loader
}  // namespace starboard
#endif  // SB_API_VERSION >= 12 && (SB_API_VERSION >= 12
        // || SB_HAS(MMAP)) && SB_CAN(MAP_EXECUTABLE_MEMORY)
