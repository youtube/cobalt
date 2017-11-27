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

#include "cobalt/webdriver/window_driver.h"

#include <utility>

#include "base/synchronization/waitable_event.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_rect.h"
#include "cobalt/dom/location.h"
#include "cobalt/dom/pointer_event.h"
#include "cobalt/dom/pointer_event_init.h"
#include "cobalt/math/clamp.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/logging_exception_state.h"
#include "cobalt/webdriver/keyboard.h"
#include "cobalt/webdriver/search.h"
#include "cobalt/webdriver/util/call_on_message_loop.h"

namespace cobalt {
namespace webdriver {
namespace {

const int kWebDriverMousePointerId = 0x12345678;

class SyncExecuteResultHandler : public ScriptExecutorResult::ResultHandler {
 public:
  void OnResult(const std::string& result) override {
    DCHECK(!result_);
    result_ = result;
  }
  void OnTimeout() override { NOTREACHED(); }
  std::string result() const {
    DCHECK(result_);
    return result_.value_or(std::string());
  }

 private:
  base::optional<std::string> result_;
};

class AsyncExecuteResultHandler : public ScriptExecutorResult::ResultHandler {
 public:
  AsyncExecuteResultHandler() : timed_out_(false), event_(true, false) {}

  void WaitForResult() { event_.Wait(); }
  bool timed_out() const { return timed_out_; }
  const std::string& result() const { return result_; }

 private:
  void OnResult(const std::string& result) override {
    result_ = result;
    event_.Signal();
  }
  void OnTimeout() override {
    timed_out_ = true;
    event_.Signal();
  }

