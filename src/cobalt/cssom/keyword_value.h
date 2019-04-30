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

#ifndef COBALT_CSSOM_KEYWORD_VALUE_H_
#define COBALT_CSSOM_KEYWORD_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

class KeywordValue : public PropertyValue {
 public:
  enum Value {
    // "absolute" is a value of "position" property which indicates that values
    // of "top", "right", "bottom", and "left" properties specify offsets
    // with respect to the box's containing block.
    //   https://www.w3.org/TR/CSS21/visuren.html#choose-position
    kAbsolute,

    // "alternate" is a possible value of the "animation-direction" property.
    //   https://www.w3.org/TR/2013/WD-css3-animations-20130219/#animation-direction-property
    kAlternate,

    // "alternate-reverse" is a possible value of the "animation-direction"
    // property.
    //   https://www.w3.org/TR/2013/WD-css3-animations-20130219/#animation-direction-property
    kAlternateReverse,

    // "auto" is a value of "width" and "height" properties which indicates
    // that used value of these properties depends on the values of other
    // properties.
    //   https://www.w3.org/TR/CSS21/visudet.html#the-width-property
    //   https://www.w3.org/TR/CSS21/visudet.html#the-height-property
    // "auto" is a value of the "overflow" property whose behavior is dependent
    // on the user agent; it generally should provide a scrolling mechanism for
    // overflowing boxes.
    //   https://www.w3.org/TR/CSS21/visufx.html#overflow
    kAuto,

    // "backwards" is a value of "animation-fill-mode" property which causes the
    // animation results to fill in backwards around the animation's active
    // duration.
    //   https://www.w3.org/TR/css3-animations/#animation-fill-mode-property
    kBackwards,

    // "baseline" is a default value of "vertical-align" property that indicates
    // that the content should be aligned at the baselines.
    //   https://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align
    // "baseline" is a value of "align-items" and "align-self" properties which
    // causes flex items to be aligned on the cross axis at the same baseline,
    // and at the cross axis start. In default ordering and orientation, that
    // means those flex items are aligned with their baseline to baseline of
    // the item with the largest distance to it's top margin.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-align-items-baseline
    kBaseline,

    // "block" is a value of "display" property which causes an element
    // to generate a block box (block-level block container).
    //   https://www.w3.org/TR/CSS21/visuren.html#display-prop
    kBlock,

    // "both" is a value of "animation-fill-mode" property which causes the
    // animation results to fill in forwards and backwards around the
    // animation's active duration.
    //   https://www.w3.org/TR/css3-animations/#animation-fill-mode-property
    kBoth,

    // "bottom" is a value of "background-position" property that computes to
    // 100% for the vertical position if one or two values are given, otherwise
    // specifies the bottom edge as the origin for the next offset.
    //  https://www.w3.org/TR/css3-background/#the-background-position
    kBottom,

    // "break-word" is a value of "overflow-wrap" property which specifies to
    // the user agent that an unbreakable word may be broken at an arbitrary
    // point if there are no otherwise-acceptable break points in the line.
    //   https://www.w3.org/TR/css-text-3/#overflow-wrap-property
    kBreakWord,

    // "center" is a value of "text-align" property that indicates that the
    // content should be aligned horizontally centered.
    //   https://www.w3.org/TR/css-text-3/#text-align
    // "center" is a value of "align-content" property which causes flex lines
    // to be packed on the cross axis around the center of the flex container.
    // In default ordering and orientation, that means all flex lines are
    // stacked in the vertical center of the flex container.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-align-content-center
    // "center" is a value of "align-items" and "align-self" properties which
    // causes flex items to be aligned with the center of the cross axis within
    // a flex line. In default ordering and orientation, that means those flex
    // items are at the vertical center of their flex line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-align-items-center
    // "center" is a value of "justify-content" which positions flex items
    // after flexible lengths and auto margins have been resolved. It causes
    // flex items to be packed on the main axis toward the start of the flex
    // line. In default ordering and orientation, that means all flex items are
    // packed on the horizontal center of their flex line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-center
    kCenter,

    // "clip" is a value of "text-overflow" property which specifies clipping
    // content that overflows its block container element. Characters may be
    // only partially rendered.
    //   https://www.w3.org/TR/css3-ui/#propdef-text-overflow
    kClip,

