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

#ifndef COBALT_LAYOUT_BOX_H_
#define COBALT_LAYOUT_BOX_H_

#include <iosfwd>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/cssom/css_computed_style_declaration.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/dom/node.h"
#include "cobalt/layout/base_direction.h"
#include "cobalt/layout/box_intersection_observer_module.h"
#include "cobalt/layout/insets_layout_unit.h"
#include "cobalt/layout/layout_stat_tracker.h"
#include "cobalt/layout/layout_unit.h"
#include "cobalt/layout/line_wrapping.h"
#include "cobalt/layout/rect_layout_unit.h"
#include "cobalt/layout/size_layout_unit.h"
#include "cobalt/layout/vector2d_layout_unit.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/point_f.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/math/vector2d.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/render_tree/animations/animate_node.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/ui_navigation/nav_item.h"
#include "cobalt/web_animations/animation_set.h"

namespace cobalt {

namespace render_tree {
struct RoundedCorners;
}  // namespace render_tree

namespace layout {

class AnonymousBlockBox;
class BlockContainerBox;
class ContainerBox;
class TextBox;
class UsedStyleProvider;

struct LayoutParams {
  LayoutParams()
      : shrink_to_fit_width_forced(false),
        freeze_width(false),
        freeze_height(false),
        containing_block_direction(kLeftToRightBaseDirection) {}

  // Normally the used values of "width", "margin-left", and "margin-right" are
  // calculated by choosing the 1 out of 10 algorithms based on the computed
  // values of "display", "position", "overflow", and the fact whether the box
  // is replaced or not, as per:
  // https://www.w3.org/TR/CSS21/visudet.html#Computing_widths_and_margins
  //
  // If this flag is set, block container boxes will follow the algorithm
  // for inline-level, non-replaced block container boxes, which involves
  // the calculation of shrink-to-fit width, as per:
  // https://www.w3.org/TR/CSS21/visudet.html#inlineblock-width
  //
  // This override is used during the first pass of layout to calculate
  // the content size of "inline-block" elements. It's an equivalent of
  // "trying all possible line breaks", as described by:
  // https://www.w3.org/TR/CSS21/visudet.html#shrink-to-fit-float
  bool shrink_to_fit_width_forced;

  // These overrides are used for flex items when they are sized by the
  // container.
  bool freeze_width;
  bool freeze_height;

  // Many box positions and sizes are calculated with respect to the edges of
  // a rectangular box called a containing block.
  //   https://www.w3.org/TR/CSS21/visuren.html#containing-block
  SizeLayoutUnit containing_block_size;

  // Margin calculations can depend on the direction property of the containing
  // block.
  //   https://www.w3.org/TR/CSS21/visudet.html#blockwidth
  BaseDirection containing_block_direction;

  bool operator==(const LayoutParams& rhs) const {
    return shrink_to_fit_width_forced == rhs.shrink_to_fit_width_forced &&
           freeze_width == rhs.freeze_width &&
           freeze_height == rhs.freeze_height &&
           containing_block_size == rhs.containing_block_size &&
           containing_block_direction == rhs.containing_block_direction;
  }

  base::Optional<LayoutUnit> maybe_margin_top;
  base::Optional<LayoutUnit> maybe_margin_bottom;
  base::Optional<LayoutUnit> maybe_height;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const LayoutParams& params) {
  stream << "{shrink_to_fit_width_forced=" << params.shrink_to_fit_width_forced
         << " freeze_width=" << params.freeze_width
         << " freeze_height=" << params.freeze_height
         << " containing_block_size=" << params.containing_block_size << "}";
  return stream;
}

// A base class for all boxes.
//
// The CSS box model describes the rectangular boxes that are generated
// for elements in the document tree and laid out according to the visual
// formatting model.
//   https://www.w3.org/TR/CSS21/box.html
// Boxes are reference counted, because they are referred to by both parent
// boxes and LayoutBoxes objects stored with html elements in the DOM tree to
// allow incremental box generation.
class Box : public base::RefCounted<Box> {
 public:
  // Defines the formatting context in which the box should participate.
  // Do not confuse with the formatting context that the element may establish.
  enum Level {
    // The "block" value of the "display" property makes an element block-level.
    // Block-level boxes participate in a block formatting context.
    //   https://www.w3.org/TR/CSS21/visuren.html#block-boxes
    kBlockLevel,

    // The "inline" and "inline-block" values of the "display" property make
    // an element inline-level. Inline-level boxes that participate in an inline
    // formatting context.
    //   https://www.w3.org/TR/CSS21/visuren.html#inline-boxes
    kInlineLevel,
  };

  enum MarginCollapsingStatus {
    kCollapseMargins,
    kIgnore,
    kSeparateAdjoiningMargins,
  };

  enum RelationshipToBox {
    kIsBoxAncestor,
    kIsBox,
    kIsBoxDescendant,
  };

  enum TransformAction {
    kEnterTransform,
    kExitTransform,
  };

  // Info tracked on container boxes encountered when a stacking context is
  // generating its cross references.
  struct StackingContextContainerBoxInfo {
    StackingContextContainerBoxInfo(ContainerBox* container_box,
                                    bool is_absolute_containing_block,
                                    bool has_absolute_position,
                                    bool has_overflow_hidden)
        : container_box(container_box),
          is_absolute_containing_block(is_absolute_containing_block),
          is_usable_as_child_container(is_absolute_containing_block),
          has_absolute_position(has_absolute_position),
          has_overflow_hidden(has_overflow_hidden) {}

    ContainerBox* container_box;
    bool is_absolute_containing_block;
    bool is_usable_as_child_container;
    bool has_absolute_position;
    bool has_overflow_hidden;
  };

