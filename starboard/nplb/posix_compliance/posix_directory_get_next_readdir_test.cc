// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// Scenarios that aren't tested:
// 1. Invalid directory pointer (ie, readdir(invalid_pointer)) as this behavior
// crashes with a seg fault.
// TODO(b/419539055): Test seg fault scenarios.
// 2. EOVERFLOW error: This error is hard to reliably test as it depends on
// filesystem characteristics and internal structure of dirent. It's usually
// handled by the underlying filesystem implementation.
// 3. Concurrent calls on the same directory from different threads, readdir is
// not necessarily threadsafe, so this behavior shouldn't be tested.

struct DirDeleter {
  void operator()(DIR* dir) const { closedir(dir); }
};
using UniqueDir = std::unique_ptr<DIR, DirDeleter>;

class PosixReaddirTests : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.IsValid());
    test_dir_ = temp_dir_.path();
  }

  ScopedTempDir temp_dir_;
  std::string test_dir_;
  const int flags_ = O_CREAT | O_WRONLY | O_TRUNC;
};

// Test that d_ino is populated correctly (non-zero).
TEST_F(PosixReaddirTests, DInoPopulated) {
  // Create a file.
  std::string file_path = test_dir_ + "/inode_test_file.txt";
  int fd = open(file_path.c_str(), flags_, 0666);
  ASSERT_NE(fd, -1) << "Failed to create test file.";
  close(fd);

  UniqueDir dir(opendir(test_dir_.c_str()));
  ASSERT_NE(dir, nullptr) << "Failed to open directory.";

  struct dirent* entry;
  bool found_file = false;
  while ((entry = readdir(dir.get())) != nullptr) {
    std::string_view name(entry->d_name);
    if (name == "inode_test_file.txt") {
      EXPECT_NE(entry->d_ino, 0u) << "d_ino for file is zero.";
      found_file = true;
    } else if (name == ".") {
      EXPECT_NE(entry->d_ino, 0u) << "d_ino for '.' is zero.";
    } else if (name == "..") {
      EXPECT_NE(entry->d_ino, 0u) << "d_ino for '..' is zero.";
    }
  }
  EXPECT_TRUE(found_file) << "Did not find the created file in readdir.";
}

// Test that dType is populated correctly, checking for types DT_REG (file) and
// DT_DIR (directory) which are universally supported.
TEST_F(PosixReaddirTests, DTypePopulatedRegularFileAndDirectory) {
  // Create a regular file.
  std::string regular_file_path = test_dir_ + "/regular_file.txt";
  int fd = open(regular_file_path.c_str(), flags_, 0666);
  ASSERT_NE(fd, -1) << "Failed to create regular file.";
  close(fd);

  // Create a subdirectory.
  std::string sub_directory_path = test_dir_ + "/sub_directory";
  ASSERT_EQ(mkdir(sub_directory_path.c_str(), 0777), 0)
      << "Failed to create subdirectory.";

  UniqueDir dir(opendir(test_dir_.c_str()));
  ASSERT_NE(dir, nullptr) << "Failed to open directory.";

  bool found_regular_file = false;
  bool found_sub_directory = false;
  bool found_dot = false;
  bool found_dot_dot = false;

  struct dirent* entry;
  while ((entry = readdir(dir.get())) != nullptr) {
    std::string_view name(entry->d_name);
    if (name == "regular_file.txt") {
      // d_type for regular files should be DT_REG or DT_UNKNOWN.
      EXPECT_TRUE(entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN)
          << "Incorrect d_type for regular file '" << name
          << "': " << static_cast<int>(entry->d_type);
      found_regular_file = true;
    } else if (name == "sub_directory") {
      // d_type for directories should be DT_DIR or DT_UNKNOWN.
      EXPECT_TRUE(entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN)
          << "Incorrect d_type for subdirectory '" << name
          << "': " << static_cast<int>(entry->d_type);
      found_sub_directory = true;
    } else if (name == ".") {
      EXPECT_TRUE(entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN)
          << "Incorrect d_type for '.': " << static_cast<int>(entry->d_type);
      found_dot = true;
    } else if (name == "..") {
      EXPECT_TRUE(entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN)
          << "Incorrect d_type for '..': " << static_cast<int>(entry->d_type);
      found_dot_dot = true;
    }
  }

  EXPECT_TRUE(found_regular_file) << "Did not find the expected regular file.";
  EXPECT_TRUE(found_sub_directory) << "Did not find the expected subdirectory.";
  EXPECT_TRUE(found_dot) << "Did not find '.' entry.";
  EXPECT_TRUE(found_dot_dot) << "Did not find '..' entry.";
}

