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

#ifndef CSSOM_PROPERTY_VALUE_VISITOR_H_
#define CSSOM_PROPERTY_VALUE_VISITOR_H_

#include "base/compiler_specific.h"

namespace cobalt {
namespace cssom {

class AbsoluteURLValue;
class ConstStringListValue;
class FontWeightValue;
class KeywordValue;
class LengthValue;
class NumberValue;
class PropertyValue;
class RGBAColorValue;
class StringValue;
class TimeListValue;
class TimingFunctionListValue;
class TransformFunctionListValue;
class URLValue;

// Type-safe branching on a class hierarchy of CSS property values,
// implemented after a classical GoF pattern (see
// http://en.wikipedia.org/wiki/Visitor_pattern#Java_example).
class PropertyValueVisitor {
 public:
  virtual void VisitAbsoluteURL(AbsoluteURLValue* url_value) = 0;
  virtual void VisitConstStringList(
      ConstStringListValue* const_string_list_value) = 0;
  virtual void VisitFontWeight(FontWeightValue* font_weight_value) = 0;
  virtual void VisitKeyword(KeywordValue* keyword_value) = 0;
  virtual void VisitLength(LengthValue* length_value) = 0;
  virtual void VisitNumber(NumberValue* number_value) = 0;
  virtual void VisitRGBAColor(RGBAColorValue* color_value) = 0;
  virtual void VisitString(StringValue* string_value) = 0;
  virtual void VisitTimeList(TimeListValue* time_list_value) = 0;
  virtual void VisitTimingFunctionList(
      TimingFunctionListValue* timing_function_list_value) = 0;
  virtual void VisitTransformFunctionList(
      TransformFunctionListValue* transform_function_list_value) = 0;
  virtual void VisitURL(URLValue* url_value) = 0;

 protected:
  ~PropertyValueVisitor() {}
};

// A convenience class that forwards all methods to |VisitDefault|, thus one can
// derive from this class, implement only the value types that they care about,
// and handle every other value type generically.
class DefaultingPropertyValueVisitor : public PropertyValueVisitor {
 public:
  void VisitAbsoluteURL(AbsoluteURLValue* url_value) OVERRIDE;
  void VisitConstStringList(
      ConstStringListValue* const_string_list_value) OVERRIDE;
  void VisitFontWeight(FontWeightValue* font_weight_value) OVERRIDE;
  void VisitKeyword(KeywordValue* keyword_value) OVERRIDE;
  void VisitLength(LengthValue* length_value) OVERRIDE;
  void VisitNumber(NumberValue* number_value) OVERRIDE;
  void VisitRGBAColor(RGBAColorValue* color_value) OVERRIDE;
  void VisitString(StringValue* string_value) OVERRIDE;
  void VisitTimeList(TimeListValue* time_list_value) OVERRIDE;
  void VisitTimingFunctionList(
      TimingFunctionListValue* timing_function_list_value) OVERRIDE;
  void VisitTransformFunctionList(
      TransformFunctionListValue* transform_function_list_value) OVERRIDE;
  void VisitURL(URLValue* url_value) OVERRIDE;

 protected:
  virtual void VisitDefault(PropertyValue* property_value) = 0;
};

// A convenience class that implements PropertyValueVisitor with NOTREACHED()
// for each method, thus one can derive from this class, implement only the
// value types that they care about, and then every other value type will
// result in an error.
class NotReachedPropertyValueVisitor : public DefaultingPropertyValueVisitor {
 protected:
  void VisitDefault(PropertyValue* property_value) OVERRIDE;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_PROPERTY_VALUE_VISITOR_H_
