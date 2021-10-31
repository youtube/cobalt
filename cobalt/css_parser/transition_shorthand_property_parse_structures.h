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

#ifndef COBALT_CSS_PARSER_TRANSITION_SHORTHAND_PROPERTY_PARSE_STRUCTURES_H_
#define COBALT_CSS_PARSER_TRANSITION_SHORTHAND_PROPERTY_PARSE_STRUCTURES_H_

#include <memory>

#include "base/optional.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/cssom/property_key_list_value.h"
#include "cobalt/cssom/time_list_value.h"
#include "cobalt/cssom/timing_function_list_value.h"

namespace cobalt {
namespace css_parser {

// This file contains a collection of structures used as synthetic values for
// different parser reductions related to the 'transition' shorthand property.

// The SingleTransition structure is the type used for the result of the
// single_transition parser reduction.
struct SingleTransitionShorthand {
  SingleTransitionShorthand() : error(false) {}

  void ReplaceNullWithInitialValues();

  base::Optional<cssom::PropertyKey> property;
  base::Optional<base::TimeDelta> duration;
  scoped_refptr<cssom::TimingFunction> timing_function;
  base::Optional<base::TimeDelta> delay;

  bool error;
};

// As we are parsing a transition, maintain builders for all of its components.
struct TransitionShorthandBuilder {
  TransitionShorthandBuilder()
      : property_list_builder(new cssom::PropertyKeyListValue::Builder()),
        duration_list_builder(new cssom::TimeListValue::Builder()),
        timing_function_list_builder(
            new cssom::TimingFunctionListValue::Builder()),
        delay_list_builder(new cssom::TimeListValue::Builder()) {}

  std::unique_ptr<cssom::PropertyKeyListValue::Builder> property_list_builder;
  std::unique_ptr<cssom::TimeListValue::Builder> duration_list_builder;
  std::unique_ptr<cssom::TimingFunctionListValue::Builder>
      timing_function_list_builder;
  std::unique_ptr<cssom::TimeListValue::Builder> delay_list_builder;
};

struct TransitionShorthand {
  scoped_refptr<cssom::PropertyValue> property_list;
  scoped_refptr<cssom::PropertyValue> duration_list;
  scoped_refptr<cssom::PropertyValue> timing_function_list;
  scoped_refptr<cssom::PropertyValue> delay_list;
};

}  // namespace css_parser
}  // namespace cobalt

#endif  // COBALT_CSS_PARSER_TRANSITION_SHORTHAND_PROPERTY_PARSE_STRUCTURES_H_
