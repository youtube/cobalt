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

#ifndef COBALT_WORKER_FETCH_EVENT_H_
#define COBALT_WORKER_FETCH_EVENT_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "cobalt/base/token.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/worker/extendable_event.h"
#include "cobalt/worker/fetch_event_init.h"
#include "net/base/load_timing_info.h"

namespace cobalt {
namespace worker {

class FetchEvent : public ::cobalt::worker::ExtendableEvent {
 public:
  using RespondWithCallback =
      base::OnceCallback<void(std::unique_ptr<std::string>)>;
  using ReportLoadTimingInfo =
      base::OnceCallback<void(const net::LoadTimingInfo&)>;

  FetchEvent(script::EnvironmentSettings*, const std::string& type,
             const FetchEventInit& event_init_dict);
  FetchEvent(script::EnvironmentSettings*, base_token::Token type,
             const FetchEventInit& event_init_dict,
             scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
             RespondWithCallback respond_with_callback,
             ReportLoadTimingInfo report_load_timing_info);
  ~FetchEvent() override = default;

  void RespondWith(
      std::unique_ptr<script::Promise<script::ValueHandle*>>& response,
      script::ExceptionState* exception_state);
  script::HandlePromiseVoid handled();

  const script::ValueHandleHolder* request() {
    return &(request_->referenced_value());
  }

  bool respond_with_called() const { return respond_with_called_; }

  DEFINE_WRAPPABLE_TYPE(FetchEvent);

 private:
  base::Optional<v8::Local<v8::Promise>> GetText(
      v8::Local<v8::Promise> response_promise);
  base::Optional<v8::Local<v8::Promise>> DoRespondWith(
      v8::Local<v8::Promise> text_promise);
  void RespondWithDone();

  script::EnvironmentSettings* environment_settings_;
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;
  RespondWithCallback respond_with_callback_;
  ReportLoadTimingInfo report_load_timing_info_;
  std::unique_ptr<script::ValueHandleHolder::Reference> request_;
  std::unique_ptr<script::ValuePromiseVoid::Reference> handled_property_;
  std::unique_ptr<script::ValuePromiseVoid::Reference> respond_with_done_;
  bool respond_with_called_ = false;
  net::LoadTimingInfo load_timing_info_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_FETCH_EVENT_H_
