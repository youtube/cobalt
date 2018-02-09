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

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

#include "base/logging.h"
#include "starboard/log.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {

std::string AddColor(TablePrinter::Color color, const std::string& value) {
  const char* RED_START = "\x1b[31m";
  const char* GREEN_START = "\x1b[32m";
  const char* YELLOW_START = "\x1b[33m";
  const char* BLUE_START = "\x1b[34m";
  const char* MAGENTA_START = "\x1b[35m";
  const char* CYAN_START = "\x1b[36m";

  const char* COLOR_END = "\x1b[0m";

  if (color == TablePrinter::kDefault) {
    return value;
  }

  std::stringstream ss;
  switch (color) {
    case TablePrinter::kRed: { ss << RED_START; break; }
    case TablePrinter::kGreen: { ss << GREEN_START; break; }
    case TablePrinter::kYellow: { ss << YELLOW_START; break; }
    case TablePrinter::kBlue: { ss << BLUE_START; break; }
    case TablePrinter::kMagenta: { ss << MAGENTA_START; break; }
    case TablePrinter::kCyan: { ss << CYAN_START; break; }
    case TablePrinter::kDefault: { DCHECK(false) << "Unexpected"; break; }
  }

  ss << value;
  ss << COLOR_END;
  return ss.str();
}

std::string MakeFill(const char* str, size_t n) {
  std::stringstream ss;
  for (size_t i = 0; i < n; ++i) {
    ss << str;
  }
  return ss.str();
}

class TablePrinterImpl {
 public:
  // First row is the header.
  typedef std::vector<std::string> Row;
  // table[i] gets row, table[i][j] gets row/col
  typedef std::vector<Row> Table;
  static std::string ToString(const Table& table,
                              TablePrinter::Color text_color,
                              TablePrinter::Color table_color);

 private:
  static bool CheckNotEmptyAndAllColumnsTheSame(const Table& rows);
  static std::vector<size_t> MakeColumnSizeVector(const Table& table);
  static size_t ColumnPrintSize(const Table& table, size_t which_col);
  TablePrinterImpl(const std::vector<size_t>& column_sizes,
                   TablePrinter::Color text_color,
                   TablePrinter::Color table_printer);

  // " col1     col2 "
  std::string MakeHeaderRow(const Row& header_row) const;

  // ex: "| value1 | value2 |"
  std::string MakeDataRow(const Row& row) const;

  // ex: "|________|________|"
  std::string MakeRowDelimiter() const;

  // Follows a data row to provide verticle space before a TopSeparatorRow().
  // ex: "|        |        |"
  std::string MakeTopSeparatorRowAbove() const;

  // ex: " _________________ "
  std::string MakeTopSeparatorRow() const;

  const std::vector<size_t>& column_sizes_;
  const TablePrinter::Color text_color_;
  const TablePrinter::Color table_color_;
};

std::string TablePrinterImpl::ToString(const Table& rows,
                                       TablePrinter::Color text_color,
                                       TablePrinter::Color table_color) {
  if (!CheckNotEmptyAndAllColumnsTheSame(rows)) {
    std::string error_msg = "ERROR - NOT ALL COLUMNS ARE THE SAME";
    DCHECK(false) << error_msg;
    return error_msg;
  }

  std::vector<size_t> column_sizes = MakeColumnSizeVector(rows);
  TablePrinterImpl printer(column_sizes, text_color, table_color);

  std::stringstream output_ss;
  output_ss << printer.MakeHeaderRow(rows[0]) << "\n";
  output_ss << printer.MakeTopSeparatorRow() << "\n";

  std::string separator_row_above = printer.MakeTopSeparatorRowAbove();
  std::string row_delimiter = printer.MakeRowDelimiter();

  // Print body.
  for (size_t i = 1; i < rows.size(); ++i) {
    const Row& row = rows[i];

    const std::string row_string = printer.MakeDataRow(row);

    output_ss << separator_row_above << "\n";
    output_ss << row_string << "\n";
    output_ss << row_delimiter << "\n";
  }
  return output_ss.str();
}

bool TablePrinterImpl::CheckNotEmptyAndAllColumnsTheSame(const Table& rows) {
  if (rows.empty()) {
    return false;
  }
  size_t num_columns = rows[0].size();
  for (size_t i = 1; i < rows.size(); ++i) {
    if (num_columns != rows[i].size()) {
      return false;
    }
  }
  return true;
}

