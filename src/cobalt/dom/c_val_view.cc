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

#include <set>

#include "cobalt/base/c_val.h"

namespace cobalt {
namespace dom {

CValView::CValView() {}

scoped_refptr<CValKeyList> CValView::Keys() {
  scoped_refptr<CValKeyList> key_list(new CValKeyList);
  typedef std::set<std::string> CValKeySet;
  CValKeySet key_set = base::CValManager::GetInstance()->GetOrderedCValNames();
  for (CValKeySet::iterator key_iter = key_set.begin();
       key_iter != key_set.end(); ++key_iter) {
    key_list->AppendKey(*key_iter);
  }
  return key_list;
}

base::optional<std::string> CValView::GetValue(const std::string& name) {
  return base::CValManager::GetInstance()->GetValueAsString(name);
}

std::string CValView::GetPrettyValue(const std::string& name) {
  base::optional<std::string> result =
      base::CValManager::GetInstance()->GetValueAsPrettyString(name);
  return result ? *result : "<undefined>";
}

}  // namespace dom
}  // namespace cobalt
