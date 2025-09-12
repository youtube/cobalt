// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "base/base_paths_starboard.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontMgr_cobalt.h"

sk_sp<SkFontMgr> SkFontMgr::Factory() {
  base::FilePath cobalt_font_directory;
  CHECK(base::PathService::Get(base::DIR_EXE, &cobalt_font_directory));
  cobalt_font_directory =
      cobalt_font_directory.Append(FILE_PATH_LITERAL("fonts"));

  base::FilePath system_font_config_directory;
  base::PathService::Get(base::DIR_SYSTEM_FONTS_CONFIGURATION,
                         &system_font_config_directory);

  base::FilePath system_font_files_directory;
  base::PathService::Get(base::DIR_SYSTEM_FONTS, &system_font_files_directory);

  SkTArray<SkString, true> default_families;
  default_families.push_back(SkString("sans-serif"));

  sk_sp<SkFontMgr> font_manager(new SkFontMgr_Cobalt(
      cobalt_font_directory.value().c_str(),
      cobalt_font_directory.value().c_str(),
      system_font_config_directory.value().c_str(),
      system_font_files_directory.value().c_str(), default_families));

  return font_manager;
}
