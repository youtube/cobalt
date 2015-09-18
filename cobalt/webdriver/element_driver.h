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

#ifndef WEBDRIVER_ELEMENT_DRIVER_H_
#define WEBDRIVER_ELEMENT_DRIVER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/threading/thread_checker.h"
#include "cobalt/dom/element.h"
#include "cobalt/webdriver/protocol/element_id.h"

namespace cobalt {
namespace webdriver {

// ElementDriver could be considered a WebElement as described in the WebDriver
// spec.
// https://code.google.com/p/selenium/wiki/JsonWireProtocol#WebElement
// Commands that interact with a WebElement, such as:
//     /session/:sessionId/element/:id/some_command
// will map to a method on this class.
class ElementDriver {
 public:
  ElementDriver(const protocol::ElementId& element_id,
                const base::WeakPtr<dom::Element>& element,
                const scoped_refptr<base::MessageLoopProxy>& message_loop);
  const protocol::ElementId& element_id() { return element_id_; }

  std::string GetTagName();

 private:
  std::string GetTagNameInternal();

  base::ThreadChecker thread_checker_;
  protocol::ElementId element_id_;
  base::WeakPtr<dom::Element> element_;
  scoped_refptr<base::MessageLoopProxy> element_message_loop_;
};

}  // namespace webdriver
}  // namespace cobalt

#endif  // WEBDRIVER_ELEMENT_DRIVER_H_