// Test readdir on a directory opened with opendir.
TEST_F(PosixReaddirTests, ReaddirOnValidDIRPointer) {
  UniqueDir dir(opendir(test_dir_.c_str()));
  ASSERT_NE(dir, nullptr) << "Failed to open directory.";

  struct dirent* entry = readdir(dir.get());
  ASSERT_NE(entry, nullptr)
      << "readdir returned nullptr for a non-empty directory.";
  // We expect to get at least "." or "..".
  std::string_view name(entry->d_name);
  EXPECT_TRUE(name == "." || name == "..") << "First entry was not '.' or '..'";
}

// Test readdir returning nullptr at end of directory stream.
TEST_F(PosixReaddirTests, ReaddirReturnsNullAtEndOfStream) {
  UniqueDir dir(opendir(test_dir_.c_str()));
  ASSERT_NE(dir, nullptr) << "Failed to open directory.";

  // Read all entries until readdir returns nullptr.
  int count = 0;
  while (readdir(dir.get()) != nullptr) {
    count++;
  }
  // An empty directory should have at least "." and "..".
  EXPECT_GE(count, 2) << "Did not read at least '.' and '..'";

  // The next call should return nullptr.
  EXPECT_EQ(readdir(dir.get()), nullptr)
      << "readdir did not return nullptr at the end of the stream.";
}

// Test readdir with a large number of entries to check for buffer handling.
TEST_F(PosixReaddirTests, LargeNumberOfEntries) {
  constexpr int kNumFiles = 1000;  // A reasonably large number of files.
  for (int i = 0; i < kNumFiles; ++i) {
    std::string file_path = test_dir_ + "/file_" + std::to_string(i);
    int fd = open(file_path.c_str(), flags_, 0666);
    ASSERT_NE(fd, -1) << "Failed to create file " << i;
    close(fd);
  }

  UniqueDir dir(opendir(test_dir_.c_str()));
  ASSERT_NE(dir, nullptr) << "Failed to open directory.";

  int count = 0;
  struct dirent* entry;
  while ((entry = readdir(dir.get())) != nullptr) {
    std::string_view name(entry->d_name);
    if (name != "." && name != "..") {
      count++;
    }
  }

  EXPECT_EQ(count, kNumFiles) << "Did not read the expected number of files.";
}

