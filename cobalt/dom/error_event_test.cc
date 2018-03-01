// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/error_event.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/dom/error_event_init.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/testing/gtest_workarounds.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/media_session/media_session.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/source_code.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "nb/pointer_arithmetic.h"
#include "starboard/window.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
using ::cobalt::script::testing::FakeScriptValue;

class MockErrorCallback : public base::Callback<void(const std::string&)> {
 public:
  MOCK_METHOD1(Run, void(const std::string&));
};

namespace {
class ErrorEventTest : public ::testing::Test {
 public:
  ErrorEventTest()
      : environment_settings_(new script::EnvironmentSettings),
        message_loop_(MessageLoop::TYPE_DEFAULT),
        css_parser_(css_parser::Parser::Create()),
        dom_parser_(new dom_parser::Parser(mock_error_callback_)),
        fetcher_factory_(new loader::FetcherFactory(NULL)),
        local_storage_database_(NULL),
        url_("about:blank"),
        window_(new Window(
            1920, 1080, 1.f, base::kApplicationStateStarted, css_parser_.get(),
            dom_parser_.get(), fetcher_factory_.get(), NULL, NULL, NULL, NULL,
            NULL, NULL, &local_storage_database_, NULL, NULL, NULL, NULL, NULL,
            NULL, NULL, url_, "", "en-US", "en",
            base::Callback<void(const GURL&)>(),
            base::Bind(&MockErrorCallback::Run,
                       base::Unretained(&mock_error_callback_)),
            NULL, network_bridge::PostSender(), csp::kCSPRequired,
            kCspEnforcementEnable, base::Closure() /* csp_policy_changed */,
            base::Closure() /* ran_animation_frame_callbacks */,
            dom::Window::CloseCallback() /* window_close */,
            base::Closure() /* window_minimize */, NULL, NULL, NULL,
            dom::Window::OnStartDispatchEventCallback(),
            dom::Window::OnStopDispatchEventCallback(),
            dom::ScreenshotManager::ProvideScreenshotFunctionCallback(),
            NULL)) {
    engine_ = script::JavaScriptEngine::CreateEngine();
    global_environment_ = engine_->CreateGlobalEnvironment();
    global_environment_->CreateGlobalObject(window_,
                                            environment_settings_.get());
  }

  bool EvaluateScript(const std::string& js_code, std::string* result);

 private:
  scoped_ptr<script::JavaScriptEngine> engine_;
  scoped_refptr<script::GlobalEnvironment> global_environment_;

  const scoped_ptr<script::EnvironmentSettings> environment_settings_;
  MessageLoop message_loop_;
  MockErrorCallback mock_error_callback_;
  scoped_ptr<css_parser::Parser> css_parser_;
  scoped_ptr<dom_parser::Parser> dom_parser_;
  scoped_ptr<loader::FetcherFactory> fetcher_factory_;
  dom::LocalStorageDatabase local_storage_database_;
  GURL url_;
  const scoped_refptr<Window> window_;
};

bool ErrorEventTest::EvaluateScript(const std::string& js_code,
                                    std::string* result) {
  DCHECK(global_environment_);
  DCHECK(result);
  scoped_refptr<script::SourceCode> source_code =
      script::SourceCode::CreateSourceCode(
          js_code, base::SourceLocation(__FILE__, __LINE__, 1));

  global_environment_->EnableEval();
  global_environment_->SetReportEvalCallback(base::Closure());
  bool succeeded = global_environment_->EvaluateScript(source_code, result);
  return succeeded;
}
}  // namespace

TEST_F(ErrorEventTest, ConstructorWithEventTypeString) {
  scoped_refptr<ErrorEvent> event = new ErrorEvent("mytestevent");

  EXPECT_EQ("mytestevent", event->type());
  EXPECT_EQ(NULL, event->target());
  EXPECT_EQ(NULL, event->current_target());
  EXPECT_EQ(Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());
  EXPECT_EQ("", event->message());
  EXPECT_EQ("", event->filename());
  EXPECT_EQ(0, event->lineno());
  EXPECT_EQ(0, event->colno());
  EXPECT_EQ(NULL, event->error());
}

TEST_F(ErrorEventTest, ConstructorWithEventTypeAndDefaultInitDict) {
  ErrorEventInit init;
  scoped_refptr<ErrorEvent> event = new ErrorEvent("mytestevent", init);

  EXPECT_EQ("mytestevent", event->type());
  EXPECT_EQ(NULL, event->target());
  EXPECT_EQ(NULL, event->current_target());
  EXPECT_EQ(Event::kNone, event->event_phase());
  EXPECT_FALSE(event->bubbles());
  EXPECT_FALSE(event->cancelable());
  EXPECT_FALSE(event->default_prevented());
  EXPECT_FALSE(event->IsBeingDispatched());
  EXPECT_FALSE(event->propagation_stopped());
  EXPECT_FALSE(event->immediate_propagation_stopped());
  EXPECT_EQ("", event->message());
  EXPECT_EQ("", event->filename());
  EXPECT_EQ(0, event->lineno());
  EXPECT_EQ(0, event->colno());
  EXPECT_EQ(NULL, event->error());
}

TEST_F(ErrorEventTest, ConstructorWithEventTypeAndErrorInitDict) {
  std::string result;
  bool success = EvaluateScript(
      "var event = new ErrorEvent('dog', "
      "    {'cancelable':true, "
      "     'message':'error_message', "
      "     'filename':'error_filename', "
      "     'lineno':100, "
      "     'colno':50, "
      "     'error':{'cobalt':'rulez'}});"
      "if (event.type == 'dog' &&"
      "    event.bubbles == false &&"
      "    event.cancelable == true &&"
      "    event.message == 'error_message' &&"
      "    event.filename == 'error_filename' &&"
      "    event.lineno == 100 &&"
      "    event.colno == 50) "
      "    event.error.cobalt;",
      &result);
  EXPECT_EQ("rulez", result);

  if (!success) {
    DLOG(ERROR) << "Failed to evaluate test: "
                << "\"" << result << "\"";
  } else {
    LOG(INFO) << "Test result : "
              << "\"" << result << "\"";
  }
}

}  // namespace dom
}  // namespace cobalt
