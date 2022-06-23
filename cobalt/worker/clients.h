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

#ifndef COBALT_WORKER_CLIENTS_H_
#define COBALT_WORKER_CLIENTS_H_

#include <memory>
#include <string>

#include "cobalt/script/environment_settings.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/client_query_options.h"

namespace cobalt {
namespace worker {

class Clients : public script::Wrappable {
 public:
  explicit Clients(script::EnvironmentSettings* settings);

  script::HandlePromiseWrappable Get(const std::string& id);
  script::HandlePromiseSequenceWrappable MatchAll(
      const ClientQueryOptions& options = ClientQueryOptions());
  script::HandlePromiseVoid Claim();

  DEFINE_WRAPPABLE_TYPE(Clients);

 private:
  web::EnvironmentSettings* settings_ = nullptr;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_CLIENTS_H_
