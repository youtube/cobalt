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

#include "cobalt/webdriver/window_driver.h"

#include <utility>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/location.h"
#include "cobalt/script/global_object_proxy.h"
#include "cobalt/script/source_code.h"
#include "cobalt/webdriver/keyboard.h"
#include "cobalt/webdriver/search.h"
#include "cobalt/webdriver/util/call_on_message_loop.h"

namespace cobalt {
namespace webdriver {
namespace {
// Path to the script to initialize the script execution harness.
const char kWebDriverInitScriptPath[] = "webdriver/webdriver-init.js";

// Wrapper around a scoped_refptr<script::SourceCode> instance. The script
// at kWebDriverInitScriptPath will be loaded from disk and a new
// script::SourceCode will be created.
class LazySourceLoader {
 public:
  LazySourceLoader() {
    FilePath exe_path;
    if (!PathService::Get(base::DIR_EXE, &exe_path)) {
      NOTREACHED() << "Failed to get EXE path.";
      return;
    }
    FilePath script_path = exe_path.Append(kWebDriverInitScriptPath);
    std::string script_contents;
    if (!file_util::ReadFileToString(script_path, &script_contents)) {
      NOTREACHED() << "Failed to read script contents.";
      return;
    }
    source_code_ = script::SourceCode::CreateSourceCode(
        script_contents.c_str(),
        base::SourceLocation(kWebDriverInitScriptPath, 1, 1));
  }
  const scoped_refptr<script::SourceCode>& source_code() {
    return source_code_;
  }

 private:
  scoped_refptr<script::SourceCode> source_code_;
};

// The script only needs to be loaded once, so allow it to persist as a
// LazyInstance and be shared amongst different WindowDriver instances.
base::LazyInstance<LazySourceLoader> lazy_source_loader =
    LAZY_INSTANCE_INITIALIZER;

scoped_refptr<ScriptExecutor> CreateScriptExecutor(
    ElementMapping* element_mapping,
    const scoped_refptr<script::GlobalObjectProxy>& global_object_proxy) {
  // This could be NULL if there was an error loading the harness source from
  // disk.
  scoped_refptr<script::SourceCode> source =
      lazy_source_loader.Get().source_code();
  if (!source) {
    return NULL;
  }

  // Create a new ScriptExecutor and bind it to the global object.
  scoped_refptr<ScriptExecutor> script_executor =
      new ScriptExecutor(element_mapping);
  global_object_proxy->Bind("webdriverExecutor", script_executor);

  // Evaluate the harness initialization script.
  std::string result;
  if (!global_object_proxy->EvaluateScript(source, &result)) {
    return NULL;
  }

  // The initialization script should have set this.
  DCHECK(script_executor->execute_script_harness());
  return script_executor;
}

std::string GetCurrentUrl(dom::Window* window) {
  DCHECK(window);
  DCHECK(window->location());
  return window->location()->href();
}

std::string GetTitle(dom::Window* window) {
  DCHECK(window);
  DCHECK(window->document());
  return window->document()->title();
}

protocol::Size GetWindowSize(dom::Window* window) {
  DCHECK(window);
  float width = window->outer_width();
  float height = window->outer_height();
  return protocol::Size(width, height);
}

std::string GetSource(dom::Window* window) {
  DCHECK(window);
  DCHECK(window->document());
  DCHECK(window->document()->document_element());
  return window->document()->document_element()->outer_html();
}

}  // namespace

WindowDriver::WindowDriver(
    const protocol::WindowId& window_id,
    const base::WeakPtr<dom::Window>& window,
    const scoped_refptr<script::GlobalObjectProxy>& global_object,
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : window_id_(window_id),
      window_(window),
      global_object_proxy_(global_object),
      window_message_loop_(message_loop),
      element_driver_map_deleter_(&element_drivers_),
      next_element_id_(0) {}

WindowDriver::~WindowDriver() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

ElementDriver* WindowDriver::GetElementDriver(
    const protocol::ElementId& element_id) {
  if (base::MessageLoopProxy::current() != window_message_loop_) {
    // It's expected that the WebDriver thread is the only other thread to call
    // this function.
    DCHECK(thread_checker_.CalledOnValidThread());
    return util::CallOnMessageLoop(window_message_loop_,
        base::Bind(&WindowDriver::GetElementDriver, base::Unretained(this),
                   element_id));
  }
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  ElementDriverMap::iterator it = element_drivers_.find(element_id.id());
  if (it != element_drivers_.end()) {
    return it->second;
  }
  return NULL;
}

util::CommandResult<protocol::Size> WindowDriver::GetWindowSize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::CallWeakOnMessageLoopAndReturnResult(
      window_message_loop_,
      base::Bind(&WindowDriver::GetWeak, base::Unretained(this)),
      base::Bind(&::cobalt::webdriver::GetWindowSize),
      protocol::Response::kNoSuchWindow);
}

util::CommandResult<std::string> WindowDriver::GetCurrentUrl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::CallWeakOnMessageLoopAndReturnResult(
      window_message_loop_,
      base::Bind(&WindowDriver::GetWeak, base::Unretained(this)),
      base::Bind(&::cobalt::webdriver::GetCurrentUrl),
      protocol::Response::kNoSuchWindow);
}

util::CommandResult<std::string> WindowDriver::GetTitle() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::CallWeakOnMessageLoopAndReturnResult(
      window_message_loop_,
      base::Bind(&WindowDriver::GetWeak, base::Unretained(this)),
      base::Bind(&::cobalt::webdriver::GetTitle),
      protocol::Response::kNoSuchWindow);
}

