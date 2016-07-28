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

#ifndef COBALT_H5VCC_H5VCC_C_VAL_H_
#define COBALT_H5VCC_H5VCC_C_VAL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/h5vcc/h5vcc_c_val_key_list.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class H5vccCVal : public script::Wrappable {
 public:
  H5vccCVal();

  scoped_refptr<H5vccCValKeyList> Keys();
  base::optional<std::string> GetValue(const std::string& name);

  DEFINE_WRAPPABLE_TYPE(H5vccCVal);

 private:
  DISALLOW_COPY_AND_ASSIGN(H5vccCVal);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_C_VAL_H_
