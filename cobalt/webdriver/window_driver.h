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

  protocol::Size GetWindowSize();
  ElementDriver* FindElement(const protocol::SearchStrategy& strategy);

 private:
  typedef base::hash_map<std::string, ElementDriver*> ElementDriverMap;
  typedef ElementDriverMap::iterator ElementDriverMapIt;

  protocol::ElementId GetUniqueElementId();

  protocol::Size GetWindowSizeInternal();
  base::WeakPtr<dom::Element> FindElementInternal(
      const protocol::SearchStrategy& strategy);

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
