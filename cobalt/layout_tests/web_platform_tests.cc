/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ostream>
#include <vector>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/layout_tests/test_utils.h"
#include "cobalt/layout_tests/web_platform_test_parser.h"
#include "cobalt/math/size.h"
#include "cobalt/media/media_module_stub.h"
#include "cobalt/network/network_module.h"
#include "cobalt/render_tree/resource_provider_stub.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace layout_tests {

namespace {

// Match the enums from testharness.js.
enum TestStatus {
  kPass = 0,
  kFail,
  kTimeout,
  kNotrun,
};

std::string TestStatusToString(int status) {
  switch (status) {
    case kPass:
      return "PASS";
    case kFail:
      return "FAIL";
    case kTimeout:
      return "TIMEOUT";
    case kNotrun:
      return "NOTRUN";
  }
  NOTREACHED();
  return "FAIL";
}

struct TestResult {
  std::string name;
  int status;
  std::string message;
  std::string stack;
};

const char* kLogSuppressions[] = {
    "<link> has unsupported rel value: help.",
    "<link> has unsupported rel value: author.",
    "synchronous XHR is not supported",
};

// Called when layout completes and results have been produced.  We use this
// signal to stop the WebModule's message loop since our work is done after a
// layout has been performed.
void WebModuleOnRenderTreeProducedCallback(
    base::optional<browser::WebModule::LayoutResults>* out_results,
    base::RunLoop* run_loop, const browser::WebModule::LayoutResults& results) {
  out_results->emplace(results.render_tree, results.animations,
                       results.layout_time);
  MessageLoop::current()->PostTask(FROM_HERE, run_loop->QuitClosure());
}

// This callback, when called, quits a message loop, outputs the error message
// and sets the success flag to false.
void WebModuleErrorCallback(base::RunLoop* run_loop, const std::string& error) {
  LOG(FATAL) << "Error loading document: " << error;
  MessageLoop::current()->PostTask(FROM_HERE, run_loop->QuitClosure());
}

std::string RunWebPlatformTest(const std::string& url) {
  LogFilter log_filter;
  for (size_t i = 0; i < arraysize(kLogSuppressions); ++i) {
    log_filter.Add(kLogSuppressions[i]);
  }

  // Setup a message loop for the current thread since we will be constructing
  // a WebModule, which requires a message loop to exist for the current
  // thread.
  std::string test_server =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "web-platform-test-server");
  if (test_server.empty()) {
    test_server = "http://cobalt-build.***REMOVED***:8000";
  }
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);

  const math::Size kDefaultViewportSize(640, 360);
  render_tree::ResourceProviderStub resource_provider;

  // Setup WebModule's auxiliary components.
  network::NetworkModule::Options net_options;
  net_options.require_https = false;
  network::NetworkModule network_module(net_options);

  // We do not support a media module in this mode.
  scoped_ptr<media::MediaModule> stub_media_module(
      new media::MediaModuleStub());

  // Use test runner mode to allow the content itself to dictate when it is
  // ready for layout should be performed.  See cobalt/dom/test_runner.h.
  browser::WebModule::Options web_module_options;
  web_module_options.layout_trigger = layout::LayoutManager::kTestRunnerMode;

  // Prepare a slot for our results to be placed when ready.
  base::optional<browser::WebModule::LayoutResults> results;
  base::RunLoop run_loop;

  GURL test_url = GURL(test_server).Resolve(url);
  // Create the WebModule and wait for a layout to occur.
  browser::WebModule web_module(
      test_url,
      base::Bind(&WebModuleOnRenderTreeProducedCallback, &results, &run_loop),
      base::Bind(&WebModuleErrorCallback, &run_loop), stub_media_module.get(),
      &network_module, kDefaultViewportSize, &resource_provider,
      60.0f, web_module_options);
  run_loop.Run();
  const std::string extract_results =
      "document.getElementById(\"__testharness__results__\").textContent;";
  std::string output = web_module.ExecuteJavascript(
      extract_results, base::SourceLocation(__FILE__, __LINE__, 1));
  return output;
}

bool ParseResults(const std::string& json_results) {
  scoped_ptr<base::Value> root;
  base::JSONReader reader;
  root.reset(reader.ReadToValue(json_results));
  base::DictionaryValue* root_as_dict;
  EXPECT_EQ(true, root->GetAsDictionary(&root_as_dict));

  base::ListValue* test_list;
  EXPECT_EQ(true, root_as_dict->GetList("tests", &test_list));

  std::vector<TestResult> test_results;
  for (size_t i = 0; i < test_list->GetSize(); ++i) {
    TestResult result;
    base::DictionaryValue* test_dict;
    EXPECT_EQ(true, test_list->GetDictionary(i, &test_dict));
    EXPECT_EQ(true, test_dict->GetInteger("status", &result.status));
    EXPECT_EQ(true, test_dict->GetString("name", &result.name));

    // These fields may be null.
    test_dict->GetString("message", &result.message);
    test_dict->GetString("stack", &result.stack);
    test_results.push_back(result);
  }

  bool any_failure = false;
  for (std::vector<TestResult>::const_iterator it = test_results.begin();
       it != test_results.end(); ++it) {
    if (it->status != kPass) {
      any_failure = true;
      DLOG(INFO) << "Test \"" << it->name << "\" failed with status "
                 << TestStatusToString(it->status);
      if (!it->message.empty()) {
        DLOG(INFO) << it->message;
      }
      if (!it->stack.empty()) {
        DLOG(INFO) << it->stack;
      }
    }
  }
  return any_failure;
}

}  // namespace

class WebPlatformTest : public ::testing::TestWithParam<WebPlatformTestInfo> {};
TEST_P(WebPlatformTest, WebPlatformTest) {
  // Output the name of the current input file so that it is visible in test
  // output.
  std::cout << "(" << GetParam() << ")" << std::endl;

  std::string json_results = RunWebPlatformTest(GetParam().url);
  bool any_failure = ParseResults(json_results);
  EXPECT_TRUE(any_failure != GetParam().expected_success);
}

// Disable on Windows until network stack is implemented.
#if !defined(COBALT_WIN)
// XML Http Request test cases.
INSTANTIATE_TEST_CASE_P(
    XMLHttpRequestTests, WebPlatformTest,
    ::testing::ValuesIn(EnumerateWebPlatformTests("XMLHttpRequest")));
#endif  // !defined(COBALT_WIN)

}  // namespace layout_tests
}  // namespace cobalt
