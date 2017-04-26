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

#include "base/logging.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {

class TablePrinterImpl {
 public:
  // First row is the header.
  typedef std::vector<std::string> Row;
  // table[i] gets row, table[i][j] gets row/col
  typedef std::vector<Row> Table;
  static std::string ToString(const Table& table);

 private:
  static bool CheckNotEmptyAndAllColumnsTheSame(const Table& rows);
  static std::vector<size_t> MakeColumnSizeVector(const Table& table);
  static size_t ColumnPrintSize(const Table& table, size_t which_col);
  explicit TablePrinterImpl(const std::vector<size_t>& column_sizes);
  std::string MakeHeaderRow(const Row& header_row) const;
  std::string MakeDataRow(const Row& row) const;
  std::string MakeDelimiterRow(const std::vector<size_t>& column_sizes) const;

  const std::vector<size_t>& column_sizes_;
};

std::string TablePrinterImpl::ToString(const Table& rows) {
  if (!CheckNotEmptyAndAllColumnsTheSame(rows)) {
    std::string error_msg = "ERROR - NOT ALL COLUMNS ARE THE SAME";
    DCHECK(false) << error_msg;
    return error_msg;
  }

  std::vector<size_t> column_sizes = MakeColumnSizeVector(rows);
  TablePrinterImpl printer(column_sizes);

  std::stringstream output_ss;
  std::string row_delimiter = printer.MakeDelimiterRow(column_sizes);

  for (size_t i = 0; i < rows.size(); ++i) {
    const Row& row = rows[i];

    const std::string row_string =
        (i == 0) ? printer.MakeHeaderRow(row) : printer.MakeDataRow(row);

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

TablePrinterImpl::TablePrinterImpl(const std::vector<size_t>& column_sizes)
    : column_sizes_(column_sizes) {}

std::string TablePrinterImpl::MakeHeaderRow(const Row& header_row) const {
  std::stringstream ss;

  for (size_t i = 0; i < header_row.size(); ++i) {
    ss << header_row[i];
    const size_t name_size = header_row[i].size();
    const size_t col_width = column_sizes_[i];

    DCHECK_LT(name_size, col_width);

    for (size_t j = 0; j < col_width + 1 - name_size; ++j) {
      ss << " ";
    }
  }
  ss << " ";
  return ss.str();
}

std::string TablePrinterImpl::MakeDataRow(const Row& row) const {
  std::stringstream output_ss;

  for (size_t i = 0; i < row.size(); ++i) {
    std::stringstream token_ss;
    token_ss << " ";  // Padding
    token_ss << row[i];
    token_ss << " ";

    std::string token = token_ss.str();

    std::stringstream row_ss;
    const std::streamsize col_size =
        static_cast<std::streamsize>(column_sizes_[i]);
    row_ss << "|";
    row_ss << std::setfill(' ');
    row_ss << std::setw(col_size);
    if (i == 0) {
      row_ss << std::left;  // Left justification for first column.
    } else {
      row_ss << std::right;  // Right justification for all other columns.
    }

    row_ss << token;
    output_ss << row_ss.str();
  }
  output_ss << "|";
  return output_ss.str();
}

std::string TablePrinterImpl::MakeDelimiterRow(
    const std::vector<size_t>& column_sizes) const {
  std::stringstream ss;
  for (size_t i = 0; i < column_sizes.size(); ++i) {
    ss << "+";
    const size_t col_width = column_sizes[i];
    for (size_t j = 0; j < col_width; ++j) {
      ss << "-";
    }
  }
  ss << "+";
  return ss.str();
}
}  // namespace.

void TablePrinter::AddRow(const TablePrinter::Row& row) {
  table_.push_back(row);
}

std::string TablePrinter::ToString() const {
  return TablePrinterImpl::ToString(table_);
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
