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

#ifndef COBALT_WEBDRIVER_WINDOW_DRIVER_H_
#define COBALT_WEBDRIVER_WINDOW_DRIVER_H_

#if defined(ENABLE_WEBDRIVER)

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cobalt/dom/keyboard_event.h"
#include "cobalt/dom/pointer_event.h"
#include "cobalt/dom/pointer_event_init.h"
#include "cobalt/dom/window.h"
#include "cobalt/webdriver/element_driver.h"
#include "cobalt/webdriver/element_mapping.h"
#include "cobalt/webdriver/keyboard.h"
#include "cobalt/webdriver/protocol/button.h"
#include "cobalt/webdriver/protocol/cookie.h"
#include "cobalt/webdriver/protocol/element_id.h"
#include "cobalt/webdriver/protocol/frame_id.h"
#include "cobalt/webdriver/protocol/keys.h"
#include "cobalt/webdriver/protocol/moveto.h"
#include "cobalt/webdriver/protocol/script.h"
#include "cobalt/webdriver/protocol/search_strategy.h"
#include "cobalt/webdriver/protocol/size.h"
#include "cobalt/webdriver/protocol/window_id.h"
#include "cobalt/webdriver/script_executor.h"
#include "cobalt/webdriver/util/call_on_message_loop.h"
#include "cobalt/webdriver/util/command_result.h"

namespace cobalt {
namespace webdriver {

// A WebDriver windowHandle will map to a WindowDriver instance.
// WebDriver commands that interact with a Window, such as:
//     /session/:sessionId/window/:windowHandle/size
// will map to a method on this class.
class WindowDriver : private ElementMapping {
 public:
  typedef base::Callback<void(scoped_refptr<dom::Element>, const base::Token,
                              const dom::KeyboardEventInit&)>
      KeyboardEventInjector;
  typedef base::Callback<void(scoped_refptr<dom::Element>, const base::Token,
                              const dom::PointerEventInit&)>
      PointerEventInjector;

  typedef base::Callback<scoped_refptr<script::GlobalEnvironment>()>
      GetGlobalEnvironmentFunction;
  WindowDriver(const protocol::WindowId& window_id,
               const base::WeakPtr<dom::Window>& window,
               const GetGlobalEnvironmentFunction& get_global_environment,
               KeyboardEventInjector keyboard_event_injector,
               PointerEventInjector pointer_event_injector,
               const scoped_refptr<base::MessageLoopProxy>& message_loop);
  ~WindowDriver();
  const protocol::WindowId& window_id() { return window_id_; }
  ElementDriver* GetElementDriver(const protocol::ElementId& element_id);

  util::CommandResult<protocol::Size> GetWindowSize();
  util::CommandResult<void> Navigate(const GURL& url);
  util::CommandResult<std::string> GetCurrentUrl();
  util::CommandResult<std::string> GetTitle();
  util::CommandResult<protocol::ElementId> FindElement(
      const protocol::SearchStrategy& strategy);
  util::CommandResult<std::vector<protocol::ElementId> > FindElements(
      const protocol::SearchStrategy& strategy);
  // Note: The source may be a fairly large string that is being copied around
  // by value here.
  util::CommandResult<std::string> GetSource();
  util::CommandResult<protocol::ScriptResult> Execute(
      const protocol::Script& script);
  util::CommandResult<protocol::ScriptResult> ExecuteAsync(
      const protocol::Script& script);
  util::CommandResult<void> SendKeys(const protocol::Keys& keys);
  util::CommandResult<protocol::ElementId> GetActiveElement();
  util::CommandResult<void> SwitchFrame(const protocol::FrameId& frame_id);
  util::CommandResult<std::vector<protocol::Cookie> > GetAllCookies();
  util::CommandResult<std::vector<protocol::Cookie> > GetCookie(
      const std::string& name);
  util::CommandResult<void> AddCookie(const protocol::Cookie& cookie);
  util::CommandResult<void> MouseMoveTo(const protocol::Moveto& moveto);
  util::CommandResult<void> MouseButtonDown(const protocol::Button& button);
  util::CommandResult<void> MouseButtonUp(const protocol::Button& button);
  util::CommandResult<void> SendClick(const protocol::Button& button);

