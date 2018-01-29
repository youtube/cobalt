// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/glyph_buffer.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

GlyphBuffer::GlyphBuffer(const math::RectF& bounds, SkTextBlobBuilder* builder)
    : render_tree::GlyphBuffer(bounds), text_blob_(builder->make()) {}

const SkTextBlob* GlyphBuffer::GetTextBlob() const {
  return SkSafeRef(text_blob_.get());
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
