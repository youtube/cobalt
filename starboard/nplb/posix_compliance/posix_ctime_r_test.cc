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

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

const int kBufferSize = 26;  // ctime_r requires at least 26 bytes.

// Test fixture for ctime_r tests.
class PosixCtimeRTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // A known time: 2023-10-26 10:30:00 UTC
    time_ = 1698316200;
  }

  time_t time_;
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
  // Note: The output of ctime_r depends on the system's timezone.
  // We will set the TZ to UTC for consistent testing.
  const char* old_tz_cstr = getenv("TZ");
  std::string old_tz_str;
  if (old_tz_cstr) {
    old_tz_str = old_tz_cstr;
  }
  setenv("TZ", "UTC", 1);
  tzset();

  char* result = ctime_r(&time_, buffer_);
  ASSERT_NE(result, nullptr);
  EXPECT_STREQ("Thu Oct 26 10:30:00 2023\n", buffer_);
  EXPECT_EQ(result, buffer_);

  // Restore original timezone
  if (old_tz_cstr) {
    setenv("TZ", old_tz_str.c_str(), 1);
  } else {
    unsetenv("TZ");
  }
  tzset();
}

TEST_F(PosixCtimeRTest, AnotherTime) {
  // A different time: 1999-01-01 23:59:59 UTC
  // Timestamp: 915235199
  time_ = 915235199;

  const char* old_tz_cstr = getenv("TZ");
  std::string old_tz_str;
  if (old_tz_cstr) {
    old_tz_str = old_tz_cstr;
  }
  setenv("TZ", "UTC", 1);
  tzset();

  char* result = ctime_r(&time_, buffer_);
  ASSERT_NE(result, nullptr);
  EXPECT_STREQ("Fri Jan  1 23:59:59 1999\n", buffer_);

  if (old_tz_cstr) {
    setenv("TZ", old_tz_str.c_str(), 1);
  } else {
    unsetenv("TZ");
  }
  tzset();
}

TEST_F(PosixCtimeRTest, ThreadSafety) {
  pthread_t thread1, thread2;

  const char* old_tz_cstr = getenv("TZ");
  std::string old_tz_str;
  if (old_tz_cstr) {
    old_tz_str = old_tz_cstr;
  }
  setenv("TZ", "UTC", 1);
  tzset();

  ThreadData data1;
  data1.timestamp = 1749986400;  // 2025-06-15 11:20:00 UTC
  data1.expected = "Sun Jun 15 11:20:00 2025\n";

  ThreadData data2;
  data2.timestamp = 1735115400;  // 2024-12-25 08:30:00 UTC
  data2.expected = "Wed Dec 25 08:30:00 2024\n";

  pthread_create(&thread1, nullptr, CtimeRThread, &data1);
  pthread_create(&thread2, nullptr, CtimeRThread, &data2);

  pthread_join(thread1, nullptr);
  pthread_join(thread2, nullptr);

  EXPECT_STREQ(data1.expected, data1.buffer);
  EXPECT_STREQ(data2.expected, data2.buffer);

  if (old_tz_cstr) {
    setenv("TZ", old_tz_str.c_str(), 1);
  } else {
    unsetenv("TZ");
  }
  tzset();
}

TEST_F(PosixCtimeRTest, NullTimeInput) {
  char* result = ctime_r(nullptr, buffer_);
  ASSERT_EQ(result, nullptr);
}

TEST_F(PosixCtimeRTest, NullBufferInput) {
  char* result = ctime_r(&time_, nullptr);
  ASSERT_EQ(result, nullptr);
}

}  // namespace
}  // namespace nplb
