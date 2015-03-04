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
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

// Lower-case names of CSS keywords used as property values.
extern const char* const kAutoKeywordName;
extern const char* const kBlockKeywordName;
extern const char* const kHiddenKeywordName;
extern const char* const kInheritKeywordName;
extern const char* const kInitialKeywordName;
extern const char* const kInlineKeywordName;
extern const char* const kInlineBlockKeywordName;
extern const char* const kNoneKeywordName;
extern const char* const kVisibleKeywordName;

class KeywordValue : public PropertyValue {
 public:
  enum Value {
    // "auto" is a value of "width" and "height" properties which indicates
    // that used value of these properties depends on the values of other
    // properties.
    //   http://www.w3.org/TR/CSS21/visudet.html#the-width-property
    //   http://www.w3.org/TR/CSS21/visudet.html#the-height-property
    kAuto,

    // "block" is a value of "display" property which causes an element
    // to generate a block box.
    //   http://www.w3.org/TR/CSS21/visuren.html#display-prop
    kBlock,

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

    // "none" is a value of "transform" property which means that HTML element
    // is rendered as is.
    //   http://www.w3.org/TR/css3-transforms/#transform-property
    kNone,
  };

  // Since keyword values do not hold additional information and some of them
  // (namely "inherit" and "initial") are used extensively, for the sake of
  // saving memory explicit instantiation of this class is discouraged in favor
  // of shared instances returned by the methods below.
  static const scoped_refptr<KeywordValue>& GetAuto();
  static const scoped_refptr<KeywordValue>& GetBlock();
  static const scoped_refptr<KeywordValue>& GetInherit();
  static const scoped_refptr<KeywordValue>& GetInitial();
  static const scoped_refptr<KeywordValue>& GetInline();
  static const scoped_refptr<KeywordValue>& GetInlineBlock();
  static const scoped_refptr<KeywordValue>& GetNone();

  explicit KeywordValue(Value value) : value_(value) {}

  virtual void Accept(PropertyValueVisitor* visitor) OVERRIDE;

  Value value() const { return value_; }

 private:
  ~KeywordValue() OVERRIDE {}

  const Value value_;

  DISALLOW_COPY_AND_ASSIGN(KeywordValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_KEYWORD_VALUE_H_
