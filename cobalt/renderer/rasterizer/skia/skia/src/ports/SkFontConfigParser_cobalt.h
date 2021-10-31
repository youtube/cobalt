// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTCONFIGPARSER_COBALT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTCONFIGPARSER_COBALT_H_

#include "SkFontUtil_cobalt.h"
#include "SkTDArray.h"

namespace SkFontConfigParser {

// Parses all system font configuration files and returns the results in an
// array of FontFamily structures.
void GetFontFamilies(const char* directory,
                     SkTDArray<FontFamilyInfo*>* font_families);

}  // namespace SkFontConfigParser

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTCONFIGPARSER_COBALT_H_