  typedef std::vector<StackingContextContainerBoxInfo>
      StackingContextContainerBoxStack;

  // List of containing blocks with overflow hidden property. Used by stacking
  // context children that are added to a stacking context container higher up
  // the tree than their containing block, so that they can still have the
  // overflow hidden from those containing blocks applied.
  typedef std::vector<ContainerBox*> ContainingBlocksWithOverflowHidden;

  // The RenderSequence of a box is used to compare the relative drawing order
  // of boxes. It stores a value for the box's drawing order position at each
  // stacking context up to the root of the render tree. As a result, starting
  // from the root ancestor, the box for which the render sequence ends first,
  // or for which the draw order position at a stacking context is lower is
  // drawn before the other box.
  typedef std::vector<size_t> RenderSequence;

  Box(const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
          css_computed_style_declaration,
      UsedStyleProvider* used_style_provider,
      LayoutStatTracker* layout_stat_tracker);
  virtual ~Box();

  // Computed style contains CSS values from the last stage of processing
  // before the layout. The computed value resolves the specified value as far
  // as possible without laying out the document or performing other expensive
  // or hard-to-parallelize operations, such as resolving network requests or
  // retrieving values other than from the element and its parent.
  //   https://www.w3.org/TR/css-cascade-3/#computed
  const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
  css_computed_style_declaration() const {
    return css_computed_style_declaration_;
  }

  const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style()
      const {
    return css_computed_style_declaration_->data();
  }

  // The animation set specifies all currently active animations applying
  // to this box's computed_style() CSS Style Declaration.
  //   https://w3c.github.io/web-animations
  const web_animations::AnimationSet* animations() const {
    return css_computed_style_declaration_->animations().get();
  }

  // Specifies the formatting context in which the box should participate.
  // Do not confuse with the formatting context that the element may establish.
  virtual Level GetLevel() const = 0;

  virtual MarginCollapsingStatus GetMarginCollapsingStatus() const {
    return Box::kCollapseMargins;
  }

  // Returns true if the box is positioned (e.g. position is non-static or
  // transform is not None).  Intuitively, this is true if the element does
  // not follow standard layout flow rules for determining its position.
  //   https://www.w3.org/TR/CSS21/visuren.html#positioned-element.
  bool IsPositioned() const;

  // Returns true if the box has a non-"none" value for its transform property.
  //   https://www.w3.org/TR/css3-transforms/#transform-property
  bool IsTransformed() const;

  // Absolutely positioned box implies that the element's "position" property
  // has the value "absolute" or "fixed".
  //   https://www.w3.org/TR/CSS21/visuren.html#absolutely-positioned
  bool IsAbsolutelyPositioned() const;

  // Returns true if the box serves as a stacking context for descendant
  // elements. The core stacking context creation criteria is given here
  // (https://www.w3.org/TR/CSS21/visuren.html#z-index) however it is extended
  // by various other specification documents such as those describing opacity
  // (https://www.w3.org/TR/css3-color/#transparency) and transforms
  // (https://www.w3.org/TR/css3-transforms/#transform-rendering).
  virtual bool IsStackingContext() const { return false; }

  // Updates the size of margin, border, padding, and content boxes. Lays out
  // in-flow descendants, estimates static positions (but not sizes) of
  // out-of-flow descendants. Does not update the position of the box.
  void UpdateSize(const LayoutParams& layout_params);

  // Returns the offset from root to this box's containing block.
  Vector2dLayoutUnit GetContainingBlockOffsetFromRoot(
      bool transform_forms_root) const;

  // Returns the offset from the containing block (which can be either the
  // containing block's content box or padding box) to its content box.
  Vector2dLayoutUnit GetContainingBlockOffsetFromItsContentBox(
      const ContainerBox* containing_block) const;
  InsetsLayoutUnit GetContainingBlockInsetFromItsContentBox(
      const ContainerBox* containing_block) const;

  // Returns boxes relative to the root or containing block, that take into
  // account transforms.
  RectLayoutUnit GetTransformedBoxFromRoot(
      const RectLayoutUnit& box_from_margin_box) const;
  RectLayoutUnit GetTransformedBoxFromRootWithScroll(
      const RectLayoutUnit& box_from_margin_box) const;
  RectLayoutUnit GetTransformedBoxFromContainingBlock(
      const ContainerBox* containing_block,
      const RectLayoutUnit& box_from_margin_box) const;
  RectLayoutUnit GetTransformedBoxFromContainingBlockContentBox(
      const ContainerBox* containing_block,
      const RectLayoutUnit& box_from_margin_box) const;

  // Used values of "left" and "top" are publicly readable and writable so that
  // they can be calculated and adjusted by the formatting context of the parent
  // box.
  void set_left(LayoutUnit left) {
    margin_box_offset_from_containing_block_.set_x(left);
  }
  LayoutUnit left() const {
    return margin_box_offset_from_containing_block_.x();
  }
  void set_top(LayoutUnit top) {
    margin_box_offset_from_containing_block_.set_y(top);
  }
  LayoutUnit top() const {
    return margin_box_offset_from_containing_block_.y();
  }

  // The following static position functions are only used with absolutely
  // positioned boxes.
  // https://www.w3.org/TR/CSS21/visuren.html#absolute-positioning

  // The static position for 'left' is the distance from the left edge of the
  // containing block to the left margin edge of a hypothetical box that would
  // have been the first box of the element if its 'position' property had been
  // 'static' and 'float' had been 'none'. The value is negative if the
  // hypothetical box is to the left of the containing block.
  //   https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width
  void SetStaticPositionLeftFromParent(LayoutUnit left);
  void SetStaticPositionLeftFromContainingBlockToParent(LayoutUnit left);
  LayoutUnit GetStaticPositionLeft() const;