// Test readdir on two separate directories and verify distinct entries.
TEST_F(PosixReaddirTests, SeparateDirectoriesSeparateReaddir) {
  // Create first directory INSIDE test_dir_ to guarantee cleanup.
  std::string dir1_path = test_dir_ + "/dir1";
  ASSERT_EQ(mkdir(dir1_path.c_str(), 0777), 0) << "Failed to create dir1.";
  std::string file1_path = dir1_path + "/file_in_dir1.txt";
  int fd1 = open(file1_path.c_str(), flags_, 0666);
  ASSERT_NE(fd1, -1) << "Failed to create file in dir1.";
  close(fd1);

  // Create second directory INSIDE test_dir_ to guarantee cleanup.
  std::string dir2_path = test_dir_ + "/dir2";
  ASSERT_EQ(mkdir(dir2_path.c_str(), 0777), 0) << "Failed to create dir2.";
  std::string file2_path = dir2_path + "/file_in_dir2.txt";
  int fd2 = open(file2_path.c_str(), flags_, 0666);
  ASSERT_NE(fd2, -1) << "Failed to create file in dir2.";
  close(fd2);

  UniqueDir dir1(opendir(dir1_path.c_str()));
  ASSERT_NE(dir1, nullptr) << "Failed to open dir1.";

  UniqueDir dir2(opendir(dir2_path.c_str()));
  ASSERT_NE(dir2, nullptr) << "Failed to open dir2.";

  struct dirent* entry1;
  struct dirent* entry2;

  std::vector<std::string> entries_dir1;
  while ((entry1 = readdir(dir1.get())) != nullptr) {
    entries_dir1.push_back(entry1->d_name);
  }

  std::vector<std::string> entries_dir2;
  while ((entry2 = readdir(dir2.get())) != nullptr) {
    entries_dir2.push_back(entry2->d_name);
  }

  // Sort and compare contents. We expect the common "." and ".." and their
  // distinct files.
  std::sort(entries_dir1.begin(), entries_dir1.end());
  std::sort(entries_dir2.begin(), entries_dir2.end());

  std::vector<std::string> expected_dir1_entries = {".", "..",
                                                    "file_in_dir1.txt"};
  std::vector<std::string> expected_dir2_entries = {".", "..",
                                                    "file_in_dir2.txt"};

  std::sort(expected_dir1_entries.begin(), expected_dir1_entries.end());
  std::sort(expected_dir2_entries.begin(), expected_dir2_entries.end());

  EXPECT_EQ(entries_dir1, expected_dir1_entries)
      << "Contents of dir1 do not match expected.";
  EXPECT_EQ(entries_dir2, expected_dir2_entries)
      << "Contents of dir2 do not match expected.";
}

// Test deterministically that readdir does not use a global static buffer
// shared across different directory streams.
TEST_F(PosixReaddirTests, DeterministicBufferIsolation) {
  // Create two subdirectories.
  std::string dir1_path = test_dir_ + "/det_dir1";
  std::string dir2_path = test_dir_ + "/det_dir2";
  ASSERT_EQ(mkdir(dir1_path.c_str(), 0777), 0);
  ASSERT_EQ(mkdir(dir2_path.c_str(), 0777), 0);

  // Create a unique file in each.
  std::string file1_path = dir1_path + "/file_in_dir1.txt";
  int fd1 = open(file1_path.c_str(), flags_, 0666);
  ASSERT_NE(fd1, -1);
  close(fd1);

  std::string file2_path = dir2_path + "/file_in_dir2.txt";
  int fd2 = open(file2_path.c_str(), flags_, 0666);
  ASSERT_NE(fd2, -1);
  close(fd2);

  UniqueDir dir1(opendir(dir1_path.c_str()));
  ASSERT_NE(dir1, nullptr);
  UniqueDir dir2(opendir(dir2_path.c_str()));
  ASSERT_NE(dir2, nullptr);

  // Read first entry from dir1.
  struct dirent* entry1 = readdir(dir1.get());
  ASSERT_NE(entry1, nullptr);

  // Save the pointer and copy the content.
  const struct dirent* entry1_ptr = entry1;
  std::string entry1_name_before = entry1->d_name;
  unsigned char entry1_type_before = entry1->d_type;
  ino_t entry1_ino_before = entry1->d_ino;

  // Interleave: Read from dir2.
  // If the implementation uses a global static buffer, this call will
  // overwrite the memory pointed to by entry1.
  struct dirent* entry2 = readdir(dir2.get());
  ASSERT_NE(entry2, nullptr);

  // Verify that entry1's memory was NOT modified.
  EXPECT_NE(entry1_ptr, entry2) << "readdir returned the same pointer for "
                                   "different streams (global static buffer!)";
  EXPECT_EQ(entry1->d_name, entry1_name_before)
      << "readdir on dir2 overwrote d_name of dir1";
  EXPECT_EQ(entry1->d_type, entry1_type_before)
      << "readdir on dir2 overwrote d_type of dir1";
  EXPECT_EQ(entry1->d_ino, entry1_ino_before)
      << "readdir on dir2 overwrote d_ino of dir1";
}

