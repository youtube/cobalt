// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFREETYPE_COBALT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFREETYPE_COBALT_H_

#include "SkStream.h"
#include "SkTypeface.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontUtil_cobalt.h"

namespace sk_freetype_cobalt {

// Scans the font stream using FreeType, setting its name, style and whether or
// not it has a fixed pitch. It also generates the font's character map if that
// optional parameter is provided.
bool ScanFont(SkStreamAsset* stream, int face_index, SkString* name,
              SkFontStyle* style, bool* is_fixed_pitch,
              font_character_map::CharacterMap* maybe_character_map = NULL);

}  // namespace sk_freetype_cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFREETYPE_COBALT_H_
