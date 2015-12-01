/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RENDER_TREE_BRUSH_H_
#define RENDER_TREE_BRUSH_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/render_tree/brush_visitor.h"
#include "cobalt/render_tree/color_rgba.h"

namespace cobalt {
namespace render_tree {

// A base class of all render tree brushes.
class Brush {
 public:
  virtual ~Brush() {}

  // A type-safe branching.
  virtual void Accept(BrushVisitor* visitor) const = 0;
};

class SolidColorBrush : public Brush {
 public:
  explicit SolidColorBrush(const ColorRGBA& color) : color_(color) {}

  // A type-safe branching.
  void Accept(BrushVisitor* visitor) const OVERRIDE;

  const ColorRGBA& color() const { return color_; }

 private:
  ColorRGBA color_;
};

class LinearGradientBrush : public Brush {
 public:
  // A type-safe branching.
  void Accept(BrushVisitor* visitor) const OVERRIDE;

  // TODO(***REMOVED***): Define linear gradient.
};

scoped_ptr<Brush> CloneBrush(const Brush* brush);

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_BRUSH_H_
