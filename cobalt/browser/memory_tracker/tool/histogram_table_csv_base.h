// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_TOOL_HISTOGRAM_TABLE_CSV_BASE_H_
#define COBALT_BROWSER_MEMORY_TRACKER_TOOL_HISTOGRAM_TABLE_CSV_BASE_H_

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "cobalt/browser/memory_tracker/tool/util.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

// This HistogramTableCSV provides most of the functionality for generating a
// csv table from a histogram of data.
//
// Subclasses need to override ValueToString(...) so that the
// data-type can be stringified during the call to ToString().
//
// Example:
//  class MyHistogram : public HistogramTableCSV<int64_t>() {...}
//  MyHistogram my_histogram;
//  my_histogram.set_title("Memory Values");
//
//  // Add first row.
//  my_histogram.BeginRow(time_delta);
//  my_histogram.AddRowValue("ColumnA", 0);
//  my_histogram.AddRowValue("ColumnB", 0);
//  my_histogram.FinilizeRow();
//
//  // Add second row.
//  my_histogram.BeginRow(time_delta);
//  my_histogram.AddRowValue("ColumnA", 1);
//  my_histogram.AddRowValue("ColumnB", 2);
//  my_histogram.AddRowValue("ColumnC", 1);  // Ok, ColumnC will be autofilled.
//  my_histogram.FinilizeRow();
//
//  Print(my_histogram.ToString());
template <typename ValueType>
class HistogramTableCSVBase {
 public:
  typedef std::map<std::string, std::vector<ValueType> > TableData;

  // default_value is used to auto-fill values, such as when a new column
  // is introduced.
  explicit HistogramTableCSVBase(const ValueType& default_value)
      : default_value_(default_value) {}

  virtual std::string ValueToString(const ValueType& value) const = 0;

  void set_title(const std::string& title) {
    title_ = title;
  }

  void BeginRow(const base::TimeDelta time_value) {
    time_values_.push_back(time_value);
  }

  void AddRowValue(const std::string& column_key, const ValueType& value) {
    if (time_values_.empty()) {
      NOTREACHED() << "table_data_ was empty.";
      time_values_.push_back(base::TimeDelta());
    }
    const size_t n = time_values_.size();
    std::vector<ValueType>& column = table_data_[column_key];
    while (column.size() < n-1) {
      column.push_back(default_value_);
    }
    column.push_back(value);
  }

  void FinalizeRow() {
    const size_t n = time_values_.size();
    for (typename TableData::iterator it = table_data_.begin();
         it != table_data_.end(); ++it) {
      std::vector<ValueType>& column = it->second;
      while (column.size() < n) {
        column.push_back(default_value_);
      }
    }
  }

  std::string ToString() const {
    const char kSeparator[] = "//////////////////////////////////////////////";
    std::stringstream ss;
    ss << kSeparator << kNewLine;
    if (title_.size()) {
      ss << "// CSV of " << title_ << kNewLine;
    }
    for (size_t i = 0; i < NumberOfRows(); ++i) {
      ss << StringifyRow(i);
    }
    ss << kSeparator;
    return ss.str();
  }

  // All odd elements are removed. Effectively compressing the table in half.
  void RemoveOddElements() {
    memory_tracker::RemoveOddElements(&time_values_);
    for (typename TableData::iterator it = table_data_.begin();
         it != table_data_.end(); ++it) {
      memory_tracker::RemoveOddElements(&it->second);
    }
  }

  size_t NumberOfRows() const {
    return time_values_.size();
  }

 protected:
  static std::string JoinValues(const std::vector<std::string>& row_values) {
    std::stringstream ss;
    for (size_t i = 0; i < row_values.size(); ++i) {
      ss << kQuote << row_values[i] << kQuote << kDelimiter;
    }
    ss << kNewLine;
    return ss.str();
  }

  static std::string TimeToMinutesString(base::TimeDelta dt) {
    double value_minutes = dt.InSecondsF() / 60.;
    char buff[128];
    SbStringFormatF(buff, sizeof(buff), "%.2f", value_minutes);
    return std::string(buff);
  }

  std::string StringifyRow(size_t index) const {
    if (index == 0) {
      // Create header row.
      std::vector<std::string> column_keys;
      column_keys.push_back("Time(min)");
      for (typename TableData::const_iterator it = table_data_.begin();
           it != table_data_.end(); ++it) {
        column_keys.push_back(SanitizeCSVKey(it->first));
      }
      return JoinValues(column_keys);
    } else {
      // Create data row.
      std::vector<std::string> row_values;
      row_values.push_back(TimeToMinutesString(time_values_[index]));
      for (typename TableData::const_iterator it = table_data_.begin();
           it != table_data_.end(); ++it) {
        const std::vector<ValueType>& column = it->second;
        const std::string value_str = ValueToString(column[index]);
        row_values.push_back(value_str);
      }
      return JoinValues(row_values);
    }
  }

  std::string title_;
  ValueType default_value_;
  std::vector<base::TimeDelta> time_values_;
  TableData table_data_;
};

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_HISTOGRAM_TABLE_CSV_BASE_H_
