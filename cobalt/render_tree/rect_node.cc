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

#include "cobalt/render_tree/rect_node.h"

#include "cobalt/render_tree/brush_visitor.h"
#include "cobalt/render_tree/node_visitor.h"

namespace cobalt {
namespace render_tree {

RectNode::Builder::Builder(
    const math::SizeF& size, scoped_ptr<Brush> background_brush)
    : size(size), background_brush(background_brush.Pass()) {}

namespace {

class BrushCloner : public BrushVisitor {
 public:
  virtual ~BrushCloner() {}

  void Visit(const SolidColorBrush* solid_color_brush) OVERRIDE {
    cloned_.reset(new SolidColorBrush(*solid_color_brush));
  }

  void Visit(const LinearGradientBrush* linear_gradient_brush) OVERRIDE {
    cloned_.reset(new LinearGradientBrush(*linear_gradient_brush));
  }

  scoped_ptr<Brush> PassClone() { return cloned_.Pass(); }

 private:
  scoped_ptr<Brush> cloned_;
};

}  // namespace

RectNode::Builder::Builder(const Builder& other) {
  size = other.size;

  BrushCloner cloner;
  other.background_brush->Accept(&cloner);
  background_brush = cloner.PassClone();
}

RectNode::Builder::Builder(Moved moved)
    : size(moved->size),
      background_brush(moved->background_brush.Pass()) {}

void RectNode::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

}  // namespace render_tree
}  // namespace cobalt
