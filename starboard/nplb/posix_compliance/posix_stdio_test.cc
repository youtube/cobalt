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
#include <pthread.h>
#include <stdio.h>
#include <sys/stat.h>

#include <atomic>
#include <string>
#include <vector>

#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

struct ConcurrentWorkerContext {
  std::atomic<bool>* stop;
};

void* ConcurrentFcloseWorker(void* context) {
  auto* ctx = static_cast<ConcurrentWorkerContext*>(context);
  while (!ctx->stop->load(std::memory_order_relaxed)) {
    FILE* file = fopen("/dev/null", "w");
    EXPECT_NE(file, nullptr);
    if (file) {
      fputs("testing concurrent fclose and fflush\n", file);
      EXPECT_EQ(fclose(file), 0);
    } else {
      break;
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

}  // namespace nplb