  // The static position for 'right' is the distance from the right edge of the
  // containing block to the right margin edge of the same hypothetical box as
  // above. The value is positive if the hypothetical box is to the left of the
  // containing block's edge.
  //   https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width
  void SetStaticPositionRightFromParent(LayoutUnit right);
  void SetStaticPositionRightFromContainingBlockToParent(LayoutUnit right);
  LayoutUnit GetStaticPositionRight() const;

  // For the purposes of this section and the next, the term "static position"
  // (of an element) refers, roughly, to the position an element would have had
  // in the normal flow. More precisely, the static position for 'top' is the
  // distance from the top edge of the containing block to the top margin edge
  // of a hypothetical box that would have been the first box of the element if
  // its specified 'position' value had been 'static'.
  //   https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-height
  void SetStaticPositionTopFromParent(LayoutUnit top);
  void SetStaticPositionTopFromContainingBlockToParent(LayoutUnit top);
  LayoutUnit GetStaticPositionTop() const;

  // Each box has a content area and optional surrounding padding, border,
  // and margin areas.
  //   https://www.w3.org/TR/CSS21/box.html#box-dimensions
  //
  // Methods below provide read-only access to dimensions and edges of margin,
  // border, padding, and content boxes.

  // Margin box.
  LayoutUnit margin_left() const { return margin_insets_.left(); }
  LayoutUnit margin_top() const { return margin_insets_.top(); }
  LayoutUnit margin_right() const { return margin_insets_.right(); }
  LayoutUnit margin_bottom() const { return margin_insets_.bottom(); }
  LayoutUnit GetMarginBoxWidth() const;
  LayoutUnit GetMarginBoxHeight() const;

  // Used values of "margin" properties are set by overriders
  // of |UpdateContentSizeAndMargins| method.
  void set_margin_left(LayoutUnit margin_left) {
    margin_insets_.set_left(margin_left);
  }
  void set_margin_top(LayoutUnit margin_top) {
    margin_insets_.set_top(margin_top);
  }
  void set_margin_right(LayoutUnit margin_right) {
    margin_insets_.set_right(margin_right);
  }
  void set_margin_bottom(LayoutUnit margin_bottom) {
    margin_insets_.set_bottom(margin_bottom);
  }

  math::Matrix3F GetMarginBoxTransformFromContainingBlock(
      const ContainerBox* containing_block) const;
  math::Matrix3F GetMarginBoxTransformFromContainingBlockWithScroll(
      const ContainerBox* containing_block) const;

  Vector2dLayoutUnit GetMarginBoxOffsetFromRoot(
      bool transform_forms_root) const;
  const Vector2dLayoutUnit& margin_box_offset_from_containing_block() const {
    return margin_box_offset_from_containing_block_;
  }
  LayoutUnit GetMarginBoxRightEdgeOffsetFromContainingBlock() const;
  LayoutUnit GetMarginBoxBottomEdgeOffsetFromContainingBlock() const;
  LayoutUnit GetMarginBoxStartEdgeOffsetFromContainingBlock(
      BaseDirection base_direction) const;
  LayoutUnit GetMarginBoxEndEdgeOffsetFromContainingBlock(
      BaseDirection base_direction) const;

  // Border box.
  LayoutUnit border_left_width() const { return border_insets_.left(); }
  LayoutUnit border_top_width() const { return border_insets_.top(); }
  LayoutUnit border_right_width() const { return border_insets_.right(); }
  LayoutUnit border_bottom_width() const { return border_insets_.bottom(); }

  RectLayoutUnit GetBorderBoxFromRoot(bool transform_forms_root) const;

  LayoutUnit GetBorderBoxWidth() const;
  LayoutUnit GetBorderBoxHeight() const;
  SizeLayoutUnit GetClampedBorderBoxSize() const;

  RectLayoutUnit GetBorderBoxFromMarginBox() const;
  Vector2dLayoutUnit GetBorderBoxOffsetFromRoot(
      bool transform_forms_root) const;
  Vector2dLayoutUnit GetBorderBoxOffsetFromMarginBox() const;

  void ResetBorderInsets() { border_insets_ = InsetsLayoutUnit(); }

  // Padding box.
  LayoutUnit padding_left() const { return padding_insets_.left(); }
  LayoutUnit padding_top() const { return padding_insets_.top(); }
  LayoutUnit padding_right() const { return padding_insets_.right(); }
  LayoutUnit padding_bottom() const { return padding_insets_.bottom(); }
  LayoutUnit GetPaddingBoxWidth() const;
  LayoutUnit GetPaddingBoxHeight() const;
  SizeLayoutUnit GetClampedPaddingBoxSize() const;

  RectLayoutUnit GetPaddingBoxFromMarginBox() const;
  Vector2dLayoutUnit GetPaddingBoxOffsetFromRoot(
      bool transform_forms_root) const;
  Vector2dLayoutUnit GetPaddingBoxOffsetFromBorderBox() const;
  LayoutUnit GetPaddingBoxLeftEdgeOffsetFromMarginBox() const;
  LayoutUnit GetPaddingBoxTopEdgeOffsetFromMarginBox() const;

  // Set padding insets in InlineContainerBox UpdatePaddings to an empty
  // LayoutUnit or the computed_style value using is_split_on_*_.
  void SetPaddingInsets(LayoutUnit left, LayoutUnit top, LayoutUnit right,
                        LayoutUnit bottom);

  // Content box.
  LayoutUnit width() const { return content_size_.width(); }
  LayoutUnit height() const { return content_size_.height(); }
  const SizeLayoutUnit& content_box_size() const { return content_size_; }

