/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef LAYOUT_REPLACED_BOX_H_
#define LAYOUT_REPLACED_BOX_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "cobalt/layout/box.h"
#include "cobalt/render_tree/image.h"

namespace cobalt {
namespace layout {

// The class represents a Replaced element in the layout tree. It is used to
// render elements like embed, iframe or video. Currently it renders the element
// as an image retrieved from a callback passed into its ctor.
//   http://www.w3.org/TR/html5/rendering.html#replaced-elements
//
// TODO(***REMOVED***) : Make ReplacedBox support elements other than media element.
class ReplacedBox : public Box {
 public:
  typedef base::Callback<scoped_refptr<render_tree::Image>()> ReplaceImageCB;

  ReplacedBox(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions,
      const ReplaceImageCB& replace_image_cb);

  // From |Box|.
  Level GetLevel() const OVERRIDE;

  void Layout(const LayoutParams& layout_params) OVERRIDE;
  scoped_ptr<Box> TrySplitAt(float available_width) OVERRIDE;

  bool IsCollapsed() const OVERRIDE { return false; }
  bool HasLeadingWhiteSpace() const OVERRIDE { return false; }
  bool HasTrailingWhiteSpace() const OVERRIDE { return false; }
  void CollapseLeadingWhiteSpace() OVERRIDE {}
  void CollapseTrailingWhiteSpace() OVERRIDE {}

  bool JustifiesLineExistence() const OVERRIDE { return false; }
  bool AffectsBaselineInBlockFormattingContext() const OVERRIDE {
    return false;
  }
  float GetHeightAboveBaseline() const OVERRIDE;

 protected:
  typedef render_tree::CompositionNode::Builder NodeBuilder;

  // From |Box|.
  void AddContentToRenderTree(
      render_tree::CompositionNode::Builder* composition_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const OVERRIDE;
  bool IsTransformable() const OVERRIDE { return false; }

  void DumpClassName(std::ostream* stream) const OVERRIDE;

 private:
  ReplaceImageCB replace_image_cb_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_REPLACED_BOX_H_