// Test readdir's thread safety when called concurrently on different directory
// streams.
TEST_F(PosixReaddirTests, ThreadSafetyDifferentStreams) {
  constexpr int kNumThreads = 10;
  constexpr int kFilesPerDir = 5;
  std::vector<std::string> subdirs;

  // Create subdirectories and files with unique names per directory.
  for (int i = 0; i < kNumThreads; ++i) {
    std::string subdir_path = test_dir_ + "/thread_dir_" + std::to_string(i);
    ASSERT_EQ(mkdir(subdir_path.c_str(), 0777), 0)
        << "Failed to create subdir " << i;
    subdirs.push_back(subdir_path);

    for (int j = 0; j < kFilesPerDir; ++j) {
      std::string file_path = subdir_path + "/file_" + std::to_string(i) + "_" +
                              std::to_string(j) + ".txt";
      int fd = open(file_path.c_str(), flags_, 0666);
      ASSERT_NE(fd, -1) << "Failed to create file " << j << " in subdir " << i;
      close(fd);
    }
  }

  // Synchronization primitives for the barrier.
  std::mutex ready_mutex;
  std::condition_variable ready_cv;
  int ready_count = 0;
  std::mutex start_mutex;
  std::condition_variable start_cv;
  bool start_signal = false;

  std::vector<std::string> thread_errors(kNumThreads);
  std::vector<std::thread> threads;

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([i, &subdirs, &ready_mutex, &ready_cv, &ready_count,
                          &start_mutex, &start_cv, &start_signal,
                          &thread_errors]() {
      std::string subdir_path = subdirs[i];

      // Open the directory stream before synchronizing to maximize concurrent
      // readdir start.
      UniqueDir dir(opendir(subdir_path.c_str()));

      // Construct expected entries BEFORE waiting to minimize post-barrier
      // latency.
      std::vector<std::string> expected = {".", ".."};
      for (int j = 0; j < kFilesPerDir; ++j) {
        expected.push_back("file_" + std::to_string(i) + "_" +
                           std::to_string(j) + ".txt");
      }
      std::sort(expected.begin(), expected.end());

      // Phase 1: Signal ready. We must do this even if opendir failed to avoid
      // deadlocking the barrier.
      {
        std::unique_lock<std::mutex> lock(ready_mutex);
        ready_count++;
        if (ready_count == kNumThreads) {
          ready_cv.notify_all();
        }
      }  // Lock is released immediately.

      if (dir == nullptr) {
        thread_errors[i] = "opendir failed with errno " + std::to_string(errno);
        return;
      }

      // Phase 2: Wait for start signal (CPU-friendly).
      {
        std::unique_lock<std::mutex> lock(start_mutex);
        start_cv.wait(lock, [&] { return start_signal; });
      }

      std::vector<std::string> entries;
      struct dirent* entry;

      while (true) {
        errno = 0;
        entry = readdir(dir.get());
        if (entry == nullptr) {
          if (errno != 0) {
            thread_errors[i] =
                "readdir failed with errno " + std::to_string(errno);
            return;
          }
          break;
        }
        entries.push_back(entry->d_name);
      }

      // Verify entries.
      std::sort(entries.begin(), entries.end());

      if (entries != expected) {
        std::string error = "Unexpected entries. Expected: {";
        for (const auto& e : expected) {
          error += e + ", ";
        }
        error += "}, Got: {";
        for (const auto& e : entries) {
          error += e + ", ";
        }
        error += "}";
        thread_errors[i] = error;
      }
    });
  }

  // Wait for all threads to be ready (CPU-friendly).
  {
    std::unique_lock<std::mutex> lock(ready_mutex);
    ready_cv.wait(lock, [&] { return ready_count == kNumThreads; });
  }

  // Trigger parallel release of all waiting threads.
  {
    std::unique_lock<std::mutex> lock(start_mutex);
    start_signal = true;
  }
  start_cv.notify_all();

  // Join all threads.
  for (auto& thread : threads) {
    thread.join();
  }

  // Verify all threads succeeded.
  for (int i = 0; i < kNumThreads; ++i) {
    EXPECT_TRUE(thread_errors[i].empty())
        << "Thread " << i << " failed: " << thread_errors[i];
  }
}