  RectLayoutUnit GetContentBoxFromMarginBox() const;
  Vector2dLayoutUnit GetContentBoxOffsetFromRoot(
      bool transform_forms_root) const;
  Vector2dLayoutUnit GetContentBoxOffsetFromMarginBox() const;
  Vector2dLayoutUnit GetContentBoxOffsetFromBorderBox() const;
  Vector2dLayoutUnit GetContentBoxOffsetFromPaddingBox() const;
  LayoutUnit GetContentBoxLeftEdgeOffsetFromMarginBox() const;
  LayoutUnit GetContentBoxTopEdgeOffsetFromMarginBox() const;
  Vector2dLayoutUnit GetContentBoxOffsetFromContainingBlockContentBox(
      const ContainerBox* containing_block) const;
  InsetsLayoutUnit GetContentBoxInsetFromContainingBlockContentBox(
      const ContainerBox* containing_block) const;
  Vector2dLayoutUnit GetContentBoxOffsetFromContainingBlock() const;
  InsetsLayoutUnit GetContentBoxInsetFromContainingBlock(
      const ContainerBox* containing_block) const;
  LayoutUnit GetContentBoxLeftEdgeOffsetFromContainingBlock() const;
  LayoutUnit GetContentBoxTopEdgeOffsetFromContainingBlock() const;
  LayoutUnit GetContentBoxStartEdgeOffsetFromContainingBlock(
      BaseDirection base_direction) const;
  LayoutUnit GetContentBoxEndEdgeOffsetFromContainingBlock(
      BaseDirection base_direction) const;

  // Return the size difference between the content and margin box on an axis.
  LayoutUnit GetContentToMarginHorizontal() const;
  LayoutUnit GetContentToMarginVertical() const;

  // The height of each inline-level box in the line box is calculated. For
  // replaced elements, inline-block elements, and inline-table elements, this
  // is the height of their margin box; for inline boxes, this is their
  // 'line-height'.
  //   http://www.w3.org/TR/CSS21/visudet.html#line-height
  virtual LayoutUnit GetInlineLevelBoxHeight() const;
  virtual LayoutUnit GetInlineLevelTopMargin() const;

  // When an element is blockified, that should not affect the static position.
  //   https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width
  //   https://www.w3.org/TR/CSS21/visuren.html#dis-pos-flo
  // Return true if the element's outer display type was inline before any
  // optional blockificiation has occurred.
  bool is_inline_before_blockification() const {
    return css_computed_style_declaration_->data()
        ->is_inline_before_blockification();
  }

  // Attempts to wrap the box based upon the provided wrap policies.
  // If |is_line_existence_justified| is true, then the line does not require
  // additional content before wrapping is possible. Otherwise, content
  // justifying the line must be encountered first.
  // |available_width| indicates the amount of width remaining on the line
  // before the boxes overflow it.
  // If |should_collapse_trailing_white_space| is true, the trailing whitespace
  // of the box will be collapsed and should not be included in width
  // calculations.
  //
  // Returns the result of the wrap attempt. If the result is
  // |kWrapResultSplitWrap|, then the box has been split, and the portion of the
  // box split from the initial box is available via GetSplitSibling().
  //
  // Note that only inline boxes are wrappable.
  virtual WrapResult TryWrapAt(WrapAtPolicy wrap_at_policy,
                               WrapOpportunityPolicy wrap_opportunity_policy,
                               bool is_line_existence_justified,
                               LayoutUnit available_width,
                               bool should_collapse_trailing_white_space) = 0;

  // Returns the next box in a linked list of sibling boxes produced from
  // splits of the original box.
  //
  // Note that only inline boxes are splittable. All other box types will return
  // NULL.
  virtual Box* GetSplitSibling() const { return NULL; }

  // Verifies that either an ellipsis can be placed within the box, or that an
  // ellipsis has already been placed in a previous box in the line, and calls
  // DoPlaceEllipsisOrProcessPlacedEllipsis() to handle ellipsis placement and
  // updating of ellipsis-related state within the box. It also sets
  // |is_placement_requirement_met| to true if the box fulfills the requirement
  // that the first character or atomic inline-level element must appear on a
  // line before an ellipsis
  // (https://www.w3.org/TR/css3-ui/#propdef-text-overflow), regardless of
  // whether or not the ellipsis can be placed within this specific box.
  void TryPlaceEllipsisOrProcessPlacedEllipsis(
      BaseDirection base_direction, LayoutUnit desired_offset,
      bool* is_placement_requirement_met, bool* is_placed,
      LayoutUnit* placed_offset);
  // Whether or not the box fulfills the ellipsis requirement that it not be
  // be placed until after the "the first character or atomic inline-level
  // element on a line."
  //   https://www.w3.org/TR/css3-ui/#propdef-text-overflow
  virtual bool DoesFulfillEllipsisPlacementRequirement() const { return false; }
  // Do any processing needed prior to ellipsis placement. This involves caching
  // the old value and resetting the current value so it can be determined
  // whether or not the ellipsis state within a box changed as a a result of
  // ellipsis placement.
  virtual void DoPreEllipsisPlacementProcessing() {}
  // Do any processing needed following ellipsis placement. This involves
  // checking the old value against the new value and resetting the cached
  // render tree node if the ellipsis state changed.
  virtual void DoPostEllipsisPlacementProcessing() {}
  // Whether or not the box is fully hidden by an ellipsis. This applies to
  // atomic inline-level elements that have had an ellipsis placed before them
  // on a line. https://www.w3.org/TR/css3-ui/#propdef-text-overflow
  virtual bool IsHiddenByEllipsis() const { return false; }

