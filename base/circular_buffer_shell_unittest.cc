/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base/circular_buffer_shell.h"

#include <string.h>

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// 100 characters, repeating every 16 characters.
const char kTestData[] =
    "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
    "01234567890ABCDEF0123456789ABCDEF0123";

// 100 characters, repeating every 17 characters.
#define UNSET_DATA                                      \
  "GHIJKLMNOPQRSTUVWGHIJKLMNOPQRSTUVWGHIJKLMNOPQRSTUVW" \
  "GHIJKLMNOPQRSTUVWGHIJKLMNOPQRSTUVWGHIJKLMNOPQRSTU"

const char kUnsetData[] = UNSET_DATA;
const size_t kUnsetSize = 1024;

// Like memcmp, but reports which index and values failed.
void IsSame(const char* expected, const char* actual, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    if (expected[i] != actual[i]) {
      EXPECT_EQ(expected[i], actual[i]) << "at " << i;
      return;
    }
  }
}

size_t read_pos = 0;
size_t write_pos = 0;

// If the test uses testWrite and TestRead, then it needs to call ClearPos to
// avoid contamination from previous tests.
void ClearPos() {
  read_pos = 0;
  write_pos = 0;
}

void TestWrite(base::CircularBufferShell* circular_buffer, size_t to_write) {
  size_t before_length = circular_buffer->GetLength();
  size_t bytes_written = kUnsetSize;
  bool result =
      circular_buffer->Write(kTestData + write_pos, to_write, &bytes_written);
  EXPECT_EQ(true, result);
  EXPECT_EQ(to_write, bytes_written);
  EXPECT_EQ(before_length + to_write, circular_buffer->GetLength());
  write_pos += to_write;
}

void TestRead(base::CircularBufferShell* circular_buffer, size_t to_read) {
  size_t before_length = circular_buffer->GetLength();
  char data[] = UNSET_DATA UNSET_DATA;
  char* buffer = data + strlen(kUnsetData);
  size_t bytes_read = kUnsetSize;
  circular_buffer->Read(buffer, to_read, &bytes_read);
  EXPECT_EQ(to_read, bytes_read);
  EXPECT_EQ(before_length - to_read, circular_buffer->GetLength());
  IsSame(kTestData + read_pos, buffer, to_read);
  IsSame(kUnsetData + to_read, buffer + to_read, to_read);
  IsSame(kUnsetData + strlen(kUnsetData) - to_read,
         buffer - to_read, to_read);
  read_pos += to_read;
}

}  // namespace

// --- Sunny Day Tests ---

TEST(CircularBufferShellTest, Construct) {
  ClearPos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));
}

TEST(CircularBufferShellTest, SimpleWriteAndRead) {
  ClearPos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));

  TestWrite(circular_buffer.get(), 15);
  TestRead(circular_buffer.get(), 5);
  TestRead(circular_buffer.get(), 4);
  TestRead(circular_buffer.get(), 3);
  TestRead(circular_buffer.get(), 2);
  TestRead(circular_buffer.get(), 1);
}

TEST(CircularBufferShellTest, ReadWriteOverBoundary) {
  ClearPos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));

  // Fill the buffer.
  TestWrite(circular_buffer.get(), 20);

  // Read half the data from the front.
  TestRead(circular_buffer.get(), 10);

  // Fill the back half, making the data wrap around the end.
  TestWrite(circular_buffer.get(), 10);

  // Read the whole thing, which should require two memcpys.
  TestRead(circular_buffer.get(), 20);

  // Fill the buffer, again around the end, should require two memcpys.
  TestWrite(circular_buffer.get(), 20);

  // Read the buffer to verify, should again require two memcpys.
  TestRead(circular_buffer.get(), 20);
}

TEST(CircularBufferShellTest, ExpandWhileNotWrapped) {
  ClearPos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));

  // Set the size with the first write.
  TestWrite(circular_buffer.get(), 5);

  // Expand with the second write.
  TestWrite(circular_buffer.get(), 5);

  // Read to verify the data is intact
  TestRead(circular_buffer.get(), 10);
}

TEST(CircularBufferShellTest, ExpandWhileNotWrapped2) {
  ClearPos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));

  // Set the size with the first write.
  TestWrite(circular_buffer.get(), 5);

  // Read a couple out so that the data doesn't start at the beginning of the
  // buffer.
  TestRead(circular_buffer.get(), 2);

  // Expand with the second write.
  TestWrite(circular_buffer.get(), 5);

  // Read to verify the data is intact
  TestRead(circular_buffer.get(), 8);
}

