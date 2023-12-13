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

#include "cobalt/browser/memory_settings/pretty_print.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/strings/string_split.h"
#include "cobalt/browser/memory_settings/memory_settings.h"
#include "cobalt/browser/memory_settings/table_printer.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {

std::string StringifyMemoryType(const MemorySetting& setting) {
  switch (setting.memory_type()) {
    case MemorySetting::kCPU: {
      return "CPU";
    }
    case MemorySetting::kGPU: {
      return "GPU";
    }
    case MemorySetting::kNotApplicable: {
      return "N/A";
    }
  }

  SB_NOTIMPLEMENTED() << "Unimplemented string for memory type: "
                      << setting.memory_type();
  return "UNKNOWN";
}

std::string StringifyValue(const MemorySetting* setting) {
  if (!setting->valid()) {
    return "N/A";
  }
  return setting->ValueToString();
}

void FillStream(size_t count, const char fill_ch, std::stringstream* ss) {
  for (size_t i = 0; i < count; ++i) {
    (*ss) << fill_ch;
  }
}

}  // namespace

std::string GeneratePrettyPrintTable(
    bool use_color_ascii, const std::vector<const MemorySetting*>& settings) {
  TablePrinter printer;
  if (use_color_ascii) {
    printer.set_text_color(TablePrinter::kGreen);
  }
  std::vector<std::string> header;
  header.push_back("SETTING NAME");
  header.push_back("VALUE");
  header.push_back("");
  header.push_back("TYPE");
  header.push_back("SOURCE");
  printer.AddRow(header);

  for (size_t i = 0; i < settings.size(); ++i) {
    const MemorySetting* setting = settings[i];

    std::vector<std::string> row;
    row.push_back(setting->name());
    row.push_back(StringifyValue(setting));
    if (setting->valid()) {
      row.push_back(ToMegabyteString(setting->MemoryConsumption(), 1));
    } else {
      row.push_back("N/A");
    }
    row.push_back(StringifyMemoryType(*setting));
    row.push_back(StringifySourceType(*setting));
    printer.AddRow(row);
  }

  std::string table_string = printer.ToString();
  return table_string;
}

std::string GenerateMemoryTable(bool use_color_ascii,
                                const IntSetting& total_cpu_memory,
                                const IntSetting& total_gpu_memory,
                                int64_t settings_cpu_consumption,
                                int64_t settings_gpu_consumption) {
  TablePrinter printer;
  if (use_color_ascii) {
    printer.set_text_color(TablePrinter::kGreen);
  }
  std::vector<std::string> header;
  header.push_back("MEMORY");
  header.push_back("SOURCE");
  header.push_back("TOTAL");
  header.push_back("SETTINGS CONSUME");
  printer.AddRow(header);

  std::vector<std::string> data_row;
  data_row.push_back(total_cpu_memory.name());
  data_row.push_back(StringifySourceType(total_cpu_memory));
  data_row.push_back(ToMegabyteString(total_cpu_memory.value(), 1));
  data_row.push_back(ToMegabyteString(settings_cpu_consumption, 1));
  printer.AddRow(data_row);
  data_row.clear();

  data_row.push_back(total_gpu_memory.name());
  data_row.push_back(StringifySourceType(total_gpu_memory));
  std::string total_gpu_consumption_str;

  const bool available_gpu_settings_invalid = (total_gpu_memory.value() <= 0);

  if (available_gpu_settings_invalid) {
    total_gpu_consumption_str = "<UNKNOWN>";
  } else {
    total_gpu_consumption_str = ToMegabyteString(total_gpu_memory.value(), 1);
  }

  data_row.push_back(total_gpu_consumption_str);
  data_row.push_back(ToMegabyteString(settings_gpu_consumption, 1));
  printer.AddRow(data_row);
  data_row.clear();
  return printer.ToString();
}

std::string ToMegabyteString(int64_t bytes, int decimal_places) {
  float megabytes = bytes / (1024.0f * 1024.0f);

  std::stringstream ss_fmt;
  // e.g. "%.1f MB"
  ss_fmt << "%." << decimal_places << "f MB";

  char buff[128];
  SbStringFormatF(buff, sizeof(buff), ss_fmt.str().c_str(), megabytes);
  // Use 16
  return std::string(buff);
}

std::string MakeBorder(const std::string& body, const char border_ch) {
  std::vector<std::string> lines = base::SplitString(
      body, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  size_t max_span = 0;
  for (size_t i = 0; i < lines.size(); ++i) {
    max_span = std::max(max_span, lines[i].size());
  }

  std::stringstream ss;
  ss << "\n";

  FillStream(max_span + 4, border_ch, &ss);
  ss << "\n";
  for (size_t i = 0; i < lines.size(); ++i) {
    const std::string& line = lines[i];

    ss << border_ch << ' ' << line;
    FillStream(max_span - line.size() + 1, ' ', &ss);
    ss << border_ch;
    ss << "\n";
  }
  FillStream(max_span + 4, border_ch, &ss);
  ss << "\n";
  return ss.str();
}

std::string StringifySourceType(const MemorySetting& setting) {
  if (setting.memory_type() == MemorySetting::kNotApplicable) {
    return "N/A";
  }

  switch (setting.source_type()) {
    case MemorySetting::kUnset: {
      return "Unset";
    }
    case MemorySetting::kStarboardAPI: {
      return "Starboard API";
    }
    case MemorySetting::kBuildSetting: {
      return "Build";
    }
    case MemorySetting::kCmdLine: {
      return "CmdLine";
    }
    case MemorySetting::kAutoSet: {
      return "AutoSet";
    }
    case MemorySetting::kAutosetConstrained: {
      return "AutoSet (Constrained)";
    }
  }

  SB_NOTIMPLEMENTED() << "Unimplemented string for type: "
                      << setting.source_type();
  return "UNKNOWN";
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
