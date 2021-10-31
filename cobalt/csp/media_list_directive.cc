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
#include "cobalt/csp/media_list_directive.h"

namespace cobalt {
namespace csp {

MediaListDirective::MediaListDirective(const std::string& name,
                                       const std::string& value,
                                       ContentSecurityPolicy* policy)
    : Directive(name, value, policy) {
  Parse(base::StringPiece(value));
}

bool MediaListDirective::Allows(const std::string& type) const {
  return plugin_types_.find(type) != plugin_types_.end();
}

void MediaListDirective::Parse(const base::StringPiece& str) {
  const char* begin = str.begin();
  const char* end = str.end();
  const char* position = begin;

  // 'plugin-types ____;' OR 'plugin-types;'
  if (position == end) {
    policy()->ReportInvalidPluginTypes(std::string());
    return;
  }

  while (position < end) {
    // _____ OR _____mime1/mime1
    // ^        ^
    SkipWhile<base::IsAsciiWhitespace>(&position, end);
    if (position == end) {
      return;
    }

    // mime1/mime1 mime2/mime2
    // ^
    begin = position;
    if (!SkipExactly<IsMediaTypeCharacter>(&position, end)) {
      SkipWhile<IsNotAsciiWhitespace>(&position, end);
      policy()->ReportInvalidPluginTypes(ToString(begin, position));
      continue;
    }
    SkipWhile<IsMediaTypeCharacter>(&position, end);

    // mime1/mime1 mime2/mime2
    //      ^
    if (!SkipExactly(&position, end, '/')) {
      SkipWhile<IsNotAsciiWhitespace>(&position, end);
      policy()->ReportInvalidPluginTypes(ToString(begin, position));
      continue;
    }

    // mime1/mime1 mime2/mime2
    //       ^
    if (!SkipExactly<IsMediaTypeCharacter>(&position, end)) {
      SkipWhile<IsNotAsciiWhitespace>(&position, end);
      policy()->ReportInvalidPluginTypes(ToString(begin, position));
      continue;
    }
    SkipWhile<IsMediaTypeCharacter>(&position, end);

    // mime1/mime1 mime2/mime2 OR mime1/mime1  OR mime1/mime1/error
    //            ^                          ^               ^
    if (position < end && IsNotAsciiWhitespace(*position)) {
      SkipWhile<IsNotAsciiWhitespace>(&position, end);
      policy()->ReportInvalidPluginTypes(ToString(begin, position));
      continue;
    }
    plugin_types_.insert(ToString(begin, position));

    DCHECK(position == end || base::IsAsciiWhitespace(*position));
  }
}

}  // namespace csp
}  // namespace cobalt
