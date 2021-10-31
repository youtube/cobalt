/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/memory_settings/table_printer.h"

#include "starboard/common/log.h"
#include "starboard/string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

#define RED_START "\x1b[31m"
#define GREEN_START "\x1b[32m"
#define COLOR_END "\x1b[0m"

namespace {
std::vector<std::string> MakeRow2(const std::string& col1,
                                  const std::string& col2) {
  std::vector<std::string> row;
  row.push_back(col1);
  row.push_back(col2);
  return row;
}

// Returns true if all tokens exist in the string in the given order.
bool HasTokensInOrder(const std::string& value,
                      std::initializer_list<const char*> tokens) {
  std::string::size_type current_position = 0;
  for (auto token : tokens) {
    std::string::size_type position = value.find(token, current_position);
    EXPECT_NE(position, std::string::npos);
    EXPECT_GE(position, current_position);
    if (position == std::string::npos) {
      SB_DLOG(INFO) << "Token \"" << token << "\" not found in order.";
      return false;
    }
    current_position = position + strlen(token);
  }
  return true;
}
}  // namespace

TEST(TablePrinter, ToString) {
  TablePrinter table;

  // Add header.
  table.AddRow(MakeRow2("col1", "col2"));
  table.AddRow(MakeRow2("value1", "value2"));

  EXPECT_TRUE(HasTokensInOrder(
      table.ToString(), {"col1", "col2", "\n", "value1", "value2", "\n"}));
}

TEST(TablePrinter, ToStringWithColor) {
  TablePrinter table;

  // Adds color to table.
  table.set_text_color(TablePrinter::kRed);
  table.set_table_color(TablePrinter::kGreen);

  // Add header.
  table.AddRow(MakeRow2("col1", "col2"));
  table.AddRow(MakeRow2("value1", "value2"));

  EXPECT_TRUE(HasTokensInOrder(
      table.ToString(),
      {"col1", "col2", "\n", RED_START, "value1", COLOR_END, RED_START,
       "value2", COLOR_END, "\n", GREEN_START, COLOR_END}));
}

#undef RED_START
#undef GREEN_START
#undef COLOR_END

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
