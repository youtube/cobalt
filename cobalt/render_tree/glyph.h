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

#ifndef COBALT_RENDER_TREE_GLYPH_H_
#define COBALT_RENDER_TREE_GLYPH_H_

#include "base/basictypes.h"

namespace cobalt {
namespace render_tree {

typedef uint16_t GlyphIndex;
static const GlyphIndex kInvalidGlyphIndex = 0;
static const GlyphIndex kUnknownGlyphIndex = 0xffff;

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_GLYPH_H_
