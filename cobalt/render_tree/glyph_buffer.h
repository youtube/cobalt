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

#ifndef COBALT_RENDER_TREE_GLYPH_BUFFER_H_
#define COBALT_RENDER_TREE_GLYPH_BUFFER_H_

#include "base/memory/ref_counted.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace render_tree {

// The GlyphBuffer class is a base class, which contains the data needed to
// render shaped text. It is intended to be immutable and thread-safe. Since
// GlyphBuffer objects may be created in the front-end, but must be accessed
// by the rasterizer, it is expected that they will be downcast again to a
// rasterizer-specific type through base::polymorphic_downcast().
class GlyphBuffer : public base::RefCountedThreadSafe<GlyphBuffer> {
 public:
  explicit GlyphBuffer(const math::RectF& bounds) : bounds_(bounds) {}
  virtual ~GlyphBuffer() {}

  const math::RectF& GetBounds() const { return bounds_; }

 private:
  const math::RectF bounds_;

  // Allow the reference counting system access to our destructor.
  friend class base::RefCountedThreadSafe<GlyphBuffer>;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_GLYPH_BUFFER_H_
