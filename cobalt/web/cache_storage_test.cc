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

#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/web/cache_utils.h"
#include "cobalt/web/testing/test_with_javascript.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace web {

namespace {

class GetGlobalScopeTypeIdWindow : public ::testing::Test {
 public:
  base::TypeId GetGlobalScopeTypeId() const {
    return base::GetTypeId<dom::Window>();
  }
};

class CacheStorageTest
    : public testing::TestWithJavaScriptBase<GetGlobalScopeTypeIdWindow> {};

v8::Local<v8::Value> Await(base::Optional<v8::Local<v8::Value>> promise_value) {
  EXPECT_TRUE(promise_value.has_value());
  EXPECT_TRUE(promise_value.value()->IsPromise());
  auto promise = promise_value->As<v8::Promise>();
  auto* isolate = promise->GetIsolate();
  auto context = isolate->GetCurrentContext();
  auto e = std::make_unique<base::WaitableEvent>();
  auto result_promise = promise->Then(
      context, v8::Function::New(
                   context,
                   [](const v8::FunctionCallbackInfo<v8::Value>& info) {
                     static_cast<base::WaitableEvent*>(
                         info.Data().As<v8::External>()->Value())
                         ->Signal();
                   },
                   v8::External::New(isolate, e.get()))
                   .ToLocalChecked());
  EXPECT_TRUE(!result_promise.IsEmpty());
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  // Ensure promise is scheduled to be resolved.
  isolate->PerformMicrotaskCheckpoint();
  e->Wait();
  EXPECT_TRUE(promise->State() == v8::Promise::PromiseState::kFulfilled);
  return promise->Result();
}

base::Value MakeHeader(const std::string& name, const std::string& value) {
  base::Value::List header;
  header.Append(name);
  header.Append(value);
  return base::Value(std::move(header));
}

std::vector<uint8_t> ToVector(const std::string& data) {
  auto* begin = reinterpret_cast<const uint8_t*>(data.data());
  auto* end = begin + data.size();
  return std::vector<uint8_t>(begin, end);
}

}  // namespace

TEST_F(CacheStorageTest, Work) {
  auto* isolate = web_context()->global_environment()->isolate();
  script::v8c::EntryScope entry_scope(isolate);

  auto delete_result =
      Await(cache_utils::Evaluate(isolate, "caches.delete('test');"));
  EXPECT_TRUE(delete_result->IsBoolean() &&
              delete_result.As<v8::Boolean>()->Value());

  auto v8_cache = Await(cache_utils::Evaluate(isolate, "caches.open('test');"));
  EXPECT_FALSE(v8_cache.IsEmpty());
  EXPECT_FALSE(v8_cache->IsNullOrUndefined());
  EXPECT_TRUE(v8_cache->IsObject());

  std::string url = "https://www.example.com/1";
  auto request = cache_utils::CreateRequest(isolate, url).value();
  base::Value::Dict options;
  options.Set("status", base::Value(200));
  options.Set("statusText", base::Value("OK"));
  base::Value::List headers;
  headers.Append(MakeHeader("a", "1"));
  options.Set("headers", std::move(headers));

  std::string body = "abcde";
  auto response = cache_utils::CreateResponse(isolate, ToVector(body),
                                              base::Value(std::move(options)))
                      .value();
  auto v8_put_result =
      Await(cache_utils::Call(v8_cache, "put", {request, response}));
  EXPECT_TRUE(v8_put_result->IsUndefined());

  auto v8_match_result = Await(cache_utils::Call(v8_cache, "match", {request}));
  EXPECT_EQ(200, cache_utils::FromNumber(
                     cache_utils::Get(v8_match_result, "status").value()));
  EXPECT_EQ("OK", cache_utils::GetString(v8_match_result, "statusText"));
  EXPECT_EQ(
      "1", cache_utils::FromV8String(
               isolate, cache_utils::Call(v8_match_result, "headers.get",
                                          {cache_utils::V8String(isolate, "a")})
                            .value()));
  auto match_result_options =
      cache_utils::ExtractResponseOptions(v8_match_result).value();
  EXPECT_EQ(200, cache_utils::Get(match_result_options, "status")->GetInt());
  EXPECT_EQ("OK",
            cache_utils::Get(match_result_options, "statusText")->GetString());
  EXPECT_EQ(1, match_result_options.FindKey("headers")->GetList().size());
  EXPECT_EQ("a",
            cache_utils::Get(match_result_options, "headers.0.0")->GetString());
  EXPECT_EQ("1",
            cache_utils::Get(match_result_options, "headers.0.1")->GetString());
  EXPECT_EQ(body,
            cache_utils::FromV8String(
                isolate, Await(cache_utils::Call(v8_match_result, "text"))));

  auto v8_keys_result = Await(cache_utils::Call(v8_cache, "keys"));
  EXPECT_TRUE(v8_keys_result->IsArray());
  auto keys = v8_keys_result.As<v8::Array>();
  EXPECT_EQ(1, keys->Length());
  EXPECT_EQ(url, cache_utils::GetString(keys, "0.url"));
}

}  // namespace web
}  // namespace cobalt
