// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/webapi_extension.h"

#include "base/compiler_specific.h"
#include "cobalt/script/global_environment.h"

#if SB_API_VERSION < 12

namespace cobalt {
namespace browser {

base::Optional<std::string> GetWebAPIExtensionObjectPropertyName() {
  return base::nullopt;
}

scoped_refptr<script::Wrappable> CreateWebAPIExtensionObject(
    const scoped_refptr<dom::Window>& window,
    script::GlobalEnvironment* global_environment) {

  // We should never get called if GetWindowExtensionObjectName() above returns
  // base::nullopt.
  NOTREACHED();

  return scoped_refptr<script::Wrappable>();
}

}  // namespace browser
}  // namespace cobalt

#endif  // SB_API_VERSION < 12
