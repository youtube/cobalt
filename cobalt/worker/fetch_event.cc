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

#include "cobalt/script/v8c/conversion_helpers.h"
#include "cobalt/script/v8c/v8c_value_handle.h"
#include "cobalt/web/cache_utils.h"
#include "cobalt/web/environment_settings_helper.h"

namespace cobalt {
namespace worker {

FetchEvent::FetchEvent(script::EnvironmentSettings* environment_settings,
                       const std::string& type,
                       const FetchEventInit& event_init_dict)
    : FetchEvent(
          environment_settings, base::Token(type), event_init_dict,
          /*respond_with_callback=*/std::make_unique<RespondWithCallback>(),
          /*report_load_timing_info=*/
          std::make_unique<ReportLoadTimingInfo>()) {}

FetchEvent::FetchEvent(
    script::EnvironmentSettings* environment_settings, base::Token type,
    const FetchEventInit& event_init_dict,
    std::unique_ptr<RespondWithCallback> respond_with_callback,
    std::unique_ptr<ReportLoadTimingInfo> report_load_timing_info)
    : ExtendableEvent(type, event_init_dict),
      respond_with_callback_(std::move(respond_with_callback)),
      report_load_timing_info_(std::move(report_load_timing_info)) {
  auto script_value_factory =
      web::get_script_value_factory(environment_settings);
  handled_property_ = std::make_unique<script::ValuePromiseVoid::Reference>(
      this, script_value_factory->CreateBasicPromise<void>());
  request_ = std::make_unique<script::ValueHandleHolder::Reference>(
      this, event_init_dict.request());

  load_timing_info_.request_start = base::TimeTicks::Now();
  load_timing_info_.request_start_time = base::Time::Now();
  load_timing_info_.send_start = base::TimeTicks::Now();
  load_timing_info_.send_end = base::TimeTicks::Now();
  load_timing_info_.service_worker_start_time = base::TimeTicks::Now();
}

void FetchEvent::RespondWith(
    script::EnvironmentSettings* environment_settings,
    std::unique_ptr<script::Promise<script::ValueHandle*>>& response) {
  respond_with_called_ = true;

  // TODO: call |WaitUntil()|.
  v8::Local<v8::Promise> v8_response = response->promise();
  auto* global_environment = web::get_global_environment(environment_settings);
  auto* isolate = global_environment->isolate();
  auto context = isolate->GetCurrentContext();
  auto data = v8::Object::New(isolate);
  web::cache_utils::SetExternal(context, data, "environment_settings",
                                environment_settings);
  web::cache_utils::SetOwnedExternal(context, data, "respond_with_callback",
                                     std::move(respond_with_callback_));
  web::cache_utils::SetOwnedExternal(context, data, "report_load_timing_info",
                                     std::move(report_load_timing_info_));
  web::cache_utils::SetExternal(context, data, "load_timing_info",
                                &load_timing_info_);
  web::cache_utils::SetExternal(context, data, "handled",
                                handled_property_.get());
  auto result = v8_response->Then(
      context,
      v8::Function::New(
          context,
          [](const v8::FunctionCallbackInfo<v8::Value>& info) {
            auto* isolate = info.GetIsolate();
            auto context = info.GetIsolate()->GetCurrentContext();
            v8::Local<v8::Value> text_result;
            if (!web::cache_utils::TryCall(context, /*object=*/info[0], "text")
                     .ToLocal(&text_result)) {
              auto respond_with_callback =
                  web::cache_utils::GetOwnedExternal<RespondWithCallback>(
                      context, info.Data(), "respond_with_callback");
              std::move(*respond_with_callback)
                  .Run(std::make_unique<std::string>());
              return;
            }
            auto* load_timing_info =
                web::cache_utils::GetExternal<net::LoadTimingInfo>(
                    context, info.Data(), "load_timing_info");
            load_timing_info->receive_headers_end = base::TimeTicks::Now();
            auto result = text_result.As<v8::Promise>()->Then(
                context,
                v8::Function::New(
                    context,
                    [](const v8::FunctionCallbackInfo<v8::Value>& info) {
                      auto* isolate = info.GetIsolate();
                      auto context = info.GetIsolate()->GetCurrentContext();
                      auto* handled = web::cache_utils::GetExternal<
                          script::ValuePromiseVoid::Reference>(
                          context, info.Data(), "handled");
                      handled->value().Resolve();
                      auto respond_with_callback =
                          web::cache_utils::GetOwnedExternal<
                              RespondWithCallback>(context, info.Data(),
                                                   "respond_with_callback");
                      auto body = std::make_unique<std::string>();
                      FromJSValue(isolate, info[0],
                                  script::v8c::kNoConversionFlags, nullptr,
                                  body.get());
                      auto* load_timing_info =
                          web::cache_utils::GetExternal<net::LoadTimingInfo>(
                              context, info.Data(), "load_timing_info");
                      auto report_load_timing_info =
                          web::cache_utils::GetOwnedExternal<
                              ReportLoadTimingInfo>(context, info.Data(),
                                                    "report_load_timing_info");
                      std::move(*report_load_timing_info)
                          .Run(*load_timing_info);
                      auto* environment_settings =
                          web::cache_utils::GetExternal<
                              script::EnvironmentSettings>(
                              context, info.Data(), "environment_settings");
                      web::get_context(environment_settings)
                          ->network_module()
                          ->task_runner()
                          ->PostTask(
                              FROM_HERE,
                              base::BindOnce(
                                  [](std::unique_ptr<base::OnceCallback<void(
                                         std::unique_ptr<std::string>)>>
                                         respond_with_callback,
                                     std::unique_ptr<std::string> body) {
                                    std::move(*respond_with_callback)
                                        .Run(std::move(body));
                                  },
                                  std::move(respond_with_callback),
                                  std::move(body)));
                    },
                    info.Data())
                    .ToLocalChecked());
            if (result.IsEmpty()) {
              LOG(WARNING) << "Failure during FetchEvent respondWith handling. "
                              "Retrieving Response text failed.";
            }
          },
          data)
          .ToLocalChecked());
  if (result.IsEmpty()) {
    LOG(WARNING) << "Failure during FetchEvent respondWith handling.";
  }
}

script::HandlePromiseVoid FetchEvent::handled(
    script::EnvironmentSettings* environment_settings) {
  return script::HandlePromiseVoid(handled_property_->referenced_value());
}

}  // namespace worker
}  // namespace cobalt