// Test readdir's thread safety when called concurrently on the same directory
// (different DIR* handles).
TEST_F(PosixReaddirTests, ThreadSafetySameDirectoryDifferentStreams) {
  constexpr int kNumThreads = 10;
  constexpr int kNumFiles = 10;

  // Create one directory with files.
  std::string shared_dir = test_dir_ + "/shared_thread_dir";
  ASSERT_EQ(mkdir(shared_dir.c_str(), 0777), 0)
      << "Failed to create shared dir";

  for (int j = 0; j < kNumFiles; ++j) {
    std::string file_path = shared_dir + "/file_" + std::to_string(j) + ".txt";
    int fd = open(file_path.c_str(), flags_, 0666);
    ASSERT_NE(fd, -1) << "Failed to create file " << j << " in shared dir";
    close(fd);
  }

  // Construct expected entries once.
  std::vector<std::string> expected = {".", ".."};
  for (int j = 0; j < kNumFiles; ++j) {
    expected.push_back("file_" + std::to_string(j) + ".txt");
  }
  std::sort(expected.begin(), expected.end());

  // Synchronization primitives for the barrier.
  std::mutex ready_mutex;
  std::condition_variable ready_cv;
  int ready_count = 0;
  std::mutex start_mutex;
  std::condition_variable start_cv;
  bool start_signal = false;

  std::vector<std::string> thread_errors(kNumThreads);
  std::vector<std::thread> threads;

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([i, &shared_dir, &ready_mutex, &ready_cv, &ready_count,
                          &start_mutex, &start_cv, &start_signal,
                          &thread_errors, &expected]() {
      // Open the directory stream before synchronizing to maximize concurrent
      // readdir start.
      UniqueDir dir(opendir(shared_dir.c_str()));

      // Phase 1: Signal ready. We must do this even if opendir failed to avoid
      // deadlocking the barrier.
      {
        std::unique_lock<std::mutex> lock(ready_mutex);
        ready_count++;
        if (ready_count == kNumThreads) {
          ready_cv.notify_all();
        }
      }  // Lock is released immediately.

      if (dir == nullptr) {
        thread_errors[i] = "opendir failed with errno " + std::to_string(errno);
        return;
      }

      // Phase 2: Wait for start signal (CPU-friendly).
      {
        std::unique_lock<std::mutex> lock(start_mutex);
        start_cv.wait(lock, [&] { return start_signal; });
      }

      std::vector<std::string> entries;
      struct dirent* entry;

      while (true) {
        errno = 0;
        entry = readdir(dir.get());
        if (entry == nullptr) {
          if (errno != 0) {
            thread_errors[i] =
                "readdir failed with errno " + std::to_string(errno);
            return;
          }
          break;
        }
        entries.push_back(entry->d_name);
      }

      // Verify entries.
      std::sort(entries.begin(), entries.end());

      if (entries != expected) {
        std::string error = "Unexpected entries. Expected: {";
        for (const auto& e : expected) {
          error += e + ", ";
        }
        error += "}, Got: {";
        for (const auto& e : entries) {
          error += e + ", ";
        }
        error += "}";
        thread_errors[i] = error;
      }
    });
  }

  // Wait for all threads to be ready (CPU-friendly).
  {
    std::unique_lock<std::mutex> lock(ready_mutex);
    ready_cv.wait(lock, [&] { return ready_count == kNumThreads; });
  }

  // Trigger parallel release of all waiting threads.
  {
    std::unique_lock<std::mutex> lock(start_mutex);
    start_signal = true;
  }
  start_cv.notify_all();

  // Join all threads.
  for (auto& thread : threads) {
    thread.join();
  }

  // Verify all threads succeeded.
  for (int i = 0; i < kNumThreads; ++i) {
    EXPECT_TRUE(thread_errors[i].empty())
        << "Thread " << i << " failed: " << thread_errors[i];
  }
}

}  // namespace
}  // namespace nplb
