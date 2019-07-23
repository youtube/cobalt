// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_COMMON_FIND_NODE_H_
#define COBALT_RENDERER_RASTERIZER_COMMON_FIND_NODE_H_

#include "base/bind.h"
#include "cobalt/base/enable_if.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/type_id.h"
#include "cobalt/render_tree/child_iterator.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/node_visitor.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace common {

using NodeReplaceFunction =
    base::Callback<render_tree::Node*(render_tree::Node*)>;

template <typename T = render_tree::Node>
using NodeFilterFunction = base::Callback<bool(T*)>;

template <typename T = render_tree::Node>
struct NodeSearchResult {
  scoped_refptr<T> found_node;  // Null if no such node is found.
  scoped_refptr<render_tree::Node> replaced_tree;
};

// Finds the first node of type |T| that passes an optional |filter_function|,
// and optionally replaces it in the tree with the result of |replace_function|.
//
// Example usage:
//     auto filter = base::Bind(&CheckSomethingOnNode)
//     auto replace = base::Bind(&ReplaceNode)
//     auto search_results = FindNode<SomeNode>(tree, filter, replace);
//     DoSomethingWithNode(search_results.found_node);
//     DoSomethingWithTree(search_results.replaced_tree);
template <typename T>
NodeSearchResult<T> FindNode(
    const scoped_refptr<render_tree::Node>& tree,
    NodeFilterFunction<T> typed_filter_function = base::Bind([](T*) {
      return true;
    }),
    base::Optional<NodeReplaceFunction> replace_function = base::nullopt) {
  // Wrap the typed filter with an untyped callback.
  auto type_checking_filter_function = base::Bind(
      [](NodeFilterFunction<T> typed_filter_function, render_tree::Node* node) {
        if (node->GetTypeId() != base::GetTypeId<T>()) {
          return false;
        }

        auto* typed_node = base::polymorphic_downcast<T*>(node);
        return typed_filter_function.Run(typed_node);
      },
      typed_filter_function);

  // Call the default untyped FindNode with the above wrap.
  NodeSearchResult<> untyped_result = FindNode<render_tree::Node>(
      tree, type_checking_filter_function, replace_function);
  NodeSearchResult<T> typed_result;
  typed_result.replaced_tree = untyped_result.replaced_tree;
  typed_result.found_node =
      base::polymorphic_downcast<T*>(untyped_result.found_node.get());

  return typed_result;
}

// Same as the above, but not filtering nodes by type, so the filter can act on
// nodes of different classes.
template <>
NodeSearchResult<render_tree::Node> FindNode<render_tree::Node>(
    const scoped_refptr<render_tree::Node>& tree,
    NodeFilterFunction<render_tree::Node> filter_function,
    base::Optional<NodeReplaceFunction> replace_function);

// Checks whether the given filter node has a MapToMesh filter in it.
bool HasMapToMesh(render_tree::FilterNode* node);

render_tree::Node* ReplaceWithEmptyCompositionNode(render_tree::Node* node);

}  // namespace common
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_COMMON_FIND_NODE_H_
