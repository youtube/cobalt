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

#include "cobalt/browser/memory_settings/pretty_print.h"

#include <algorithm>
#include <vector>

#include "cobalt/browser/memory_settings/memory_settings.h"
#include "starboard/log.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {

class TablePrinter {
 public:
  struct Row {
    std::string name;
    std::string value;
    std::string source;
  };

  static std::string ToString(const std::vector<Row>& rows) {
    // First column is the name, the second is the value, then source.
    size_t name_col_width = 0;
    size_t value_col_width = 0;
    size_t source_col_width = 0;

    for (size_t i = 0; i < rows.size(); ++i) {
      const Row& row = rows[i];
      name_col_width = std::max<size_t>(name_col_width, row.name.size());
      value_col_width = std::max<size_t>(value_col_width, row.value.size());
      source_col_width = std::max<size_t>(source_col_width, row.source.size());
    }

    TablePrinter printer(name_col_width, value_col_width, source_col_width);

    std::stringstream output_ss;
    std::string row_delimiter = printer.MakeDelimiterRow();

    output_ss << printer.MakeHeaderRow() << "\n";
    output_ss << row_delimiter << "\n";

    for (size_t i = 0; i < rows.size(); ++i) {
      const Row& row = rows[i];
      std::string row_str =
          printer.MakeDataRow(row.name, row.value, row.source);
      output_ss << row_str << "\n";
      output_ss << row_delimiter << "\n";
    }

    return output_ss.str();
  }

 private:
  TablePrinter(size_t name_col_width, size_t value_col_width,
               size_t source_col_width)
      : name_col_width_(name_col_width),
        value_col_width_(value_col_width),
        source_col_width_(source_col_width) {
    Init();
  }

  void Init() {
    std::stringstream fmt_row_ss;
    // Example: "| %-30s | %9s | %9s |\n"
    fmt_row_ss << "| %-" << name_col_width_ << "s | %" << value_col_width_
               << "s | %" << source_col_width_ << "s |";
    fmt_data_row_ = fmt_row_ss.str();
  }

  std::string MakeHeaderRow() const {
    std::string name_str = "NAME";
    std::string value_str = "VALUE";
    std::string source_str = "SOURCE";

    std::stringstream ss;
    ss << name_str;
    for (size_t i = 0; i < name_col_width_ + 3 - name_str.size(); ++i) {
      ss << " ";
    }

    ss << value_str;
    for (size_t i = 0; i < value_col_width_ + 3 - value_str.size(); ++i) {
      ss << " ";
    }

    ss << source_str;
    for (size_t i = 0; i < source_col_width_ + 4 - source_str.size(); ++i) {
      ss << " ";
    }
    return ss.str();
  }

  std::string MakeDataRow(const std::string& name, const std::string& value,
                          const std::string& source) const {
    std::string row_str = base::StringPrintf(
        fmt_data_row_.c_str(), name.c_str(), value.c_str(), source.c_str());
    return row_str;
  }

  std::string MakeDelimiterRow() const {
    std::stringstream ss;
    ss << "+";
    for (size_t i = 0; i < name_col_width_ + 2; ++i) {
      ss << "-";
    }
    ss << "+";
    for (size_t i = 0; i < value_col_width_ + 2; ++i) {
      ss << "-";
    }
    ss << "+";
    for (size_t i = 0; i < source_col_width_ + 2; ++i) {
      ss << "-";
    }
    ss << "+";
    return ss.str();
  }

  const size_t name_col_width_;
  const size_t value_col_width_;
  const size_t source_col_width_;
  std::string fmt_data_row_;
};

std::string StringifySourceType(MemorySetting::SourceType source_type) {
  switch (source_type) {
    case MemorySetting::kUnset: {
      return "Unset";
    }
    case MemorySetting::kCmdLine: {
      return "CmdLine";
    }
    case MemorySetting::kBuildSetting: {
      return "BuildSetting";
    }
    case MemorySetting::kAutoSet: {
      return "AutoSet";
    }
    case MemorySetting::kNotApplicable: {
      return "N/A";
    }
  }

  SB_NOTIMPLEMENTED() << "Unimplemented string for type " << source_type;
  return "UNKNOWN";
}

}  // namespace

std::string GeneratePrettyPrintTable(
    const std::vector<const MemorySetting*>& settings) {
  std::vector<TablePrinter::Row> rows;

  for (size_t i = 0; i < settings.size(); ++i) {
    const MemorySetting* setting = settings[i];

    TablePrinter::Row row;
    row.name = setting->name();
    row.value = setting->ValueToString();
    row.source = StringifySourceType(setting->source_type());

    rows.push_back(row);
  }

  std::string table_string = TablePrinter::ToString(rows);
  return table_string;
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
