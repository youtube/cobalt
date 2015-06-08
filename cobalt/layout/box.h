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

#ifndef LAYOUT_BOX_H_
#define LAYOUT_BOX_H_

#include <iosfwd>

#include "base/optional.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/css_transition_set.h"
#include "cobalt/math/point_f.h"
#include "cobalt/math/size_f.h"
#include "cobalt/render_tree/animations/node_animations_map.h"
#include "cobalt/render_tree/composition_node.h"

namespace cobalt {
namespace layout {

class AnonymousBlockBox;
class ContainerBox;
class UsedStyleProvider;

struct LayoutParams {
  LayoutParams() : shrink_if_width_depends_on_containing_block(false) {}

  // Many box positions and sizes are calculated with respect to the edges of
  // a rectangular box called a containing block.
  //   http://www.w3.org/TR/CSS21/visuren.html#containing-block
  math::SizeF containing_block_size;

  // Boxes whose width depends on a containing block will shrink instead
  // of expanding during the first pass of the layout initiated by inline-level
  // block container boxes.
  bool shrink_if_width_depends_on_containing_block;
};

// A base class for all boxes.
//
// The CSS box model describes the rectangular boxes that are generated
// for elements in the document tree and laid out according to the visual
// formatting model.
//   http://www.w3.org/TR/CSS21/box.html
class Box {
 public:
  // Defines the formatting context in which the box should participate.
  // Do not confuse with the formatting context that the element may establish.
  enum Level {
    // The "block" value of the "display" property makes an element block-level.
    // Block-level boxes participate in a block formatting context.
    //   http://www.w3.org/TR/CSS21/visuren.html#block-boxes
    kBlockLevel,

    // The "inline" and "inline-block" values of the "display" property make
    // an element inline-level. Inline-level boxes that participate in an inline
    // formatting context.
    //   http://www.w3.org/TR/CSS21/visuren.html#inline-boxes
    kInlineLevel,
  };

  Box(const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions,
      const UsedStyleProvider* used_style_provider);
  virtual ~Box();

  // Computed style contains CSS values from the last stage of processing
  // before the layout. The computed value resolves the specified value as far
  // as possible without laying out the document or performing other expensive
  // or hard-to-parallelize operations, such as resolving network requests or
  // retrieving values other than from the element and its parent.
  //   http://www.w3.org/TR/css-cascade-3/#computed
  const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style()
      const {
    return computed_style_;
  }

  // The transition set specifies all currently active transitions appyling
  // to this box's computed_style() CSS Style Declaration.
  //   http://www.w3.org/TR/css3-transitions
  const cssom::TransitionSet* transitions() const { return transitions_; }

  // Specifies the formatting context in which the box should participate.
  // Do not confuse with the formatting context that the element may establish.
  virtual Level GetLevel() const = 0;

  // Used values of "left" and "top" are publicly readable and writable so that
  // they can be calculated and adjusted by the formatting context of
  // the parent box.
  void set_used_left(float used_left) { maybe_used_left_ = used_left; }
  float used_left() const { return *maybe_used_left_; }
  void set_used_top(float used_top) { maybe_used_top_ = used_top; }
  float used_top() const { return *maybe_used_top_; }

  // Used values of "width" and "height" are publicly readable so that
  // the parent box can calculate the position of subsequent siblings.
  float used_width() const { return *maybe_used_width_; }
  float used_height() const { return *maybe_used_height_; }

  // Read-only synthetic properties that are fully defined by used values
  // of "left", "top", "width", and "height".
  math::PointF used_position() const {
    return math::PointF(used_left(), used_top());
  }
  math::SizeF used_size() const {
    return math::SizeF(used_width(), used_height());
  }
  float used_right() const { return used_left() + used_width(); }
  float used_bottom() const { return used_top() + used_height(); }

  // Updates used values of "width" and "height" if they are invalid, otherwise
  // does nothing. As a side effect, lays out all box descendants recursively.
  void UpdateUsedSizeIfInvalid(const LayoutParams& layout_params);
  // Invalidates used values of "left", "top", "width", and "height".
  void InvalidateUsedRect();

  // Returns true if the box is positioned (e.g. position is non-static or
  // transform is not None).  Intuitively, this is true if the element does
  // not follow standard layout flow rules for determining its position.
  bool IsPositioned() const;

  // Attempts to split the box, so that the part before the split would fit
  // the available width. Returns the part after the split if the split
  // succeeded. Note that only inline boxes are splittable.
  virtual scoped_ptr<Box> TrySplitAt(float available_width) = 0;