  // Initial splitting of boxes between bidi level runs prior to layout, so that
  // they will not need to occur during layout.
  virtual void SplitBidiLevelRuns() = 0;

  // Attempt to split the box at the second level run within it.
  // Returns true if a split occurs. The second box produced by the split is
  // retrievable by calling GetSplitSibling().
  // NOTE: The splits that occur at the intersection of bidi level runs is
  // unrelated to line-wrapping and does not introduce wrappable locations.
  // It is used to facilitate bidi level reordering of the boxes within a
  // line.
  virtual bool TrySplitAtSecondBidiLevelRun() = 0;

  // Retrieve the bidi level for the box, if it has one.
  virtual base::Optional<int> GetBidiLevel() const = 0;

  // Sets whether a leading white space in the box or its first non-collapsed
  // descendant should be collapsed.
  virtual void SetShouldCollapseLeadingWhiteSpace(
      bool should_collapse_leading_white_space) = 0;
  // Sets whether a trailing white space in the box or its last non-collapsed
  // descendant should be collapsed.
  virtual void SetShouldCollapseTrailingWhiteSpace(
      bool should_collapse_trailing_white_space) = 0;
  // Whether the box or its first non-collapsed descendant starts with a white
  // space.
  //
  // WARNING: undefined, unless the box's size is up-to-date.
  virtual bool HasLeadingWhiteSpace() const = 0;
  // Whether the box or its last non-collapsed descendant ends with a white
  // space.
  //
  // WARNING: undefined, unless the box's size is up-to-date.
  virtual bool HasTrailingWhiteSpace() const = 0;
  // A box is collapsed if it has no text or white space, nor have its children.
  // A collapsed box may still have a non-zero width. Atomic inline-level boxes
  // are never collapsed, even if empty.
  //
  // This is used to decide whether two white spaces are following each other in
  // an inline formatting context.
  //
  // WARNING: undefined, unless the box's size is up-to-date.
  virtual bool IsCollapsed() const = 0;

  // Line boxes that contain no text, no preserved white space, no inline
  // elements with non-zero margins, padding, or borders, and no other in-flow
  // content must be treated as zero-height line boxes for the purposes
  // of determining the positions of any elements inside of them, and must be
  // treated as not existing for any other purpose.
  //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  virtual bool JustifiesLineExistence() const = 0;
  // Whether or not the box or its last descendant has a trailing line break,
  // disallowing additional boxes on the same line.
  virtual bool HasTrailingLineBreak() const { return false; }
  // Boxes that don't establish a baseline (such as empty blocks or lines)
  // should not affect the baseline calculation in the block formatting context.
  virtual bool AffectsBaselineInBlockFormattingContext() const = 0;
  // Returns the vertical offset of the baseline relatively to the top margin
  // edge. If the box does not have a baseline, returns the bottom margin edge,
  // as per https://www.w3.org/TR/CSS21/visudet.html#line-height.
  virtual LayoutUnit GetBaselineOffsetFromTopMarginEdge() const = 0;

  // Marks the current set of UpdateSize parameters (which includes the
  // LayoutParams parameter as well as object member variable state) as valid.
  // Returns true if previously calculated results from UpdateSize() are still
  // valid.  This is used to avoid redundant recalculations, and is an extremely
  // important optimization since it applies to all levels of the box hierarchy.
  // Derived classes may override this method to check if local box state has
  // changed as well.
  virtual bool ValidateUpdateSizeInputs(const LayoutParams& params);

  // Invalidating the sizes causes them to be re-calculated the next time they
  // are needed.
  void InvalidateUpdateSizeInputsOfBox();
  void InvalidateUpdateSizeInputsOfBoxAndAncestors();

  // Invalidating the cross references causes them to be re-calculated the next
  // time they are needed.
  virtual void InvalidateCrossReferencesOfBoxAndAncestors();

  // Invalidating the render tree nodes causes them to be re-generated the next
  // time they are needed.
  void InvalidateRenderTreeNodesOfBoxAndAncestors();

  // Converts a layout subtree into a render subtree.
  // This method defines the overall strategy of the conversion and relies
  // on the subclasses to provide the actual content.
  void RenderAndAnimate(
      render_tree::CompositionNode::Builder* parent_content_node_builder,
      const math::Vector2dF& offset_from_parent_node,
      ContainerBox* stacking_context);

  scoped_refptr<render_tree::Node> RenderAndAnimateOverflow(
      const scoped_refptr<render_tree::Node>& content_node,
      const math::Vector2dF& border_offset);

  // Poor man's reflection.
  virtual AnonymousBlockBox* AsAnonymousBlockBox();
  virtual const AnonymousBlockBox* AsAnonymousBlockBox() const;
  virtual BlockContainerBox* AsBlockContainerBox();
  virtual const BlockContainerBox* AsBlockContainerBox() const;
  virtual ContainerBox* AsContainerBox();
  virtual const ContainerBox* AsContainerBox() const;
  virtual TextBox* AsTextBox();
  virtual const TextBox* AsTextBox() const;

#ifdef COBALT_BOX_DUMP_ENABLED
  // Used by box generator to set a DOM node that produced this box.
  void SetGeneratingNode(dom::Node* generating_node);
  // Used by derived classes to dump their children.
  void DumpWithIndent(std::ostream* stream, int indent) const;
#endif  // COBALT_BOX_DUMP_ENABLED

  ContainerBox* parent() { return parent_; }
  const ContainerBox* parent() const { return parent_; }

