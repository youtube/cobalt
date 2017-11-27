// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_FILTER_NODE_H_
#define COBALT_RENDER_TREE_FILTER_NODE_H_

#include "base/compiler_specific.h"
#include "base/optional.h"
#include "cobalt/base/type_id.h"
#include "cobalt/render_tree/blur_filter.h"
#include "cobalt/render_tree/map_to_mesh_filter.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/opacity_filter.h"
#include "cobalt/render_tree/shadow.h"
#include "cobalt/render_tree/viewport_filter.h"

namespace cobalt {
namespace render_tree {

// Wrapping a render tree inside of a FilterMode expresses the desire to modify
// the image produced by the source render tree by passing it through a set
// of filters, such as an opacity filter which makes the resulting image
// transparent.
class FilterNode : public Node {
 public:
  struct Builder {
    explicit Builder(const scoped_refptr<render_tree::Node>& source);

    Builder(const OpacityFilter& opacity_filter,
            const scoped_refptr<render_tree::Node>& source);

    Builder(const ViewportFilter& viewport_filter,
            const scoped_refptr<render_tree::Node>& source);

    Builder(const BlurFilter& blur_filter,
            const scoped_refptr<render_tree::Node>& source);

    Builder(const MapToMeshFilter& map_to_mesh_filter,
            const scoped_refptr<render_tree::Node>& source);

    math::RectF GetBounds() const;

    bool operator==(const Builder& other) const {
      return source == other.source && opacity_filter == other.opacity_filter &&
             viewport_filter == other.viewport_filter &&
             blur_filter == other.blur_filter &&
             map_to_mesh_filter == other.map_to_mesh_filter;
    }

    // The source tree, which will be used as the input to the filters specified
    // in this FilterNode.
    scoped_refptr<render_tree::Node> source;

    // If set, this filter will make the source subtree appear transparent,
    // with the level of transparency dictated by the OpacityFilter's value.
    base::optional<OpacityFilter> opacity_filter;

    // If set, this filter will specify the viewport of source content. Only
    // the source content within the viewport rectangle will be rendered.
    // Rounded corners may be specified on this filter.
    base::optional<ViewportFilter> viewport_filter;

    // If set, then a Gaussian blur will be applied to the source with a
    // Gaussian kernel of standard deviation |blur_sigma|.
    base::optional<BlurFilter> blur_filter;

    // If set, indicates that the rasterized output of the source content should
    // be used as a texture which is then mapped onto a 3D mesh specified by the
    // filter.
    base::optional<MapToMeshFilter> map_to_mesh_filter;
  };

  explicit FilterNode(const Builder& builder) : data_(builder) {
    AssertValid();
  }

  FilterNode(const OpacityFilter& opacity_filter,
             const scoped_refptr<render_tree::Node>& source);

  FilterNode(const ViewportFilter& viewport_filter,
             const scoped_refptr<render_tree::Node>& source);

  FilterNode(const BlurFilter& blur_filter,
             const scoped_refptr<render_tree::Node>& source);

  FilterNode(const MapToMeshFilter& map_to_mesh_filter,
             const scoped_refptr<render_tree::Node>& source);

  void Accept(NodeVisitor* visitor) override;
  math::RectF GetBounds() const override { return data_.GetBounds(); }

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<FilterNode>();
  }

  const Builder& data() const { return data_; }

 private:
  void AssertValid() const;

  const Builder data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_FILTER_NODE_H_
