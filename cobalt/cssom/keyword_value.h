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

#ifndef CSSOM_KEYWORD_VALUE_H_
#define CSSOM_KEYWORD_VALUE_H_

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
    //   http://www.w3.org/TR/CSS21/visuren.html#choose-position
    kAbsolute,

    // "auto" is a value of "width" and "height" properties which indicates
    // that used value of these properties depends on the values of other
    // properties.
    //   http://www.w3.org/TR/CSS21/visudet.html#the-width-property
    //   http://www.w3.org/TR/CSS21/visudet.html#the-height-property
    kAuto,

    // "middle" is the default value of "vertical-align" property that indicates
    // that the content should be aligned at the baselines.
    //   http://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align
    kBaseline,

    // "block" is a value of "display" property which causes an element
    // to generate a block box.
    //   http://www.w3.org/TR/CSS21/visuren.html#display-prop
    kBlock,

    // "bottom" is a value of "background-position" property that computes to
    // 100% for the vertical position if one or two values are given, otherwise
    // specifies the bottom edge as the origin for the next offset.
    //  http://www.w3.org/TR/css3-background/#the-background-position
    kBottom,

    // "break-word" is a value of "overflow-wrap" property which specifies to
    // the user agent that an unbreakable word may be broken at an arbitrary
    // point if there are no otherwise-acceptable break points in the line.
    //   http://www.w3.org/TR/css-text-3/#overflow-wrap-property
    kBreakWord,

    // "center" is a value of "text-align" property that indicates that the
    // content should be aligned horizontally centered.
    //   http://www.w3.org/TR/CSS21/text.html#propdef-text-align
    kCenter,

    // "clip" is a value of "text-overflow" property which specifies clipping
    // content that overflows its block container element. Characters may be
    // only partially rendered.
    //   http://www.w3.org/TR/css3-ui/#propdef-text-overflow
    kClip,

    // "contain" is a value of "background-size" property which scales the
    // image to the largest size such that both its width and its height can
    // completely cover the background positioning area.
    //   http://www.w3.org/TR/css3-background/#the-background-size
    kContain,

    // "cover" is a value of "background-size" property which scales the image
    // to the smallest size such that both its width and its height can fit
    // inside the background positioning area.
    //   http://www.w3.org/TR/css3-background/#the-background-size
    kCover,

    // "cursive" is a value of "font_family" property which indicates a generic
    // font family using a more informal script style.
    //   http://www.w3.org/TR/css3-fonts/#generic-font-families
    kCursive,

    // "ellipsis" is a value of "text-overflow" property which specifies
    // rendering an ellipsis to represent clipped inline content.
    //   http://www.w3.org/TR/css3-ui/#propdef-text-overflow
    kEllipsis,

    // "fantasy" is a value of "font_family" property which indicates a generic
    // font family using decorative or expressive representations of characters.
    //   http://www.w3.org/TR/css3-fonts/#generic-font-families
    kFantasy,

    // "fixed" is a value of the "position" property which indicates that
    // the element is positioned and the element's containing block should be
    // set to the viewport.
    //   http://www.w3.org/TR/CSS21/visuren.html#choose-position
    kFixed,

    // "hidden" is a value of "overflow" property which indicates that
    // the content is clipped.
    //   http://www.w3.org/TR/CSS21/visufx.html#overflow
    // "hidden" is a value of "visibility" property which indicates that
    // the generated box is invisible.
    //   http://www.w3.org/TR/CSS21/visufx.html#propdef-visibility
    kHidden,

    // Applicable to any property, represents a cascaded value of "inherit",
    // which means that, for a given element, the property takes the same
    // specified value as the property for the element's parent.
    //   http://www.w3.org/TR/CSS21/cascade.html#value-def-inherit
    kInherit,

    // Applicable to any property, the "initial" keyword represents
    // the specified value that is designated as the property's initial value.
    //   http://www.w3.org/TR/css3-values/#common-keywords
    kInitial,

    // "inline" is a value of "display" property which causes an element
    // to generate one or more inline boxes.
    //   http://www.w3.org/TR/CSS21/visuren.html#display-prop
    kInline,

    // "inline-block" is a value of "display" property which causes an element
    // to generate an inline-level block container.
    //   http://www.w3.org/TR/CSS21/visuren.html#display-prop
    kInlineBlock,

    // "left" is a value of "text-align" property that indicates that the
    // content should be aligned horizontally to the left.
    //   http://www.w3.org/TR/CSS21/text.html#propdef-text-align
    kLeft,

    // "middle" is a value of "vertical-align" property that indicates that the
    // content should be aligned vertically centered.
    //   http://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align
    kMiddle,

    // "monospace" is a value of "font_family" property which indicates a
    // generic
    // font family using glyphs with the same fixed width.
    //   http://www.w3.org/TR/css3-fonts/#generic-font-families
    kMonospace,

    // "none" is a value of "transform" property which means that HTML element
    // is rendered as is.
    //   http://www.w3.org/TR/css3-transforms/#transform-property
    kNone,

    // "no-repeat" is a value of "background-repeat" property which means that
    // image is not repeated in a specific direction.
    //   http://www.w3.org/TR/css3-background/#background-repeat
    kNoRepeat,

    // "normal" is a value of "line-height" property which tells user agents
    // to set the used value to a "reasonable" value based on the font
    // of the element.
    //   http://www.w3.org/TR/CSS21/visudet.html#line-height
    kNormal,

    // "nowrap" is a value of "white-space" property which tells user agents
    // that white space should be collapsed as for "normal" but line breaks
    // should be suppressed within text.
    //   http://www.w3.org/TR/css3-text/#white-space-property
    kNoWrap,

    // "pre" is a value of "white-space" property which tells user agents that
    // white space inside the element should not be collapsed and lines should
    // only be broken at preserved newline characters.
    //   http://www.w3.org/TR/css3-text/#white-space-property
    kPre,

    // "relative" is a value of "position" property which indicates that values
    // of "top", "right", "bottom", and "left" properties specify offsets
    // with respect to the box's in-flow position.
    //   http://www.w3.org/TR/CSS21/visuren.html#choose-position
    kRelative,

    // "repeat" is a value of "background-repeat" property which means that
    // image is repeated in a specific direction.
    //   http://www.w3.org/TR/css3-background/#background-repeat
    kRepeat,

    // "right" is a value of "text-align" property that indicates that the
    // content should be aligned horizontally to the right.
    //   http://www.w3.org/TR/CSS21/text.html#propdef-text-align
    kRight,

    // "sans-serif" is a value of "font_family" property which indicates a
    // generic font family using glyphs with low contrast and plain stroke
    // endings (without flaring, cross stroke or other ornamentation).
    //   http://www.w3.org/TR/css3-fonts/#generic-font-families
    kSansSerif,

    // "serif" is a value of "font_family" property which indicates a generic
    // font family representing the formal text style for script.
    //   http://www.w3.org/TR/css3-fonts/#generic-font-families
    kSerif,

    // "static" is a value of "position" property which indicates that a box
    // is laid out according to the normal flow.
    //   http://www.w3.org/TR/CSS21/visuren.html#choose-position
    kStatic,

    // "top" is a value of "vertical-align" property that indicates that the
    // content should be aligned vertically at the top.
    //   http://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align
    kTop,

    // "uppercase" is a value of "text_transform" property that indicates that
    // all characters in each word of the element's text should be put in
    // uppercase.
    //   http://www.w3.org/TR/css3-text/#text-transform-property
    kUppercase,

    // "visible" is a value of "overflow" property which indicates that
    // the content is not clipped.
    //   http://www.w3.org/TR/CSS21/visufx.html#overflow
    // "visible" is a value of "visibility" property which indicates that
    // the generated box is visible.
    //   http://www.w3.org/TR/CSS21/visufx.html#propdef-visibility
    kVisible,
  };

  // Since keyword values do not hold additional information and some of them
  // (namely "inherit" and "initial") are used extensively, for the sake of
  // saving memory an explicit instantiation of this class is disallowed.
  // Use factory methods below to obtain shared instances.
  static const scoped_refptr<KeywordValue>& GetAbsolute();
  static const scoped_refptr<KeywordValue>& GetAuto();
  static const scoped_refptr<KeywordValue>& GetBaseline();
  static const scoped_refptr<KeywordValue>& GetBlock();
  static const scoped_refptr<KeywordValue>& GetBottom();
  static const scoped_refptr<KeywordValue>& GetBreakWord();
  static const scoped_refptr<KeywordValue>& GetCenter();
  static const scoped_refptr<KeywordValue>& GetClip();
  static const scoped_refptr<KeywordValue>& GetContain();
  static const scoped_refptr<KeywordValue>& GetCover();
  static const scoped_refptr<KeywordValue>& GetCursive();
  static const scoped_refptr<KeywordValue>& GetEllipsis();
  static const scoped_refptr<KeywordValue>& GetFantasy();
  static const scoped_refptr<KeywordValue>& GetFixed();
  static const scoped_refptr<KeywordValue>& GetHidden();
  static const scoped_refptr<KeywordValue>& GetInherit();
  static const scoped_refptr<KeywordValue>& GetInitial();
  static const scoped_refptr<KeywordValue>& GetInline();
  static const scoped_refptr<KeywordValue>& GetInlineBlock();
  static const scoped_refptr<KeywordValue>& GetLeft();
  static const scoped_refptr<KeywordValue>& GetMiddle();
  static const scoped_refptr<KeywordValue>& GetMonospace();
  static const scoped_refptr<KeywordValue>& GetNone();
  static const scoped_refptr<KeywordValue>& GetNoRepeat();
  static const scoped_refptr<KeywordValue>& GetNormal();
  static const scoped_refptr<KeywordValue>& GetNoWrap();
  static const scoped_refptr<KeywordValue>& GetPre();
  static const scoped_refptr<KeywordValue>& GetRelative();
  static const scoped_refptr<KeywordValue>& GetRepeat();
  static const scoped_refptr<KeywordValue>& GetRight();
  static const scoped_refptr<KeywordValue>& GetSansSerif();
  static const scoped_refptr<KeywordValue>& GetSerif();
  static const scoped_refptr<KeywordValue>& GetStatic();
  static const scoped_refptr<KeywordValue>& GetTop();
  static const scoped_refptr<KeywordValue>& GetUppercase();
  static const scoped_refptr<KeywordValue>& GetVisible();

  void Accept(PropertyValueVisitor* visitor) OVERRIDE;

  Value value() const { return value_; }

  std::string ToString() const OVERRIDE;

  bool operator==(const KeywordValue& other) const {
    return value_ == other.value_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(KeywordValue);

  // Implementation detail, has to be public in order to be constructible.
  struct NonTrivialStaticFields;

 private:
  explicit KeywordValue(Value value) : value_(value) {}
  ~KeywordValue() OVERRIDE {}

  const Value value_;

  DISALLOW_COPY_AND_ASSIGN(KeywordValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_KEYWORD_VALUE_H_