  std::string result_;
  bool timed_out_;
  base::WaitableEvent event_;
};

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
  return window->document()->document_element()->outer_html(NULL);
}

std::vector<protocol::Cookie> GetAllCookies(dom::Window* window) {
  DCHECK(window);
  DCHECK(window->document());
  std::string cookies_string = window->document()->cookie();
  std::vector<protocol::Cookie> cookies;
  protocol::Cookie::ToCookieVector(cookies_string, &cookies);
  return cookies;
}

}  // namespace

WindowDriver::WindowDriver(
    const protocol::WindowId& window_id,
    const base::WeakPtr<dom::Window>& window,
    const GetGlobalEnvironmentFunction& get_global_environment_function,
    KeyboardEventInjector keyboard_event_injector,
    PointerEventInjector pointer_event_injector,
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : window_id_(window_id),
      window_(window),
      get_global_environment_(get_global_environment_function),
      keyboard_event_injector_(keyboard_event_injector),
      pointer_event_injector_(pointer_event_injector),
      window_message_loop_(message_loop),
      element_driver_map_deleter_(&element_drivers_),
      next_element_id_(0),
      pointer_buttons_(0),
      pointer_x_(0),
      pointer_y_(0) {
  // The WindowDriver may have been created on some arbitrary thread (i.e. the
  // thread that owns the Window). Detach the thread checker so it can be
  // re-bound to the next thread that calls a webdriver API, which should be
  // the WebDriver thread.
  thread_checker_.DetachFromThread();
}

WindowDriver::~WindowDriver() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

ElementDriver* WindowDriver::GetElementDriver(
    const protocol::ElementId& element_id) {
  if (base::MessageLoopProxy::current() != window_message_loop_) {
    // It's expected that the WebDriver thread is the only other thread to call
    // this function.
    DCHECK(thread_checker_.CalledOnValidThread());
    ElementDriver* result;
    bool success = util::TryCallOnMessageLoop(
        window_message_loop_,
        base::Bind(&WindowDriver::GetElementDriver, base::Unretained(this),
                   element_id),
        &result);
    return success ? result : NULL;
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

util::CommandResult<void> WindowDriver::Navigate(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::CallOnMessageLoop(
      window_message_loop_,
      base::Bind(&WindowDriver::NavigateInternal, base::Unretained(this), url),
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

  return util::CallOnMessageLoop(
      window_message_loop_,
      base::Bind(&WindowDriver::FindElementsInternal<protocol::ElementId>,
                 base::Unretained(this), strategy),
      protocol::Response::kNoSuchElement);
}

util::CommandResult<std::vector<protocol::ElementId> >
WindowDriver::FindElements(const protocol::SearchStrategy& strategy) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::CallOnMessageLoop(
      window_message_loop_,
      base::Bind(&WindowDriver::FindElementsInternal<ElementIdVector>,
                 base::Unretained(this), strategy),
      protocol::Response::kNoSuchElement);
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
  typedef util::CommandResult<protocol::ScriptResult> CommandResult;
  DCHECK(thread_checker_.CalledOnValidThread());
  // Pre-load the ScriptExecutor source so we don't hit the disk on
  // window_message_loop_.
  ScriptExecutor::LoadExecutorSourceCode();

  SyncExecuteResultHandler result_handler;

  CommandResult result = util::CallOnMessageLoop(
      window_message_loop_,
      base::Bind(&WindowDriver::ExecuteScriptInternal, base::Unretained(this),
                 script, base::nullopt, &result_handler),
      protocol::Response::kNoSuchWindow);
  if (result.is_success()) {
    return CommandResult(protocol::ScriptResult(result_handler.result()));
  } else {
    return result;
  }
}

util::CommandResult<protocol::ScriptResult> WindowDriver::ExecuteAsync(
    const protocol::Script& script) {
  typedef util::CommandResult<protocol::ScriptResult> CommandResult;
  DCHECK(thread_checker_.CalledOnValidThread());
  // Pre-load the ScriptExecutor source so we don't hit the disk on
  // window_message_loop_.
  ScriptExecutor::LoadExecutorSourceCode();

  const base::TimeDelta kDefaultAsyncTimeout =
      base::TimeDelta::FromMilliseconds(0);
  AsyncExecuteResultHandler result_handler;
  CommandResult result = util::CallOnMessageLoop(
      window_message_loop_,
      base::Bind(&WindowDriver::ExecuteScriptInternal, base::Unretained(this),
                 script, kDefaultAsyncTimeout, &result_handler),
      protocol::Response::kNoSuchWindow);

  if (!result.is_success()) {
    return result;
  }

  result_handler.WaitForResult();
  if (result_handler.timed_out()) {
    return CommandResult(protocol::Response::kTimeOut);
  } else {
    return CommandResult(protocol::ScriptResult(result_handler.result()));
  }
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
                 base::Passed(&events)),
      protocol::Response::kNoSuchWindow);
}

util::CommandResult<protocol::ElementId> WindowDriver::GetActiveElement() {
  return util::CallOnMessageLoop(
      window_message_loop_,
      base::Bind(&WindowDriver::GetActiveElementInternal,
                 base::Unretained(this)),
      protocol::Response::kNoSuchWindow);
}

util::CommandResult<void> WindowDriver::SwitchFrame(
    const protocol::FrameId& frame_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Cobalt doesn't support frames, but if a WebDriver client requests to
  // switch to the top-level browsing context, trivially return success.
  if (frame_id.is_top_level_browsing_context()) {
    return util::CommandResult<void>(protocol::Response::kSuccess);
  } else {
    return util::CommandResult<void>(protocol::Response::kNoSuchFrame);
  }
}

util::CommandResult<std::vector<protocol::Cookie> >
WindowDriver::GetAllCookies() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::CallWeakOnMessageLoopAndReturnResult(
      window_message_loop_,
      base::Bind(&WindowDriver::GetWeak, base::Unretained(this)),
      base::Bind(&::cobalt::webdriver::GetAllCookies),
      protocol::Response::kNoSuchWindow);
}

util::CommandResult<std::vector<protocol::Cookie> > WindowDriver::GetCookie(
    const std::string& name) {
  typedef util::CommandResult<std::vector<protocol::Cookie> > CommandResult;
  CommandResult command_result = GetAllCookies();
  if (command_result.is_success()) {
    std::vector<protocol::Cookie> filtered_cookies;
    for (size_t i = 0; i < command_result.result().size(); ++i) {
      if (command_result.result()[i].name() == name) {
        filtered_cookies.push_back(command_result.result()[i]);
        break;
      }
    }
    command_result = CommandResult(filtered_cookies);
  }
  return command_result;
}

util::CommandResult<void> WindowDriver::AddCookie(
    const protocol::Cookie& cookie) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::CallOnMessageLoop(window_message_loop_,
                                 base::Bind(&WindowDriver::AddCookieInternal,
                                            base::Unretained(this), cookie),
                                 protocol::Response::kNoSuchWindow);
}

