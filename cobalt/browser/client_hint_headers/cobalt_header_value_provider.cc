// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/client_hint_headers/cobalt_header_value_provider.h"
#include <iostream>
#include "base/no_destructor.h"

namespace cobalt {
namespace browser {

// static
CobaltHeaderValueProvider* CobaltHeaderValueProvider::GetInstance() {
  static base::NoDestructor<CobaltHeaderValueProvider> instance;
  return instance.get();
}

const HeaderMap& CobaltHeaderValueProvider::GetHeaderValues() const {
  return header_values_;
}

void CobaltHeaderValueProvider::SetHeaderValue(
    const std::string& header_name,
    const std::string& header_value) {
  header_values_[header_name] = header_value;
}

}  // namespace browser
}  // namespace cobalt
