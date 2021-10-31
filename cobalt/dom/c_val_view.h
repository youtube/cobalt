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

#ifndef COBALT_DOM_C_VAL_VIEW_H_
#define COBALT_DOM_C_VAL_VIEW_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/script/sequence.h"

namespace cobalt {
namespace dom {

class CValView : public script::Wrappable {
 public:
  CValView();

  script::Sequence<std::string> Keys();
  base::Optional<std::string> GetValue(const std::string& name);
  std::string GetPrettyValue(const std::string& name);

  DEFINE_WRAPPABLE_TYPE(CValView);

 private:
  DISALLOW_COPY_AND_ASSIGN(CValView);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_C_VAL_VIEW_H_