 private:
  typedef base::hash_map<std::string, ElementDriver*> ElementDriverMap;
  typedef ElementDriverMap::iterator ElementDriverMapIt;
  typedef std::vector<protocol::ElementId> ElementIdVector;

  // ScriptExecutor::ElementMapping implementation.
  protocol::ElementId ElementToId(
      const scoped_refptr<dom::Element>& element) override;
  scoped_refptr<dom::Element> IdToElement(
      const protocol::ElementId& id) override;

  dom::Window* GetWeak() {
    DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
    return window_.get();
  }

  // Create a new ElementDriver that wraps this weak_element and return the
  // ElementId that maps to the new ElementDriver.
  protocol::ElementId CreateNewElementDriver(
      const base::WeakPtr<dom::Element>& weak_element);

  // Shared logic between FindElement and FindElements.
  template <typename T>
  util::CommandResult<T> FindElementsInternal(
      const protocol::SearchStrategy& strategy);

  util::CommandResult<protocol::ScriptResult> ExecuteScriptInternal(
      const protocol::Script& script,
      base::optional<base::TimeDelta> async_timeout,
      ScriptExecutorResult::ResultHandler* result_handler);

  util::CommandResult<void> SendKeysInternal(
      scoped_ptr<Keyboard::KeyboardEventVector> keyboard_events);

  util::CommandResult<void> NavigateInternal(const GURL& url);

  util::CommandResult<void> AddCookieInternal(const protocol::Cookie& cookie);

  void InitPointerEvent(dom::PointerEventInit* event);

  // Used to receive pointer positions from events injected from an
  // ElementDriver.
  void InjectPointerEvent(scoped_refptr<dom::Element> element, base::Token type,
                          const dom::PointerEventInit& event);

  util::CommandResult<void> MouseMoveToInternal(const protocol::Moveto& moveto);

  void InjectMouseButtonUp(const protocol::Button& button);

  void InjectMouseButtonDown(const protocol::Button& button);

  util::CommandResult<void> MouseButtonDownInternal(
      const protocol::Button& button);

  util::CommandResult<void> MouseButtonUpInternal(
      const protocol::Button& button);

  util::CommandResult<void> SendClickInternal(const protocol::Button& button);

  util::CommandResult<protocol::ElementId> GetActiveElementInternal();

  const protocol::WindowId window_id_;

  // Bound to the WebDriver thread.
  base::ThreadChecker thread_checker_;

  KeyboardEventInjector keyboard_event_injector_;
  PointerEventInjector pointer_event_injector_;

  // Anything that interacts with the window must be run on this message loop.
  scoped_refptr<base::MessageLoopProxy> window_message_loop_;

  // Weak handle to the dom::Window that must only be accessed from
  // |window_message_loop|
  base::WeakPtr<dom::Window> window_;

  // This must only be accessed from |window_message_loop_|.
  GetGlobalEnvironmentFunction get_global_environment_;

  // Helper object for commands that execute script. This must only be accessed
  // from the |window_message_loop_|.
  base::WeakPtr<ScriptExecutor> script_executor_;

  // Mapping of protocol::ElementId to ElementDriver*. This should only be
  // accessed from the |window_message_loop_|, though it will be destructed
  // from the WebDriver thread.
  ElementDriverMap element_drivers_;

  // Ensure ElementDrivers that this WindowDriver owns are deleted on
  // destruction of the WindowDriver.
  STLValueDeleter<ElementDriverMap> element_driver_map_deleter_;

  // Monotonically increasing number to provide unique element ids.
  int32 next_element_id_;

  int pointer_buttons_;
  float pointer_x_;
  float pointer_y_;
};

}  // namespace webdriver
}  // namespace cobalt

#endif  // defined(ENABLE_WEBDRIVER)
#endif  // COBALT_WEBDRIVER_WINDOW_DRIVER_H_
