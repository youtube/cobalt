/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

#define RED_START "\x1b[31m"
#define GREEN_START "\x1b[32m"
#define COLOR_END "\x1b[0m"

std::vector<std::string> MakeRow2(const std::string& col1,
                                  const std::string& col2) {
  std::vector<std::string> row;
  row.push_back(col1);
  row.push_back(col2);
  return row;
}

TEST(TablePrinter, ToString) {
  TablePrinter table;

  // Add header.
  table.AddRow(MakeRow2("col1", "col2"));
  table.AddRow(MakeRow2("value1", "value2"));

  std::string table_str = table.ToString();
  std::string expected_table_str =
      " col1     col2     \n"
      " _________________ \n"
      "|        |        |\n"
      "| value1 | value2 |\n"
      "|________|________|\n";

  EXPECT_STREQ(expected_table_str.c_str(), table_str.c_str());
}

TEST(TablePrinter, ToStringWithColor) {
  TablePrinter table;

  // Adds color to table.
  table.set_text_color(TablePrinter::kRed);
  table.set_table_color(TablePrinter::kGreen);

  // Add header.
  table.AddRow(MakeRow2("col1", "col2"));
  table.AddRow(MakeRow2("value1", "value2"));

  // These defines will wrap color constants for string literals.
#define R(STR) RED_START STR COLOR_END
#define G(STR) GREEN_START STR COLOR_END

  std::string table_str = table.ToString();
  std::string expected_table_str =
      " col1     col2     \n"
      G(" _________________ ") "\n"
      G("|        |        |") "\n"
      G("|") " " R("value1") " " G("|") " " R("value2") " " G("|") "\n"
      G("|________|________|") "\n";

#undef R
#undef G

  EXPECT_STREQ(expected_table_str.c_str(), table_str.c_str());
}

#undef RED_START
#undef GREEN_START
#undef COLOR_END

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
