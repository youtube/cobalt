// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_USER_AGENT_STYLE_SHEET_H_
#define COBALT_CSSOM_USER_AGENT_STYLE_SHEET_H_

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_sheet.h"

namespace cobalt {
namespace cssom {

// Uses a given CSSParser to load the user agent style sheet CSS source into
// a parsed CSSStyleSheet which is then returned.
scoped_refptr<CSSStyleSheet> ParseUserAgentStyleSheet(CSSParser* css_parser);

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_USER_AGENT_STYLE_SHEET_H_