TEST(CircularBufferShellTest, ExpandWhileWrapped) {
  ClearPos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));

  // Set the size with the first write.
  TestWrite(circular_buffer.get(), 10);

  // Read front half.
  TestRead(circular_buffer.get(), 5);

  // Wrap with second write
  TestWrite(circular_buffer.get(), 5);

  // Write again to expand while wrapped.
  TestWrite(circular_buffer.get(), 5);

  // Read to verify the data is intact
  TestRead(circular_buffer.get(), 15);
}

// --- Rainy Day Tests ---

TEST(CircularBufferShellTest, WriteTooMuch) {
  ClearPos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));

  {
    size_t bytes_written = kUnsetSize;
    bool result = circular_buffer->Write(kTestData, 25, &bytes_written);
    EXPECT_FALSE(result);
    EXPECT_EQ(kUnsetSize, bytes_written);
    EXPECT_EQ(0, circular_buffer->GetLength());
  }
}

TEST(CircularBufferShellTest, ReadEmpty) {
  ClearPos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));

  EXPECT_EQ(circular_buffer->GetLength(), 0);

  {
    char buffer[] = UNSET_DATA;
    size_t bytes_read = kUnsetSize;
    circular_buffer->Read(buffer, 0, &bytes_read);
    EXPECT_EQ(0, bytes_read);
    EXPECT_EQ(0, circular_buffer->GetLength());
    IsSame(kUnsetData, buffer, 10);
  }

  {
    char buffer[] = UNSET_DATA;
    size_t bytes_read = kUnsetSize;
    circular_buffer->Read(buffer, 10, &bytes_read);
    EXPECT_EQ(0, bytes_read);
    EXPECT_EQ(0, circular_buffer->GetLength());
    IsSame(kUnsetData, buffer, 10);
  }
}

TEST(CircularBufferShellTest, ReadToNull) {
  ClearPos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));

  {
    size_t bytes_read = kUnsetSize;
    circular_buffer->Read(NULL, 0, &bytes_read);
    EXPECT_EQ(0, bytes_read);
  }
}

TEST(CircularBufferShellTest, Peek) {
  const size_t kMaxCapacity = 20;
  base::CircularBufferShell circular_buffer(kMaxCapacity);
  size_t bytes_peeked;
  size_t peek_offset = 0;

  circular_buffer.Write(kTestData, kMaxCapacity, NULL);

  // Peek with offset 0.
  {
    char destination[] = UNSET_DATA;
    circular_buffer.Peek(destination + 9, 10, peek_offset, &bytes_peeked);

    EXPECT_EQ(10, bytes_peeked);
    IsSame(UNSET_DATA, destination, 9);
    IsSame(UNSET_DATA + 9 + bytes_peeked,
           destination + 9 + bytes_peeked,
           sizeof(UNSET_DATA) - 9 - bytes_peeked);
    IsSame(kTestData, destination + 9, 10);
    peek_offset += bytes_peeked;
  }

  // Peek with non-zero offset.
  {
    char destination[] = UNSET_DATA;
    circular_buffer.Peek(destination + 9, 7, peek_offset, &bytes_peeked);

    EXPECT_EQ(7, bytes_peeked);
    IsSame(UNSET_DATA, destination, 9);
    IsSame(UNSET_DATA + 9 + bytes_peeked,
           destination + 9 + bytes_peeked,
           sizeof(UNSET_DATA) - 9 - bytes_peeked);
    IsSame(kTestData + peek_offset, destination + 9, bytes_peeked);
    peek_offset += bytes_peeked;
  }

  // Peek more data than available.
  {
    char destination[] = UNSET_DATA;
    circular_buffer.Peek(destination + 9, 7, peek_offset, &bytes_peeked);

    EXPECT_EQ(3, bytes_peeked);
    IsSame(UNSET_DATA, destination, 9);
    IsSame(UNSET_DATA + 9 + bytes_peeked,
           destination + 9 + bytes_peeked,
           sizeof(UNSET_DATA) - 9 - bytes_peeked);
    IsSame(kTestData + peek_offset, destination + 9, bytes_peeked);
    peek_offset += bytes_peeked;
  }

  // Peek an empty buffer.
  {
    char destination[] = UNSET_DATA;
    circular_buffer.Peek(destination + 9, 7, peek_offset, &bytes_peeked);

    IsSame(UNSET_DATA, destination, sizeof(destination));
    EXPECT_EQ(0, bytes_peeked);
    // Verify that we are actually peeking instead of reading.
    EXPECT_EQ(kMaxCapacity, circular_buffer.GetLength());
  }
}

TEST(CircularBufferShellTest, Skip) {
  const size_t kMaxCapacity = 20;
  base::CircularBufferShell circular_buffer(kMaxCapacity);
  char destination[] = UNSET_DATA UNSET_DATA;
  size_t bytes;

  circular_buffer.Write(kTestData, kMaxCapacity, NULL);
  circular_buffer.Skip(10, &bytes);
  EXPECT_EQ(10, bytes);
  EXPECT_EQ(kMaxCapacity - 10, circular_buffer.GetLength());

  circular_buffer.Read(destination, kMaxCapacity, &bytes);

  EXPECT_EQ(kMaxCapacity - 10, bytes);
  IsSame(kTestData + 10, destination, bytes);
}