    // "collapse" is a value of "visibility" property which causes a flex item
    // to become a collapsed flex item.
    // https://www.w3.org/TR/css-flexbox-1/#visibility-collapse
    // For other elements, "collapse" has the same meaning as "hidden".
    // https://www.w3.org/TR/CSS21/visufx.html#propdef-visibility
    kCollapse,

    // "column" is a value of "flex-direction" property, which specifies that
    // the main axis of the flex container has the same orientation as the
    // block axis of the current writing mode. For the default writing mode,
    // that is top to bottom.
    //   https://www.w3.org/TR/css-flexbox-1/#propdef-flex-direction
    kColumn,

    // "column-reverse" is a value of "flex-direction" property, which
    // specifies that the main axis of the flex container has the opposite
    // orientation as the block axis of the current writing mode. For the
    // default writing mode, that is bottom to top.
    //   https://www.w3.org/TR/css-flexbox-1/#propdef-flex-direction
    kColumnReverse,

    // "contain" is a value of "background-size" property which scales the
    // image to the largest size such that both its width and its height can
    // completely cover the background positioning area.
    //   https://www.w3.org/TR/css3-background/#the-background-size
    kContain,

    // "content" is a value of the "flex-basis" property which indicates an
    // automatic size based on the flex item's content.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-flex-basis-content
    kContent,

    // "cover" is a value of "background-size" property which scales the image
    // to the smallest size such that both its width and its height can fit
    // inside the background positioning area.
    //   https://www.w3.org/TR/css3-background/#the-background-size
    kCover,

    // "currentColor" is the initial value of "border-color" property.
    // CSS3 extends the color value to include the 'currentColor' keyword
    // to allow its use with all properties that accept a <color> value.
    //   https://www.w3.org/TR/css3-color/#currentcolor
    kCurrentColor,

    // "cursive" is a value of "font_family" property which indicates a generic
    // font family using a more informal script style.
    //   https://www.w3.org/TR/css3-fonts/#generic-font-families
    kCursive,

    // "ellipsis" is a value of "text-overflow" property which specifies
    // rendering an ellipsis to represent clipped inline content.
    //   https://www.w3.org/TR/css3-ui/#propdef-text-overflow
    kEllipsis,

    // "end" is a value of "text-align" property that indicates that content
    // is aligned at the end edge of the line box.
    //   https://www.w3.org/TR/css-text-3/#text-align
    kEnd,

    // "equirectangular" is a value of a parameter of the "map-to-mesh"
    // filter function which indicates that the built-in equirectangular mesh
    // should be used.
    kEquirectangular,

    // "fantasy" is a value of "font_family" property which indicates a generic
    // font family using decorative or expressive representations of characters.
    //   https://www.w3.org/TR/css3-fonts/#generic-font-families
    kFantasy,

    // "fixed" is a value of the "position" property which indicates that
    // the element is positioned and the element's containing block should be
    // set to the viewport.
    //   https://www.w3.org/TR/CSS21/visuren.html#choose-position
    kFixed,

    // "flex" is a value of "display" property which causes an element
    // to generate a block-level flex container.
    //   https://www.w3.org/TR/css-flexbox-1/#flex-containers
    kFlex,

    // "flex-end" is a value of "align-content" property which causes flex
    // lines to be packed on the cross axis toward the end of the flex
    // container. In default ordering and orientation, that means all flex
    // lines are stacked at the bottom of the flex container.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-align-content-flex-end
    // "flex-end" is a value of "align-items" and "align-self" properties which
    // causes flex items to be aligned with the end of the cross axis within a
    // flex line. In default ordering and orientation, that means those flex
    // items are at the bottom of their flex line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-align-items-flex-end
    // "flex-end" is a value of "justify-content" which positions flex items
    // after flexible lengths and auto margins have been resolved. It causes
    // flex items to be packed on the main axis toward the end of the flex
    // line. In default ordering and orientation, that means all flex items are
    // packed on the right side of their flex line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-flex-end
    kFlexEnd,

