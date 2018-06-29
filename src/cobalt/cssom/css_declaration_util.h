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

#ifndef COBALT_CSSOM_CSS_DECLARATION_UTIL_H_
#define COBALT_CSSOM_CSS_DECLARATION_UTIL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

void AppendPropertyDeclaration(
    const char* const property_name,
    const scoped_refptr<PropertyValue>& property_value, std::string* output);

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CSS_DECLARATION_UTIL_H_
