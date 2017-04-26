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

std::vector<std::string> MakeRow2(const std::string& col1,
                                  const std::string& col2) {
  std::vector<std::string> row;
  row.push_back(col1);
  row.push_back(col2);
  return row;
}

TEST(TablePrinterImpl, ToString) {
  TablePrinter table;

  // Add header.
  table.AddRow(MakeRow2("col1", "col2"));
  table.AddRow(MakeRow2("value1", "value2"));

  std::string table_str = table.ToString();
  std::string expected_table_str =
      "col1     col2      \n"
      "+--------+--------+\n"
      "| value1 | value2 |\n"
      "+--------+--------+\n";

  EXPECT_STREQ(expected_table_str.c_str(), table_str.c_str());
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
