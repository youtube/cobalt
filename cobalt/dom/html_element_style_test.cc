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

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "cobalt/bindings/testing/utils.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/screen.h"
#include "cobalt/dom/testing/test_with_javascript.h"
#include "cobalt/network_bridge/net_poster.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/web/error_event.h"
#include "cobalt/web/message_event.h"
#include "cobalt/web/testing/mock_event_listener.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

using script::testing::FakeScriptValue;

namespace {
std::string GetStyleName(::testing::TestParamInfo<const char*> info) {
  return std::string(info.param);
}

std::string CamelCaseToPropertyName(const char* camel_case) {
  std::string snake;
  for (const char* input = camel_case; *input; ++input) {
    if (*input >= 'A' && *input <= 'Z') {
      snake.push_back('-');
      snake.push_back(std::tolower(*input));
    } else {
      snake.push_back(*input);
    }
  }
  return snake;
}

const char* longhand_styles[] = {"alignContent",
                                 "alignItems",
                                 "alignSelf",
                                 "animationDelay",
                                 "animationDirection",
                                 "animationDuration",
                                 "animationFillMode",
                                 "animationIterationCount",
                                 "animationName",
                                 "animationTimingFunction",
                                 "backgroundColor",
                                 "backgroundImage",
                                 "backgroundPosition",
                                 "backgroundRepeat",
                                 "backgroundSize",
                                 "borderBottomColor",
                                 "borderBottomLeftRadius",
                                 "borderBottomRightRadius",
                                 "borderBottomStyle",
                                 "borderBottomWidth",
                                 "borderLeftColor",
                                 "borderLeftStyle",
                                 "borderLeftWidth",
                                 "borderRightColor",
                                 "borderRightStyle",
                                 "borderRightWidth",
                                 "borderTopColor",
                                 "borderTopLeftRadius",
                                 "borderTopRightRadius",
                                 "borderTopStyle",
                                 "borderTopWidth",
                                 "bottom",
                                 "boxShadow",
                                 "color",
                                 "content",
                                 "display",
                                 "filter",
                                 "flexBasis",
                                 "flexDirection",
                                 "flexGrow",
                                 "flexShrink",
                                 "flexWrap",
                                 "fontFamily",
                                 "fontSize",
                                 "fontStyle",
                                 "fontWeight",
                                 "height",
                                 "justifyContent",
                                 "left",
                                 "lineHeight",
                                 "marginBottom",
                                 "marginLeft",
                                 "marginRight",
                                 "marginTop",
                                 "maxHeight",
                                 "maxWidth",
                                 "minHeight",
                                 "minWidth",
                                 "opacity",
                                 "order",
                                 "outlineColor",
                                 "outlineStyle",
                                 "outlineWidth",
                                 "overflow",
                                 "overflowWrap",
                                 "paddingBottom",
                                 "paddingLeft",
                                 "paddingRight",
                                 "paddingTop",
                                 "pointerEvents",
                                 "position",
                                 "right",
                                 "textAlign",
                                 "textDecorationColor",
                                 "textDecorationLine",
                                 "textIndent",
                                 "textOverflow",
                                 "textShadow",
                                 "textTransform",
                                 "top",
                                 "transform",
                                 "transformOrigin",
                                 "transitionDelay",
                                 "transitionDuration",
                                 "transitionProperty",
                                 "transitionTimingFunction",
                                 "verticalAlign",
                                 "visibility",
                                 "whiteSpace",
                                 "width",
                                 "zIndex"};

class HTMLElementStyleTestLongHandStyles
    : public testing::TestWithJavaScriptBase,
      public ::testing::TestWithParam<const char*> {};
}  // namespace

TEST_P(HTMLElementStyleTestLongHandStyles, SetStyle) {
  const char* style_attribute_name = GetParam();
  std::string property_name = CamelCaseToPropertyName(style_attribute_name);
  std::string script = base::StringPrintf(
      R"(
    var style_name = '%s';
    var result = style_name;
    var div = document.createElement('div');
    div.style.%s = 'initial';
    if (div.style.%s != 'initial') { result = 'Could not be set to \'initial\': \'' + div.style.%s + '\''; }
    if (div.style.cssText != '%s: initial;') { result = 'cssText is not set to \'initial\': cssText \'' + div.style.cssText + '\''; }
    div.style.%s = 'inherit';
    if (div.style.%s != 'inherit') { result = 'Could not be set to \'inherit\': \'' + div.style.%s + '\''; }
    if (div.style.cssText != '%s: inherit;') { result = 'cssText is not set to \'inherit\': cssText \'' + div.style.cssText + '\''; }
    result
  )",
      style_attribute_name, style_attribute_name, style_attribute_name,
      style_attribute_name, property_name.c_str(), style_attribute_name,
      style_attribute_name, style_attribute_name, property_name.c_str());
  std::string result;
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_EQ(style_attribute_name, result) << script;
}

