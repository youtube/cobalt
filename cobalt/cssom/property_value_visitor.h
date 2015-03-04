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

namespace cobalt {
namespace cssom {

class KeywordValue;
class LengthValue;
class NumberValue;
class RGBAColorValue;
class StringValue;
class TransformListValue;

// Type-safe branching on a class hierarchy of CSS property values,
// implemented after a classical GoF pattern (see
// http://en.wikipedia.org/wiki/Visitor_pattern#Java_example).
class PropertyValueVisitor {
 public:
  virtual void VisitKeyword(KeywordValue* keyword_value) = 0;
  virtual void VisitLength(LengthValue* length_value) = 0;
  virtual void VisitNumber(NumberValue* number_value) = 0;
  virtual void VisitRGBAColor(RGBAColorValue* color_value) = 0;
  virtual void VisitString(StringValue* string_value) = 0;
  virtual void VisitTransformList(TransformListValue* transform_list_value) = 0;

 protected:
  ~PropertyValueVisitor() {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_PROPERTY_VALUE_VISITOR_H_