util::CommandResult<protocol::ElementId> WindowDriver::FindElement(
    const protocol::SearchStrategy& strategy) {
  DCHECK(thread_checker_.CalledOnValidThread());

  return util::CallOnMessageLoop(window_message_loop_,
      base::Bind(&WindowDriver::FindElementsInternal<protocol::ElementId>,
                 base::Unretained(this), strategy));
}

util::CommandResult<std::vector<protocol::ElementId> >
WindowDriver::FindElements(const protocol::SearchStrategy& strategy) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::CallOnMessageLoop(window_message_loop_,
      base::Bind(&WindowDriver::FindElementsInternal<ElementIdVector>,
                 base::Unretained(this), strategy));
}

util::CommandResult<std::string> WindowDriver::GetSource() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::CallWeakOnMessageLoopAndReturnResult(
      window_message_loop_,
      base::Bind(&WindowDriver::GetWeak, base::Unretained(this)),
      base::Bind(&::cobalt::webdriver::GetSource),
      protocol::Response::kNoSuchWindow);
}

util::CommandResult<protocol::ScriptResult> WindowDriver::Execute(
    const protocol::Script& script) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Poke the lazy loader so we don't hit the disk on window_message_loop_.
  lazy_source_loader.Get();
  return util::CallOnMessageLoop(
      window_message_loop_, base::Bind(&WindowDriver::ExecuteScriptInternal,
                                       base::Unretained(this), script));
}

util::CommandResult<void> WindowDriver::SendKeys(const protocol::Keys& keys) {
  // Translate the keys into KeyboardEvents. Don't reset modifiers.
  scoped_ptr<Keyboard::KeyboardEventVector> events(
      new Keyboard::KeyboardEventVector());
  Keyboard::TranslateToKeyEvents(keys.utf8_keys(), Keyboard::kKeepModifiers,
                                 events.get());
  // Dispatch the keyboard events.
  return util::CallOnMessageLoop(
      window_message_loop_,
      base::Bind(&WindowDriver::SendKeysInternal, base::Unretained(this),
                 base::Passed(&events)));
}

protocol::ElementId WindowDriver::ElementToId(
    const scoped_refptr<dom::Element>& element) {
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  return CreateNewElementDriver(base::AsWeakPtr(element.get()));
}

scoped_refptr<dom::Element> WindowDriver::IdToElement(
    const protocol::ElementId& id) {
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  return make_scoped_refptr(GetElementDriver(id)->GetWeakElement());
}

protocol::ElementId WindowDriver::CreateNewElementDriver(
    const base::WeakPtr<dom::Element>& weak_element) {
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);

  protocol::ElementId element_id(
      base::StringPrintf("element-%d", next_element_id_++));
  std::pair<ElementDriverMapIt, bool> pair_it =
      element_drivers_.insert(std::make_pair(
          element_id.id(), new ElementDriver(element_id, weak_element, this,
                                             window_message_loop_)));
  DCHECK(pair_it.second)
      << "An ElementDriver was already mapped to the element id: "
      << element_id.id();
  return element_id;
}

// Internal logic for FindElement and FindElements that must be run on the
// Window's message loop.
template <typename T>
util::CommandResult<T> WindowDriver::FindElementsInternal(
    const protocol::SearchStrategy& strategy) {
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  typedef util::CommandResult<T> CommandResult;
  if (!window_) {
    return CommandResult(protocol::Response::kNoSuchWindow);
  }
  return Search::FindElementsUnderNode<T>(strategy, window_->document().get(),
                                          this);
}

util::CommandResult<protocol::ScriptResult> WindowDriver::ExecuteScriptInternal(
    const protocol::Script& script) {
  typedef util::CommandResult<protocol::ScriptResult> CommandResult;
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  if (!window_) {
    return CommandResult(protocol::Response::kNoSuchWindow);
  }

  // Lazily initialize this the first time we need to run a script. It must be
  // initialized on window_message_loop_. It can persist across multiple calls
  // to execute script, but must be destroyed along with the associated
  // global object, thus with the WindowDriver.
  if (!script_executor_) {
    scoped_refptr<ScriptExecutor> script_executor =
        CreateScriptExecutor(this, global_object_proxy_);
    if (!script_executor) {
      DLOG(INFO) << "Failed to create ScriptExecutor.";
      return CommandResult(protocol::Response::kUnknownError);
    }
    script_executor_ = base::AsWeakPtr(script_executor.get());
  }

  DLOG(INFO) << "Executing: " << script.function_body();
  DLOG(INFO) << "Arguments: " << script.argument_array();

  base::optional<std::string> script_result = script_executor_->Execute(
      script.function_body(), script.argument_array());
  if (script_result) {
    return CommandResult(protocol::ScriptResult(script_result.value()));
  } else {
    return CommandResult(protocol::Response::kJavaScriptError);
  }
}

util::CommandResult<void> WindowDriver::SendKeysInternal(
    scoped_ptr<KeyboardEventVector> events) {
  typedef util::CommandResult<void> CommandResult;
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  if (!window_) {
    return CommandResult(protocol::Response::kNoSuchWindow);
  }

  for (size_t i = 0; i < events->size(); ++i) {
    // InjectEvent will send to the focused element.
    window_->InjectEvent((*events)[i]);
  }
  return CommandResult(protocol::Response::kSuccess);
}

}  // namespace webdriver
}  // namespace cobalt
