// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <algorithm>
#include <vector>

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/webdriver/script_executor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DefaultValue;
using ::testing::Return;

namespace cobalt {
namespace webdriver {
namespace {

class MockElementMapping : public ElementMapping {
 public:
  MOCK_METHOD1(ElementToId,
               protocol::ElementId(const scoped_refptr<dom::Element>&));
  MOCK_METHOD1(IdToElement,
               scoped_refptr<dom::Element>(const protocol::ElementId& id));
};

class MockScriptExecutorResult : public ScriptExecutorResult::ResultHandler {
 public:
  MOCK_METHOD1(OnResult, void(const std::string&));
  MOCK_METHOD0(OnTimeout, void());
};

class JSONScriptExecutorResult : public ScriptExecutorResult::ResultHandler {
 public:
  void OnResult(const std::string& result) {
    json_result_.reset(base::JSONReader::Read(result.c_str()));
  }
  void OnTimeout() { NOTREACHED(); }
  base::Value* json_result() { return json_result_.get(); }

 private:
  scoped_ptr<base::Value> json_result_;
};

class ScriptExecutorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    stub_window_.reset(new dom::testing::StubWindow());
    script_executor_ =
        ScriptExecutor::Create(&element_mapping_, global_environment());

    ON_CALL(element_mapping_, IdToElement(_))
        .WillByDefault(Return(scoped_refptr<dom::Element>()));
    ON_CALL(element_mapping_, ElementToId(_))
        .WillByDefault(Return(protocol::ElementId("bad-id")));
  }

  scoped_refptr<dom::Window> window() { return stub_window_->window(); }
  scoped_refptr<script::GlobalEnvironment> global_environment() {
    return stub_window_->global_environment();
  }

 protected:
  scoped_ptr<dom::testing::StubWindow> stub_window_;
  MockElementMapping element_mapping_;
  scoped_refptr<ScriptExecutor> script_executor_;
};

}  // namespace

TEST_F(ScriptExecutorTest, CreateSyncScript) {
  auto gc_prevented_params =
      ScriptExecutorParams::Create(global_environment(), "return 5;", "[]");
  ASSERT_TRUE(gc_prevented_params.params);
  EXPECT_NE(
      reinterpret_cast<intptr_t>(gc_prevented_params.params->function_object()),
      NULL);
  EXPECT_STREQ(gc_prevented_params.params->json_args().c_str(), "[]");
  EXPECT_EQ(gc_prevented_params.params->async_timeout(), base::nullopt);
}

TEST_F(ScriptExecutorTest, CreateAsyncScript) {
  auto gc_prevented_params =
      ScriptExecutorParams::Create(global_environment(), "return 5;", "[]",
                                   base::TimeDelta::FromMilliseconds(5));
  ASSERT_TRUE(gc_prevented_params.params);
  EXPECT_NE(
      reinterpret_cast<intptr_t>(gc_prevented_params.params->function_object()),
      NULL);
  EXPECT_STREQ(gc_prevented_params.params->json_args().c_str(), "[]");
  EXPECT_EQ(gc_prevented_params.params->async_timeout(), 5);
}

TEST_F(ScriptExecutorTest, CreateInvalidScript) {
  auto gc_prevented_params =
      ScriptExecutorParams::Create(global_environment(), "retarn 5ish;", "[]");
  ASSERT_TRUE(gc_prevented_params.params);
  EXPECT_EQ(
      reinterpret_cast<intptr_t>(gc_prevented_params.params->function_object()),
      NULL);
}

TEST_F(ScriptExecutorTest, ExecuteSync) {
  auto gc_prevented_params = ScriptExecutorParams::Create(
      global_environment(), "return \"retval\";", "[]");
  ASSERT_TRUE(gc_prevented_params.params);
  MockScriptExecutorResult result_handler;
  EXPECT_CALL(result_handler, OnResult(std::string("\"retval\"")));
  EXPECT_TRUE(
      script_executor_->Execute(gc_prevented_params.params, &result_handler));
}