util::CommandResult<void> WindowDriver::MouseMoveTo(
    const protocol::Moveto& moveto) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::CallOnMessageLoop(window_message_loop_,
                                 base::Bind(&WindowDriver::MouseMoveToInternal,
                                            base::Unretained(this), moveto),
                                 protocol::Response::kNoSuchWindow);
}

util::CommandResult<void> WindowDriver::MouseButtonDown(
    const protocol::Button& button) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::CallOnMessageLoop(
      window_message_loop_,
      base::Bind(&WindowDriver::MouseButtonDownInternal, base::Unretained(this),
                 button),
      protocol::Response::kNoSuchWindow);
}

util::CommandResult<void> WindowDriver::MouseButtonUp(
    const protocol::Button& button) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::CallOnMessageLoop(
      window_message_loop_,
      base::Bind(&WindowDriver::MouseButtonUpInternal, base::Unretained(this),
                 button),
      protocol::Response::kNoSuchWindow);
}

util::CommandResult<void> WindowDriver::SendClick(
    const protocol::Button& button) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::CallOnMessageLoop(window_message_loop_,
                                 base::Bind(&WindowDriver::SendClickInternal,
                                            base::Unretained(this), button),
                                 protocol::Response::kNoSuchWindow);
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
          element_id.id(),
          new ElementDriver(element_id, weak_element, this,
                            keyboard_event_injector_,
                            base::Bind(&WindowDriver::InjectPointerEvent,
                                       base::Unretained(this)),
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
    const protocol::Script& script,
    base::optional<base::TimeDelta> async_timeout,
    ScriptExecutorResult::ResultHandler* async_handler) {
  typedef util::CommandResult<protocol::ScriptResult> CommandResult;
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  if (!window_) {
    return CommandResult(protocol::Response::kNoSuchWindow);
  }

  scoped_refptr<script::GlobalEnvironment> global_environment =
      get_global_environment_.Run();
  DCHECK(global_environment);

  // Lazily initialize this the first time we need to run a script. It must be
  // initialized on window_message_loop_. It can persist across multiple calls
  // to execute script, but must be destroyed along with the associated
  // global object, thus with the WindowDriver.
  if (!script_executor_) {
    scoped_refptr<ScriptExecutor> script_executor =
        ScriptExecutor::Create(this, global_environment);
    if (!script_executor) {
      DLOG(INFO) << "Failed to create ScriptExecutor.";
      return CommandResult(protocol::Response::kUnknownError);
    }
    script_executor_ = base::AsWeakPtr(script_executor.get());
  }

  DLOG(INFO) << "Executing: " << script.function_body();
  DLOG(INFO) << "Arguments: " << script.argument_array();

  auto gc_prevented_params =
      ScriptExecutorParams::Create(global_environment, script.function_body(),
                                   script.argument_array(), async_timeout);

  if (gc_prevented_params.params->function_object()) {
    if (script_executor_->Execute(gc_prevented_params.params, async_handler)) {
      return CommandResult(protocol::Response::kSuccess);
    }
  }
  return CommandResult(protocol::Response::kJavaScriptError);
}

