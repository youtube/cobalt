// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/c_val_view.h"

#include <algorithm>

#include "cobalt/base/c_val.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"

namespace cobalt {
namespace dom {

CValView::CValView() {}

script::Sequence<std::string> CValView::Keys() {
  auto key_set = base::CValManager::GetInstance()->GetOrderedCValNames();
  script::Sequence<std::string> key_list;
  std::copy(key_set.begin(), key_set.end(), std::back_inserter(key_list));
  return key_list;
}

base::Optional<std::string> CValView::GetValue(const std::string& name) {
  return base::CValManager::GetInstance()->GetValueAsString(name);
}

std::string CValView::GetPrettyValue(const std::string& name) {
  base::Optional<std::string> result =
      base::CValManager::GetInstance()->GetValueAsPrettyString(name);
  return result ? *result : "<undefined>";
}

}  // namespace dom
}  // namespace cobalt
