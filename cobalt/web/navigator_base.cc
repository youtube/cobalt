// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/web/navigator_base.h"

#include <memory>
#include <vector>

#include "base/optional.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/navigator_ua_data.h"
#include "cobalt/web/user_agent_platform_info.h"
#include "cobalt/worker/service_worker_container.h"
#include "starboard/configuration_constants.h"

namespace cobalt {
namespace web {

NavigatorBase::NavigatorBase(script::EnvironmentSettings* settings)
    : environment_settings_(
          base::polymorphic_downcast<web::EnvironmentSettings*>(settings)),
      user_agent_(environment_settings_->context()->GetUserAgent()),
      user_agent_data_(
          new NavigatorUAData(environment_settings_->context()->platform_info(),
                              script_value_factory())),
      language_(environment_settings_->context()->GetPreferredLanguage()),
      service_worker_(new worker::ServiceWorkerContainer(settings)) {}

NavigatorBase::~NavigatorBase() {}

const std::string& NavigatorBase::language() const { return language_; }

script::Sequence<std::string> NavigatorBase::languages() const {
  script::Sequence<std::string> languages;
  languages.push_back(language_);
  return languages;
}

const std::string& NavigatorBase::user_agent() const { return user_agent_; }

const scoped_refptr<NavigatorUAData>& NavigatorBase::user_agent_data() const {
  return user_agent_data_;
}

bool NavigatorBase::on_line() const {
#if SB_API_VERSION >= 13
  return !SbSystemNetworkIsDisconnected();
#else
  return true;
#endif
}

}  // namespace web
}  // namespace cobalt