TEST(CircularBufferShellTest, PeekWrapped) {
  const size_t kMaxCapacity = 20;
  base::CircularBufferShell circular_buffer(kMaxCapacity);
  char destination[] = UNSET_DATA;
  size_t bytes_peeked;

  circular_buffer.Write(kTestData, kMaxCapacity, NULL);

  // Skip 10 bytes to free some space.
  circular_buffer.Skip(10, &bytes_peeked);

  // Fill the free space with new data.
  circular_buffer.Write(kTestData + kMaxCapacity, 10, NULL);
  EXPECT_EQ(kMaxCapacity, circular_buffer.GetLength());

  // Peek with a non-zero offset.
  circular_buffer.Peek(destination, kMaxCapacity, 5, &bytes_peeked);

  EXPECT_EQ(kMaxCapacity - 5, bytes_peeked);
  IsSame(kTestData + 15, destination, bytes_peeked);
}

TEST(CircularBufferShellTest, SkipWrapped) {
  const size_t kMaxCapacity = 20;
  base::CircularBufferShell circular_buffer(kMaxCapacity);
  char destination[] = UNSET_DATA;
  size_t bytes;

  circular_buffer.Write(kTestData, kMaxCapacity, NULL);
  circular_buffer.Skip(10, &bytes);

  circular_buffer.Write(kTestData + kMaxCapacity, 10, NULL);
  EXPECT_EQ(kMaxCapacity, circular_buffer.GetLength());

  circular_buffer.Skip(kMaxCapacity - 5, NULL);
  EXPECT_EQ(5, circular_buffer.GetLength());

  circular_buffer.Read(destination, 5, &bytes);
  EXPECT_EQ(5, bytes);
  IsSame(kTestData + kMaxCapacity + 5, destination, 5);
}

// --- Legacy Tests ---

TEST(CircularBufferShellTest, Basic) {
  const int max_buffer_length = 10;
  const int kReadSize1 = 4;
  const int kReadSize2 = 2;
  const int kReadSize3 = 4;
  const int kReadSize4 = 6;

  // Create a Circular Buffer.
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(max_buffer_length));
  ASSERT_TRUE(circular_buffer);
  EXPECT_EQ(circular_buffer->GetLength(), 0);

  char read_buffer[20];
  size_t bytes_read = 0;
  size_t bytes_written = 0;
  // Read 4 bytes, got read_pos 0, write_pos 0
  circular_buffer->Read(read_buffer, kReadSize1, &bytes_read);
  EXPECT_EQ(bytes_read, 0);

  // Write 5 bytes, got read_pos 0, write_pos 5
  const char write_buffer[] = "hello";
  circular_buffer->Write(write_buffer, strlen(write_buffer), &bytes_written);
  EXPECT_EQ(bytes_written, strlen(write_buffer));
  EXPECT_EQ(circular_buffer->GetLength(), strlen(write_buffer));

  // Write 1 byte, increased buffer size to 10, read_pos 0, write_pos 6
  const char write_buffer2[] = " ";
  circular_buffer->Write(write_buffer2, strlen(write_buffer2), &bytes_written);
  EXPECT_EQ(bytes_written, strlen(write_buffer2));
  EXPECT_EQ(circular_buffer->GetLength(),
            strlen(write_buffer) + strlen(write_buffer2));

  // Read 2 bytes, got read_pos 2, write_pos 6
  circular_buffer->Read(read_buffer, kReadSize2, &bytes_read);
  EXPECT_EQ(0, memcmp(read_buffer, "he", kReadSize2));
  EXPECT_EQ(bytes_read, kReadSize2);
  EXPECT_EQ(circular_buffer->GetLength(),
            strlen(write_buffer) + strlen(write_buffer2) - kReadSize2);

  // Write 6 bytes, got read_pos 2, write_pos 2, full of data
  const char write_buffer3[] = "world!";
  circular_buffer->Write(write_buffer3, strlen(write_buffer3), &bytes_written);
  EXPECT_EQ(bytes_written, strlen(write_buffer3));
  EXPECT_EQ(circular_buffer->GetLength(),
            strlen(write_buffer) + strlen(write_buffer2) +
                strlen(write_buffer3) - kReadSize2);

  // Read 4 bytes, got read_pos 6, write_pos 2
  circular_buffer->Read(read_buffer, kReadSize3, &bytes_read);
  EXPECT_EQ(bytes_read, kReadSize3);
  EXPECT_EQ(0, memcmp(read_buffer, "llo ", kReadSize3));
  EXPECT_EQ(circular_buffer->GetLength(),
            strlen(write_buffer) + strlen(write_buffer2) +
                strlen(write_buffer3) - kReadSize2 - kReadSize3);

  // Read 6 bytes, got read_pos 2, write_pos 2, empty
  circular_buffer->Read(read_buffer, kReadSize4, &bytes_read);
  EXPECT_EQ(bytes_read, kReadSize4);
  EXPECT_EQ(0, memcmp(read_buffer, "world!", kReadSize4));
  EXPECT_EQ(circular_buffer->GetLength(), 0);
}

