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

#include "cobalt/web/cache_utils.h"

#include <utility>

#include "base/message_loop/message_loop.h"
#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/web/agent.h"
#include "cobalt/web/context.h"
#include "cobalt/web/testing/test_with_javascript.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace web {

namespace {

v8::Local<v8::Value> ExpectState(
    base::Optional<v8::Local<v8::Value>> promise_value,
    v8::Promise::PromiseState expected) {
  EXPECT_TRUE(promise_value.has_value());
  EXPECT_TRUE(promise_value.value()->IsPromise());
  auto promise = promise_value->As<v8::Promise>();
  auto* isolate = promise->GetIsolate();
  auto context = isolate->GetCurrentContext();
  auto e = std::make_unique<base::WaitableEvent>();
  auto fulfilled_promise = promise->Then(
      context, v8::Function::New(
                   context,
                   [](const v8::FunctionCallbackInfo<v8::Value>& info) {
                     static_cast<base::WaitableEvent*>(
                         info.Data().As<v8::External>()->Value())
                         ->Signal();
                   },
                   v8::External::New(isolate, e.get()))
                   .ToLocalChecked());
  auto rejected_promise = promise->Catch(
      context, v8::Function::New(
                   context,
                   [](const v8::FunctionCallbackInfo<v8::Value>& info) {
                     static_cast<base::WaitableEvent*>(
                         info.Data().As<v8::External>()->Value())
                         ->Signal();
                   },
                   v8::External::New(isolate, e.get()))
                   .ToLocalChecked());
  EXPECT_TRUE(!fulfilled_promise.IsEmpty());
  EXPECT_TRUE(!rejected_promise.IsEmpty());
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  // Ensure promise is scheduled to be resolved.
  isolate->PerformMicrotaskCheckpoint();
  e->Wait();
  EXPECT_TRUE(promise->State() == expected);
  return promise->Result();
}

class GetGlobalScopeTypeIdWindow : public ::testing::Test {
 public:
  base::TypeId GetGlobalScopeTypeId() const {
    return base::GetTypeId<dom::Window>();
  }
};

class CacheUtilsTest
    : public testing::TestWithJavaScriptBase<GetGlobalScopeTypeIdWindow> {};

}  // namespace

TEST_F(CacheUtilsTest, Eval) {
  auto* isolate = web_context()->global_environment()->isolate();
  script::v8c::EntryScope entry_scope(isolate);
  auto v8_this = v8::Object::New(isolate);
  cache_utils::Set(v8_this, "a", v8::Number::New(isolate, 1.0));
  EXPECT_TRUE(v8_this->IsObject());
  EXPECT_DOUBLE_EQ(1.0,
                   cache_utils::Get(v8_this, "a")->As<v8::Number>()->Value());
  auto context = isolate->GetCurrentContext();
  auto global = context->Global();
  cache_utils::Set(global, "obj", v8_this);
  auto result =
      cache_utils::Evaluate(isolate, "obj.a++; obj;")->As<v8::Object>();

  EXPECT_DOUBLE_EQ(2.0,
                   cache_utils::Get(result, "a")->As<v8::Number>()->Value());
  EXPECT_DOUBLE_EQ(2.0,
                   cache_utils::Get(v8_this, "a")->As<v8::Number>()->Value());
}

TEST_F(CacheUtilsTest, ResponseHeaders) {
  auto* isolate = web_context()->global_environment()->isolate();
  script::v8c::EntryScope entry_scope(isolate);
  std::string body_string = "body";
  auto* body_begin = reinterpret_cast<const uint8_t*>(body_string.data());
  auto* body_end = body_begin + body_string.size();
  std::vector<uint8_t> body(body_begin, body_end);
  base::Value::Dict initial_options;
  initial_options.Set("status", base::Value(200));
  initial_options.Set("statusText", base::Value("OK"));
  base::Value::List initial_headers;
  base::Value::List header_1;
  header_1.Append("a");
  header_1.Append("1");
  initial_headers.Append(std::move(header_1));
  base::Value::List header_2;
  header_2.Append("b");
  header_2.Append("2");
  initial_headers.Append(std::move(header_2));
  initial_options.Set("headers", std::move(initial_headers));
  auto response = cache_utils::CreateResponse(
                      isolate, body, base::Value(std::move(initial_options)))
                      .value();

  EXPECT_DOUBLE_EQ(200, cache_utils::FromNumber(
                            cache_utils::Get(response, "status").value()));
  EXPECT_EQ("OK",
            cache_utils::FromV8String(
                isolate, cache_utils::Get(response, "statusText").value()));
  base::Value options = cache_utils::ExtractResponseOptions(response).value();

  EXPECT_DOUBLE_EQ(200, cache_utils::Get(options, "status")->GetDouble());
  EXPECT_EQ("OK", cache_utils::Get(options, "statusText")->GetString());
  EXPECT_EQ(2, cache_utils::Get(options, "headers")->GetList().size());
  EXPECT_EQ("a", cache_utils::Get(options, "headers.0.0")->GetString());
  EXPECT_EQ("1", cache_utils::Get(options, "headers.0.1")->GetString());
  EXPECT_EQ("b", cache_utils::Get(options, "headers.1.0")->GetString());
  EXPECT_EQ("2", cache_utils::Get(options, "headers.1.1")->GetString());
}

