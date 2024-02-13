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

#include "cobalt/web/cache.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/json/string_escape.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "cobalt/base/source_location.h"
#include "cobalt/cache/cache.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/source_code.h"
#include "cobalt/script/v8c/conversion_helpers.h"
#include "cobalt/script/v8c/v8c_user_object_holder.h"
#include "cobalt/script/v8c/v8c_value_handle.h"
#include "cobalt/web/cache_utils.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings_helper.h"
#include "net/url_request/url_request_context.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace web {

namespace {

const network::disk_cache::ResourceType kResourceType =
    network::disk_cache::ResourceType::kCacheApi;

}  // namespace

Cache::Fetcher::Fetcher(network::NetworkModule* network_module, const GURL& url,
                        base::OnceCallback<void(bool)> callback)
    : network_module_(network_module),
      url_(url),
      callback_(std::move(callback)),
      buffer_(new net::GrowableIOBuffer()) {
  network_module_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Cache::Fetcher::Start, base::Unretained(this)));
}

std::vector<uint8_t> Cache::Fetcher::BufferToVector() const {
  auto* buffer_begin = reinterpret_cast<const uint8_t*>(buffer_->data());
  return std::vector<uint8_t>(buffer_begin, buffer_begin + buffer_size_);
}

std::string Cache::Fetcher::BufferToString() const {
  return std::string(buffer_->data(), buffer_size_);
}

void Cache::Fetcher::Start() {
  request_ = network_module_->url_request_context()->CreateRequest(
      url_, net::RequestPriority::DEFAULT_PRIORITY, this);
  request_->Start();
}

void Cache::Fetcher::Notify(bool success) {
  // Need to delete |URLRequest| instance on network thread.
  request_.reset();
  std::move(callback_).Run(success);
}

void Cache::Fetcher::OnDone(bool success) {
  buffer_size_ = buffer_->offset();
  buffer_->set_offset(0);
  network_module_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Cache::Fetcher::Notify, base::Unretained(this), success));
}

void Cache::Fetcher::ReadResponse(net::URLRequest* request) {
  int bytes_read = request->Read(buffer_, buffer_->RemainingCapacity());
  bool read_running_async = bytes_read == net::ERR_IO_PENDING;
  bool response_complete = !read_running_async && bytes_read <= 0;
  // If read is still running, the buffer will be written to and then
  // |OnReadCompleted()| will be called.
  if (read_running_async) {
    return;
  }
  if (response_complete) {
    OnDone(/*success=*/bytes_read == net::OK);
    return;
  }

  // Read completed synchronously. Call |OnReadCompleted()| asynchronously to
  // avoid blocking IO.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&Cache::Fetcher::OnReadCompleted,
                                base::Unretained(this), request, bytes_read));
}

void Cache::Fetcher::OnResponseStarted(net::URLRequest* request,
                                       int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  request->GetMimeType(&mime_type_);
  if (net_error != net::OK) {
    OnDone(/*success=*/false);
    return;
  }
  status_text_ = request->response_headers()->GetStatusText();
  response_code_ = request->response_headers()->response_code();
  size_t iter = 0;
  std::string name;
  std::string value;
  while (
      request->response_headers()->EnumerateHeaderLines(&iter, &name, &value)) {
    base::Value::List header;
    header.Append(name);
    header.Append(value);
    headers_.Append(std::move(header));
  }
  int initial_capacity = request->response_headers()->HasHeader(
                             net::HttpRequestHeaders::kContentLength)
                             ? request->response_headers()->GetContentLength()
                             : 64 * 1024;
  buffer_->SetCapacity(initial_capacity);
  ReadResponse(request);
}

void Cache::Fetcher::OnReadCompleted(net::URLRequest* request, int bytes_read) {
  DCHECK_NE(net::ERR_IO_PENDING, bytes_read);
  if (bytes_read <= 0) {
    OnDone(/*success=*/bytes_read == net::OK);
    return;
  }
  // The offset is how much of the buffer is used.
  buffer_->set_offset(buffer_->offset() + bytes_read);
  // Grow the buffer, if needed.
  if (buffer_->RemainingCapacity() == 0) {
    buffer_->SetCapacity(buffer_->capacity() + 16 * 1024);
  }
  ReadResponse(request);
}

