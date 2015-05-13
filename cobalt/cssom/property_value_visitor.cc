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
#include "cobalt/cssom/absolute_url_value.h"
#include "cobalt/cssom/const_string_list_value.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/time_list_value.h"
#include "cobalt/cssom/timing_function_list_value.h"
#include "cobalt/cssom/transform_function_list_value.h"
#include "cobalt/cssom/url_value.h"

namespace cobalt {
namespace cssom {

void DefaultingPropertyValueVisitor::VisitAbsoluteURL(
    AbsoluteURLValue* url_value) {
  VisitDefault(url_value);
}

void DefaultingPropertyValueVisitor::VisitConstStringList(
    ConstStringListValue* const_string_list_value) {
  VisitDefault(const_string_list_value);
}

void DefaultingPropertyValueVisitor::VisitFontWeight(
    FontWeightValue* font_weight_value) {
  VisitDefault(font_weight_value);
}

void DefaultingPropertyValueVisitor::VisitKeyword(KeywordValue* keyword_value) {
  VisitDefault(keyword_value);
}

void DefaultingPropertyValueVisitor::VisitLength(LengthValue* length_value) {
  VisitDefault(length_value);
}

void DefaultingPropertyValueVisitor::VisitNumber(NumberValue* number_value) {
  VisitDefault(number_value);
}


void DefaultingPropertyValueVisitor::VisitPercentage(
    PercentageValue* percentage_value) {
  VisitDefault(percentage_value);
}

void DefaultingPropertyValueVisitor::VisitRGBAColor(
    RGBAColorValue* color_value) {
  VisitDefault(color_value);
}

void DefaultingPropertyValueVisitor::VisitString(StringValue* string_value) {
  VisitDefault(string_value);
}

void DefaultingPropertyValueVisitor::VisitTimeList(
    TimeListValue* time_list_value) {
  VisitDefault(time_list_value);
}

void DefaultingPropertyValueVisitor::VisitTimingFunctionList(
    TimingFunctionListValue* timing_function_list_value) {
  VisitDefault(timing_function_list_value);
}

void DefaultingPropertyValueVisitor::VisitTransformFunctionList(
    TransformFunctionListValue* transform_function_list_value) {
  VisitDefault(transform_function_list_value);
}

void DefaultingPropertyValueVisitor::VisitURL(URLValue* url_value) {
  VisitDefault(url_value);
}

void NotReachedPropertyValueVisitor::VisitDefault(
    PropertyValue* /*property_value*/) {
  NOTREACHED();
}

}  // namespace cssom
}  // namespace cobalt
