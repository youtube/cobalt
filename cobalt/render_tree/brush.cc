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

#include "cobalt/render_tree/brush.h"

#include "cobalt/render_tree/brush_visitor.h"

namespace cobalt {
namespace render_tree {

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

void SolidColorBrush::Accept(BrushVisitor* visitor) const {
  visitor->Visit(this);
}

void LinearGradientBrush::Accept(BrushVisitor* visitor) const {
  visitor->Visit(this);
}

scoped_ptr<Brush> CloneBrush(const Brush* brush) {
  DCHECK(brush);

  BrushCloner cloner;
  brush->Accept(&cloner);
  return cloner.PassClone();
}

}  // namespace render_tree
}  // namespace cobalt