script::HandlePromiseAny Cache::Match(
    script::EnvironmentSettings* environment_settings,
    const script::ValueHandleHolder& request) {
  auto* isolate = get_isolate(environment_settings);
  script::v8c::EntryScope entry_scope(isolate);
  auto resolver =
      v8::Promise::Resolver::New(isolate->GetCurrentContext()).ToLocalChecked();
  std::vector<v8::TracedGlobal<v8::Value>*> traced_globals;
  base::OnceClosure cleanup_traced;
  cache_utils::Trace(isolate, {resolver}, traced_globals, cleanup_traced);
  auto traced_resolver = traced_globals[0]->As<v8::Promise::Resolver>();
  auto context = get_context(environment_settings);
  context->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](script::EnvironmentSettings* environment_settings, uint32_t key,
             v8::TracedGlobal<v8::Promise::Resolver> traced_resolver,
             base::OnceClosure cleanup_traced) {
            base::ScopedClosureRunner finally(std::move(cleanup_traced));
            auto* isolate = get_isolate(environment_settings);
            auto cached =
                cache::Cache::GetInstance()->Retrieve(kResourceType, key);
            auto metadata =
                cache::Cache::GetInstance()->Metadata(kResourceType, key);
            script::v8c::EntryScope entry_scope(isolate);
            auto resolver = traced_resolver.Get(isolate);
            if (!cached || !metadata || !metadata->FindKey("options")) {
              cache_utils::Resolve(resolver);
              return;
            }
            auto response = cache_utils::CreateResponse(
                isolate, *cached, *(metadata->FindKey("options")));
            if (!response) {
              cache_utils::Reject(resolver);
              return;
            }
            cache_utils::Resolve(resolver, response.value());
          },
          environment_settings,
          cache_utils::GetKey(environment_settings->base_url(), request),
          traced_resolver, std::move(cleanup_traced)));
  return cache_utils::FromResolver(resolver);
}

void Cache::PerformAdd(
    script::EnvironmentSettings* environment_settings,
    std::unique_ptr<script::ValueHandleHolder::Reference> request_reference,
    std::unique_ptr<script::ValuePromiseVoid::Reference> promise_reference) {
  base::AutoLock auto_lock(fetcher_lock_);
  auto* global_environment = get_global_environment(environment_settings);
  auto* isolate = global_environment->isolate();
  script::v8c::EntryScope entry_scope(isolate);
  uint32_t key = cache_utils::GetKey(environment_settings->base_url(),
                                     request_reference->referenced_value());
  if (fetchers_.find(key) != fetchers_.end()) {
    auto* promises = &(fetch_contexts_[key].first);
    promises->push_back(std::move(promise_reference));
    return;
  }
  auto promises =
      std::vector<std::unique_ptr<script::ValuePromiseVoid::Reference>>();
  promises.push_back(std::move(promise_reference));
  fetch_contexts_[key] =
      std::make_pair(std::move(promises), environment_settings);
  auto* context = get_context(environment_settings);
  fetchers_[key] = std::make_unique<Cache::Fetcher>(
      context->network_module(),
      GURL(cache_utils::GetUrl(environment_settings->base_url(),
                               request_reference->referenced_value())),
      base::BindOnce(&Cache::OnFetchCompleted, base::Unretained(this), key));
}

script::HandlePromiseVoid Cache::Add(
    script::EnvironmentSettings* environment_settings,
    const script::ValueHandleHolder& request) {
  auto* global_wrappable = get_global_wrappable(environment_settings);
  auto request_reference =
      std::make_unique<script::ValueHandleHolder::Reference>(global_wrappable,
                                                             request);
  script::HandlePromiseVoid promise =
      get_script_value_factory(environment_settings)
          ->CreateBasicPromise<void>();
  auto promise_reference =
      std::make_unique<script::ValuePromiseVoid::Reference>(global_wrappable,
                                                            promise);
  auto context = get_context(environment_settings);
  context->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Cache::PerformAdd, base::Unretained(this),
                     environment_settings, std::move(request_reference),
                     std::move(promise_reference)));
  return promise;
}

