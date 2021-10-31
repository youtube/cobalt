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

#include "cobalt/cssom/property_value_visitor.h"

#include "base/logging.h"
#include "cobalt/cssom/absolute_url_value.h"
#include "cobalt/cssom/calc_value.h"
#include "cobalt/cssom/filter_function_list_value.h"
#include "cobalt/cssom/font_style_value.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/integer_value.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/linear_gradient_value.h"
#include "cobalt/cssom/local_src_value.h"
#include "cobalt/cssom/media_feature_keyword_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_key_list_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/radial_gradient_value.h"
#include "cobalt/cssom/ratio_value.h"
#include "cobalt/cssom/resolution_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/shadow_value.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/time_list_value.h"
#include "cobalt/cssom/timing_function_list_value.h"
#include "cobalt/cssom/transform_property_value.h"
#include "cobalt/cssom/unicode_range_value.h"
#include "cobalt/cssom/url_src_value.h"
#include "cobalt/cssom/url_value.h"

namespace cobalt {
namespace cssom {

void DefaultingPropertyValueVisitor::VisitAbsoluteURL(
    AbsoluteURLValue* url_value) {
  VisitDefault(url_value);
}

void DefaultingPropertyValueVisitor::VisitCalc(CalcValue* calc_value) {
  VisitDefault(calc_value);
}

void DefaultingPropertyValueVisitor::VisitFilterFunctionList(
    FilterFunctionListValue* filter_function_list_value) {
  VisitDefault(filter_function_list_value);
}

void DefaultingPropertyValueVisitor::VisitFontStyle(
    FontStyleValue* font_style_value) {
  VisitDefault(font_style_value);
}

void DefaultingPropertyValueVisitor::VisitFontWeight(
    FontWeightValue* font_weight_value) {
  VisitDefault(font_weight_value);
}

void DefaultingPropertyValueVisitor::VisitInteger(
    IntegerValue* integer_value) {
  VisitDefault(integer_value);
}

void DefaultingPropertyValueVisitor::VisitKeyword(KeywordValue* keyword_value) {
  VisitDefault(keyword_value);
}

void DefaultingPropertyValueVisitor::VisitLength(LengthValue* length_value) {
  VisitDefault(length_value);
}

void DefaultingPropertyValueVisitor::VisitLinearGradient(
    LinearGradientValue* linear_gradient_value) {
  VisitDefault(linear_gradient_value);
}

void DefaultingPropertyValueVisitor::VisitLocalSrc(
    LocalSrcValue* local_src_value) {
  VisitDefault(local_src_value);
}

void DefaultingPropertyValueVisitor::VisitMediaFeatureKeywordValue(
    MediaFeatureKeywordValue* media_feature_keyword_value) {
  VisitDefault(media_feature_keyword_value);
}

void DefaultingPropertyValueVisitor::VisitNumber(NumberValue* number_value) {
  VisitDefault(number_value);
}

void DefaultingPropertyValueVisitor::VisitPercentage(
    PercentageValue* percentage_value) {
  VisitDefault(percentage_value);
}

void DefaultingPropertyValueVisitor::VisitPropertyKeyList(
    PropertyKeyListValue* property_key_list_value) {
  VisitDefault(property_key_list_value);
}

void DefaultingPropertyValueVisitor::VisitPropertyList(
    PropertyListValue* property_list_value) {
  VisitDefault(property_list_value);
}

void DefaultingPropertyValueVisitor::VisitRadialGradient(
    RadialGradientValue* radial_gradient_value) {
  VisitDefault(radial_gradient_value);
}

void DefaultingPropertyValueVisitor::VisitRatio(RatioValue* ratio_value) {
  VisitDefault(ratio_value);
}

void DefaultingPropertyValueVisitor::VisitResolution(
    ResolutionValue* resolution_value) {
  VisitDefault(resolution_value);
}

void DefaultingPropertyValueVisitor::VisitRGBAColor(
    RGBAColorValue* color_value) {
  VisitDefault(color_value);
}

void DefaultingPropertyValueVisitor::VisitShadow(ShadowValue* shadow_value) {
  VisitDefault(shadow_value);
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

void DefaultingPropertyValueVisitor::VisitTransformPropertyValue(
    TransformPropertyValue* transform_property_value) {
  VisitDefault(transform_property_value);
}

void DefaultingPropertyValueVisitor::VisitUnicodeRange(
    UnicodeRangeValue* unicode_range_value) {
  VisitDefault(unicode_range_value);
}

void DefaultingPropertyValueVisitor::VisitURL(URLValue* url_value) {
  VisitDefault(url_value);
}

void DefaultingPropertyValueVisitor::VisitUrlSrc(UrlSrcValue* url_src_value) {
  VisitDefault(url_src_value);
}

void NotReachedPropertyValueVisitor::VisitDefault(
    PropertyValue* property_value) {
  DLOG(ERROR) << "Unsupported property value: " << property_value->ToString();
  NOTREACHED();
}

}  // namespace cssom
}  // namespace cobalt
