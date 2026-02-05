// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may- obtain a copy of the License at
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

const int kBufferSize = 26;  // asctime_r requires at least 26 bytes.

class PosixAsctimeRTest : public ::testing::Test {
 protected:
  char buffer_[kBufferSize];
};

struct ThreadData {
  struct tm time_data;
  char buffer[kBufferSize];
  const char* expected;
};

// Thread function to call asctime_r
void* AsctimeRThread(void* arg) {
  ThreadData* data = static_cast<ThreadData*>(arg);
  asctime_r(&data->time_data, data->buffer);
  return nullptr;
}

TEST_F(PosixAsctimeRTest, BasicConversion) {
  struct tm tm = {0};
  tm.tm_year = 2023 - 1900;
  tm.tm_mon = 9;  // October
  tm.tm_mday = 26;
  tm.tm_hour = 10;
  tm.tm_min = 30;
  tm.tm_sec = 0;
  tm.tm_wday = 4;  // Thursday

  char* result = asctime_r(&tm, buffer_);
  ASSERT_NE(result, nullptr);
  EXPECT_STREQ("Thu Oct 26 10:30:00 2023\n", buffer_);
  EXPECT_EQ(result, buffer_);
}

TEST_F(PosixAsctimeRTest, AnotherTime) {
  struct tm tm = {0};
  tm.tm_year = 1999 - 1900;
  tm.tm_mon = 0;  // January
  tm.tm_mday = 1;
  tm.tm_hour = 23;
  tm.tm_min = 59;
  tm.tm_sec = 59;
  tm.tm_wday = 5;  // Friday

  char* result = asctime_r(&tm, buffer_);
  ASSERT_NE(result, nullptr);
  EXPECT_STREQ("Fri Jan  1 23:59:59 1999\n", buffer_);
}

TEST_F(PosixAsctimeRTest, ThreadSafety) {
  pthread_t thread1, thread2;

  ThreadData data1;
  data1.time_data = {0};
  data1.time_data.tm_year = 2025 - 1900;
  data1.time_data.tm_mon = 5;  // June
  data1.time_data.tm_mday = 15;
  data1.time_data.tm_hour = 14;
  data1.time_data.tm_min = 0;
  data1.time_data.tm_sec = 0;
  data1.time_data.tm_wday = 0;  // Sunday
  data1.expected = "Sun Jun 15 14:00:00 2025\n";

  ThreadData data2;
  data2.time_data = {0};
  data2.time_data.tm_year = 2024 - 1900;
  data2.time_data.tm_mon = 11;  // December
  data2.time_data.tm_mday = 25;
  data2.time_data.tm_hour = 8;
  data2.time_data.tm_min = 30;
  data2.time_data.tm_sec = 0;
  data2.time_data.tm_wday = 3;  // Wednesday
  data2.expected = "Wed Dec 25 08:30:00 2024\n";

  pthread_create(&thread1, nullptr, AsctimeRThread, &data1);
  pthread_create(&thread2, nullptr, AsctimeRThread, &data2);

  pthread_join(thread1, nullptr);
  pthread_join(thread2, nullptr);

  EXPECT_STREQ(data1.expected, data1.buffer);
  EXPECT_STREQ(data2.expected, data2.buffer);
}

}  // namespace
}  // namespace nplb