    // "flex-start" is a value of "align-content" property which causes flex
    // lines to be packed on the cross axis toward the start of the flex
    // container. In default ordering and orientation, that means all flex
    // lines are stacked at the top of the flex container.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-align-content-flex-start
    // "flex-start" is a value of "align-items" and "align-self" properties
    // which causes flex items to be aligned with the start of the cross axis
    // within a flex line. In default ordering and orientation, that means
    // those flex items are at the top of their flex line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-align-items-flex-start
    // "flex-start" is a value of "justify-content" which positions flex items
    // after flexible lengths and auto margins have been resolved. It causes
    // flex items to be packed on the main axis toward the start of the flex
    // line. In default ordering and orientation, that means all flex items are
    // packed on the left side of their flex line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-flex-start
    kFlexStart,

    // "forwards" is a value of "animation-fill-mode" property which causes the
    // animation results to fill in forwards around the animation's active
    // duration.
    //   https://www.w3.org/TR/css3-animations/#animation-fill-mode-property
    kForwards,

    // "hidden" is a value of "overflow" property which indicates that
    // the content is clipped.
    //   https://www.w3.org/TR/CSS21/visufx.html#overflow
    // "hidden" is a value of "visibility" property which indicates that
    // the generated box is invisible.
    //   https://www.w3.org/TR/CSS21/visufx.html#propdef-visibility
    kHidden,

    // "infinite" is a value of "animation-iteration-count" property which
    // causes the animation to loop forever.
    //   https://www.w3.org/TR/css3-animations/#animation-iteration-count-property
    kInfinite,

    // Applicable to any property, represents a cascaded value of "inherit",
    // which means that, for a given element, the property takes the same
    // specified value as the property for the element's parent.
    //   https://www.w3.org/TR/CSS21/cascade.html#value-def-inherit
    kInherit,

    // Applicable to any property, the "initial" keyword represents
    // the specified value that is designated as the property's initial value.
    //   https://www.w3.org/TR/css3-values/#common-keywords
    kInitial,

    // "inline" is a value of "display" property which causes an element
    // to generate one or more inline boxes.
    //   https://www.w3.org/TR/CSS21/visuren.html#display-prop
    kInline,

    // "inline-block" is a value of "display" property which causes an element
    // to generate an inline-level block container.
    //   https://www.w3.org/TR/CSS21/visuren.html#display-prop
    kInlineBlock,

    // "inline-flex" is a value of "display" property which causes an element
    // to generate an inline-level flex container.
    //   https://www.w3.org/TR/css-flexbox-1/#flex-containers
    kInlineFlex,

    // "left" is a value of "text-align" property that indicates that the
    // content should be aligned horizontally to the left.
    //   https://www.w3.org/TR/css-text-3/#text-align
    kLeft,

    // "line-through" is a value of "text-decoration-line" property that
    // indicates that the line of text has a line through the middle.
    kLineThrough,

    // "middle" is a value of "vertical-align" property that indicates that the
    // content should be aligned vertically centered.
    //   https://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align
    kMiddle,

    // "monoscopic" is a value of the "cobalt-mtm" property which indicates
    // that the mesh should only be rendered through one eye.
    kMonoscopic,

    // "monospace" is a value of "font_family" property which indicates a
    // generic
    // font family using glyphs with the same fixed width.
    //   https://www.w3.org/TR/css3-fonts/#generic-font-families
    kMonospace,

    // "none" is a value of "transform" property which means that HTML element
    // is rendered as is.
    //   https://www.w3.org/TR/css3-transforms/#transform-property
    kNone,

    // "no-repeat" is a value of "background-repeat" property which means that
    // image is not repeated in a specific direction.
    //   https://www.w3.org/TR/css3-background/#background-repeat
    kNoRepeat,

    // "normal" is a value of "line-height" property which tells user agents
    // to set the used value to a "reasonable" value based on the font
    // of the element.
    //   https://www.w3.org/TR/CSS21/visudet.html#line-height
    kNormal,

    // "nowrap" is a value of "white-space" property which tells user agents
    // that white space should be collapsed as for "normal" but line breaks
    // should be suppressed within text.
    //   https://www.w3.org/TR/css3-text/#white-space-property
    // "nowrap" is the initial value of "flex-wrap" property which indicates
    // that the flex container is single-line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-flex-wrap-wrap
    kNowrap,

