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

#ifndef COBALT_RENDER_TREE_NODE_H_
#define COBALT_RENDER_TREE_NODE_H_

#include "base/memory/ref_counted.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace render_tree {

class NodeVisitor;

// A base class of all objects that form a render tree.
class Node : public base::RefCountedThreadSafe<Node> {
 public:
  // A type-safe branching.
  virtual void Accept(NodeVisitor* visitor) = 0;

  // Returns an axis-aligned bounding rectangle for this render tree node in
  // units of pixels.
  virtual math::RectF GetBounds() const = 0;

  // Returns an ID that is unique to the node type.  This can be used to
  // polymorphically identify what type a node is.
  virtual base::TypeId GetTypeId() const = 0;

  // Number to help differentiate nodes. This is specific to the local process
  // and is not deterministic. Node identifiers from different processes may
  // overlap. This is intended to be used as a key when, for example, caching
  // render results of nodes.
  int64_t GetId() const { return node_id_; }

 protected:
  Node();
  virtual ~Node() {}
  friend class base::RefCountedThreadSafe<Node>;

 private:
  int64_t node_id_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_NODE_H_