script::HandlePromiseVoid Cache::Put(
    script::EnvironmentSettings* environment_settings,
    const script::ValueHandleHolder& request,
    const script::ValueHandleHolder& response) {
  auto* global_wrappable = get_global_wrappable(environment_settings);
  auto request_reference =
      std::make_unique<script::ValueHandleHolder::Reference>(global_wrappable,
                                                             request);
  auto response_reference =
      std::make_unique<script::ValueHandleHolder::Reference>(global_wrappable,
                                                             response);
  script::HandlePromiseVoid promise =
      get_script_value_factory(environment_settings)
          ->CreateBasicPromise<void>();
  auto promise_reference =
      std::make_unique<script::ValuePromiseVoid::Reference>(global_wrappable,
                                                            promise);

  auto* global_environment = get_global_environment(environment_settings);
  auto* isolate = global_environment->isolate();
  auto context = isolate->GetCurrentContext();
  script::v8c::EntryScope entry_scope(isolate);
  auto v8_response =
      GetV8Value(response_reference->referenced_value()).As<v8::Object>();
  auto body_used = cache_utils::Get(v8_response, "bodyUsed");
  if (!body_used || body_used->As<v8::Boolean>()->Value()) {
    promise_reference->value().Reject(script::kTypeError);
    return promise;
  }
  auto array_buffer_promise = cache_utils::Call(v8_response, "arrayBuffer");
  if (!array_buffer_promise) {
    promise_reference->value().Reject();
    return promise;
  }
  cache_utils::Then(
      array_buffer_promise.value(),
      base::BindOnce(
          [](script::EnvironmentSettings* environment_settings,
             std::unique_ptr<script::ValueHandleHolder::Reference>
                 request_reference,
             std::unique_ptr<script::ValueHandleHolder::Reference>
                 response_reference,
             std::unique_ptr<script::ValuePromiseVoid::Reference>
                 promise_reference,
             v8::Local<v8::Promise> array_buffer_promise)
              -> base::Optional<v8::Local<v8::Promise>> {
            uint32_t key =
                cache_utils::GetKey(environment_settings->base_url(),
                                    request_reference->referenced_value());
            std::string url =
                cache_utils::GetUrl(environment_settings->base_url(),
                                    request_reference->referenced_value());
            auto options = cache_utils::ExtractResponseOptions(
                cache_utils::ToV8Value(response_reference->referenced_value()));
            if (!options) {
              promise_reference->value().Reject();
              return base::nullopt;
            }
            base::Value::Dict metadata;
            metadata.Set("url", base::Value(url));
            metadata.Set("options", std::move(options.value()));
            cache::Cache::GetInstance()->Store(
                kResourceType, key,
                cache_utils::ToUint8Vector(array_buffer_promise->Result()),
                base::Value(std::move(metadata)));
            promise_reference->value().Resolve();
            return base::nullopt;
          },
          environment_settings, std::move(request_reference),
          std::move(response_reference), std::move(promise_reference)));
  return promise;
}

script::HandlePromiseBool Cache::Delete(
    script::EnvironmentSettings* environment_settings,
    const script::ValueHandleHolder& request) {
  auto* global_wrappable = get_global_wrappable(environment_settings);
  script::HandlePromiseBool promise =
      get_script_value_factory(environment_settings)
          ->CreateBasicPromise<bool>();
  auto request_reference =
      std::make_unique<script::ValueHandleHolder::Reference>(global_wrappable,
                                                             request);
  auto promise_reference =
      std::make_unique<script::ValuePromiseBool::Reference>(global_wrappable,
                                                            promise);
  auto context = get_context(environment_settings);
  context->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](script::EnvironmentSettings* environment_settings,
             std::unique_ptr<script::ValueHandleHolder::Reference>
                 request_reference,
             std::unique_ptr<script::ValuePromiseBool::Reference>
                 promise_reference) {
            auto* global_environment =
                get_global_environment(environment_settings);
            auto* isolate = global_environment->isolate();
            script::v8c::EntryScope entry_scope(isolate);
            promise_reference->value().Resolve(
                cache::Cache::GetInstance()->Delete(
                    kResourceType, cache_utils::GetKey(
                                       environment_settings->base_url(),
                                       request_reference->referenced_value())));
          },
          environment_settings, std::move(request_reference),
          std::move(promise_reference)));
  return promise;
}

