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

#ifndef COBALT_DOM_DOM_PARSER_H_
#define COBALT_DOM_DOM_PARSER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/dom_parser_supported_type.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class Document;
class DOMSettings;
class HTMLElementContext;

// DOMParser can parse XML or HTML source stored in a string into a DOM
// Document.
//   https://www.w3.org/TR/DOM-Parsing/#the-domparser-interface
class DOMParser : public script::Wrappable {
 public:
  explicit DOMParser(script::EnvironmentSettings* environment_settings);
  explicit DOMParser(HTMLElementContext* html_element_context);

  // Web API: DOMParser
  scoped_refptr<Document> ParseFromString(const std::string& str,
                                          DOMParserSupportedType type);

  DEFINE_WRAPPABLE_TYPE(DOMParser);

 private:
  HTMLElementContext* html_element_context_;

  DISALLOW_COPY_AND_ASSIGN(DOMParser);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOM_PARSER_H_
