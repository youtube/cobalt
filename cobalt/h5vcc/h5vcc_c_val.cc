/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/h5vcc/h5vcc_c_val.h"

#include <set>

#include "cobalt/base/console_values.h"

namespace cobalt {
namespace h5vcc {

H5vccCVal::H5vccCVal() {}

scoped_refptr<H5vccCValKeyList> H5vccCVal::Keys() {
  scoped_refptr<H5vccCValKeyList> key_list(new H5vccCValKeyList);
  typedef std::set<std::string> CValKeySet;
  CValKeySet key_set =
      base::ConsoleValueManager::GetInstance()->GetOrderedCValNames();
  for (CValKeySet::iterator key_iter = key_set.begin();
       key_iter != key_set.end(); ++key_iter) {
    key_list->AppendKey(*key_iter);
  }
  return key_list;
}

base::optional<std::string> H5vccCVal::GetValue(const std::string& name) {
  return base::ConsoleValueManager::GetInstance()->GetValueAsString(name);
}

}  // namespace h5vcc
}  // namespace cobalt