  const ContainerBox* GetAbsoluteContainingBlock() const;
  ContainerBox* GetAbsoluteContainingBlock() {
    // Return a mutable ContainerBox.
    return const_cast<ContainerBox*>(
        static_cast<const Box*>(this)->GetAbsoluteContainingBlock());
  }
  const ContainerBox* GetFixedContainingBlock() const;
  ContainerBox* GetFixedContainingBlock() {
    // Return a mutable ContainerBox.
    return const_cast<ContainerBox*>(
        static_cast<const Box*>(this)->GetFixedContainingBlock());
  }

  const ContainerBox* GetContainingBlock() const;
  ContainerBox* GetContainingBlock() {
    // Return a mutable ContainerBox.
    return const_cast<ContainerBox*>(
        static_cast<const Box*>(this)->GetContainingBlock());
  }

  const ContainerBox* GetStackingContext() const;
  ContainerBox* GetStackingContext() {
    // Return a mutable ContainerBox.
    return const_cast<ContainerBox*>(
        static_cast<const Box*>(this)->GetStackingContext());
  }

  // Returns the z-index of this box, based on its computed style.
  int GetZIndex() const;

  // Returns the order value of this box, based on its computed style.
  int GetOrder() const;

  // Invalidates the parent of the box, used in box generation for partial
  // layout.
  void InvalidateParent() { parent_ = NULL; }

  // Returns true if the box is positioned under the passed in coordinate.
  bool IsUnderCoordinate(const Vector2dLayoutUnit& coordinate) const;

  // Returns a data structure that can be used by Box::IsRenderedLater().
  RenderSequence GetRenderSequence() const;

  // Returns true if the box for the given render_sequence is rendered after
  // the box for the other_render_sequence. The boxes must be from the same
  // layout tree.
  static bool IsRenderedLater(RenderSequence render_sequence,
                              RenderSequence other_render_sequence);

  // Applies the specified transform action to the provided coordinates.
  // Returns false if the transform is not invertible and the action requires
  // it being inverted.
  bool ApplyTransformActionToCoordinate(TransformAction action,
                                        math::Vector2dF* coordinate) const;
  bool ApplyTransformActionToCoordinates(
      TransformAction action, std::vector<math::Vector2dF>* coordinates) const;

  // Intended to be set to false on the initial containing block, this indicates
  // that when the background color is rendered, it will be blended with what,
  // is behind it (only relevant when the color is not opaque). As an example,
  // if set to false, a background color of transparent will replace any
  // previous pixel values instead of being a no-op.
  void set_blend_background_color(bool value) {
    blend_background_color_ = value;
  }

  // Configure the box's UI navigation item with the box's position, size, etc.
  void UpdateUiNavigationItem();

  void SetUiNavItem(const scoped_refptr<ui_navigation::NavItem>& item) {
    ui_nav_item_ = item;
  }

  void AddIntersectionObserverRootsAndTargets(
      BoxIntersectionObserverModule::IntersectionObserverRootVector&& roots,
      BoxIntersectionObserverModule::IntersectionObserverTargetVector&&
          targets);
  bool ContainsIntersectionObserverRoot(
      const scoped_refptr<IntersectionObserverRoot>& intersection_observer_root)
      const;

  base::Optional<LayoutUnit> collapsed_margin_top_;
  base::Optional<LayoutUnit> collapsed_margin_bottom_;
  base::Optional<LayoutUnit> collapsed_empty_margin_;

  static bool IsBorderStyleNoneOrHidden(
      const scoped_refptr<cssom::PropertyValue>& border_style);

 protected:
  UsedStyleProvider* used_style_provider() const {
    return used_style_provider_;
  }

  LayoutStatTracker* layout_stat_tracker() const {
    return layout_stat_tracker_;
  }

  // Updates used values of "width", "height", and "margin" properties based on
  // https://www.w3.org/TR/CSS21/visudet.html#Computing_widths_and_margins and
  // https://www.w3.org/TR/CSS21/visudet.html#Computing_heights_and_margins.
  // Limits set by "min-width" and "max-width" are honored for non-replaced
  // boxes, based on https://www.w3.org/TR/CSS21/visudet.html#min-max-widths.
  virtual void UpdateContentSizeAndMargins(
      const LayoutParams& layout_params) = 0;

  // Content box setters.
  //
  // Used values of "width" and "height" properties are set by overriders
  // of |UpdateContentSizeAndMargins| method.
  void set_width(LayoutUnit width) { content_size_.set_width(width); }
  void set_height(LayoutUnit height) { content_size_.set_height(height); }

  // Used to determine whether this box justifies the existence of a line,
  // as per:
  //
  // Line boxes that contain no inline elements with non-zero margins, padding,
  // or borders must be treated as not existing.
  //   https://www.w3.org/TR/CSS21/visuren.html#phantom-line-box
  bool HasNonZeroMarginOrBorderOrPadding() const;

  // Add a box and all of its descendants that are contained within the
  // specified stacking context to the stacking context's draw order. This is
  // used when a render tree node that is already cached is encountered to
  // ensure that it maintains the proper draw order in its stacking context.
  virtual void AddBoxAndDescendantsToDrawOrderInStackingContext(
      ContainerBox* stacking_context);

  // Renders the content of the box.
  virtual void RenderAndAnimateContent(
      render_tree::CompositionNode::Builder* border_node_builder,
      ContainerBox* stacking_context) const = 0;

  // A transformable element is an element whose layout is governed by the CSS
  // box model which is either a block-level or atomic inline-level element.
  //   https://www.w3.org/TR/css3-transforms/#transformable-element
  virtual bool IsTransformable() const = 0;

#ifdef COBALT_BOX_DUMP_ENABLED
  void DumpIndent(std::ostream* stream, int indent) const;
  virtual void DumpClassName(std::ostream* stream) const = 0;
  // Overriders must call the base method.
  virtual void DumpProperties(std::ostream* stream) const;
  // Overriders must call the base method.
  virtual void DumpChildrenWithIndent(std::ostream* stream, int indent) const;
#endif  // COBALT_BOX_DUMP_ENABLED

