// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "skia/ext/fontmgr_default.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontMgr_cobalt.h"

namespace skia {

SK_API sk_sp<SkFontMgr> CreateDefaultSkFontMgr() {
  base::FilePath cobalt_font_directory;
  CHECK(base::PathService::Get(base::DIR_EXE, &cobalt_font_directory));
  cobalt_font_directory =
      cobalt_font_directory.Append(FILE_PATH_LITERAL("fonts"));

  base::FilePath system_font_files_directory;
  if (!base::PathService::Get(base::DIR_SYSTEM_FONTS, &system_font_files_directory) ||
      system_font_files_directory.empty() ||
      !base::DirectoryExists(system_font_files_directory)) {
    // Fallback: resolve relative to executable file!
    base::FilePath exe_path;
    if (base::PathService::Get(base::FILE_EXE, &exe_path)) {
      system_font_files_directory = exe_path.DirName().Append(FILE_PATH_LITERAL("fonts"));
      LOG(INFO) << "FontManager: Fallback system_font_files_directory to: " << system_font_files_directory.value();
    }
  }

  base::FilePath system_font_config_directory;
  if (!base::PathService::Get(base::DIR_SYSTEM_FONTS_CONFIGURATION, &system_font_config_directory) ||
      system_font_config_directory.empty() ||
      !base::PathExists(system_font_config_directory.Append(FILE_PATH_LITERAL("fonts.xml")))) {
    // Fallback to same as files directory!
    system_font_config_directory = system_font_files_directory;
    LOG(INFO) << "FontManager: Fallback system_font_config_directory to: " << system_font_config_directory.value();
  }

  skia_private::TArray<SkString, true> default_families;
  default_families.push_back(SkString("sans-serif"));

  sk_sp<SkFontMgr> font_manager(new SkFontMgr_Cobalt(
      cobalt_font_directory.value().c_str(),
      cobalt_font_directory.value().c_str(),
      system_font_config_directory.value().c_str(),
      system_font_files_directory.value().c_str(), default_families));

  return font_manager;
}

}  // namespace skia
