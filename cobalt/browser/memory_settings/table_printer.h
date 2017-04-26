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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_TABLE_PRINTER_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_TABLE_PRINTER_H_

#include <string>
#include <vector>

namespace cobalt {
namespace browser {
namespace memory_settings {

// TablePrinter takes in rows of strings and then converts them into
// an ascii table that can be used to visualize information on stdout.
// Example:
//    table.AddRow({"col1", "col2"});
//    table.AddRow({"value1", "value2"});
//    std::cout << table.ToString();
//
//  --> outputs -->
//
//      "col1     col2      \n"
//      "+--------+--------+\n"
//      "| value1 | value2 |\n"
//      "+--------+--------+\n";
class TablePrinter {
 public:
  typedef std::vector<std::string> Row;

  void AddRow(const Row& row);
  std::string ToString() const;

 private:
  std::vector<Row> table_;
};

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_TABLE_PRINTER_H_