util::CommandResult<void> WindowDriver::SendKeysInternal(
    scoped_ptr<Keyboard::KeyboardEventVector> events) {
  typedef util::CommandResult<void> CommandResult;
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  if (!window_) {
    return CommandResult(protocol::Response::kNoSuchWindow);
  }

  for (size_t i = 0; i < events->size(); ++i) {
    keyboard_event_injector_.Run(scoped_refptr<dom::Element>(),
                                 (*events)[i].first, (*events)[i].second);
  }
  return CommandResult(protocol::Response::kSuccess);
}

util::CommandResult<void> WindowDriver::NavigateInternal(const GURL& url) {
  typedef util::CommandResult<void> CommandResult;
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  if (!window_) {
    return CommandResult(protocol::Response::kNoSuchWindow);
  }
  window_->location()->Replace(url.spec());
  return CommandResult(protocol::Response::kSuccess);
}

util::CommandResult<void> WindowDriver::AddCookieInternal(
    const protocol::Cookie& cookie) {
  typedef util::CommandResult<void> CommandResult;
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  if (!window_) {
    return CommandResult(protocol::Response::kNoSuchWindow);
  }
  // If the domain was set, ensure that it is valid with respect to the
  // the document's domain.
  std::string document_domain = window_->document()->location()->host();

  if (cookie.domain()) {
    // This is the same way the domain is checked in FirefoxDriver.
    if (document_domain.find(cookie.domain().value()) == std::string::npos) {
      return CommandResult(protocol::Response::kInvalidCookieDomain);
    }
  }

  std::string cookie_string = cookie.ToCookieString(document_domain);
  window_->document()->set_cookie(cookie_string);
  return CommandResult(protocol::Response::kSuccess);
}

void WindowDriver::InitPointerEvent(dom::PointerEventInit* event) {
  event->set_buttons(pointer_buttons_);

  event->set_screen_x(pointer_x_);
  event->set_screen_y(pointer_y_);
  event->set_client_x(pointer_x_);
  event->set_client_y(pointer_y_);

  event->set_pointer_type("mouse");
  event->set_pointer_id(kWebDriverMousePointerId);
#if SB_API_VERSION >= 6
  event->set_width(0.0f);
  event->set_height(0.0f);
  event->set_pressure(pointer_buttons_ ? 0.5f : 0.0f);
  event->set_tilt_x(0.0f);
  event->set_tilt_y(0.0f);
#endif
  event->set_is_primary(true);
}

void WindowDriver::InjectPointerEvent(scoped_refptr<dom::Element> element,
                                      base::Token type,
                                      const dom::PointerEventInit& event) {
  pointer_x_ = event.screen_x();
  pointer_y_ = event.screen_y();
  pointer_event_injector_.Run(element, type, event);
}

util::CommandResult<void> WindowDriver::MouseMoveToInternal(
    const protocol::Moveto& moveto) {
  typedef util::CommandResult<void> CommandResult;
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  if (!window_) {
    return CommandResult(protocol::Response::kNoSuchWindow);
  }

  // Move the mouse by an offset of the specified element. If no element is
  // specified, the move is relative to the current mouse cursor. If an element
  // is provided but no offset, the mouse will be moved to the center of the
  // element.
  //   https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#sessionsessionidmoveto
  float x = 0;
  float y = 0;
  scoped_refptr<dom::Element> element;
  const base::optional<protocol::ElementId>& element_id = moveto.element();
  if (element_id) {
    // The element to move to.
    element = IdToElement(*element_id);
  }
  if (element) {
    scoped_refptr<dom::DOMRect> rect = element->GetBoundingClientRect();
    if (rect) {
      if (moveto.xoffset()) {
        // X offset to move to, relative to the top-left corner of the element.
        x = rect->left() + *moveto.xoffset();
      } else {
        // If not specified, the mouse will move to the middle of the element.
        x = rect->left() + rect->width() / 2;
      }

      if (moveto.yoffset()) {
        // Y offset to move to, relative to the top-left corner of the element.
        y = rect->top() + *moveto.yoffset();
      } else {
        // If not specified, the mouse will move to the middle of the element.
        y = rect->top() + rect->height() / 2;
      }
    }
  } else {
    // If the element not specified or is null, the offset is relative to
    // current position of the mouse.
    x = pointer_x_;
    y = pointer_y_;
    if (moveto.xoffset()) {
      x += *moveto.xoffset();
    }
    if (moveto.yoffset()) {
      y += *moveto.yoffset();
    }
  }

  pointer_x_ = math::Clamp(x, 0.0f, GetWeak()->inner_width());
  pointer_y_ = math::Clamp(y, 0.0f, GetWeak()->inner_height());

  dom::PointerEventInit event;
  InitPointerEvent(&event);
  pointer_event_injector_.Run(scoped_refptr<dom::Element>(),
                              base::Tokens::pointermove(), event);

  return CommandResult(protocol::Response::kSuccess);
}

