// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_NODE_VISITOR_H_
#define COBALT_RENDER_TREE_NODE_VISITOR_H_

namespace cobalt {
namespace render_tree {

class ClearRectNode;
class CompositionNode;
class FilterNode;
class ImageNode;
class LottieNode;
class MatrixTransform3DNode;
class MatrixTransformNode;
class PunchThroughVideoNode;
class RectNode;
class RectShadowNode;
class TextNode;

namespace animations {
class AnimateNode;
}  // namespace animations

// Type-safe branching on a class hierarchy of render tree nodes,
// implemented after a classical GoF pattern (see
// http://en.wikipedia.org/wiki/Visitor_pattern#Java_example).
class NodeVisitor {
 public:
  virtual void Visit(animations::AnimateNode* animate) = 0;
  virtual void Visit(ClearRectNode* clear_rect) = 0;
  virtual void Visit(CompositionNode* composition) = 0;
  virtual void Visit(FilterNode* text) = 0;
  virtual void Visit(ImageNode* image) = 0;
  virtual void Visit(LottieNode* lottie) = 0;
  virtual void Visit(MatrixTransform3DNode* transform) = 0;
  virtual void Visit(MatrixTransformNode* transform) = 0;
  virtual void Visit(PunchThroughVideoNode* punch_through) = 0;
  virtual void Visit(RectNode* rect) = 0;
  virtual void Visit(RectShadowNode* rect) = 0;
  virtual void Visit(TextNode* text) = 0;

 protected:
  ~NodeVisitor() {}
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_NODE_VISITOR_H_
