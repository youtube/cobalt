// Copyright 2015 Google Inc. All Rights Reserved.
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

#include <ostream>
#include <sstream>
#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/string_util.h"
#include "base/values.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/dom/csp_delegate_factory.h"
#include "cobalt/layout_tests/test_utils.h"
#include "cobalt/layout_tests/web_platform_test_parser.h"
#include "cobalt/math/size.h"
#include "cobalt/media/media_module.h"
#include "cobalt/network/network_module.h"
#include "cobalt/render_tree/resource_provider_stub.h"
#include "googleurl/src/gurl.h"
#include "starboard/window.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace layout_tests {

namespace {

// A CspDelegate that behaves more like a "standard" one. i.e. it
// is permissive by default. This is for testing our compliance
// with the web-platform-test suite.
class CspDelegatePermissive : public dom::CspDelegateSecure {
 public:
  CspDelegatePermissive(
      scoped_ptr<dom::CspViolationReporter> violation_reporter, const GURL& url,
      csp::CSPHeaderPolicy require_csp,
      const base::Closure& policy_changed_callback)
      : dom::CspDelegateSecure(violation_reporter.Pass(), url, require_csp,
                               policy_changed_callback) {
    // Lies, but some checks in our parent require this.
    was_header_received_ = true;
  }

  static CspDelegate* Create(
      scoped_ptr<dom::CspViolationReporter> violation_reporter, const GURL& url,
      csp::CSPHeaderPolicy require_csp,
      const base::Closure& policy_changed_callback,
      int insecure_allowed_token) {
    UNREFERENCED_PARAMETER(insecure_allowed_token);
    return new CspDelegatePermissive(violation_reporter.Pass(), url,
                                     require_csp, policy_changed_callback);
  }

  bool OnReceiveHeaders(const csp::ResponseHeaders& headers) OVERRIDE {
    csp_->OnReceiveHeaders(headers);
    if (!policy_changed_callback_.is_null()) {
      policy_changed_callback_.Run();
    }
    // |true| to allow the document to continue loading.
    return true;
  }
};

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

void Quit(base::RunLoop* run_loop) {
  MessageLoop::current()->PostTask(FROM_HERE, run_loop->QuitClosure());
}

// Called when layout completes and results have been produced.  We use this
// signal to stop the WebModule's message loop since our work is done after a
// layout has been performed.
void WebModuleOnRenderTreeProducedCallback(
    base::optional<browser::WebModule::LayoutResults>* out_results,
    base::RunLoop* run_loop, MessageLoop* message_loop,
    const browser::WebModule::LayoutResults& results) {
  out_results->emplace(results.render_tree, results.layout_time);
  message_loop->PostTask(FROM_HERE, base::Bind(Quit, run_loop));
}

// This callback, when called, quits a message loop, outputs the error message
// and sets the success flag to false.
void WebModuleErrorCallback(base::RunLoop* run_loop, MessageLoop* message_loop,
                            const GURL& url, const std::string& error) {
  LOG(FATAL) << "Error loading document: " << error << ". URL: " << url;
  message_loop->PostTask(FROM_HERE, base::Bind(Quit, run_loop));
}

std::string RunWebPlatformTest(const GURL& url, bool* got_results) {
  LogFilter log_filter;
  for (size_t i = 0; i < arraysize(kLogSuppressions); ++i) {
    log_filter.Add(kLogSuppressions[i]);
  }

  // Setup a message loop for the current thread since we will be constructing
  // a WebModule, which requires a message loop to exist for the current
  // thread.
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);

  const math::Size kDefaultViewportSize(640, 360);

  // Setup WebModule's auxiliary components.

  // Network module
  network::NetworkModule::Options net_options;
  net_options.https_requirement = network::kHTTPSOptional;
  network::NetworkModule network_module(net_options);

  // Media module
  render_tree::ResourceProviderStub resource_provider;
  media::MediaModule::Options options;
  options.output_resolution_override = kDefaultViewportSize;
  scoped_ptr<media::MediaModule> media_module(
      media::MediaModule::Create(NULL, &resource_provider, options));
  scoped_ptr<media::CanPlayTypeHandler> can_play_type_handler(
      media::MediaModule::CreateCanPlayTypeHandler());

  dom::CspDelegateFactory::GetInstance()->OverrideCreator(
      dom::kCspEnforcementEnable, CspDelegatePermissive::Create);
  // Use test runner mode to allow the content itself to dictate when it is
  // ready for layout should be performed.  See cobalt/dom/test_runner.h.
  browser::WebModule::Options web_module_options;
  web_module_options.layout_trigger = layout::LayoutManager::kTestRunnerMode;

  // Prepare a slot for our results to be placed when ready.
  base::optional<browser::WebModule::LayoutResults> results;
  base::RunLoop run_loop;

  // Create the WebModule and wait for a layout to occur.
  browser::WebModule web_module(
      url, base::kApplicationStateStarted,
      base::Bind(&WebModuleOnRenderTreeProducedCallback, &results, &run_loop,
                 MessageLoop::current()),
      base::Bind(&WebModuleErrorCallback, &run_loop, MessageLoop::current()),
      browser::WebModule::CloseCallback() /* window_close_callback */,
      base::Closure() /* window_minimize_callback */,
      base::Callback<SbWindow()>() /* get_sb_window */,
      can_play_type_handler.get(), media_module.get(), &network_module,
      kDefaultViewportSize, 1.f, &resource_provider, 60.0f, web_module_options);
  run_loop.Run();
  const std::string extract_results =
      "document.getElementById(\"__testharness__results__\").textContent;";
  std::string output = web_module.ExecuteJavascript(
      extract_results, base::SourceLocation(__FILE__, __LINE__, 1),
      got_results);
  return output;
}

