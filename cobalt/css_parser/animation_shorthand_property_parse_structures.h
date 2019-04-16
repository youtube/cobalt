// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSS_PARSER_ANIMATION_SHORTHAND_PROPERTY_PARSE_STRUCTURES_H_
#define COBALT_CSS_PARSER_ANIMATION_SHORTHAND_PROPERTY_PARSE_STRUCTURES_H_

#include <memory>

#include "base/optional.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/time_list_value.h"
#include "cobalt/cssom/timing_function_list_value.h"

namespace cobalt {
namespace css_parser {

// This file contains a collection of structures used as synthetic values for
// different parser reductions related to the 'animation' shorthand property.

// The SingleAnimation structure is the type used for the result of the
// single_animation parser reduction.
struct SingleAnimationShorthand {
  SingleAnimationShorthand() : error(false) {}

  void ReplaceNullWithInitialValues();

  base::Optional<base::TimeDelta> delay;
  scoped_refptr<cssom::PropertyValue> direction;
  base::Optional<base::TimeDelta> duration;
  scoped_refptr<cssom::PropertyValue> fill_mode;
  scoped_refptr<cssom::PropertyValue> iteration_count;
  scoped_refptr<cssom::PropertyValue> name;
  scoped_refptr<cssom::TimingFunction> timing_function;

  bool error;
};

// As we are parsing a animation, maintain builders for all of its components.
struct AnimationShorthandBuilder {
  AnimationShorthandBuilder()
      : delay_list_builder(new cssom::TimeListValue::Builder()),
        direction_list_builder(new cssom::PropertyListValue::Builder()),
        duration_list_builder(new cssom::TimeListValue::Builder()),
        fill_mode_list_builder(new cssom::PropertyListValue::Builder()),
        iteration_count_list_builder(new cssom::PropertyListValue::Builder()),
        name_list_builder(new cssom::PropertyListValue::Builder()),
        timing_function_list_builder(
            new cssom::TimingFunctionListValue::Builder()) {}

  // The AnimationShorthandBuilder is a struct-of-arrays, so the animation
  // builder is empty if any of its sub-builder arrays are empty.
  bool empty() const { return delay_list_builder->empty(); }

  std::unique_ptr<cssom::TimeListValue::Builder> delay_list_builder;
  std::unique_ptr<cssom::PropertyListValue::Builder> direction_list_builder;
  std::unique_ptr<cssom::TimeListValue::Builder> duration_list_builder;
  std::unique_ptr<cssom::PropertyListValue::Builder> fill_mode_list_builder;
  std::unique_ptr<cssom::PropertyListValue::Builder>
      iteration_count_list_builder;
  std::unique_ptr<cssom::PropertyListValue::Builder> name_list_builder;
  std::unique_ptr<cssom::TimingFunctionListValue::Builder>
      timing_function_list_builder;
};

struct AnimationShorthand {
  scoped_refptr<cssom::PropertyValue> delay_list;
  scoped_refptr<cssom::PropertyValue> direction_list;
  scoped_refptr<cssom::PropertyValue> duration_list;
  scoped_refptr<cssom::PropertyValue> fill_mode_list;
  scoped_refptr<cssom::PropertyValue> iteration_count_list;
  scoped_refptr<cssom::PropertyValue> name_list;
  scoped_refptr<cssom::PropertyValue> timing_function_list;
};

}  // namespace css_parser
}  // namespace cobalt

#endif  // COBALT_CSS_PARSER_ANIMATION_SHORTHAND_PROPERTY_PARSE_STRUCTURES_H_
