#include "base/circular_buffer_shell.h"

#include <string.h>

#include "external/chromium/base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

// 100 characters, repeating every 16 characters.
static const char kTestData[] = "0123456789ABCDEF01234567890ABCDEF"
    "0123456789ABCDEF0123456789ABCDEF01234567890ABCDEF0123456789ABCDEF" "0123";

// 100 characters, repeating every 17 characters.
#define UNSET_DATA "GHIJKLMNOPQRSTUVWGHIJKLMNOPQRSTUVWGHIJKLMNOPQRSTUVW" \
    "GHIJKLMNOPQRSTUVWGHIJKLMNOPQRSTUVWGHIJKLMNOPQRSTU"

static const char kUnsetData[] = UNSET_DATA;
static const size_t kUnsetSize = 1024;


// Like memcmp, but reports which index and values failed.
static bool is_same(const char *expected, const char *actual, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    if (expected[i] != actual[i]) {
      printf("Expected '%c' at %lu, but got '%c'\n", expected[i], i, actual[i]);
      return false;
    }
  }

  return true;
}

static size_t read_pos = 0;
static size_t write_pos = 0;

// If the test uses testWrite and test_read, then it needs to call clear_pos to
// avoid contamination from previous tests.
static void clear_pos() {
  read_pos = 0;
  write_pos = 0;
}

static void test_write(base::CircularBufferShell *circular_buffer,
                      size_t to_write) {
  size_t before_length = circular_buffer->GetLength();
  size_t bytes_written = kUnsetSize;
  bool result = circular_buffer->Write(kTestData + write_pos, to_write,
                                      &bytes_written);
  EXPECT_EQ(true, result);
  EXPECT_EQ(to_write, bytes_written);
  EXPECT_EQ(before_length + to_write, circular_buffer->GetLength());
  write_pos += to_write;
}

static void test_read(base::CircularBufferShell *circular_buffer,
                     size_t to_read) {
  size_t before_length = circular_buffer->GetLength();
  char data[] = UNSET_DATA UNSET_DATA;
  char *buffer = data + strlen(kUnsetData);
  size_t bytes_read = kUnsetSize;
  circular_buffer->Read(buffer, to_read, &bytes_read);
  EXPECT_EQ(to_read, bytes_read);
  EXPECT_EQ(before_length - to_read, circular_buffer->GetLength());
  EXPECT_TRUE(is_same(kTestData + read_pos, buffer, to_read));
  EXPECT_TRUE(is_same(kUnsetData + to_read, buffer + to_read, to_read));
  EXPECT_TRUE(is_same(kUnsetData + strlen(kUnsetData) - to_read,
                          buffer - to_read, to_read));
  read_pos += to_read;
}

// --- Sunny Day Tests ---

TEST(CircularBufferShellTest, Construct) {
  clear_pos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));
}

TEST(CircularBufferShellTest, SimpleWriteAndRead) {
  clear_pos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));

  test_write(circular_buffer.get(), 15);
  test_read(circular_buffer.get(), 5);
  test_read(circular_buffer.get(), 4);
  test_read(circular_buffer.get(), 3);
  test_read(circular_buffer.get(), 2);
  test_read(circular_buffer.get(), 1);
}

TEST(CircularBufferShellTest, ReadWriteOverBoundary) {
  clear_pos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));

  // Fill the buffer.
  test_write(circular_buffer.get(), 20);

  // Read half the data from the front.
  test_read(circular_buffer.get(), 10);

  // Fill the back half, making the data wrap around the end.
  test_write(circular_buffer.get(), 10);

  // Read the whole thing, which should require two memcpys.
  test_read(circular_buffer.get(), 20);

  // Fill the buffer, again around the end, should require two memcpys.
  test_write(circular_buffer.get(), 20);

  // Read the buffer to verify, should again require two memcpys.
  test_read(circular_buffer.get(), 20);
}

TEST(CircularBufferShellTest, ExpandWhileNotWrapped) {
  clear_pos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));

  // Set the size with the first write.
  test_write(circular_buffer.get(), 5);

  // Expand with the second write.
  test_write(circular_buffer.get(), 5);

  // Read to verify the data is intact
  test_read(circular_buffer.get(), 10);

}

TEST(CircularBufferShellTest, ExpandWhileWrapped) {
  clear_pos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));

  // Set the size with the first write.
  test_write(circular_buffer.get(), 10);

  // Read front half.
  test_read(circular_buffer.get(), 5);

  // Wrap with second write
  test_write(circular_buffer.get(), 5);

  // Write again to expand while wrapped.
  test_write(circular_buffer.get(), 5);

  // Read to verify the data is intact
  test_read(circular_buffer.get(), 15);
}


// --- Rainy Day Tests ---

TEST(CircularBufferShellTest, WriteTooMuch) {
  clear_pos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));

  {
    size_t bytes_written = kUnsetSize;
    bool result = circular_buffer->Write(kTestData, 25, &bytes_written);
    EXPECT_EQ(false, result);
    EXPECT_EQ(kUnsetSize, bytes_written);
    EXPECT_EQ(0, circular_buffer->GetLength());
  }
}

TEST(CircularBufferShellTest, ReadEmpty) {
  clear_pos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));

  EXPECT_EQ(circular_buffer->GetLength(), 0);

  {
    char buffer[] = UNSET_DATA;
    size_t bytes_read = kUnsetSize;
    circular_buffer->Read(buffer, 0, &bytes_read);
    EXPECT_EQ(0, bytes_read);
    EXPECT_EQ(0, circular_buffer->GetLength());
    EXPECT_TRUE(is_same(kUnsetData, buffer, 10));
  }

  {
    char buffer[] = UNSET_DATA;
    size_t bytes_read = kUnsetSize;
    circular_buffer->Read(buffer, 10, &bytes_read);
    EXPECT_EQ(0, bytes_read);
    EXPECT_EQ(0, circular_buffer->GetLength());
    EXPECT_TRUE(is_same(kUnsetData, buffer, 10));
  }
}

TEST(CircularBufferShellTest, ReadToNull) {
  clear_pos();
  scoped_ptr<base::CircularBufferShell> circular_buffer(
      new base::CircularBufferShell(20));

  {
    size_t bytes_read = kUnsetSize;
    circular_buffer->Read(NULL, 0, &bytes_read);
    EXPECT_EQ(0, bytes_read);
  }
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
      strlen(write_buffer) + strlen(write_buffer2) + strlen(write_buffer3)
      - kReadSize2);

  // Read 4 bytes, got read_pos 6, write_pos 2
  circular_buffer->Read(read_buffer, kReadSize3, &bytes_read);
  EXPECT_EQ(bytes_read, kReadSize3);
  EXPECT_EQ(0, memcmp(read_buffer, "llo ", kReadSize3));
  EXPECT_EQ(circular_buffer->GetLength(),
      strlen(write_buffer) + strlen(write_buffer2) + strlen(write_buffer3)
      - kReadSize2 - kReadSize3);

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