std::vector<TestResult> ParseResults(const std::string& json_results) {
  std::vector<TestResult> test_results;

  scoped_ptr<base::Value> root;
  base::JSONReader reader;
  root.reset(reader.ReadToValue(json_results));
  if (!root) {
    // Unparseable JSON, or empty string.
    return test_results;
  }

  base::DictionaryValue* root_as_dict;
  EXPECT_EQ(true, root->GetAsDictionary(&root_as_dict));

  base::ListValue* test_list;
  EXPECT_EQ(true, root_as_dict->GetList("tests", &test_list));

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
  return test_results;
}

::testing::AssertionResult CheckResult(const char* /* expectation_str */,
                                       const char* /* results_str */,
                                       bool should_pass,
                                       const TestResult& result) {
  bool test_passed = result.status == WebPlatformTestInfo::kPass;

  if (test_passed == should_pass) {
    return ::testing::AssertionSuccess();
  } else {
    std::ostringstream output;
    output << std::endl
           << "Test \"" << result.name
           << "\" status: " << TestStatusToString(result.status);
    if (!result.message.empty()) {
      output << std::endl << result.message;
    }
    if (!result.stack.empty()) {
      output << std::endl << result.stack;
    }
    WebPlatformTestInfo::State expectation =
        should_pass ? WebPlatformTestInfo::kPass : WebPlatformTestInfo::kFail;
    return ::testing::AssertionFailure()
           << "Expected: " << TestStatusToString(expectation) << std::endl
           << "Actual: " << TestStatusToString(result.status) << std::endl
           << output.str();
  }
}

}  // namespace

class WebPlatformTest : public ::testing::TestWithParam<WebPlatformTestInfo> {};
TEST_P(WebPlatformTest, Run) {
  // Output the name of the current input file so that it is visible in test
  // output.
  std::string test_server =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "web-platform-test-server");
  if (test_server.empty()) {
    FilePath url_path = GetTestInputRootDirectory()
                        .Append(FILE_PATH_LITERAL("web-platform-tests"));
#if defined(COBALT_LINUX) || defined(COBALT_WIN)
    // Get corp configuration.
    url_path = url_path.Append(FILE_PATH_LITERAL("corp.url"));
#else
    // Get lab configuration.
    url_path = url_path.Append(FILE_PATH_LITERAL("lab.url"));
#endif
    file_util::ReadFileToString(url_path, &test_server);
    TrimWhitespaceASCII(test_server, TRIM_ALL, &test_server);
    ASSERT_FALSE(test_server.empty());
  }
  GURL test_url = GURL(test_server).Resolve(GetParam().url);

  std::cout << "(" << test_url << ")" << std::endl;

  bool got_results = false;
  std::string json_results = RunWebPlatformTest(test_url, &got_results);
  EXPECT_TRUE(got_results);
  std::vector<TestResult> results = ParseResults(json_results);
  bool failed_at_least_once = false;
  for (size_t i = 0; i < results.size(); ++i) {
    const WebPlatformTestInfo& test_info = GetParam();
    const TestResult& test_result = results[i];
    bool should_pass = test_info.expectation == WebPlatformTestInfo::kPass;
    std::set<std::string>::const_iterator it =
        test_info.exceptions.find(test_result.name);
    if (it != test_info.exceptions.end()) {
      should_pass = !should_pass;
    }
    // If expected to fail but current subtest did not fail, wait to report
    // the entire test failed after the last subtest passed and none failed.
    if (!should_pass && test_result.status == WebPlatformTestInfo::kPass &&
        (i != results.size() - 1 || failed_at_least_once)) {
      should_pass = true;
    } else {
      failed_at_least_once = failed_at_least_once ||
                             test_result.status == WebPlatformTestInfo::kFail;
    }
    EXPECT_PRED_FORMAT2(CheckResult, should_pass, test_result);
  }
}

// Disable on Windows until network stack is implemented.
#if !defined(COBALT_WIN)
// XML Http Request test cases.
INSTANTIATE_TEST_CASE_P(
    xhr, WebPlatformTest,
    ::testing::ValuesIn(EnumerateWebPlatformTests("XMLHttpRequest")));

INSTANTIATE_TEST_CASE_P(
    csp, WebPlatformTest,
    ::testing::ValuesIn(EnumerateWebPlatformTests("content-security-policy")));

INSTANTIATE_TEST_CASE_P(dom, WebPlatformTest,
                        ::testing::ValuesIn(EnumerateWebPlatformTests("dom")));

INSTANTIATE_TEST_CASE_P(cors, WebPlatformTest,
                        ::testing::ValuesIn(EnumerateWebPlatformTests("cors")));

INSTANTIATE_TEST_CASE_P(
    fetch, WebPlatformTest,
    ::testing::ValuesIn(EnumerateWebPlatformTests("fetch", "'fetch' in this")));

INSTANTIATE_TEST_CASE_P(
    mediasession, WebPlatformTest,
    ::testing::ValuesIn(EnumerateWebPlatformTests("mediasession")));

INSTANTIATE_TEST_CASE_P(streams, WebPlatformTest,
                        ::testing::ValuesIn(EnumerateWebPlatformTests(
                            "streams", "'ReadableStream' in this")));

INSTANTIATE_TEST_CASE_P(
    cobalt_special, WebPlatformTest,
    ::testing::ValuesIn(EnumerateWebPlatformTests("cobalt_special")));

#endif  // !defined(COBALT_WIN)

}  // namespace layout_tests
}  // namespace cobalt