    // "pre" is a value of "white-space" property which tells user agents that
    // white space inside the element should not be collapsed and lines should
    // only be broken at preserved newline characters.
    //   https://www.w3.org/TR/css3-text/#white-space-property
    kPre,

    // "pre-line" is a value of "white-space" property which tells user agents
    // that white space inside the element should be collapsed and lines should
    // be broken at preserved newline characters and as necessary to fill line
    // boxes, meaning that wrapping is allowed.
    //   https://www.w3.org/TR/css3-text/#white-space-property
    kPreLine,

    // "pre-wrap" is a value of "white-space" property which tells user agents
    // that white space inside the element should not be collapsed and lines
    // should be broken at preserved newline characters and as necessary to fill
    // line boxes, meaning that wrapping is allowed.
    //   https://www.w3.org/TR/css3-text/#white-space-property
    kPreWrap,

    // "relative" is a value of "position" property which indicates that values
    // of "top", "right", "bottom", and "left" properties specify offsets
    // with respect to the box's in-flow position.
    //   https://www.w3.org/TR/CSS21/visuren.html#choose-position
    kRelative,

    // "repeat" is a value of "background-repeat" property which means that
    // image is repeated in a specific direction.
    //   https://www.w3.org/TR/css3-background/#background-repeat
    kRepeat,

    // "reverse" is a possible value of the "animation-direction" property.
    //   https://www.w3.org/TR/2013/WD-css3-animations-20130219/#animation-direction-property
    kReverse,

    // "right" is a value of "text-align" property that indicates that the
    // content should be aligned horizontally to the right.
    //   https://www.w3.org/TR/css-text-3/#text-align
    kRight,

    // "row" is a value of "flex-direction" property, which specifies that the
    // main axis of the flex container has the same orientation as the inline
    // axis of the current writing mode. For the default writing mode, that is
    // left to right.
    //   https://www.w3.org/TR/css-flexbox-1/#propdef-flex-direction
    kRow,

    // "row-reverse" is a value of "flex-direction" property, which specifies
    // that the main axis of the flex container has the opposite orientation as
    // the inline axis of the current writing mode. For the default writing
    // mode, that is right to left.
    //   https://www.w3.org/TR/css-flexbox-1/#propdef-flex-direction
    kRowReverse,

    // "sans-serif" is a value of "font_family" property which indicates a
    // generic font family using glyphs with low contrast and plain stroke
    // endings (without flaring, cross stroke or other ornamentation).
    //   https://www.w3.org/TR/css3-fonts/#generic-font-families
    kSansSerif,

    // "scroll" is a value of the "overflow" property which indicates that
    // content is clipped and a scrolling mechanism should be provided.
    //   https://www.w3.org/TR/CSS21/visufx.html#overflow
    kScroll,

    // "serif" is a value of "font_family" property which indicates a generic
    // font family representing the formal text style for script.
    //   https://www.w3.org/TR/css3-fonts/#generic-font-families
    kSerif,

    // "solid" is a value of "border-style" property which indicates a single
    // line segment.
    //   https://www.w3.org/TR/css3-background/#border-style
    kSolid,

    // "space-around" is a value of "align-content" property which causes flex
    // lines to be evenly distributed on the cross axis of the flex container,
    // with half-size spaces on either end.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-align-content-space-around
    // "space-around" is a value of "justify-content" which positions flex
    // items after flexible lengths and auto margins have been resolved. It
    // causes flex items to be evenly distributed over a flex line with
    // half-size spaces on either end.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-space-around
    kSpaceAround,

    // "space-between" is a value of "align-content" property which causes flex
    // lines to be evenly distributed on the cross axis of the flex container,
    // with no space before the first or after the last line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-align-content-space-between
    // "space-between" is a value of "justify-content" which positions flex
    // items after flexible lengths and auto margins have been resolved. It
    // causes flex items to be evenly distributed over a flex line with no
    // space before the first or after the last flex item.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-justify-content-space-between
    kSpaceBetween,

    // "start" is a value of "text-align" property that indicates that content
    // is aligned at the start edge of the line box. This is the initial
    // value for "text-align"
    //   https://www.w3.org/TR/css-text-3/#text-align
    kStart,

