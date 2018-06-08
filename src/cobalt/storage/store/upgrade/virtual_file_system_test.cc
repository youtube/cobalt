// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/storage/virtual_file.h"
#include "cobalt/storage/virtual_file_system.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "starboard/client_porting/poem/string_poem.h"

namespace cobalt {
namespace storage {

class VirtualFileSystemTest : public testing::Test {
 protected:
  VirtualFileSystem vfs_;
};

TEST_F(VirtualFileSystemTest, WriteAndRead) {
  VirtualFile* file = vfs_.Open("file1.tmp");
  ASSERT_TRUE(file);
  EXPECT_EQ(0, file->Size());

  int expected[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  file->Write(expected, sizeof(expected), 0);

  char out_data[128];
  int bytes = file->Read(out_data, sizeof(out_data), 0);
  EXPECT_EQ(bytes, sizeof(expected));
  EXPECT_EQ(0, memcmp(expected, out_data, sizeof(expected)));
}

TEST_F(VirtualFileSystemTest, ReadWriteOffsets) {
  VirtualFile* file = vfs_.Open("file1.tmp");
  ASSERT_TRUE(file);
  EXPECT_EQ(0, file->Size());

  char expected[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  file->Write(expected, sizeof(expected), 0);

  // Seek to position 4 and read bytes.
  char out_data[128];
  int bytes = file->Read(out_data, sizeof(out_data), 4);

  const int kDataSize = 6;
  EXPECT_EQ(kDataSize, bytes);
  EXPECT_EQ(0, memcmp(&expected[4], out_data, kDataSize));

  // Write some bytes into the middle
  file->Write(expected, 3, 3);
  char expected1[] = {0, 1, 2, 0, 1, 2, 6, 7, 8, 9};
  EXPECT_EQ(sizeof(expected1), file->Size());

  bytes = file->Read(out_data, sizeof(out_data), 0);
  EXPECT_EQ(0, memcmp(expected1, out_data, static_cast<size_t>(bytes)));

  // Try to read past the end.
  const int kOffsetPastEnd = 100000;
  bytes = file->Read(out_data, sizeof(out_data), kOffsetPastEnd);
  EXPECT_EQ(0, bytes);

  // Try to write past the end.
  file->Write(expected, sizeof(expected), kOffsetPastEnd);
  EXPECT_EQ(kOffsetPastEnd + sizeof(expected), file->Size());

  // Make sure the write past the end worked.
  bytes = file->Read(out_data, sizeof(out_data), kOffsetPastEnd);
  EXPECT_EQ(0, memcmp(expected, out_data, static_cast<size_t>(bytes)));
}

TEST_F(VirtualFileSystemTest, Open) {
  // Create a few files and write some data
  VirtualFile* file = vfs_.Open("file1.tmp");
  ASSERT_TRUE(file);
  EXPECT_EQ(0, file->Size());

  // Write a few bytes of random data.
  file->Write("test", 4, 0);
  EXPECT_EQ(4, file->Size());

  // Open a new file
  file = vfs_.Open("file2.tmp");
  ASSERT_TRUE(file);
  EXPECT_EQ(0, file->Size());

  // Try to reopen the existing file.
  file = vfs_.Open("file1.tmp");
  ASSERT_TRUE(file);
  EXPECT_EQ(4, file->Size());
}

TEST_F(VirtualFileSystemTest, Truncate) {
  VirtualFile* file = vfs_.Open("file1.tmp");
  ASSERT_TRUE(file);
  EXPECT_EQ(0, file->Size());

  const char* data = "test";
  char file_contents[4];
  // Write a few bytes of random data.
  file->Write(data, 4, 0);
  EXPECT_EQ(4, file->Size());
  int bytes = file->Read(file_contents, sizeof(file_contents), 0);
  EXPECT_EQ(4, bytes);
  EXPECT_EQ(0, memcmp(file_contents, data, static_cast<size_t>(bytes)));

  file->Truncate(3);
  EXPECT_EQ(3, file->Size());
  bytes = file->Read(file_contents, sizeof(file_contents), 0);
  EXPECT_EQ(3, bytes);
  EXPECT_EQ(0, memcmp(file_contents, data, static_cast<size_t>(bytes)));

  file->Truncate(0);
  EXPECT_EQ(0, file->Size());
  bytes = file->Read(file_contents, sizeof(file_contents), 0);
  EXPECT_EQ(0, bytes);
}

TEST_F(VirtualFileSystemTest, SerializeDeserialize) {
  // Create a few files and write some data
  VirtualFile* file = vfs_.Open("file1.tmp");
  EXPECT_TRUE(file != NULL);
  const char data1[] = "abc";
  int data1_size = 3;
  file->Write(data1, data1_size, 0);

  file = vfs_.Open("file2.tmp");
  EXPECT_TRUE(file != NULL);
  const char data2[] = "defg";
  int data2_size = 4;
  file->Write(data2, data2_size, 0);

  file = vfs_.Open("file3.tmp");
  EXPECT_TRUE(file != NULL);
  const char data3[] = "";
  int data3_size = 0;
  file->Write(data3, data3_size, 0);

  // First perform a dry run to figure out how much space we need.
  int bytes = vfs_.Serialize(NULL, true /*dry run*/);
  scoped_array<uint8> buffer(new uint8[static_cast<size_t>(bytes)]);

  // Now serialize and deserialize
  vfs_.Serialize(buffer.get(), false /*dry run*/);

  // Deserialize the data into a new vfs
  VirtualFileSystem new_vfs;
  ASSERT_TRUE(new_vfs.Deserialize(buffer.get(), bytes));

  // Make sure the new vfs contains all the expected data.
  char file_contents[VirtualFile::kMaxVfsPathname];
  file = new_vfs.Open("file1.tmp");
  EXPECT_TRUE(file != NULL);
  bytes = file->Read(file_contents, sizeof(file_contents), 0);
  EXPECT_EQ(data1_size, bytes);
  EXPECT_EQ(0, memcmp(file_contents, data1, static_cast<size_t>(bytes)));

  file = new_vfs.Open("file2.tmp");
  EXPECT_TRUE(file != NULL);
  bytes = file->Read(file_contents, sizeof(file_contents), 0);
  EXPECT_EQ(data2_size, bytes);
  EXPECT_EQ(0, memcmp(file_contents, data2, static_cast<size_t>(bytes)));

  file = new_vfs.Open("file3.tmp");
  EXPECT_TRUE(file != NULL);
  bytes = file->Read(file_contents, sizeof(file_contents), 0);
  EXPECT_EQ(data3_size, bytes);
  EXPECT_EQ(0, memcmp(file_contents, data3, static_cast<size_t>(bytes)));
}

TEST_F(VirtualFileSystemTest, DeserializeTruncated) {
  // Create a few files and write some data
  VirtualFile* file = vfs_.Open("file1.tmp");
  EXPECT_TRUE(file != NULL);
  const char data1[] = "abc";
  int data1_size = 3;
  file->Write(data1, data1_size, 0);

  file = vfs_.Open("file2.tmp");
  EXPECT_TRUE(file != NULL);
  const char data2[] = "defg";
  int data2_size = 4;
  file->Write(data2, data2_size, 0);

  // First perform a dry run to figure out how much space we need.
  int bytes = vfs_.Serialize(NULL, true /*dry run*/);
  scoped_array<uint8> buffer(new uint8[static_cast<size_t>(bytes)]);

  // Now serialize and deserialize
  vfs_.Serialize(buffer.get(), false /*dry run*/);

  for (int i = 1; i < bytes; i++) {
    // Corrupt the header
    VirtualFileSystem::SerializedHeader header;
    memcpy(&header, buffer.get(), sizeof(header));
    header.file_size = header.file_size - i;
    memcpy(buffer.get(), &header, sizeof(header));

    // Deserialize the data into a new vfs
    VirtualFileSystem new_vfs;
    EXPECT_FALSE(new_vfs.Deserialize(buffer.get(), bytes - i));
  }
}

TEST_F(VirtualFileSystemTest, DeserializeBadData) {
  scoped_array<uint8> buffer(new uint8[0]);
  EXPECT_FALSE(vfs_.Deserialize(buffer.get(), 0));
  EXPECT_FALSE(vfs_.Deserialize(buffer.get(), -1));
  buffer.reset(new uint8[1]);
  EXPECT_FALSE(vfs_.Deserialize(buffer.get(), 1));
  buffer.reset(new uint8[sizeof(VirtualFileSystem::SerializedHeader)]);
  VirtualFileSystem::SerializedHeader header = {};
  header.version = -1;
  memcpy(buffer.get(), &header, sizeof(header));
  EXPECT_FALSE(vfs_.Deserialize(buffer.get(),
                                sizeof(VirtualFileSystem::SerializedHeader)));
  memcpy(&(header.version), "SAV0", sizeof(header.version));
  header.file_size = sizeof(VirtualFileSystem::SerializedHeader);
  memcpy(buffer.get(), &header, sizeof(header));
  EXPECT_TRUE(vfs_.Deserialize(buffer.get(),
                               sizeof(VirtualFileSystem::SerializedHeader)));
  ASSERT_EQ(0, vfs_.ListFiles().size());
}

TEST_F(VirtualFileSystemTest, GetHeaderVersion) {
  VirtualFileSystem::SerializedHeader header;
  memset(&header, 0, sizeof(header));
  header.file_size = sizeof(header);

  COMPILE_ASSERT(sizeof(header.version) == sizeof("SAV0") - 1,
                 Invalid_header_version_size);
  memcpy(&header.version, "XYZW", sizeof(header.version));
  EXPECT_EQ(-1, VirtualFileSystem::GetHeaderVersion(header));
  memcpy(&header.version, "SAV0", sizeof(header.version));
  EXPECT_EQ(0, VirtualFileSystem::GetHeaderVersion(header));
  memcpy(&header.version, "SAV1", sizeof(header.version));
  EXPECT_EQ(1, VirtualFileSystem::GetHeaderVersion(header));
}

}  // namespace storage
}  // namespace cobalt
