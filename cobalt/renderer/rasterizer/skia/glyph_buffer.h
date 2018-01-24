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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_GLYPH_BUFFER_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_GLYPH_BUFFER_H_

#include "cobalt/render_tree/glyph_buffer.h"

#include "third_party/skia/include/core/SkTextBlob.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// Describes a render_tree::GlyphBuffer using Skia. This object contain all of
// the information needed by Skia to render glyphs and is both immutable and
// thread-safe.
class GlyphBuffer : public render_tree::GlyphBuffer {
 public:
  GlyphBuffer(const math::RectF& bounds, SkTextBlobBuilder* builder);

  const SkTextBlob* GetTextBlob() const;

 private:
  sk_sp<const SkTextBlob> text_blob_;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_GLYPH_BUFFER_H_
