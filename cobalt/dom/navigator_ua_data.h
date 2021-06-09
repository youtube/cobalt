// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_NAVIGATOR_UA_DATA_H_
#define COBALT_DOM_NAVIGATOR_UA_DATA_H_

#include <string>

#include "cobalt/dom/cobalt_ua_data_values_interface.h"
#include "cobalt/dom/navigator_ua_brand_version.h"
#include "cobalt/dom/ua_low_entropy_json.h"
#include "cobalt/dom/user_agent_platform_info.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The NavigatorUAData object holds the User-Agent Client Hints information.
// https://wicg.github.io/ua-client-hints/#navigatoruadata
class NavigatorUAData : public script::Wrappable {
 public:
  using InterfacePromise = script::Promise<scoped_refptr<script::Wrappable>>;

  NavigatorUAData(UserAgentPlatformInfo* platform_info,
                  script::ScriptValueFactory* script_value_factory);

  script::Sequence<NavigatorUABrandVersion> brands() const { return brands_; }

  bool mobile() const { return mobile_; }

  std::string platform() const { return platform_; }

  script::Handle<InterfacePromise> GetHighEntropyValues(
      script::Sequence<std::string> hints);

  UALowEntropyJSON ToJSON() { return low_entropy_json_; }

  DEFINE_WRAPPABLE_TYPE(NavigatorUAData);

 private:
  ~NavigatorUAData() override {}

  script::Sequence<NavigatorUABrandVersion> brands_;
  bool mobile_;
  std::string platform_;
  CobaltUADataValues all_high_entropy_values_;
  UALowEntropyJSON low_entropy_json_;
  script::ScriptValueFactory* script_value_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorUAData);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_NAVIGATOR_UA_DATA_H_