    // "static" is a value of "position" property which indicates that a box
    // is laid out according to the normal flow.
    //   https://www.w3.org/TR/CSS21/visuren.html#choose-position
    kStatic,

    // "stereoscopic-left-right" is a value of the "cobalt-mtm" property which
    // indicates that the mesh should be rendered in two views
    // side-by-side.
    kStereoscopicLeftRight,

    // "stereoscopic-top-bottom" is a value of the "cobalt-mtm" property which
    // indicates that the mesh should be rendered in two views above and below.
    kStereoscopicTopBottom,

    // "stretch" is a value of "align-content" property which causes flex lines
    // to be stretched on the cross axis to fill the free space of the flex
    // container. In default ordering and orientation, that means the flex
    // lines fill all available vertical space of the flex container.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-align-content-stretch
    // "stretch" is a value of "align-items" and "align-self" properties which
    // causes flex items with cross size "auto" and with cross axis margins
    // other than "auto" to be stretched over the cross axis within a flex
    // line. In default ordering and orientation, that means those flex items
    // use all available vertical space of their flex line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-align-items-stretch
    // "stretch" not is a value of "justify-content". To cause flex items to
    // stretch on the main axis to fill the free space, the functionality of
    // the "flex-grow" property should be used instead.
    //   https://www.w3.org/TR/css-flexbox-1/#justify-content-property
    kStretch,

    // "top" is a value of "vertical-align" property that indicates that the
    // content should be aligned vertically at the top.
    //   https://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align
    kTop,

    // "uppercase" is a value of "text_transform" property that indicates that
    // all characters in each word of the element's text should be put in
    // uppercase.
    //   https://www.w3.org/TR/css3-text/#text-transform-property
    kUppercase,

    // "visible" is a value of "overflow" property which indicates that
    // the content is not clipped.
    //   https://www.w3.org/TR/CSS21/visufx.html#overflow
    // "visible" is a value of "visibility" property which indicates that
    // the generated box is visible.
    //   https://www.w3.org/TR/CSS21/visufx.html#propdef-visibility
    kVisible,

    // "wrap" is a value of "flex-wrap" property which indicates that the flex
    // container is multi-line.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-flex-wrap-wrap
    kWrap,

    // "wrap-reverse" is a value of "flex-wrap" property which indicates that
    // the flex container is multi-line, and that the cross-axis orientation is
    // reversed.
    //   https://www.w3.org/TR/css-flexbox-1/#valdef-flex-wrap-wrap-reverse
    kWrapReverse,

    kMaxKeywordValue = kWrapReverse,
  };

