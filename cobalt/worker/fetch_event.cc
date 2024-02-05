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

#include "cobalt/worker/fetch_event.h"

#include <memory>
#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "cobalt/script/v8c/conversion_helpers.h"
#include "cobalt/script/v8c/v8c_value_handle.h"
#include "cobalt/web/cache_utils.h"
#include "cobalt/web/environment_settings_helper.h"

namespace cobalt {
namespace worker {

FetchEvent::FetchEvent(script::EnvironmentSettings* environment_settings,
                       const std::string& type,
                       const FetchEventInit& event_init_dict)
    : FetchEvent(environment_settings, base_token::Token(type), event_init_dict,
                 base::ThreadTaskRunnerHandle::Get(), RespondWithCallback(),
                 ReportLoadTimingInfo()) {}

FetchEvent::FetchEvent(
    script::EnvironmentSettings* environment_settings, base_token::Token type,
    const FetchEventInit& event_init_dict,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    RespondWithCallback respond_with_callback,
    ReportLoadTimingInfo report_load_timing_info)
    : ExtendableEvent(environment_settings, type, event_init_dict),
      environment_settings_(environment_settings),
      callback_task_runner_(callback_task_runner),
      respond_with_callback_(std::move(respond_with_callback)),
      report_load_timing_info_(std::move(report_load_timing_info)) {
  auto script_value_factory =
      web::get_script_value_factory(environment_settings_);
  handled_property_ = std::make_unique<script::ValuePromiseVoid::Reference>(
      this, script_value_factory->CreateBasicPromise<void>());
  request_ = std::make_unique<script::ValueHandleHolder::Reference>(
      this, event_init_dict.request());
  respond_with_done_ = std::make_unique<script::ValuePromiseVoid::Reference>(
      this, script_value_factory->CreateBasicPromise<void>());

  load_timing_info_.request_start = base::TimeTicks::Now();
  load_timing_info_.request_start_time = base::Time::Now();
  load_timing_info_.send_start = base::TimeTicks::Now();
  load_timing_info_.send_end = base::TimeTicks::Now();
  load_timing_info_.service_worker_start_time = base::TimeTicks::Now();
}

base::Optional<v8::Local<v8::Promise>> FetchEvent::GetText(
    v8::Local<v8::Promise> response_promise) {
  callback_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](ReportLoadTimingInfo report_load_timing_info,
                        const net::LoadTimingInfo& load_timing_info) {
                       std::move(report_load_timing_info).Run(load_timing_info);
                     },
                     std::move(report_load_timing_info_), load_timing_info_));
  handled_property_->value().Resolve();
  return web::cache_utils::OptionalPromise(
      web::cache_utils::Call(response_promise->Result(), "text"));
}

void FetchEvent::RespondWithDone() { respond_with_done_->value().Resolve(); }

base::Optional<v8::Local<v8::Promise>> FetchEvent::DoRespondWith(
    v8::Local<v8::Promise> text_promise) {
  auto* isolate = text_promise->GetIsolate();
  auto context = isolate->GetCurrentContext();
  auto body = web::cache_utils::FromV8String(text_promise->GetIsolate(),
                                             text_promise->Result());
  auto callback =
      base::BindOnce(&FetchEvent::RespondWithDone, base::Unretained(this));
  web::get_context(environment_settings_)
      ->network_module()
      ->task_runner()
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
                 RespondWithCallback respond_with_callback, std::string body,
                 base::MessageLoop* loop, base::OnceClosure callback) {
                callback_task_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(
                        [](RespondWithCallback respond_with_callback,
                           std::string body) {
                          std::move(respond_with_callback)
                              .Run(std::make_unique<std::string>(
                                  std::move(body)));
                        },
                        std::move(respond_with_callback), std::move(body)));
                loop->task_runner()->PostTask(FROM_HERE, std::move(callback));
              },
              callback_task_runner_, std::move(respond_with_callback_),
              std::move(body), base::MessageLoop::current(),
              std::move(callback)));
  return respond_with_done_->value().promise();
}

void FetchEvent::RespondWith(
    std::unique_ptr<script::Promise<script::ValueHandle*>>& response,
    script::ExceptionState* exception_state) {
  respond_with_called_ = true;

  auto text_promise = web::cache_utils::Then(
      response->promise(),
      base::BindOnce(&FetchEvent::GetText, base::Unretained(this)));
  if (!text_promise) {
    return;
  }
  auto done_promise = web::cache_utils::Then(
      text_promise.value(),
      base::BindOnce(&FetchEvent::DoRespondWith, base::Unretained(this)));
  if (!done_promise) {
    return;
  }
  auto* isolate = response->promise()->GetIsolate();
  std::unique_ptr<script::Promise<script::ValueHandle*>> wait_promise;
  script::v8c::FromJSValue(isolate, done_promise.value(), 0, exception_state,
                           &wait_promise);
  WaitUntil(environment_settings_, response, exception_state);
  WaitUntil(environment_settings_, wait_promise, exception_state);
}

script::HandlePromiseVoid FetchEvent::handled() {
  return script::HandlePromiseVoid(handled_property_->referenced_value());
}

}  // namespace worker
}  // namespace cobalt
