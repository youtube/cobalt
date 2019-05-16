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
  static bool SystemSupportsColor();

  enum Color { kDefault, kRed, kGreen, kYellow, kBlue, kMagenta, kCyan };

  typedef std::vector<std::string> Row;
  TablePrinter();

  void AddRow(const Row& row);
  std::string ToString() const;

  // Adds color to the string output. kDefault will omit any color value and
  // just do straight text.
  // Note: If SbLogIsTty() returns false then color output is disabled and its
  //       recommended that these functions are not called.
  void set_text_color(Color text_color) { text_color_ = text_color; }
  void set_table_color(Color table_color) { table_color_ = table_color; }

 private:
  std::vector<Row> table_;
  Color text_color_;
  Color table_color_;
};

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_TABLE_PRINTER_H_
