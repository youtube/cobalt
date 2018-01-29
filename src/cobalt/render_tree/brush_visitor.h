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

#ifndef COBALT_RENDER_TREE_BRUSH_VISITOR_H_
#define COBALT_RENDER_TREE_BRUSH_VISITOR_H_

namespace cobalt {
namespace render_tree {

class LinearGradientBrush;
class RadialGradientBrush;
class SolidColorBrush;

// Type-safe branching on a class hierarchy of render tree brushes,
// implemented after a classical GoF pattern (see
// http://en.wikipedia.org/wiki/Visitor_pattern#Java_example).
//
// Usage example:
//
//     class ShaderFactory : public BrushVisitor {
//       SkShader* shader() {
//         return shader_;
//       }
//
//       void Visit(SolidColorBrush* solid_color_brush) override {
//         shader_ = new SkColorShader(...);
//       }
//
//       void Visit(LinearGradientBrush* linear_gradient_brush) override {
//         shader_ = new SkGradientShader(...);
//       }
//
//       void Visit(RadialGradientBrush* radial_gradient_brush) override {
//         shader_ = new SkGradientShader(...);
//       }
//     };
//
//     ShaderFactory shader_factory;
//     render_tree_rect->background_brush().Accept(&shader_factory);
//     paint.setShader(shader_factory.shader());
//
class BrushVisitor {
 public:
  virtual void Visit(const SolidColorBrush* solid_color_brush) = 0;
  virtual void Visit(const LinearGradientBrush* linear_gradient_brush) = 0;
  virtual void Visit(const RadialGradientBrush* radial_gradient_brush) = 0;

 protected:
  ~BrushVisitor() {}
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_BRUSH_VISITOR_H_