  // Updates the source container box's cross references with its descendants in
  // the box tree that have it as their containing block or stacking context.
  // This function is called recursively.
  virtual void UpdateCrossReferencesOfContainerBox(
      ContainerBox* source_box, RelationshipToBox nearest_containing_block,
      RelationshipToBox nearest_absolute_containing_block,
      RelationshipToBox nearest_fixed_containing_block,
      RelationshipToBox nearest_stacking_context,
      StackingContextContainerBoxStack* stacking_context_container_box_stack);

  // Updates the horizontal margins for block level in-flow boxes. This is used
  // for both non-replaced and replaced elements. See
  // https://www.w3.org/TR/CSS21/visudet.html#blockwidth and
  // https://www.w3.org/TR/CSS21/visudet.html#block-replaced-width.
  void UpdateHorizontalMarginsAssumingBlockLevelInFlowBox(
      BaseDirection containing_block_direction,
      LayoutUnit containing_block_width, LayoutUnit border_box_width,
      const base::Optional<LayoutUnit>& possibly_overconstrained_margin_left,
      const base::Optional<LayoutUnit>& possibly_overconstrained_margin_right);

  // Set border insets in InlineContainerBox UpdateBorders to an empty
  // LayoutUnit or the computed_style value using is_split_on_*_.
  void SetBorderInsets(LayoutUnit left, LayoutUnit top, LayoutUnit right,
                       LayoutUnit bottom);

 private:
  struct CachedRenderTreeNodeInfo {
    explicit CachedRenderTreeNodeInfo(const math::Vector2dF& offset)
        : offset_(offset) {}

    math::Vector2dF offset_;
    scoped_refptr<render_tree::Node> node_;
  };

  // Updates used values of "border" properties.
  virtual void UpdateBorders();
  // Updates used values of "padding" properties.
  virtual void UpdatePaddings(const LayoutParams& layout_params);

  // Computes the normalized "outer" rounded corners (if there are any) from the
  // border radii.
  base::Optional<render_tree::RoundedCorners> ComputeRoundedCorners() const;

  // Computes the corresponding "inner" rounded corners.
  base::Optional<render_tree::RoundedCorners> ComputePaddingRoundedCorners(
      const base::Optional<render_tree::RoundedCorners>& rounded_corners) const;

  // Called after TryPlaceEllipsisOrProcessPlacedEllipsis() determines that the
  // box is impacted by the ellipsis. This handles both determining the location
  // of the ellipsis, if it has not already been placed, and updating the
  // ellipsis-related state of the box, such as whether or not it should be
  // fully or partially hidden.
  virtual void DoPlaceEllipsisOrProcessPlacedEllipsis(
      BaseDirection base_direction, LayoutUnit desired_offset,
      bool* is_placement_requirement_met, bool* is_placed,
      LayoutUnit* placed_offset) {}

  // Get the rectangle for which gives the region that background-color
  // and background-image would populate.
  math::RectF GetBackgroundRect();

  // Get the transform for this box from the specified containing block (which
  // may be null to indicate root).
  math::Matrix3F GetMarginBoxTransformFromContainingBlockInternal(
      const ContainerBox* containing_block, bool include_scroll) const;

  // Some custom CSS transform functions require a UI navigation focus item as
  // input. This computes the appropriate UI navigation item for this box's
  // transform. This should only be called if the box IsTransformed().
  scoped_refptr<ui_navigation::NavItem> ComputeUiNavFocusForTransform() const;

  // Returns whether the overflow is animated by a UI navigation item.
  bool IsOverflowAnimatedByUiNavigation() const {
    return ui_nav_item_ && ui_nav_item_->IsContainer();
  }

  // Helper methods used by |RenderAndAnimate|.
  void RenderAndAnimateBorder(
      const base::Optional<render_tree::RoundedCorners>& rounded_corners,
      render_tree::CompositionNode::Builder* border_node_builder,
      render_tree::animations::AnimateNode::Builder* animate_node_builder);
  void RenderAndAnimateOutline(
      render_tree::CompositionNode::Builder* border_node_builder,
      render_tree::animations::AnimateNode::Builder* animate_node_builder);
  void RenderAndAnimateBackgroundColor(
      const base::Optional<render_tree::RoundedCorners>& rounded_corners,
      render_tree::CompositionNode::Builder* border_node_builder,
      render_tree::animations::AnimateNode::Builder* animate_node_builder);
  struct RenderAndAnimateBackgroundImageResult {
    // The node representing the background image (may be a CompositionNode if
    // there are multiple layers).
    scoped_refptr<render_tree::Node> node;
    // Returns whether the background image opaquely fills the entire frame.
    // If true, then we don't need to even consider rendering the background
    // color, since it will be occluded by the image.
    bool is_opaque;
  };
  RenderAndAnimateBackgroundImageResult RenderAndAnimateBackgroundImage(
      const base::Optional<render_tree::RoundedCorners>& rounded_corners);
  void RenderAndAnimateBoxShadow(
      const base::Optional<render_tree::RoundedCorners>& outer_rounded_corners,
      const base::Optional<render_tree::RoundedCorners>& inner_rounded_corners,
      render_tree::CompositionNode::Builder* border_node_builder,
      render_tree::animations::AnimateNode::Builder* animate_node_builder);

  // If opacity is animated or other than 1, wraps a border node into a filter
  // node. Otherwise returns the original border node.
  scoped_refptr<render_tree::Node> RenderAndAnimateOpacity(
      const scoped_refptr<render_tree::Node>& border_node,
      render_tree::animations::AnimateNode::Builder* animate_node_builder,
      float opacity, bool opacity_animated);

