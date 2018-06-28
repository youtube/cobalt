// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/css.h"

#include "cobalt/base/source_location.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/window.h"

namespace cobalt {
namespace cssom {

bool CSS::Supports(script::EnvironmentSettings* settings,
                   const std::string& property, const std::string& value) {
  if (settings) {
    dom::DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(settings);
    cssom::CSSParser* css_parser =
        dom_settings->window()->html_element_context()->css_parser();
    return css_parser->ParsePropertyValue(
        property, value, base::SourceLocation("[object CSS]", 1, 1));
  }

  return false;
}

}  // namespace cssom
}  // namespace cobalt