TEST_F(CacheUtilsTest, BaseToV8) {
  auto* isolate = web_context()->global_environment()->isolate();
  script::v8c::EntryScope entry_scope(isolate);
  std::string json = R"~({
  "options": {
    "headers": [["a", "1"], ["b", "2"]],
    "status": 200,
    "statusText": "OK"
  },
  "url": "https://www.example.com/1"
})~";
  base::Value base_value = cache_utils::Deserialize(json).value();
  auto v8_value = cache_utils::BaseToV8(isolate, base_value).value();
  EXPECT_TRUE(v8_value->IsObject());
  EXPECT_EQ("https://www.example.com/1",
            cache_utils::GetString(v8_value, "url"));
  EXPECT_DOUBLE_EQ(200,
                   cache_utils::GetNumber(v8_value, "options.status").value());
  EXPECT_EQ("OK", cache_utils::GetString(v8_value, "options.statusText"));
  EXPECT_EQ("a", cache_utils::GetString(v8_value, "options.headers.0.0"));
  EXPECT_EQ("1", cache_utils::GetString(v8_value, "options.headers.0.1"));
  EXPECT_EQ("b", cache_utils::GetString(v8_value, "options.headers.1.0"));
  EXPECT_EQ("2", cache_utils::GetString(v8_value, "options.headers.1.1"));

  EXPECT_EQ(base_value, cache_utils::V8ToBase(isolate, v8_value));
}

TEST_F(CacheUtilsTest, ThenFulfilled) {
  auto* isolate = web_context()->global_environment()->isolate();
  script::v8c::EntryScope entry_scope(isolate);
  auto promise =
      cache_utils::Evaluate(isolate, "Promise.resolve('success')").value();
  auto still_fulfilled_promise = cache_utils::Then(
      promise, base::BindOnce([&](v8::Local<v8::Promise> p)
                                  -> base::Optional<v8::Local<v8::Promise>> {
        auto* isolate = p->GetIsolate();
        std::string new_message =
            "still " + cache_utils::FromV8String(isolate, p->Result());
        return cache_utils::Evaluate(isolate,
                                     base::StringPrintf("Promise.resolve('%s')",
                                                        new_message.c_str()))
            ->As<v8::Promise>();
      }));
  auto promise_result = ExpectState(still_fulfilled_promise,
                                    v8::Promise::PromiseState::kFulfilled);
  EXPECT_EQ("still success",
            cache_utils::FromV8String(isolate, promise_result));
}

TEST_F(CacheUtilsTest, ThenFulfilledThenRejected) {
  auto* isolate = web_context()->global_environment()->isolate();
  script::v8c::EntryScope entry_scope(isolate);
  auto promise =
      cache_utils::Evaluate(isolate, "Promise.resolve('success')").value();
  auto rejected_promise = cache_utils::Then(
      promise, base::BindOnce([&](v8::Local<v8::Promise> p)
                                  -> base::Optional<v8::Local<v8::Promise>> {
        auto* isolate = p->GetIsolate();
        std::string new_message =
            "no longer " + cache_utils::FromV8String(isolate, p->Result());
        return cache_utils::Evaluate(isolate,
                                     base::StringPrintf("Promise.reject('%s')",
                                                        new_message.c_str()))
            ->As<v8::Promise>();
      }));
  auto promise_result =
      ExpectState(rejected_promise, v8::Promise::PromiseState::kRejected);
  EXPECT_EQ("no longer success",
            cache_utils::FromV8String(isolate, promise_result));
}

TEST_F(CacheUtilsTest, ThenRejected) {
  auto* isolate = web_context()->global_environment()->isolate();
  script::v8c::EntryScope entry_scope(isolate);
  auto promise =
      cache_utils::Evaluate(isolate, "Promise.reject('fail')").value();
  auto still_rejected_promise = cache_utils::Then(
      promise,
      base::BindOnce(
          [](v8::Local<v8::Promise>) -> base::Optional<v8::Local<v8::Promise>> {
            return base::nullopt;
          }));
  auto promise_result =
      ExpectState(still_rejected_promise, v8::Promise::PromiseState::kRejected);
  EXPECT_EQ("fail", cache_utils::FromV8String(isolate, promise_result));
}

}  // namespace web
}  // namespace cobalt
