// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/common/find_node.h"

#include "base/optional.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/render_tree/animations/animate_node.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace common {

class FinderNodeVisitor : public render_tree::NodeVisitor {
 public:
  FinderNodeVisitor(NodeFilterFunction<render_tree::Node> filter_function,
                    base::optional<NodeReplaceFunction> replace_function)
      : filter_function_(filter_function),
        replace_function_(replace_function) {}

  void Visit(render_tree::animations::AnimateNode* animate) override {
    VisitNode(animate);
  }
  void Visit(render_tree::CompositionNode* composition_node) override {
    VisitNode(composition_node);
  }
  void Visit(render_tree::FilterNode* filter_node) override {
    VisitNode(filter_node);
  }
  void Visit(render_tree::ImageNode* image_node) override {
    VisitNode(image_node);
  }
  void Visit(render_tree::MatrixTransform3DNode* transform_3d_node) override {
    VisitNode(transform_3d_node);
  }
  void Visit(render_tree::MatrixTransformNode* transform_node) override {
    VisitNode(transform_node);
  }
  void Visit(render_tree::PunchThroughVideoNode* punch_through) override {
    VisitNode(punch_through);
  }
  void Visit(render_tree::RectNode* rect) override { VisitNode(rect); }
  void Visit(render_tree::RectShadowNode* rect) override { VisitNode(rect); }
  void Visit(render_tree::TextNode* text) override { VisitNode(text); }

  virtual ~FinderNodeVisitor() {}

  scoped_refptr<render_tree::Node> GetFoundNode() const { return found_node_; }

  scoped_refptr<render_tree::Node> GetReplaceWithNode() {
    return replace_with_;
  }

 private:
  template <typename T>
  void VisitNode(T* node) {
    if (filter_function_.Run(static_cast<render_tree::Node*>(node))) {
      found_node_ = node;
      if (replace_function_) {
        replace_with_ = replace_function_->Run(node);
      }
    } else {
      VisitNodeChildren(node);
    }
  }

  template <typename T>
  typename base::enable_if<!render_tree::ChildIterator<T>::has_children>::type
  VisitNodeChildren(T* node) {
    // No children to visit.
  }

  template <typename T>
  typename base::enable_if<render_tree::ChildIterator<T>::has_children>::type
  VisitNodeChildren(T* node) {
    render_tree::ChildIterator<T> child_iterator(node);
    while (render_tree::Node* child = child_iterator.GetCurrent()) {
      child->Accept(this);
      if (found_node_) {
        if (replace_with_) {
          child_iterator.ReplaceCurrent(replace_with_);
        }
        break;
      }
      child_iterator.Next();
    }
    if (replace_with_) {
      replace_with_ = new T(child_iterator.TakeReplacedChildrenBuilder());
    }
  }

  NodeFilterFunction<render_tree::Node> filter_function_;
  base::optional<NodeReplaceFunction> replace_function_;

  scoped_refptr<render_tree::Node> found_node_;
  scoped_refptr<render_tree::Node> replace_with_;
};

template <>
NodeSearchResult<render_tree::Node> FindNode<render_tree::Node>(
    const scoped_refptr<render_tree::Node>& tree,
    NodeFilterFunction<render_tree::Node> filter_function,
    base::optional<NodeReplaceFunction> replace_function) {
  FinderNodeVisitor visitor(filter_function, replace_function);
  tree->Accept(&visitor);

  NodeSearchResult<render_tree::Node> result;
  result.found_node = visitor.GetFoundNode();
  result.replaced_tree = visitor.GetReplaceWithNode();

  if (result.replaced_tree == NULL) {
    result.replaced_tree = tree;
  }
  return result;
}

bool HasMapToMesh(render_tree::FilterNode* filter_node) {
  return static_cast<bool>(filter_node->data().map_to_mesh_filter);
}

render_tree::Node* ReplaceWithEmptyCompositionNode(
    render_tree::Node* filter_node) {
  return new render_tree::CompositionNode(
      render_tree::CompositionNode::Builder());
}

}  // namespace common
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
