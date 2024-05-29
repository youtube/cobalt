// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/webdriver/dispatcher.h"

#include <utility>

#include "base/optional.h"
#include "base/values.h"
#include "cobalt/webdriver/protocol/log_type.h"
#include "cobalt/webdriver/protocol/window_id.h"
#include "cobalt/webdriver/session_driver.h"
#include "cobalt/webdriver/util/command_result.h"
#include "cobalt/webdriver/util/dispatch_command_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace webdriver {

TEST(FromValueTest, WindowID) {
  base::DictionaryValue dict;
  dict.SetString("name", "foo");
  base::Value value(std::move(dict));
  auto result = util::internal::FromValue<protocol::WindowId>(&value);
  EXPECT_EQ(result->id(), "foo");
}

TEST(FromValueTest, WindowID_Fail) {
  base::Value empty;
  auto result = util::internal::FromValue<protocol::WindowId>(&empty);
  EXPECT_EQ(result, base::nullopt);
}

TEST(FromValueTest, LogType) {
  base::DictionaryValue dict;
  dict.SetString("type", "foo");
  base::Value value(std::move(dict));
  auto result = util::internal::FromValue<protocol::LogType>(&value);
  EXPECT_EQ(result->type(), "foo");
}

TEST(FromValueTest, LogType_Fail) {
  base::Value empty;
  auto result = util::internal::FromValue<protocol::LogType>(&empty);
  EXPECT_EQ(result, base::nullopt);
}

}  // namespace webdriver
}  // namespace cobalt
