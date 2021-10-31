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

#include "cobalt/cssom/css_declaration_util.h"

#include "base/logging.h"

namespace cobalt {
namespace cssom {

// Append a property declaration to a string.
// When the given string is not empty, this also adds the single space separator
// needed for the serialization of a CSS declaration block.
//   https://www.w3.org/TR/cssom/#serialize-a-css-declaration
void AppendPropertyDeclaration(
    const char* const property_name,
    const scoped_refptr<PropertyValue>& property_value, std::string* output) {
  if (property_value) {
    DCHECK(output);
    DCHECK(property_name);
    if (!output->empty()) {
      output->push_back(' ');
    }
    output->append(property_name);
    output->append(": ");
    output->append(property_value->ToString());
    output->push_back(';');
  }
}

}  // namespace cssom
}  // namespace cobalt