TEST_F(ScriptExecutorTest, ExecuteAsync) {
  // Create a script that will call the async callback after 50 ms, with
  // an async timeout of 100 ms.
  auto gc_prevented_params = ScriptExecutorParams::Create(
      global_environment(),
      "var callback = arguments[0];"
      "window.setTimeout(function() { callback(72); }, 50);",
      "[]", base::TimeDelta::FromMilliseconds(100));
  ASSERT_TRUE(gc_prevented_params.params);
  MockScriptExecutorResult result_handler;
  EXPECT_CALL(result_handler, OnResult(std::string("72")));
  EXPECT_CALL(result_handler, OnTimeout()).Times(0);

  EXPECT_TRUE(
      script_executor_->Execute(gc_prevented_params.params, &result_handler));

  // Let the message loop run for 200ms to allow enough time for the async
  // script to fire the callback.
  base::RunLoop run_loop;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(200));
  run_loop.Run();
}

TEST_F(ScriptExecutorTest, AsyncTimeout) {
  // Create a script that will call the async callback after 10 seconds, with
  // an async timeout of 100 ms.
  auto gc_prevented_params = ScriptExecutorParams::Create(
      global_environment(),
      "var callback = arguments[0];"
      "window.setTimeout(function() { callback(72); }, 10000);",
      "[]", base::TimeDelta::FromMilliseconds(100));
  ASSERT_TRUE(gc_prevented_params.params);
  MockScriptExecutorResult result_handler;
  EXPECT_CALL(result_handler, OnResult(_)).Times(0);
  EXPECT_CALL(result_handler, OnTimeout()).Times(1);

  EXPECT_TRUE(
      script_executor_->Execute(gc_prevented_params.params, &result_handler));

  // Let the message loop run for 200ms to allow enough time for the async
  // timeout to fire.
  base::RunLoop run_loop;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(200));
  run_loop.Run();
}

TEST_F(ScriptExecutorTest, ScriptThrowsException) {
  auto gc_prevented_params =
      ScriptExecutorParams::Create(global_environment(), "throw Error()", "[]");
  ASSERT_TRUE(gc_prevented_params.params);
  MockScriptExecutorResult result_handler;
  EXPECT_FALSE(
      script_executor_->Execute(gc_prevented_params.params, &result_handler));
}

TEST_F(ScriptExecutorTest, ConvertBoolean) {
  auto gc_prevented_params = ScriptExecutorParams::Create(
      global_environment(), "return arguments[0];", "[true]");
  ASSERT_TRUE(gc_prevented_params.params);
  MockScriptExecutorResult result_handler;
  EXPECT_CALL(result_handler, OnResult(std::string("true")));
  EXPECT_TRUE(
      script_executor_->Execute(gc_prevented_params.params, &result_handler));

  gc_prevented_params = ScriptExecutorParams::Create(
      global_environment(), "return arguments[0];", "[false]");
  ASSERT_TRUE(gc_prevented_params.params);
  EXPECT_CALL(result_handler, OnResult(std::string("false")));
  EXPECT_TRUE(
      script_executor_->Execute(gc_prevented_params.params, &result_handler));
}

TEST_F(ScriptExecutorTest, ConvertNull) {
  auto gc_prevented_params = ScriptExecutorParams::Create(
      global_environment(), "return arguments[0];", "[null]");
  ASSERT_TRUE(gc_prevented_params.params);
  MockScriptExecutorResult result_handler;
  EXPECT_CALL(result_handler, OnResult(std::string("null")));
  EXPECT_TRUE(
      script_executor_->Execute(gc_prevented_params.params, &result_handler));
}

TEST_F(ScriptExecutorTest, ConvertNumericType) {
  auto gc_prevented_params = ScriptExecutorParams::Create(
      global_environment(), "return arguments[0];", "[6]");
  ASSERT_TRUE(gc_prevented_params.params);
  MockScriptExecutorResult result_handler;
  EXPECT_CALL(result_handler, OnResult(std::string("6")));
  EXPECT_TRUE(
      script_executor_->Execute(gc_prevented_params.params, &result_handler));

  gc_prevented_params = ScriptExecutorParams::Create(
      global_environment(), "return arguments[0];", "[-6.4]");
  ASSERT_TRUE(gc_prevented_params.params);
  EXPECT_CALL(result_handler, OnResult(std::string("-6.4")));
  EXPECT_TRUE(
      script_executor_->Execute(gc_prevented_params.params, &result_handler));
}

