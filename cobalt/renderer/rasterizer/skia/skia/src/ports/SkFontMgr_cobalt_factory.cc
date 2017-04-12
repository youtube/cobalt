// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontMgr_cobalt.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"

SkFontMgr* SkFontMgr::Factory() {
  FilePath font_directory;
  CHECK(PathService::Get(base::DIR_EXE, &font_directory));
  font_directory = font_directory.Append(FILE_PATH_LITERAL("fonts"));

  SkTArray<SkString, true> default_families;
  default_families.push_back(SkString("sans-serif"));

  return new SkFontMgr_Cobalt(font_directory.value().c_str(), default_families);
}
