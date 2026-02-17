// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include <pthread.h>
#include <time.h>

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

const int kBufferSize = 26;  // ctime_r requires at least 26 bytes.

// Test fixture for ctime_r tests.
class PosixCtimeRTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* old_tz_cstr = getenv("TZ");
    if (old_tz_cstr) {
      old_tz_ = old_tz_cstr;
    }
    setenv("TZ", "UTC", 1);
    tzset();
  }

  void TearDown() override {
    if (!old_tz_.empty()) {
      setenv("TZ", old_tz_.c_str(), 1);
    } else {
      unsetenv("TZ");
    }
    tzset();
  }

  std::string old_tz_;
  char buffer_[kBufferSize];
};

// Struct to pass data to threads
struct ThreadData {
  time_t timestamp;
  char buffer[kBufferSize];
  const char* expected;
};

// Thread function to call ctime_r
void* CtimeRThread(void* arg) {
  ThreadData* data = static_cast<ThreadData*>(arg);
  ctime_r(&data->timestamp, data->buffer);
  return nullptr;
}

TEST_F(PosixCtimeRTest, BasicConversion) {
  time_t time = 1698316200;  // 2023-10-26 10:30:00 UTC
  char* result = ctime_r(&time, buffer_);
  ASSERT_NE(result, nullptr);
  EXPECT_STREQ("Thu Oct 26 10:30:00 2023\n", buffer_);
  EXPECT_EQ(result, buffer_);
}

TEST_F(PosixCtimeRTest, AnotherTime) {
  time_t time = 915235199;  // 1999-01-01 23:59:59 UTC
  char* result = ctime_r(&time, buffer_);
  ASSERT_NE(result, nullptr);
  EXPECT_STREQ("Fri Jan  1 23:59:59 1999\n", buffer_);
}

TEST_F(PosixCtimeRTest, ThreadSafety) {
  const int kNumThreads = 10;
  std::vector<pthread_t> threads(kNumThreads);
  std::vector<ThreadData> data(kNumThreads);

  const time_t time1 = 1749986400;  // 2025-06-15 11:20:00 UTC
  const char* expected1 = "Sun Jun 15 11:20:00 2025\n";
  const time_t time2 = 1735115400;  // 2024-12-25 08:30:00 UTC
  const char* expected2 = "Wed Dec 25 08:30:00 2024\n";

  for (int i = 0; i < kNumThreads; ++i) {
    if (i % 2 == 0) {
      data[i].timestamp = time1;
      data[i].expected = expected1;
    } else {
      data[i].timestamp = time2;
      data[i].expected = expected2;
    }
    pthread_create(&threads[i], nullptr, CtimeRThread, &data[i]);
  }

  for (int i = 0; i < kNumThreads; ++i) {
    pthread_join(threads[i], nullptr);
  }

  for (int i = 0; i < kNumThreads; ++i) {
    EXPECT_STREQ(data[i].expected, data[i].buffer)
        << "Thread " << i << " failed.";
  }
}

}  // namespace
}  // namespace nplb
