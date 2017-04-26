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
#include <string>
#include <vector>

#include "cobalt/browser/memory_settings/memory_settings.h"
#include "cobalt/browser/memory_settings/table_printer.h"
#include "starboard/log.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {

std::string StringifySourceType(MemorySetting::SourceType source_type) {
  switch (source_type) {
    case MemorySetting::kUnset: {
      return "Unset";
    }
    case MemorySetting::kCmdLine: {
      return "CmdLine";
    }
    case MemorySetting::kBuildSetting: {
      return "Build";
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
  TablePrinter printer;

  std::vector<std::string> header;
  header.push_back("NAME");
  header.push_back("VALUE");
  header.push_back("SOURCE");
  printer.AddRow(header);

  for (size_t i = 0; i < settings.size(); ++i) {
    const MemorySetting* setting = settings[i];

    std::vector<std::string> row;
    row.push_back(setting->name());
    row.push_back(setting->ValueToString());
    row.push_back(StringifySourceType(setting->source_type()));
    printer.AddRow(row);
  }

  std::string table_string = printer.ToString();
  return table_string;
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
