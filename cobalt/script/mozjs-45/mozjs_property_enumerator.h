// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_MOZJS_45_MOZJS_PROPERTY_ENUMERATOR_H_
#define COBALT_SCRIPT_MOZJS_45_MOZJS_PROPERTY_ENUMERATOR_H_

#include <string>

#include "base/compiler_specific.h"
#include "cobalt/script/property_enumerator.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

class MozjsPropertyEnumerator : public cobalt::script::PropertyEnumerator {
 public:
  MozjsPropertyEnumerator(JSContext* context, JS::AutoIdVector* properties);
  void AddProperty(const std::string& property_name) override;

 private:
  JSContext* context_;
  JS::AutoIdVector* properties_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_PROPERTY_ENUMERATOR_H_
