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

#include "cobalt/cssom/property_value_visitor.h"

#include "base/logging.h"

namespace cobalt {
namespace cssom {

void NotReachedPropertyValueVisitor::VisitAbsoluteURL(
    AbsoluteURLValue* /*url_value*/) {
  NOTREACHED();
}

void NotReachedPropertyValueVisitor::VisitConstStringList(
    ConstStringListValue* /*const_string_list_value*/) {
  NOTREACHED();
}

void NotReachedPropertyValueVisitor::VisitFontWeight(
    FontWeightValue* /*font_weight_value*/) {
  NOTREACHED();
}

void NotReachedPropertyValueVisitor::VisitKeyword(
    KeywordValue* /*keyword_value*/) {
  NOTREACHED();
}

void NotReachedPropertyValueVisitor::VisitLength(
    LengthValue* /*length_value*/) {
  NOTREACHED();
}

void NotReachedPropertyValueVisitor::VisitNumber(
    NumberValue* /*number_value*/) {
  NOTREACHED();
}

void NotReachedPropertyValueVisitor::VisitRGBAColor(
    RGBAColorValue* /*color_value*/) {
  NOTREACHED();
}

void NotReachedPropertyValueVisitor::VisitString(
    StringValue* /*string_value*/) {
  NOTREACHED();
}

void NotReachedPropertyValueVisitor::VisitTimeList(
    TimeListValue* /*time_list_value*/) {
  NOTREACHED();
}

void NotReachedPropertyValueVisitor::VisitTransformFunctionList(
    TransformFunctionListValue* /*transform_list_value*/) {
  NOTREACHED();
}

void NotReachedPropertyValueVisitor::VisitURL(URLValue* /*url_value*/) {
  NOTREACHED();
}

}  // namespace cssom
}  // namespace cobalt
