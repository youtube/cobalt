// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/resolve_rasterizer_settings.h"

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "cobalt/browser/switches.h"
#include "cobalt/configuration/configuration.h"
#include "cobalt/persistent_storage/persistent_settings.h"

namespace cobalt {
namespace browser {

namespace {
std::string GetRasterizerTypeFromPersistentSettings(
    persistent_storage::PersistentSettings* persistent_settings) {
  std::string rasterizer_type = "";
  if (persistent_settings != nullptr) {
    base::Value value;
    persistent_settings->Get(
        configuration::Configuration::kEnableSkiaRasterizerPersistentSettingKey,
        &value);
    if (!value.is_none() && value.is_bool()) {
      bool enable_skia = 0;
      enable_skia = value.GetBool();
      rasterizer_type = enable_skia
                            ? configuration::Configuration::kSkiaRasterizer
                            : configuration::Configuration::kGlesRasterizer;
    }
  }
  return rasterizer_type;
}

std::string GetRasterizerTypeFromCommandline() {
  std::string rasterizer_type = "";
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(browser::switches::kEnableSkiaRasterizer)) {
    int enable_skia = 0;
    base::StringToInt(command_line->GetSwitchValueASCII(
                          browser::switches::kEnableSkiaRasterizer),
                      &enable_skia);
    rasterizer_type = enable_skia
                          ? configuration::Configuration::kSkiaRasterizer
                          : configuration::Configuration::kGlesRasterizer;
  }
  return rasterizer_type;
}

}  // namespace

std::string GetRasterizerType(
    persistent_storage::PersistentSettings* persistent_settings) {
  std::string rasterizer_type = GetRasterizerTypeFromCommandline();
  if (rasterizer_type != "") {
    return rasterizer_type;
  }

  rasterizer_type =
      GetRasterizerTypeFromPersistentSettings(persistent_settings);
  if (rasterizer_type != "") {
    return rasterizer_type;
  }

  return configuration::Configuration::GetInstance()->CobaltRasterizerType();
}

}  // namespace browser
}  // namespace cobalt
