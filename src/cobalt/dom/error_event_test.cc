// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/threading/platform_thread.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/dom/error_event_init.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/testing/gtest_workarounds.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/loader_factory.h"
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

using cobalt::cssom::ViewportSize;
using cobalt::script::testing::FakeScriptValue;

namespace cobalt {
namespace dom {

class MockLoadCompleteCallback
    : public base::Callback<void(const base::Optional<std::string>&)> {
 public:
  MOCK_METHOD1(Run, void(const base::Optional<std::string>&));
};

namespace {
class ErrorEventTest : public ::testing::Test {
 public:
  ErrorEventTest()
      : message_loop_(base::MessageLoop::TYPE_DEFAULT),
        environment_settings_(new testing::StubEnvironmentSettings),
        css_parser_(css_parser::Parser::Create()),
        dom_parser_(new dom_parser::Parser(mock_load_complete_callback_)),
        fetcher_factory_(new loader::FetcherFactory(NULL)),
        loader_factory_(new loader::LoaderFactory(
            "Test", fetcher_factory_.get(), NULL, null_debugger_hooks_, 0,
            base::ThreadPriority::DEFAULT)),
        local_storage_database_(NULL),
        url_("about:blank") {
    engine_ = script::JavaScriptEngine::CreateEngine();
    global_environment_ = engine_->CreateGlobalEnvironment();

    ViewportSize view_size(1920, 1080);
    window_ = new Window(
        environment_settings_.get(), view_size, base::kApplicationStateStarted,
        css_parser_.get(), dom_parser_.get(), fetcher_factory_.get(),
        loader_factory_.get(), NULL, NULL, NULL, NULL, NULL, NULL,
        &local_storage_database_, NULL, NULL, NULL, NULL,
        global_environment_->script_value_factory(), NULL, NULL, url_, "", NULL,
        "en-US", "en", base::Callback<void(const GURL&)>(),
        base::Bind(&MockLoadCompleteCallback::Run,
                   base::Unretained(&mock_load_complete_callback_)),
        NULL, network_bridge::PostSender(), csp::kCSPRequired,
        kCspEnforcementEnable, base::Closure() /* csp_policy_changed */,
        base::Closure() /* ran_animation_frame_callbacks */,
        dom::Window::CloseCallback() /* window_close */,
        base::Closure() /* window_minimize */, NULL, NULL,
        dom::Window::OnStartDispatchEventCallback(),
        dom::Window::OnStopDispatchEventCallback(),
        dom::ScreenshotManager::ProvideScreenshotFunctionCallback(), NULL);

    global_environment_->CreateGlobalObject(window_,
                                            environment_settings_.get());
  }

  bool EvaluateScript(const std::string& js_code, std::string* result);

 private:
  base::MessageLoop message_loop_;
  base::NullDebuggerHooks null_debugger_hooks_;
  std::unique_ptr<script::JavaScriptEngine> engine_;
  const std::unique_ptr<testing::StubEnvironmentSettings> environment_settings_;
  scoped_refptr<script::GlobalEnvironment> global_environment_;
  MockLoadCompleteCallback mock_load_complete_callback_;
  std::unique_ptr<css_parser::Parser> css_parser_;
  std::unique_ptr<dom_parser::Parser> dom_parser_;
  std::unique_ptr<loader::FetcherFactory> fetcher_factory_;
  std::unique_ptr<loader::LoaderFactory> loader_factory_;
  dom::LocalStorageDatabase local_storage_database_;
  GURL url_;
  scoped_refptr<Window> window_;
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
  EXPECT_EQ(NULL, event->target().get());
  EXPECT_EQ(NULL, event->current_target().get());
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
  EXPECT_EQ(NULL, event->target().get());
  EXPECT_EQ(NULL, event->current_target().get());
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
