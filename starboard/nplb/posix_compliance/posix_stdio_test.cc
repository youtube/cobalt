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

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
extern "C" void __stdio_exit(void);
#endif  // BUILDFLAG(IS_COBALT_HERMETIC_BUILD)

namespace nplb {
namespace {

#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
std::string ReadFileDescriptorToString(const std::string& path) {
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    return "";
  }
  std::string content;
  char buffer[256];
  ssize_t bytes_read = 0;
  while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
    content.append(buffer, static_cast<size_t>(bytes_read));
  }
  close(fd);
  return content;
}
#endif  // BUILDFLAG(IS_COBALT_HERMETIC_BUILD)

struct ConcurrentWorkerContext {
  std::atomic<bool>* stop;
};

void* ConcurrentFcloseWorker(void* context) {
  auto* ctx = static_cast<ConcurrentWorkerContext*>(context);
  while (!ctx->stop->load(std::memory_order_relaxed)) {
    FILE* file = fopen("/dev/null", "w");
    if (file) {
      fputs("testing concurrent fclose and fflush\n", file);
      EXPECT_EQ(fclose(file), 0);
    }
  }
  return nullptr;
}

}  // namespace

TEST(PosixStdioTest, FopenFclose) {
  ScopedRandomFile random_file;
  std::string filename = random_file.filename();

  FILE* file = fopen(filename.c_str(), "w");
  ASSERT_NE(file, nullptr) << "Failed to open file for writing: " << filename;

  int result = fclose(file);
  ASSERT_EQ(result, 0) << "Failed to close file: " << filename;

  // Try opening for reading
  file = fopen(filename.c_str(), "r");
  ASSERT_NE(file, nullptr) << "Failed to open file for reading: " << filename;

  result = fclose(file);
  ASSERT_EQ(result, 0) << "Failed to close file: " << filename;
}

TEST(PosixStdioTest, FopenInvalidPath) {
  std::string invalid_path = "invalid/path/to/file.txt";
  FILE* file = fopen(invalid_path.c_str(), "r");
  ASSERT_EQ(file, nullptr);
  // Expect errno to be set to ENOENT (No such file or directory)
  ASSERT_EQ(errno, ENOENT);
}

TEST(PosixStdioTest, ConcurrentFcloseAndFflushNull) {
  constexpr int kNumThreads = 4;
  constexpr int kIterations = 200;
  std::atomic<bool> stop{false};
  ConcurrentWorkerContext ctx{&stop};

  std::vector<pthread_t> threads;
  threads.reserve(kNumThreads);
  bool creation_success = true;
  for (int i = 0; i < kNumThreads; ++i) {
    pthread_t thread;
    if (pthread_create(&thread, nullptr, ConcurrentFcloseWorker, &ctx) == 0) {
      threads.push_back(thread);
    } else {
      creation_success = false;
      break;
    }
  }

  if (creation_success) {
    for (int i = 0; i < kIterations; ++i) {
      EXPECT_EQ(fflush(nullptr), 0);
    }
  }

  stop.store(true, std::memory_order_relaxed);
  for (pthread_t thread : threads) {
    EXPECT_EQ(pthread_join(thread, nullptr), 0);
  }
  ASSERT_TRUE(creation_success);
}

#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
// Verifies the Starboard / Evergreen shutdown journey where an application or
// test suite writes output to block-buffered musl FILE* streams and terminates
// via __cxa_finalize() (DT_FINI) or exit() without an explicit fflush() or
// fclose() call. Invoking __stdio_exit() must flush all buffered streams to
// their underlying file descriptors and leave stdio locks unlocked so
// subsequent stdio operations (such as fclose) succeed without deadlocking.
TEST(PosixStdioTest, StdioExitFlushesBufferedStreams) {
  ScopedRandomFile random_file_1(0, ScopedRandomFile::kDontCreate);
  ScopedRandomFile random_file_2(0, ScopedRandomFile::kDontCreate);
  ScopedRandomFile random_read_file(0, ScopedRandomFile::kDontCreate);
  const std::string& filename_1 = random_file_1.filename();
  const std::string& filename_2 = random_file_2.filename();
  const std::string& read_filename = random_read_file.filename();

  {
    FILE* seed_file = fopen(read_filename.c_str(), "w");
    ASSERT_NE(seed_file, nullptr);
    ASSERT_GE(fputs("0123456789", seed_file), 0);
    ASSERT_EQ(fclose(seed_file), 0);
  }

  FILE* read_stream = fopen(read_filename.c_str(), "r");
  ASSERT_NE(read_stream, nullptr);
  ASSERT_EQ(setvbuf(read_stream, nullptr, _IOFBF, 64), 0);
  char first_char = '\0';
  ASSERT_EQ(fread(&first_char, 1, 1, read_stream), 1u);
  ASSERT_EQ(first_char, '0');

  FILE* file_1 = fopen(filename_1.c_str(), "w");
  ASSERT_NE(file_1, nullptr)
      << "Failed to open file for writing: " << filename_1;
  ASSERT_EQ(setvbuf(file_1, nullptr, _IOFBF, BUFSIZ), 0);

  FILE* file_2 = fopen(filename_2.c_str(), "w");
  ASSERT_NE(file_2, nullptr)
      << "Failed to open file for writing: " << filename_2;
  ASSERT_EQ(setvbuf(file_2, nullptr, _IOFBF, BUFSIZ), 0);

  ASSERT_GE(fputs("first buffered stream\n", file_1), 0);
  ASSERT_GE(fputs("second buffered stream\n", file_2), 0);

  EXPECT_EQ(ReadFileDescriptorToString(filename_1), "");
  EXPECT_EQ(ReadFileDescriptorToString(filename_2), "");

  __stdio_exit();

  EXPECT_EQ(ReadFileDescriptorToString(filename_1), "first buffered stream\n");
  EXPECT_EQ(ReadFileDescriptorToString(filename_2), "second buffered stream\n");

  char remaining[32] = {};
  size_t bytes_read = fread(remaining, 1, sizeof(remaining) - 1, read_stream);
  EXPECT_EQ(std::string(remaining, bytes_read), "123456789");
  EXPECT_EQ(fclose(read_stream), 0);

  ASSERT_GE(fputs("appended after stdio_exit\n", file_1), 0);
  EXPECT_EQ(fclose(file_1), 0);
  EXPECT_EQ(fclose(file_2), 0);
  EXPECT_EQ(ReadFileDescriptorToString(filename_1),
            "first buffered stream\nappended after stdio_exit\n");
}
#endif  // BUILDFLAG(IS_COBALT_HERMETIC_BUILD)

}  // namespace nplb