INSTANTIATE_TEST_CASE_P(HTMLElementStyleTest,
                        HTMLElementStyleTestLongHandStyles,
                        ::testing::ValuesIn(longhand_styles), GetStyleName);

namespace {
const char* shorthand_simple_styles[] = {
    "animation",   "background", "border",         "borderBottom", "borderLeft",
    "borderRight", "borderTop",  "flex",           "font",         "margin",
    "outline",     "padding",    "textDecoration", "transition"};

class HTMLElementStyleTestShortHandStyles
    : public testing::TestWithJavaScriptBase,
      public ::testing::TestWithParam<const char*> {};
}  // namespace

TEST_P(HTMLElementStyleTestShortHandStyles, SetStyle) {
  const char* style_attribute_name = GetParam();
  std::string property_name = CamelCaseToPropertyName(style_attribute_name);
  std::string script = base::StringPrintf(
      R"(
    var style_name = '%s';
    var result = style_name;
    var div = document.createElement('div');
    div.style.%s = 'initial';
    if (!div.style.cssText.includes('initial')) { result = 'cssText does not contain \'initial\': cssText \'' + div.style.cssText + '\''; }
    if (!div.style.cssText.includes('%s')) { result = 'cssText does not contain \'%s\': cssText \'' + div.style.cssText + '\''; }
    div.style.%s = 'inherit';
    if (!div.style.cssText.includes('inherit')) { result = 'cssText does not contain \'inherit\': cssText \'' + div.style.cssText + '\''; }
    if (!div.style.cssText.includes('%s')) { result = 'cssText does not contain \'%s\': cssText \'' + div.style.cssText + '\''; }
    result
  )",
      style_attribute_name, style_attribute_name, property_name.c_str(),
      property_name.c_str(), style_attribute_name, property_name.c_str(),
      property_name.c_str());
  std::string result;
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_EQ(style_attribute_name, result) << script;
}

INSTANTIATE_TEST_CASE_P(HTMLElementStyleTest,
                        HTMLElementStyleTestShortHandStyles,
                        ::testing::ValuesIn(shorthand_simple_styles),
                        GetStyleName);

namespace {
const char* renamed_styles[] = {
    "borderColor",   // Shorthand for border[Top|Right|Bottom|Left]Color.
    "borderRadius",  // Shorthand for
                     // border[TopLeft|TopRight|BottomRight|BottomLeft]Radius.
    "borderStyle",   // Shorthand for border[Top|Right|Bottom|Left]Style.
    "borderWidth",   // Shorthand for border[Top|Right|Bottom|Left]Width.
    "flexFlow",      // Shorthand for flexDirection and flexWrap.
    "wordWrap",      // Alias for overflowWrap.
};

class HTMLElementStyleTestRenamedStyles
    : public testing::TestWithJavaScriptBase,
      public ::testing::TestWithParam<const char*> {};
}  // namespace

TEST_P(HTMLElementStyleTestRenamedStyles, SetStyle) {
  const char* style_attribute_name = GetParam();
  std::string script = base::StringPrintf(
      R"(
    var style_name = '%s';
    var result = style_name;
    var div = document.createElement('div');
    div.style.%s = 'initial';
    if (!div.style.cssText.includes('initial')) { result = 'cssText does not contain \'initial\': cssText \'' + div.style.cssText + '\''; }
    div.style.%s = 'inherit';
    if (!div.style.cssText.includes('inherit')) { result = 'cssText does not contain \'inherit\': cssText \'' + div.style.cssText + '\''; }
    result
  )",
      style_attribute_name, style_attribute_name, style_attribute_name);
  std::string result;
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_EQ(style_attribute_name, result) << script;
}

INSTANTIATE_TEST_CASE_P(HTMLElementStyleTest, HTMLElementStyleTestRenamedStyles,
                        ::testing::ValuesIn(renamed_styles), GetStyleName);


}  // namespace dom
}  // namespace cobalt