  // Since keyword values do not hold additional information and some of them
  // (namely "inherit" and "initial") are used extensively, for the sake of
  // saving memory an explicit instantiation of this class is disallowed.
  // Use factory methods below to obtain shared instances.
  static const scoped_refptr<KeywordValue>& GetAbsolute();
  static const scoped_refptr<KeywordValue>& GetAlternate();
  static const scoped_refptr<KeywordValue>& GetAlternateReverse();
  static const scoped_refptr<KeywordValue>& GetAuto();
  static const scoped_refptr<KeywordValue>& GetBackwards();
  static const scoped_refptr<KeywordValue>& GetBaseline();
  static const scoped_refptr<KeywordValue>& GetBlock();
  static const scoped_refptr<KeywordValue>& GetBoth();
  static const scoped_refptr<KeywordValue>& GetBottom();
  static const scoped_refptr<KeywordValue>& GetBreakWord();
  static const scoped_refptr<KeywordValue>& GetCenter();
  static const scoped_refptr<KeywordValue>& GetClip();
  static const scoped_refptr<KeywordValue>& GetCollapse();
  static const scoped_refptr<KeywordValue>& GetColumn();
  static const scoped_refptr<KeywordValue>& GetColumnReverse();
  static const scoped_refptr<KeywordValue>& GetContain();
  static const scoped_refptr<KeywordValue>& GetContent();
  static const scoped_refptr<KeywordValue>& GetCover();
  static const scoped_refptr<KeywordValue>& GetCurrentColor();
  static const scoped_refptr<KeywordValue>& GetCursive();
  static const scoped_refptr<KeywordValue>& GetEllipsis();
  static const scoped_refptr<KeywordValue>& GetEnd();
  static const scoped_refptr<KeywordValue>& GetEquirectangular();
  static const scoped_refptr<KeywordValue>& GetFantasy();
  static const scoped_refptr<KeywordValue>& GetFixed();
  static const scoped_refptr<KeywordValue>& GetFlex();
  static const scoped_refptr<KeywordValue>& GetFlexEnd();
  static const scoped_refptr<KeywordValue>& GetFlexStart();
  static const scoped_refptr<KeywordValue>& GetForwards();
  static const scoped_refptr<KeywordValue>& GetHidden();
  static const scoped_refptr<KeywordValue>& GetInfinite();
  static const scoped_refptr<KeywordValue>& GetInherit();
  static const scoped_refptr<KeywordValue>& GetInitial();
  static const scoped_refptr<KeywordValue>& GetInline();
  static const scoped_refptr<KeywordValue>& GetInlineBlock();
  static const scoped_refptr<KeywordValue>& GetInlineFlex();
  static const scoped_refptr<KeywordValue>& GetLeft();
  static const scoped_refptr<KeywordValue>& GetLineThrough();
  static const scoped_refptr<KeywordValue>& GetMiddle();
  static const scoped_refptr<KeywordValue>& GetMonoscopic();
  static const scoped_refptr<KeywordValue>& GetMonospace();
  static const scoped_refptr<KeywordValue>& GetNone();
  static const scoped_refptr<KeywordValue>& GetNoRepeat();
  static const scoped_refptr<KeywordValue>& GetNormal();
  static const scoped_refptr<KeywordValue>& GetNowrap();
  static const scoped_refptr<KeywordValue>& GetPre();
  static const scoped_refptr<KeywordValue>& GetPreLine();
  static const scoped_refptr<KeywordValue>& GetPreWrap();
  static const scoped_refptr<KeywordValue>& GetRelative();
  static const scoped_refptr<KeywordValue>& GetRepeat();
  static const scoped_refptr<KeywordValue>& GetReverse();
  static const scoped_refptr<KeywordValue>& GetRight();
  static const scoped_refptr<KeywordValue>& GetRow();
  static const scoped_refptr<KeywordValue>& GetRowReverse();
  static const scoped_refptr<KeywordValue>& GetSansSerif();
  static const scoped_refptr<KeywordValue>& GetScroll();
  static const scoped_refptr<KeywordValue>& GetSerif();
  static const scoped_refptr<KeywordValue>& GetSolid();
  static const scoped_refptr<KeywordValue>& GetSpaceAround();
  static const scoped_refptr<KeywordValue>& GetSpaceBetween();
  static const scoped_refptr<KeywordValue>& GetStart();
  static const scoped_refptr<KeywordValue>& GetStatic();
  static const scoped_refptr<KeywordValue>& GetStereoscopicLeftRight();
  static const scoped_refptr<KeywordValue>& GetStereoscopicTopBottom();
  static const scoped_refptr<KeywordValue>& GetStretch();
  static const scoped_refptr<KeywordValue>& GetTop();
  static const scoped_refptr<KeywordValue>& GetUppercase();
  static const scoped_refptr<KeywordValue>& GetVisible();
  static const scoped_refptr<KeywordValue>& GetWrap();
  static const scoped_refptr<KeywordValue>& GetWrapReverse();

  void Accept(PropertyValueVisitor* visitor) override;

  Value value() const { return value_; }

  std::string ToString() const override;

  static std::string GetName(Value value) {
    scoped_refptr<KeywordValue> keyword_value(new KeywordValue(value));
    return keyword_value->ToString();
  }

  bool operator==(const KeywordValue& other) const {
    return value_ == other.value_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(KeywordValue);

  // Implementation detail, has to be public in order to be constructible.
  struct NonTrivialStaticFields;

 private:
  explicit KeywordValue(Value value) : value_(value) {}
  ~KeywordValue() override {}

  const Value value_;

  DISALLOW_COPY_AND_ASSIGN(KeywordValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_KEYWORD_VALUE_H_
