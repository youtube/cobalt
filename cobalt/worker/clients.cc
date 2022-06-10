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

#include "cobalt/worker/clients.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/task_runner.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/web/context.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/client.h"

namespace cobalt {
namespace worker {

Clients::Clients(script::EnvironmentSettings* settings)
    : settings_(
          base::polymorphic_downcast<web::EnvironmentSettings*>(settings)) {}

script::HandlePromiseWrappable Clients::Get(const std::string& id) {
  script::HandlePromiseWrappable promise =
      settings_->context()
          ->global_environment()
          ->script_value_factory()
          ->CreateInterfacePromise<scoped_refptr<Client>>();
  std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference(
      new script::ValuePromiseWrappable::Reference(this, promise));

  // Perform the rest of the steps in a task, because the promise has to be
  // returned before we can safely reject or resolve it.
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Clients::GetTask, base::Unretained(this),
                                std::move(promise_reference), id));
  return promise;
}

script::HandlePromiseSequenceWrappable Clients::MatchAll(
    const ClientQueryOptions& options) {
  auto promise = settings_->context()
                     ->global_environment()
                     ->script_value_factory()
                     ->CreateBasicPromise<script::SequenceWrappable>();

  std::unique_ptr<script::ValuePromiseSequenceWrappable::Reference>
      promise_reference(
          new script::ValuePromiseSequenceWrappable::Reference(this, promise));
  // Perform the rest of the steps in a task, because the promise has to be
  // returned before we can safely reject or resolve it.
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Clients::MatchAllTask, base::Unretained(this),
                                std::move(promise_reference), options));
  return promise;
}

script::HandlePromiseVoid Clients::Claim() {
  auto promise = settings_->context()
                     ->global_environment()
                     ->script_value_factory()
                     ->CreateBasicPromise<void>();
  std::unique_ptr<script::ValuePromiseVoid::Reference> promise_reference(
      new script::ValuePromiseVoid::Reference(this, promise));

  // Perform the rest of the steps in a task, because the promise has to be
  // returned before we can safely reject or resolve it.
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Clients::ClaimTask, base::Unretained(this),
                                std::move(promise_reference)));
  return promise;
}

void Clients::GetTask(
    std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference,
    const std::string& id) {
  DLOG(INFO) << "Clients::Task() id " << id;
  promise_reference->value().Reject(
      new web::DOMException(web::DOMException::kNotSupportedErr));
}

void Clients::MatchAllTask(
    std::unique_ptr<script::ValuePromiseSequenceWrappable::Reference>
        promise_reference,
    const ClientQueryOptions& options) {
  DLOG(INFO) << "Clients::Task() options include_uncontrolled "
             << (options.include_uncontrolled() ? "true" : "false") << " type "
             << options.type();
  promise_reference->value().Reject(
      new web::DOMException(web::DOMException::kNotSupportedErr));
}

void Clients::ClaimTask(
    std::unique_ptr<script::ValuePromiseVoid::Reference> promise_reference) {
  promise_reference->value().Reject(
      new web::DOMException(web::DOMException::kNotSupportedErr));
}


}  // namespace worker
}  // namespace cobalt