script::HandlePromiseAny Cache::Keys(
    script::EnvironmentSettings* environment_settings) {
  auto* global_wrappable = get_global_wrappable(environment_settings);
  script::HandlePromiseAny promise =
      get_script_value_factory(environment_settings)
          ->CreateBasicPromise<script::Any>();
  auto promise_reference = std::make_unique<script::ValuePromiseAny::Reference>(
      global_wrappable, promise);
  auto context = get_context(environment_settings);
  context->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](script::EnvironmentSettings* environment_settings,
             std::unique_ptr<script::ValuePromiseAny::Reference>
                 promise_reference) {
            auto* global_environment =
                get_global_environment(environment_settings);
            auto* isolate = global_environment->isolate();
            script::v8c::EntryScope entry_scope(isolate);
            std::vector<v8::Local<v8::Value>> requests;
            auto keys =
                cache::Cache::GetInstance()->KeysWithMetadata(kResourceType);
            for (uint32_t key : keys) {
              auto metadata =
                  cache::Cache::GetInstance()->Metadata(kResourceType, key);
              if (!metadata) {
                continue;
              }
              auto url = metadata->FindKey("url");
              if (!url) {
                continue;
              }
              base::Optional<v8::Local<v8::Value>> request =
                  cache_utils::CreateRequest(isolate, url->GetString());
              if (request) {
                requests.push_back(std::move(request.value()));
              }
            }
            promise_reference->value().Resolve(
                script::Any(new script::v8c::V8cValueHandleHolder(
                    isolate, v8::Array::New(isolate, requests.data(),
                                            requests.size()))));
          },
          environment_settings, std::move(promise_reference)));
  return promise;
}

void Cache::OnFetchCompleted(uint32_t key, bool success) {
  base::AutoLock auto_lock(fetcher_lock_);
  auto* environment_settings = fetch_contexts_[key].second;
  auto* context = get_context(environment_settings);
  context->message_loop()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Cache::OnFetchCompletedMainThread,
                                base::Unretained(this), key, success));
}

void Cache::OnFetchCompletedMainThread(uint32_t key, bool success) {
  base::AutoLock auto_lock(fetcher_lock_);
  auto* fetcher = fetchers_[key].get();
  auto* promises = &(fetch_contexts_[key].first);
  int status = fetcher->response_code();
  // |status| of 200-299 excluding 206 "Partial Content" should be cached.
  if (!success || status == 206 || status < 200 || status > 299) {
    while (promises->size() > 0) {
      promises->back()->value().Reject();
      promises->pop_back();
    }
    fetchers_.erase(key);
    fetch_contexts_.erase(key);
    return;
  }
  {
    base::Value::Dict metadata;
    metadata.Set("url", base::Value(fetcher->url().spec()));
    base::Value::Dict options;
    options.Set("status", base::Value(status));
    options.Set("statusText", base::Value(fetcher->status_text()));
    options.Set("headers", std::move(fetcher->headers()));
    metadata.Set("options", std::move(options));

    cache::Cache::GetInstance()->Store(kResourceType, key,
                                       fetcher->BufferToVector(),
                                       base::Value(std::move(metadata)));
  }
  if (fetcher->mime_type() == "text/javascript") {
    auto* environment_settings = fetch_contexts_[key].second;
    auto* global_environment = get_global_environment(environment_settings);
    auto* isolate = global_environment->isolate();
    script::v8c::EntryScope entry_scope(isolate);
    // TODO: compile async or maybe don't cache if compile fails.
    global_environment->Compile(script::SourceCode::CreateSourceCode(
        fetcher->BufferToString(), base::SourceLocation(__FILE__, 1, 1)));
  }
  while (promises->size() > 0) {
    promises->back()->value().Resolve();
    promises->pop_back();
  }
  fetchers_.erase(key);
  fetch_contexts_.erase(key);
}

}  // namespace web
}  // namespace cobalt