  // A box is collapsed if it has no text or white space, nor have its children.
  // A collapsed box may still have a non-zero width. Atomic inline-level boxes
  // are never collapsed, even if empty.
  //
  // This is used to decide whether two white spaces are following each other in
  // an inline formatting context.
  virtual bool IsCollapsed() const = 0;
  // Whether the box or its first non-collapsed descendant starts with a white
  // space.
  virtual bool HasLeadingWhiteSpace() const = 0;
  // Whether the box or its last non-collapsed descendant ends with a white
  // space.
  virtual bool HasTrailingWhiteSpace() const = 0;
  // Collapses a leading white space in the box or its first non-collapsed
  // descendant.
  virtual void CollapseLeadingWhiteSpace() = 0;
  // Collapses a trailing white space in the box or its last non-collapsed
  // descendant.
  virtual void CollapseTrailingWhiteSpace() = 0;

  // Line boxes that contain no text, no preserved white space, no inline
  // elements with non-zero margins, padding, or borders, and no other in-flow
  // content must be treated as zero-height line boxes for the purposes
  // of determining the positions of any elements inside of them, and must be
  // treated as not existing for any other purpose.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  virtual bool JustifiesLineExistence() const = 0;
  // Boxes that don't establish a baseline (such as empty blocks or lines)
  // should not affect the baseline calculation in the block formatting context.
  virtual bool AffectsBaselineInBlockFormattingContext() const = 0;
  // Returns the vertical offset of the baseline relatively to the origin
  // of the box. If the box does not have a baseline, returns the bottom margin
  // edge.
  //   http://www.w3.org/TR/CSS21/visudet.html#line-height
  virtual float GetHeightAboveBaseline() const = 0;

  // Converts a layout subtree into a render subtree.
  // This method defines the overall strategy of the conversion and relies
  // on the subclasses to provide the actual content.
  void AddToRenderTree(
      render_tree::CompositionNode::Builder* parent_composition_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const;

  // Poor man's reflection.
  virtual AnonymousBlockBox* AsAnonymousBlockBox();

  // Used by derived classes to dump their children.
  void DumpWithIndent(std::ostream* stream, int indent) const;

  ContainerBox* parent() const { return parent_; }

 protected:
  // Used values of "width" and "height" properties are writable only by the
  // box itself.
  virtual void UpdateUsedSize(const LayoutParams& layout_params) = 0;
  void set_used_width(float used_width) { maybe_used_width_ = used_width; }
  void set_used_height(float used_height) { maybe_used_height_ = used_height; }
  void InvalidateUsedWidth();
  void InvalidateUsedHeight();

  // Renders the background of the box.
  void AddBackgroundToRenderTree(
      render_tree::CompositionNode::Builder* composition_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const;
  // Renders the content of the box.
  virtual void AddContentToRenderTree(
      render_tree::CompositionNode::Builder* composition_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const = 0;

  // A transformable element is an element whose layout is governed by the CSS
  // box model which is either a block-level or atomic inline-level element.
  //   http://www.w3.org/TR/css3-transforms/#transformable-element
  virtual bool IsTransformable() const = 0;

  void DumpIndent(std::ostream* stream, int indent) const;
  virtual void DumpClassName(std::ostream* stream) const = 0;
  // Overriders must call the base method.
  virtual void DumpProperties(std::ostream* stream) const;
  // Overriders must call the base method.
  virtual void DumpChildrenWithIndent(std::ostream* stream, int indent) const;

  const UsedStyleProvider* used_style_provider() const {
    return used_style_provider_;
  }

 private:
  const scoped_refptr<const cssom::CSSStyleDeclarationData> computed_style_;

  const UsedStyleProvider* const used_style_provider_;

  // The transitions_ member references the cssom::TransitionSet object owned
  // by the HTML Element from which this box is derived.
  const cssom::TransitionSet* transitions_;

  base::optional<float> maybe_used_left_;
  base::optional<float> maybe_used_top_;
  base::optional<float> maybe_used_width_;
  base::optional<float> maybe_used_height_;

  ContainerBox* parent_;
  ContainerBox* containing_block_;

  // For write access to parent/containing_block members.
  friend class ContainerBox;

  DISALLOW_COPY_AND_ASSIGN(Box);
};

// Dumps a box tree recursively to a stream.
// Used for layout debugging, not intended for production.
inline std::ostream& operator<<(std::ostream& stream, const Box& box) {
  box.DumpWithIndent(&stream, 0);
  return stream;
}

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_BOX_H_
