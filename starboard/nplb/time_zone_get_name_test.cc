// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <string.h>

#include "starboard/common/log.h"
#include "starboard/extension/time_zone.h"
#include "starboard/system.h"
#include "starboard/time_zone.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyOf;
using testing::MatchesRegex;

namespace starboard {
namespace nplb {
namespace {

TEST(SbTimeZoneGetNameTest, IsKindOfSane) {
  const char* name = SbTimeZoneGetName();

  ASSERT_NE(name, static_cast<const char*>(NULL));

  // "UTC" is not a valid local time zone name.
  EXPECT_NE(name, "UTC");

  int i = 0;
  while (name[i] != '\0') {
    ++i;
  }

  // All time zones are 3 letters or more. This can include names like "PST"
  // or "Pacific Standard Time" or like "America/Los_Angeles"
  EXPECT_GE(i, 3);

  // On Linux, TZ=":Pacific/Chatham" is a good test of boundary conditions.
  // ":Pacific/Kiritimati" is the western-most timezone at UTC+14.
}

TEST(SbTimeZoneGetNameTest, IsIANAFormat) {
  const char* name = SbTimeZoneGetName();
  SB_LOG(INFO) << "time zone name: " << name;
  char cpy[100];
  snprintf(cpy, sizeof(cpy), "%s", name);
  char* continent = strtok(cpy, "/");
  // The time zone ID starts with a Continent or Ocean name.
  EXPECT_THAT(
      continent,
      testing::AnyOf(std::string("Asia"), std::string("America"),
                     std::string("Africa"), std::string("Europe"),
                     std::string("Australia"), std::string("Pacific"),
                     std::string("Atlantic"), std::string("Antarctica"),
                     // time zone can be "Etc/UTC" if unset(such as on
                     // CI builders), shouldn't happen in production.
                     // TODO(b/304351956): Remove Etc after fixing builders.
                     std::string("Indian"), std::string("Etc")));
  char* city = strtok(NULL, "/");
  ASSERT_NE(city, (char*)NULL);
  EXPECT_TRUE(strlen(city) != 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
