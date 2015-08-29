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
#include "cobalt/math/insets_f.h"
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
  LayoutParams() : shrink_to_fit_width_forced(false) {}

  // Normally the used values of "width", "margin-left", and "margin-right" are
  // calculated by choosing the 1 out of 10 algorithms based on the computed
  // values of "display", "position", "overflow", and the fact whether the box
  // is replaced or not, as per:
  // http://www.w3.org/TR/CSS21/visudet.html#Computing_widths_and_margins
  //
  // If this flag is set, block container boxes will follow the algorithm
  // for inline-level, non-replaced block container boxes, which involves
  // the calculation of shrink-to-fit width, as per:
  // http://www.w3.org/TR/CSS21/visudet.html#inlineblock-width
  //
  // This override is used during the first pass of layout to calculate
  // the content size of "inline-block" elements. It's an equivalent of
  // "trying all possible line breaks", as described by:
  // http://www.w3.org/TR/CSS21/visudet.html#shrink-to-fit-float
  bool shrink_to_fit_width_forced;

  // Many box positions and sizes are calculated with respect to the edges of
  // a rectangular box called a containing block.
  //   http://www.w3.org/TR/CSS21/visuren.html#containing-block
  math::SizeF containing_block_size;
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

  // Returns true if the box is positioned (e.g. position is non-static or
  // transform is not None).  Intuitively, this is true if the element does
  // not follow standard layout flow rules for determining its position.
  //
  // TODO(***REMOVED***): Fix the semantics of the method to correspond to
  //               the "positioned box" definition given by
  //               http://www.w3.org/TR/CSS21/visuren.html#positioned-element.
  bool IsPositioned() const;

  // Absolutely positioned box implies that the element's "position" property
  // has the value "absolute" or "fixed".
  //   http://www.w3.org/TR/CSS21/visuren.html#absolutely-positioned
  //
  // TODO(***REMOVED***): Implemented "position: fixed".
  bool IsAbsolutelyPositioned() const;

  // Updates the size of margin, border, padding, and content boxes. Lays out
  // in-flow descendants, estimates static positions (but not sizes) of
  // out-of-flow descendants. Does not update the position of the box.
  void UpdateSize(const LayoutParams& layout_params);

  // Used values of "left" and "top" are publicly readable and writable so that
  // they can be calculated and adjusted by the formatting context of
  // the parent box.
  //
  // TODO(***REMOVED***): Clean up the semantics of "left" and "top". Currently, they
  //               refer to an origin of either a parent's content edge
  //               (for in-flow boxes), or a nearest positioned ancestor's
  //               padding edge (for out-of-flow boxes).
  void set_left(float left) {
    margin_box_offset_from_containing_block_.set_x(left);
  }
  float left() const { return margin_box_offset_from_containing_block_.x(); }
  void set_top(float top) {
    margin_box_offset_from_containing_block_.set_y(top);
  }
  float top() const { return margin_box_offset_from_containing_block_.y(); }
  const math::PointF& margin_box_offset_from_containing_block() const {
    return margin_box_offset_from_containing_block_;
  }

  // Each box has a content area and optional surrounding padding, border,
  // and margin areas.
  //   http://www.w3.org/TR/CSS21/box.html#box-dimensions
  //
  // Methods below provide read-only access to dimensions and edges of margin,
  // border, padding, and content boxes.

  // Margin box.
  float GetMarginBoxWidth() const;
  float GetMarginBoxHeight() const;
  float GetRightMarginEdgeOffsetFromContainingBlock() const;
  float GetBottomMarginEdgeOffsetFromContainingBlock() const;

  // Border box.
  float GetBorderBoxWidth() const;
  float GetBorderBoxHeight() const;
  math::SizeF GetBorderBoxSize() const;

  // Padding box.
  float GetPaddingBoxWidth() const;
  float GetPaddingBoxHeight() const;
  math::SizeF GetPaddingBoxSize() const;

  // Content box.
  float width() const { return content_size_.width(); }
  float height() const { return content_size_.height(); }
  const math::SizeF& content_box_size() const { return content_size_; }
  math::Vector2dF GetContentBoxOffsetFromMarginBox() const;

  // Attempts to split the box, so that the part before the split would fit
  // the available width. However, if |allow_overflow| is true, then the split
  // is permitted to overflow the available width if no smaller split is
  // available.
  //
  // Returns the part after the split if the split succeeded.
  //
  // Note that only inline boxes are splittable.
  virtual scoped_ptr<Box> TrySplitAt(float available_width,
                                     bool allow_overflow) = 0;

  // Initial splitting of boxes between bidi level runs prior to layout, so that
  // they will not need to occur during layout.
  virtual void SplitBidiLevelRuns() = 0;

  // Attempt to split the box at the second level run within it.
  virtual scoped_ptr<Box> TrySplitAtSecondBidiLevelRun() = 0;

  // Retrieve the bidi level for the box, if it has one.
  virtual base::optional<int> GetBidiLevel() const = 0;

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
  void RenderAndAnimate(
      render_tree::CompositionNode::Builder* parent_content_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const;

  // Poor man's reflection.
  virtual AnonymousBlockBox* AsAnonymousBlockBox();

  // Used by derived classes to dump their children.
  void DumpWithIndent(std::ostream* stream, int indent) const;

  ContainerBox* parent() { return parent_; }
  const ContainerBox* parent() const { return parent_; }

  ContainerBox* containing_block() { return containing_block_; }
  const ContainerBox* containing_block() const { return containing_block_; }

  ContainerBox* stacking_context() { return stacking_context_; }
  const ContainerBox* stacking_context() const { return stacking_context_; }

  // TODO(***REMOVED***): This only depends on the computed style, maybe this function
  //               should move into a newly created CSSComputedStyleDeclaration
  //               type?  This would apply to other values such as
  //               IsPositioned().
  // Returns the z-index of this box, based on its computed style.
  int GetZIndex() const;

  // Updates all cross-references to other boxes in the box tree (e.g. stacking
  // contexts and containing blocks).  Calling this function will recursively
  // resolve these links for all elements in the box tree.
  void UpdateCrossReferences();

 protected:
  const UsedStyleProvider* used_style_provider() const {
    return used_style_provider_;
  }

  // Updates used values of "width", "height", and "margin" properties based on
  // http://www.w3.org/TR/CSS21/visudet.html#Computing_widths_and_margins and
  // http://www.w3.org/TR/CSS21/visudet.html#Computing_heights_and_margins.
  virtual void UpdateContentSizeAndMargins(
      const LayoutParams& layout_params) = 0;

  // Margin box accessors.
  //
  // Used values of "margin" properties are set by overriders
  // of |UpdateContentSizeAndMargins| method.
  float margin_left() const { return margin_insets_.left(); }
  void set_margin_left(float margin_left) {
    margin_insets_.set_left(margin_left);
  }
  float margin_top() const { return margin_insets_.top(); }
  void set_margin_top(float margin_top) { margin_insets_.set_top(margin_top); }
  float margin_right() const { return margin_insets_.right(); }
  void set_margin_right(float margin_right) {
    margin_insets_.set_right(margin_right);
  }
  float margin_bottom() const { return margin_insets_.bottom(); }
  void set_margin_bottom(float margin_bottom) {
    margin_insets_.set_bottom(margin_bottom);
  }

  // Border box read-only accessors.
  float border_left_width() const { return border_insets_.left(); }
  float border_top_width() const { return border_insets_.top(); }
  float border_right_width() const { return border_insets_.right(); }
  float border_bottom_width() const { return border_insets_.bottom(); }

  // Padding box read-only accessors.
  float padding_left() const { return padding_insets_.left(); }
  float padding_top() const { return padding_insets_.top(); }
  float padding_right() const { return padding_insets_.right(); }
  float padding_bottom() const { return padding_insets_.bottom(); }

  // Content box setters.
  //
  // Used values of "width" and "height" properties are set by overriders
  // of |UpdateContentSizeAndMargins| method.
  void set_width(float width) { content_size_.set_width(width); }
  void set_height(float height) { content_size_.set_height(height); }

  // Used to determine whether this box justifies the existence of a line,
  // as per:
  //
  // Line boxes that contain no inline elements with non-zero margins, padding,
  // or borders must be treated as not existing.
  //   http://www.w3.org/TR/CSS21/visuren.html#phantom-line-box
  bool HasNonZeroMarginOrBorderOrPadding() const;

  // Renders the content of the box.
  virtual void RenderAndAnimateContent(
      render_tree::CompositionNode::Builder* border_node_builder,
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

  // Updates the box's cross references to other boxes in the box tree (e.g. its
  // containing block and stacking context).  "Context" implies that the caller
  // has already computed what the stacking context is and containing block
  // for absolute elements.
  virtual void UpdateCrossReferencesWithContext(
      ContainerBox* absolute_containing_block, ContainerBox* stacking_context);

 private:
  // Updates used values of "border" properties.
  void UpdateBorders();
  // Updates used values of "padding" properties.
  void UpdatePaddings(const LayoutParams& layout_params);

  // Sets up this box as a positioned box (thus, Box::IsPositioned() must return
  // true) with the associated containing block and stacking context.
  // Note that the box's parent node remains unchanged throughout this, and will
  // always be the same as if the box was not positioned.
  void SetupAsPositionedChild(ContainerBox* containing_block,
                              ContainerBox* stacking_context);

  // Helper methods used by |RenderAndAnimate|.
  void RenderAndAnimateBorder(
      render_tree::CompositionNode::Builder* border_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const;
  void RenderAndAnimateBackgroundColor(
      render_tree::CompositionNode::Builder* border_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const;
  void RenderAndAnimateBackgroundImage(
      render_tree::CompositionNode::Builder* border_node_builder,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder) const;
  // If opacity is animated or other than 1, wraps a border node into a filter
  // node. Otherwise returns the original border node.
  scoped_refptr<render_tree::Node> RenderAndAnimateOpacity(
      const scoped_refptr<render_tree::Node>& border_node,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder,
      float opacity, bool opacity_animated) const;
  // If transform is not "none", wraps a border node into a composition node if
  // transform is animated or modifies |border_node_transform| otherwise.
  // If transform is "none", returns the original border node and leaves
  // |border_node_transform| intact.
  scoped_refptr<render_tree::Node> RenderAndAnimateTransform(
      const scoped_refptr<render_tree::Node>& border_node,
      render_tree::animations::NodeAnimationsMap::Builder*
          node_animations_map_builder,
      math::Matrix3F* border_node_transform) const;

  const scoped_refptr<const cssom::CSSStyleDeclarationData> computed_style_;
  const UsedStyleProvider* const used_style_provider_;

  // The transitions_ member references the cssom::TransitionSet object owned
  // by the HTML Element from which this box is derived.
  const cssom::TransitionSet* transitions_;

  // The parent of this box is the box that owns this child and is the direct
  // parent.  If DOM element A is a parent of DOM element B, and box A is
  // derived from DOM element A and box B is derived from DOM element B, then
  // box A will be the parent of box B.
  ContainerBox* parent_;

  // A pointer to this box's containing block.  The containing block is always
  // an ancestor of this element, though not necessarily the direct parent.
  ContainerBox* containing_block_;

  // A pointer to this box's stacking context.  The containing block is always
  // an ancestor of this element, though not necessarily the direct parent.
  ContainerBox* stacking_context_;

  // Used values of "left" and "top" properties.
  math::PointF margin_box_offset_from_containing_block_;
  // Used values of "margin-left", "margin-top", "margin-right",
  // and "margin-bottom".
  math::InsetsF margin_insets_;
  // Used values of "border-left-width", "border-top-width",
  // "border-right-width", and "border-bottom-width".
  math::InsetsF border_insets_;
  // Used values of "padding-left", "padding-top", "padding-right",
  // and "padding-bottom".
  math::InsetsF padding_insets_;
  // Used values of "width" and "height" properties.
  math::SizeF content_size_;

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