  scoped_refptr<render_tree::Node> RenderAndAnimateOverflow(
      const base::Optional<render_tree::RoundedCorners>& rounded_corners,
      const scoped_refptr<render_tree::Node>& content_node,
      render_tree::animations::AnimateNode::Builder* animate_node_builder,
      const math::Vector2dF& border_node_offset);

  // If transform is not "none", wraps a border node in a MatrixTransformNode.
  // If transform is "none", returns the original border node and leaves
  // |border_node_transform| intact.
  scoped_refptr<render_tree::Node> RenderAndAnimateTransform(
      const scoped_refptr<render_tree::Node>& border_node,
      render_tree::animations::AnimateNode::Builder* animate_node_builder,
      const math::Vector2dF& border_node_offset);

  // This adds an animation to reflect content scrolling by the UI navigation
  // system. Call this only if IsOverflowAnimatedByUiNavigation().
  scoped_refptr<render_tree::Node> RenderAndAnimateUiNavigationContainer(
      const scoped_refptr<render_tree::Node>& node_to_animate,
      render_tree::animations::AnimateNode::Builder* animate_node_builder);

  // The css_computed_style_declaration_ member references the
  // cssom::CSSComputedStyleDeclaration object owned by the HTML Element from
  // which this box is derived.
  const scoped_refptr<cssom::CSSComputedStyleDeclaration>
      css_computed_style_declaration_;
  UsedStyleProvider* const used_style_provider_;
  LayoutStatTracker* const layout_stat_tracker_;

#ifdef COBALT_BOX_DUMP_ENABLED
  std::string generating_html_;
#endif  // COBALT_BOX_DUMP_ENABLED

  // The parent of this box is the box that owns this child and is the direct
  // parent.  If DOM element A is a parent of DOM element B, and box A is
  // derived from DOM element A and box B is derived from DOM element B, then
  // box A will be the parent of box B.
  ContainerBox* parent_;

  // Used values of "left" and "top" properties.
  Vector2dLayoutUnit margin_box_offset_from_containing_block_;

  // The following static position variables are only used with absolutely
  // positioned boxes.
  // https://www.w3.org/TR/CSS21/visuren.html#absolute-positioning
  // The static position for 'left' is the distance from the left edge of the
  // containing block to the left margin edge of a hypothetical box that would
  // have been the first box of the element if its 'position' property had been
  // 'static' and 'float' had been 'none'. The value is negative if the
  // hypothetical box is to the left of the containing block.
  //   https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width
  // The static position for 'right' is the distance from the right edge of the
  // containing block to the right margin edge of the same hypothetical box as
  // above. The value is positive if the hypothetical box is to the left of the
  // containing block's edge.
  //   https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width
  // For the purposes of this section and the next, the term "static position"
  // (of an element) refers, roughly, to the position an element would have had
  // in the normal flow. More precisely, the static position for 'top' is the
  // distance from the top edge of the containing block to the top margin edge
  // of a hypothetical box that would have been the first box of the element if
  // its specified 'position' value had been 'static'.
  //   https://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-height
  InsetsLayoutUnit static_position_offset_from_parent_;
  InsetsLayoutUnit static_position_offset_from_containing_block_to_parent_;

  // Used values of "margin-left", "margin-top", "margin-right",
  // and "margin-bottom".
  InsetsLayoutUnit margin_insets_;
  // Used values of "border-left-width", "border-top-width",
  // "border-right-width", and "border-bottom-width".
  InsetsLayoutUnit border_insets_;
  // Used values of "padding-left", "padding-top", "padding-right",
  // and "padding-bottom".
  InsetsLayoutUnit padding_insets_;
  // Used values of "width" and "height" properties.
  SizeLayoutUnit content_size_;

  // Referenced and updated by ValidateUpdateSizeInputs() to memoize the
  // parameters we were passed during in last call to UpdateSizes().
  base::Optional<LayoutParams> last_update_size_params_;

  // Render tree node caching is used to prevent the node from needing to be
  // recalculated during each call to RenderAndAnimateContent.
  base::Optional<CachedRenderTreeNodeInfo> cached_render_tree_node_info_;

  // A value that indicates the drawing order relative to other boxes in the
  // same stacking context. Smaller values indicate boxes that are drawn
  // earlier.
  size_t draw_order_position_in_stacking_context_;

  // Determines whether the background should be rendered as a clear (i.e. with
  // blending disabled).  It is expected that this may only be set on the
  // initial containing block.
  bool blend_background_color_ = true;

  // UI navigation items are used to help animate certain elements.
  scoped_refptr<ui_navigation::NavItem> ui_nav_item_;

  std::unique_ptr<BoxIntersectionObserverModule>
      box_intersection_observer_module_;

  // For write access to parent/containing_block members.
  friend class ContainerBox;
  friend class LayoutBoxes;
  friend class FlexContainerBox;
  friend class FlexFormattingContext;
  friend class FlexLine;
  friend class MainAxisHorizontalFlexItem;
  friend class MainAxisVerticalFlexItem;

  DISALLOW_COPY_AND_ASSIGN(Box);
};

#ifdef COBALT_BOX_DUMP_ENABLED

// Dumps a box tree recursively to a stream.
// Used for layout debugging, not intended for production.
inline std::ostream& operator<<(std::ostream& stream, const Box& box) {
  box.DumpWithIndent(&stream, 0);
  return stream;
}

#endif  // COBALT_BOX_DUMP_ENABLED

typedef std::vector<scoped_refptr<Box> > Boxes;

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_BOX_H_
