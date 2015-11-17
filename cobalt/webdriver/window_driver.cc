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

#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/location.h"
#include "cobalt/webdriver/util/call_on_message_loop.h"

namespace cobalt {
namespace webdriver {
namespace {

std::string GetCurrentUrl(dom::Window* window) {
  DCHECK(window);
  return window->location()->href();
}

std::string GetTitle(dom::Window* window) {
  DCHECK(window);
  return window->document()->title();
}

protocol::Size GetWindowSize(dom::Window* window) {
  DCHECK(window);
  float width = window->outer_width();
  float height = window->outer_height();
  return protocol::Size(width, height);
}

}  // namespace

WindowDriver::WindowDriver(
    const protocol::WindowId& window_id,
    const base::WeakPtr<dom::Window>& window,
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : window_id_(window_id),
      window_(window),
      window_message_loop_(message_loop),
      element_driver_map_deleter_(&element_drivers_),
      next_element_id_(0) {}

ElementDriver* WindowDriver::GetElementDriver(
    const protocol::ElementId& element_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
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
  typedef util::CommandResult<protocol::ElementId> CommandResult;

  base::WeakPtr<dom::Element> weak_element;
  protocol::Response::StatusCode status_code = util::CallOnMessageLoop(
      window_message_loop_,
      base::Bind(&WindowDriver::FindElementInternal, base::Unretained(this),
                 strategy, &weak_element));
  if (status_code == protocol::Response::kSuccess) {
    protocol::ElementId id = GetUniqueElementId();
    std::pair<ElementDriverMapIt, bool> pair_it = element_drivers_.insert(
        std::make_pair(id.id(), new ElementDriver(id, weak_element,
                                                  window_message_loop_)));
    DCHECK(pair_it.second)
        << "An ElementDriver was already mapped to the element id: " << id.id();
    return CommandResult(id);
  } else {
    return CommandResult(status_code);
  }
}

protocol::ElementId WindowDriver::GetUniqueElementId() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string element_id = base::StringPrintf("element-%d", next_element_id_++);
  DCHECK(element_drivers_.find(element_id) == element_drivers_.end());
  return protocol::ElementId(element_id);
}

protocol::Response::StatusCode WindowDriver::FindElementInternal(
    const protocol::SearchStrategy& strategy,
    base::WeakPtr<dom::Element>* out_weak_ptr) {
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  if (!window_) {
    return protocol::Response::kNoSuchWindow;
  }

  if (strategy.strategy() == protocol::SearchStrategy::kClassName) {
    scoped_refptr<dom::HTMLCollection> collection =
        window_->document()->GetElementsByClassName(strategy.parameter());
    if (collection && collection->length()) {
      *out_weak_ptr = base::AsWeakPtr(collection->Item(0).get());
      return protocol::Response::kSuccess;
    }
  } else {
    // TODO(***REMOVED***): Implement support for other search strategies that
    // we support and provide better error messaging if unsupported strategies
    // are used.
    NOTIMPLEMENTED();
  }
  return protocol::Response::kNoSuchElement;
}

}  // namespace webdriver
}  // namespace cobalt