TEST_F(ScriptExecutorTest, ConvertString) {
  auto gc_prevented_params = ScriptExecutorParams::Create(
      global_environment(), "return arguments[0];", "[\"Mr. T\"]");
  ASSERT_TRUE(gc_prevented_params.params);
  MockScriptExecutorResult result_handler;
  EXPECT_CALL(result_handler, OnResult(std::string("\"Mr. T\"")));
  EXPECT_TRUE(
      script_executor_->Execute(gc_prevented_params.params, &result_handler));
}

TEST_F(ScriptExecutorTest, ConvertWebElement) {
  // Create a dom::Element for the MockElementMapping to return.
  scoped_refptr<dom::Element> element =
      window()->document()->CreateElement("p");
  EXPECT_CALL(element_mapping_, IdToElement(protocol::ElementId("id123")))
      .WillRepeatedly(Return(element));
  EXPECT_CALL(element_mapping_, ElementToId(element))
      .WillRepeatedly(Return(protocol::ElementId("id123")));

  // Create a script that will pass a web element argument as a parameter, and
  // return it back. This will invoke the lookup to and from an id.
  auto gc_prevented_params =
      ScriptExecutorParams::Create(global_environment(), "return arguments[0];",
                                   "[ {\"ELEMENT\": \"id123\"} ]");
  ASSERT_TRUE(gc_prevented_params.params);

  // Execute the script and parse the result as JSON, ensuring we got the same
  // web element.
  JSONScriptExecutorResult result_handler;
  EXPECT_TRUE(
      script_executor_->Execute(gc_prevented_params.params, &result_handler));
  ASSERT_TRUE(result_handler.json_result());

  std::string element_id;
  base::DictionaryValue* dictionary_value;
  ASSERT_TRUE(result_handler.json_result()->GetAsDictionary(&dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString(protocol::ElementId::kElementKey,
                                          &element_id));
  EXPECT_STREQ(element_id.c_str(), "id123");
}

TEST_F(ScriptExecutorTest, ConvertArray) {
  // Create a script that takes an array of numbers as input, and returns an
  // array of those numbers incremented by one.
  auto gc_prevented_params = ScriptExecutorParams::Create(
      global_environment(),
      "return [ (arguments[0][0]+1), (arguments[0][1]+1) ];", "[ [5, 6] ]");
  ASSERT_TRUE(gc_prevented_params.params);

  JSONScriptExecutorResult result_handler;
  EXPECT_TRUE(
      script_executor_->Execute(gc_prevented_params.params, &result_handler));
  ASSERT_TRUE(result_handler.json_result());

  base::ListValue* list_value;
  ASSERT_TRUE(result_handler.json_result()->GetAsList(&list_value));
  ASSERT_EQ(list_value->GetSize(), 2);

  int value;
  EXPECT_TRUE(list_value->GetInteger(0, &value));
  EXPECT_EQ(value, 6);
  EXPECT_TRUE(list_value->GetInteger(1, &value));
  EXPECT_EQ(value, 7);
}

TEST_F(ScriptExecutorTest, ConvertObject) {
  // Create a script that takes an Object with two properties as input, and
  // returns an Object with one property that is the sum of the other Object's
  // properties.
  auto gc_prevented_params = ScriptExecutorParams::Create(
      global_environment(),
      "return {\"sum\": arguments[0].a + arguments[0].b};",
      "[ {\"a\":5, \"b\":6} ]");
  ASSERT_TRUE(gc_prevented_params.params);

  JSONScriptExecutorResult result_handler;
  EXPECT_TRUE(
      script_executor_->Execute(gc_prevented_params.params, &result_handler));
  ASSERT_TRUE(result_handler.json_result());

  int value;
  base::DictionaryValue* dictionary_value;
  ASSERT_TRUE(result_handler.json_result()->GetAsDictionary(&dictionary_value));
  EXPECT_TRUE(dictionary_value->GetInteger("sum", &value));
  EXPECT_EQ(value, 11);
}

}  // namespace webdriver
}  // namespace cobalt