TEST(CircularBufferShellTest, CycleReadWrite) {
  const int max_buffer_length = 5000;
  // Create a Circular Buffer.
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(max_buffer_length));
  ASSERT_TRUE(circular_buffer);
  EXPECT_EQ(circular_buffer->GetLength(), 0);

  size_t bytes_written = 0;
  size_t bytes_read = 0;
  char write_buffer[500];
  char read_buffer[2000];

  for (int i = 0; i < 50; ++i) {
    circular_buffer->Write(write_buffer, sizeof(write_buffer), &bytes_written);
    EXPECT_EQ(bytes_written, sizeof(write_buffer));
    EXPECT_EQ(circular_buffer->GetLength(), sizeof(write_buffer));

    circular_buffer->Write(write_buffer, sizeof(write_buffer), &bytes_written);
    EXPECT_EQ(bytes_written, sizeof(write_buffer));
    EXPECT_EQ(circular_buffer->GetLength(),
              sizeof(write_buffer) + sizeof(write_buffer));

    circular_buffer->Read(read_buffer, sizeof(read_buffer), &bytes_read);
    EXPECT_EQ(bytes_read, sizeof(write_buffer) + sizeof(write_buffer));
    EXPECT_EQ(circular_buffer->GetLength(), 0);
  }
}

TEST(CircularBufferShellTest, IncreaseMaxCapacityTo) {
  ClearPos();

  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(0));
  EXPECT_EQ(circular_buffer->GetMaxCapacity(), 0);

  // Increase max capacity by 20 to allow for expanding.
  circular_buffer->IncreaseMaxCapacityTo(20);
  EXPECT_EQ(circular_buffer->GetMaxCapacity(), 20);

  // Set the size with the first write.
  TestWrite(circular_buffer.get(), 10);

  // Expand to the full capacity with the second write.
  TestWrite(circular_buffer.get(), 10);

  // Verify if the buffer is full by check the failure of writing one byte.
  char ch = 'a';
  size_t bytes_written = 0;
  ASSERT_FALSE(circular_buffer->Write(&ch, 1, &bytes_written));

  // Increase max capacity to 30 to allow for further expanding.
  circular_buffer->IncreaseMaxCapacityTo(30);
  EXPECT_EQ(circular_buffer->GetMaxCapacity(), 30);

  // Expand to capacity with the fourth write.
  TestWrite(circular_buffer.get(), 10);

  // Verify if the buffer is full by check the failure of writing one byte.
  ASSERT_FALSE(circular_buffer->Write(&ch, 1, &bytes_written));

  // Drain the circular_buffer.
  EXPECT_EQ(circular_buffer->GetLength(), 30);
  TestRead(circular_buffer.get(), 30);
  EXPECT_EQ(circular_buffer->GetLength(), 0);
}

TEST(CircularBufferShellTest, IncreaseMaxCapacityToWrapped) {
  ClearPos();

  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(10));
  EXPECT_EQ(circular_buffer->GetMaxCapacity(), 10);

  // Expand to the full capacity with the first write.
  TestWrite(circular_buffer.get(), 10);

  // Partial read
  TestRead(circular_buffer.get(), 5);

  // The buffer is wrapped after the second write.
  TestWrite(circular_buffer.get(), 5);

  // Verify if the buffer is full by check the failure of writing one byte.
  char ch = 'a';
  size_t bytes_written = 0;
  ASSERT_FALSE(circular_buffer->Write(&ch, 1, &bytes_written));

  // Increase max capacity to 20 to allow for further expanding.
  circular_buffer->IncreaseMaxCapacityTo(20);
  EXPECT_EQ(circular_buffer->GetMaxCapacity(), 20);

  // Expand to capacity with the fourth write.
  TestWrite(circular_buffer.get(), 10);

  // Verify if the buffer is full by check the failure of writing one byte.
  ASSERT_FALSE(circular_buffer->Write(&ch, 1, &bytes_written));

  // Drain the circular_buffer.
  EXPECT_EQ(circular_buffer->GetLength(), 20);
  TestRead(circular_buffer.get(), 20);
  EXPECT_EQ(circular_buffer->GetLength(), 0);
}
