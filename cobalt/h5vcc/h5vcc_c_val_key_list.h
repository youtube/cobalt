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

#ifndef COBALT_H5VCC_H5VCC_C_VAL_KEY_LIST_H_
#define COBALT_H5VCC_H5VCC_C_VAL_KEY_LIST_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class H5vccCValKeyList : public script::Wrappable {
 public:
  H5vccCValKeyList();

  base::optional<std::string> Item(uint32 item);
  uint32 length();

  void AppendKey(const std::string& key);

  DEFINE_WRAPPABLE_TYPE(H5vccCValKeyList);

 private:
  std::vector<std::string> keys_;

  DISALLOW_COPY_AND_ASSIGN(H5vccCValKeyList);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_C_VAL_KEY_LIST_H_