void WindowDriver::InjectMouseButtonUp(const protocol::Button& button) {
  dom::PointerEventInit event;
  event.set_button(button.button());

  // The buttons attribute reflects the state of the mouse's buttons for any
  // MouseEvent object (while it is being dispatched).
  //   https://www.w3.org/TR/2016/WD-uievents-20160804/#ref-for-dom-mouseevent-buttons-2
  // Clear the buttons state bit corresponding to the button that has just been
  // released.
  pointer_buttons_ &= ~(1 << event.button());

  InitPointerEvent(&event);

  pointer_event_injector_.Run(scoped_refptr<dom::Element>(),
                              base::Tokens::pointerup(), event);
}

void WindowDriver::InjectMouseButtonDown(const protocol::Button& button) {
  dom::PointerEventInit event;
  event.set_button(button.button());

  // The buttons attribute reflects the state of the mouse's buttons for any
  // MouseEvent object (while it is being dispatched).
  //   https://www.w3.org/TR/2016/WD-uievents-20160804/#ref-for-dom-mouseevent-buttons-2
  // Set the buttons state bit corresponding to the button that has just been
  // pressed.
  pointer_buttons_ |= 1 << event.button();

  InitPointerEvent(&event);

  pointer_event_injector_.Run(scoped_refptr<dom::Element>(),
                              base::Tokens::pointerdown(), event);
}

util::CommandResult<void> WindowDriver::MouseButtonDownInternal(
    const protocol::Button& button) {
  typedef util::CommandResult<void> CommandResult;
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  if (!window_) {
    return CommandResult(protocol::Response::kNoSuchWindow);
  }

  InjectMouseButtonDown(button);

  return CommandResult(protocol::Response::kSuccess);
}

util::CommandResult<void> WindowDriver::MouseButtonUpInternal(
    const protocol::Button& button) {
  typedef util::CommandResult<void> CommandResult;
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  if (!window_) {
    return CommandResult(protocol::Response::kNoSuchWindow);
  }

  InjectMouseButtonUp(button);

  return CommandResult(protocol::Response::kSuccess);
}

util::CommandResult<void> WindowDriver::SendClickInternal(
    const protocol::Button& button) {
  typedef util::CommandResult<void> CommandResult;
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  if (!window_) {
    return CommandResult(protocol::Response::kNoSuchWindow);
  }

  InjectMouseButtonDown(button);
  InjectMouseButtonUp(button);

  return CommandResult(protocol::Response::kSuccess);
}

util::CommandResult<protocol::ElementId>
WindowDriver::GetActiveElementInternal() {
  typedef util::CommandResult<protocol::ElementId> CommandResult;
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  if (!window_) {
    return CommandResult(protocol::Response::kNoSuchWindow);
  }
  scoped_refptr<dom::Element> active = window_->document()->active_element();
  if (active) {
    return CommandResult(ElementToId(active));
  } else {
    return CommandResult(protocol::Response::kUnknownError);
  }
}

}  // namespace webdriver
}  // namespace cobalt
