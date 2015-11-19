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

#ifndef WEBDRIVER_WINDOW_DRIVER_H_
#define WEBDRIVER_WINDOW_DRIVER_H_

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/stl_util.h"
#include "base/threading/thread_checker.h"
#include "cobalt/dom/window.h"
#include "cobalt/webdriver/element_driver.h"
#include "cobalt/webdriver/protocol/search_strategy.h"
#include "cobalt/webdriver/protocol/size.h"
#include "cobalt/webdriver/protocol/window_id.h"
#include "cobalt/webdriver/util/call_on_message_loop.h"
#include "cobalt/webdriver/util/command_result.h"

namespace cobalt {
namespace webdriver {

// A WebDriver windowHandle will map to a WindowDriver instance.
// WebDriver commands that interact with a Window, such as:
//     /session/:sessionId/window/:windowHandle/size
// will map to a method on this class.
class WindowDriver {
 public:
  WindowDriver(const protocol::WindowId& window_id,
               const base::WeakPtr<dom::Window>& window,
               const scoped_refptr<base::MessageLoopProxy>& message_loop);
  const protocol::WindowId& window_id() { return window_id_; }
  ElementDriver* GetElementDriver(const protocol::ElementId& element_id);

  util::CommandResult<protocol::Size> GetWindowSize();
  util::CommandResult<std::string> GetCurrentUrl();
  util::CommandResult<std::string> GetTitle();
  util::CommandResult<protocol::ElementId> FindElement(
      const protocol::SearchStrategy& strategy);
  util::CommandResult<std::vector<protocol::ElementId> > FindElements(
      const protocol::SearchStrategy& strategy);
  // Note: The source may be a fairly large string that is being copied around
  // by value here.
  util::CommandResult<std::string> GetSource();

 private:
  typedef base::hash_map<std::string, ElementDriver*> ElementDriverMap;
  typedef ElementDriverMap::iterator ElementDriverMapIt;
  typedef std::vector<base::WeakPtr<dom::Element> > WeakElementVector;

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
  void FindElementsOrSetError(const protocol::SearchStrategy& strategy,
                              WeakElementVector* out_weak_elements,
                              util::CommandResult<T>* out_result);

  // Returns true if window_ is still alive and the search was executed.
  bool FindElementsInternal(const protocol::SearchStrategy& strategy,
                            WeakElementVector* out_weak_ptrs);

  base::ThreadChecker thread_checker_;
  protocol::WindowId window_id_;
  base::WeakPtr<dom::Window> window_;
  scoped_refptr<base::MessageLoopProxy> window_message_loop_;
  ElementDriverMap element_drivers_;
  STLValueDeleter<ElementDriverMap> element_driver_map_deleter_;
  int32 next_element_id_;
};

}  // namespace webdriver
}  // namespace cobalt

#endif  // WEBDRIVER_WINDOW_DRIVER_H_