// Given an input of rows, compute a column size vector. Each column size
// is the MAX of all the same columns sizes, +2. The +2 will account for
// padding on either side of the token during stringification.
// Example:
//   std::vector<Row> rows;
//   Row row;
//   row.push_back("first_column");
//   row.push_back("second_column");
//   rows.push_back(row);
//   Row row2;
//   row2.push_back("first_column-2");
//   row2.push_back("second_column-2");
//   rows.push_back(row2);
//   std::vector<size_t> column_sizes = MakeColumnSizeVector(rows);
//   EXPECT_EQ(2, column_sizes.size());
//   EXPECT_EQ(16, column_sizes[0]);  // "first_column-2".size() + 2
//   EXPECT_EQ(17, column_sizes[1]);  // "second_column-2".size() + 2
std::vector<size_t> TablePrinterImpl::MakeColumnSizeVector(const Table& table) {
  DCHECK(!table.empty());
  const size_t num_cols = table[0].size();
  std::vector<size_t> column_sizes;

  for (size_t i = 0; i < num_cols; ++i) {
    column_sizes.push_back(ColumnPrintSize(table, i));
  }
  return column_sizes;
}

size_t TablePrinterImpl::ColumnPrintSize(const Table& table, size_t which_col) {
  size_t max_col_size = 0;
  for (size_t i = 0; i < table.size(); ++i) {
    size_t curr_col_size = table[i][which_col].size();
    max_col_size = std::max<size_t>(max_col_size, curr_col_size);
  }
  return max_col_size + 2;  // +2 for padding on either side.
}

TablePrinterImpl::TablePrinterImpl(const std::vector<size_t>& column_sizes,
                                   TablePrinter::Color text_color,
                                   TablePrinter::Color table_printer)
    : column_sizes_(column_sizes), text_color_(text_color),
      table_color_(table_printer) {}

std::string TablePrinterImpl::MakeHeaderRow(const Row& header_row) const {
  std::stringstream ss;

  for (size_t i = 0; i < header_row.size(); ++i) {
    ss << " " << header_row[i];
    const size_t name_size = header_row[i].size();
    const size_t col_width = column_sizes_[i];

    DCHECK_LT(name_size, col_width);

    for (size_t j = 0; j < col_width - name_size; ++j) {
      ss << " ";
    }
  }
  ss << " ";
  return ss.str();
}

std::string TablePrinterImpl::MakeDataRow(const Row& row) const {
  // Safe integer math for creating a fill size. Because this size is based on
  // subtraction of size_t values, it's possible that an empty string could
  // cause a rollover of size_t as an input to string creation, causing
  // a crash.
  // This math is made safe by doing the integer calculations in int64_t and
  // then clamping to positive values.
  struct F {
    static size_t CreateFillSize(size_t col_size, size_t element_size) {
      int64_t value = static_cast<int64_t>(col_size) -
                      static_cast<int64_t>(element_size) - 1;
      value = std::max<int64_t>(0, value);
      return static_cast<size_t>(value);
    }
  };

  std::stringstream output_ss;
  const std::string vertical_bar = AddColor(table_color_, "|");

  for (size_t i = 0; i < row.size(); ++i) {
    std::string token = AddColor(text_color_, row[i]);

    std::stringstream row_ss;
    const int col_size = static_cast<int>(column_sizes_[i]);
    row_ss << vertical_bar;
    const size_t fill_size = F::CreateFillSize(col_size, row[i].size());
    if (i == 0) {
      row_ss << " " << token << MakeFill(" ", fill_size);
    } else {
      row_ss << MakeFill(" ", fill_size) << token << " ";
    }

    output_ss << row_ss.str();
  }
  output_ss << vertical_bar;
  return output_ss.str();
}

std::string TablePrinterImpl::MakeRowDelimiter() const {
  std::stringstream ss;
  for (size_t i = 0; i < column_sizes_.size(); ++i) {
    ss << "|";
    const size_t col_width = column_sizes_[i];
    for (size_t j = 0; j < col_width; ++j) {
      ss << "_";
    }
  }
  ss << "|";

  std::string output = AddColor(table_color_, ss.str());
  return output;
}

std::string TablePrinterImpl::MakeTopSeparatorRow() const {
  std::stringstream ss;
  for (size_t i = 0; i < column_sizes_.size(); ++i) {
    if (i == 0) {
      ss << " ";
    } else {
      ss << "_";
    }
    const size_t col_width = column_sizes_[i];
    for (size_t j = 0; j < col_width; ++j) {
      ss << "_";
    }
  }
  ss << " ";
  std::string output = AddColor(table_color_, ss.str());
  return output;
}

std::string TablePrinterImpl::MakeTopSeparatorRowAbove() const {
  std::stringstream ss;
  for (size_t i = 0; i < column_sizes_.size(); ++i) {
    ss << "|";
    const size_t col_width = column_sizes_[i];
    for (size_t j = 0; j < col_width; ++j) {
      ss << " ";
    }
  }
  ss << "|";
  std::string output = AddColor(table_color_, ss.str());
  return output;
}

}  // namespace.

bool TablePrinter::SystemSupportsColor() {
  return SbLogIsTty();
}

TablePrinter::TablePrinter() : text_color_(kDefault), table_color_(kDefault) {
}

void TablePrinter::AddRow(const TablePrinter::Row& row) {
  table_.push_back(row);
}

std::string TablePrinter::ToString() const {
  return TablePrinterImpl::ToString(table_, text_color_, table_color_);
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
