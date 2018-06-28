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

#include "cobalt/cssom/user_agent_style_sheet.h"

#include <string>

#include "cobalt/cssom/embedded_resources.h"  // Generated file.

namespace cobalt {
namespace cssom {

scoped_refptr<CSSStyleSheet> ParseUserAgentStyleSheet(CSSParser* css_parser) {
  const char kUserAgentStyleSheetFileName[] = "user_agent_style_sheet.css";

  // Parse the user agent style sheet from the given file that was compiled
  // into a header and included.  We embed it in the binary via C++ header file
  // so that we can avoid the time it would take to perform a disk read
  // when it needs to be created (e.g. when creating a new DOM Document). The
  // cost of doing this is that we end up with the contents of the file in
  // memory at all times, even after it is parsed here.
  GeneratedResourceMap resource_map;
  CSSOMEmbeddedResources::GenerateMap(resource_map);
  FileContents html_css_file_contents =
      resource_map[kUserAgentStyleSheetFileName];

  scoped_refptr<CSSStyleSheet> user_agent_style_sheet =
      css_parser->ParseStyleSheet(
          std::string(
              reinterpret_cast<const char*>(html_css_file_contents.data),
              static_cast<size_t>(html_css_file_contents.size)),
          base::SourceLocation(kUserAgentStyleSheetFileName, 1, 1));
  user_agent_style_sheet->set_origin(kNormalUserAgent);
  user_agent_style_sheet->SetOriginClean(true);
  return user_agent_style_sheet;
}

}  // namespace cssom
}  // namespace cobalt
