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

#ifndef RENDER_TREE_RECT_NODE_H_
#define RENDER_TREE_RECT_NODE_H_

#include "base/compiler_specific.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace render_tree {

// TODO(***REMOVED***): Define the border, don't forget no border case.
class Border;

// TODO(***REMOVED***): Define border radiuses.
class BorderRadiuses;

// TODO(***REMOVED***): Define the brush, don't forget no brush case.
class Brush;

// TODO(***REMOVED***): Define the shadow.
class Shadow;

// A filled rectangle with a border and rounded corners.
class RectNode : public Node {
 public:
  // A type-safe branching.
  void Accept(NodeVisitor* visitor) OVERRIDE;

  // A border around rectangle. Note that this is an inner border which means
  // that width of a border is included in rectangle's own width.
  const Border& border() const;

  // A solid or gradient brush to fill the rectangle with.
  const Brush& background_brush() const;

  // A rectangle shadow.
  const Shadow& shadow() const;

  // Radiuses of corners. Most likely to be all zeros for a majority of cases.
  const BorderRadiuses& radiuses() const;

  // A position and size of a rectangle (size includes border).
  const math::RectF& rect() const;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_RECT_NODE_H_
