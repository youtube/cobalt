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
#include "cobalt/dom/node_list.h"
#include "cobalt/webdriver/util/call_on_message_loop.h"

namespace cobalt {
namespace webdriver {
namespace {

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

  CommandResult error_result;
  WeakElementVector weak_elements;
  // Even though we only need the first match, just find all and return the
  // first one.
  FindElementsOrSetError(strategy, &weak_elements, &error_result);
  if (weak_elements.empty()) {
    return error_result;
  }

  protocol::ElementId id = CreateNewElementDriver(weak_elements.front());
  return CommandResult(id);
}

util::CommandResult<std::vector<protocol::ElementId> >
WindowDriver::FindElements(const protocol::SearchStrategy& strategy) {
  DCHECK(thread_checker_.CalledOnValidThread());
  typedef util::CommandResult<std::vector<protocol::ElementId> > CommandResult;

  CommandResult error_result;
  WeakElementVector weak_elements;
  FindElementsOrSetError(strategy, &weak_elements, &error_result);
  if (weak_elements.empty()) {
    return error_result;
  }

  std::vector<protocol::ElementId> elements;
  for (size_t i = 0; i < weak_elements.size(); ++i) {
    elements.push_back(CreateNewElementDriver(weak_elements.front()));
  }
  return CommandResult(elements);
}

util::CommandResult<std::string> WindowDriver::GetSource() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::CallWeakOnMessageLoopAndReturnResult(
      window_message_loop_,
      base::Bind(&WindowDriver::GetWeak, base::Unretained(this)),
      base::Bind(&::cobalt::webdriver::GetSource),
      protocol::Response::kNoSuchWindow);
}

protocol::ElementId WindowDriver::CreateNewElementDriver(
    const base::WeakPtr<dom::Element>& weak_element) {
  DCHECK(thread_checker_.CalledOnValidThread());

  protocol::ElementId element_id(
      base::StringPrintf("element-%d", next_element_id_++));
  std::pair<ElementDriverMapIt, bool> pair_it =
      element_drivers_.insert(std::make_pair(
          element_id.id(),
          new ElementDriver(element_id, weak_element, window_message_loop_)));
  DCHECK(pair_it.second)
      << "An ElementDriver was already mapped to the element id: "
      << element_id.id();
  return element_id;
}

// This function is called from FindElements and FindElement.
// Calls FindElementsInternal on the Window's message loop. If that call fails
// because the window_ has been deleted, it will set kNoSuchWindow. If the
// element can't be found it will set kNoSuchElement. Otherwise, it simply
// returns without setting an error.
template <typename T>
void WindowDriver::FindElementsOrSetError(
    const protocol::SearchStrategy& strategy,
    WeakElementVector* out_weak_elements, util::CommandResult<T>* out_result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  typedef util::CommandResult<T> CommandResult;

  bool success = util::CallOnMessageLoop(
      window_message_loop_,
      base::Bind(&WindowDriver::FindElementsInternal, base::Unretained(this),
                 strategy, out_weak_elements));
  if (!success) {
    *out_result = CommandResult(protocol::Response::kNoSuchWindow);
  }
  if (out_weak_elements->empty()) {
    *out_result = CommandResult(protocol::Response::kNoSuchElement);
  }
}

// Internal logic for FindElement and FindElements that must be run on the
// Window's message loop.
// Returns true if window_ is still valid, implying that the search could be
// executed.
// The search result are populated in |out_weak_ptrs|.
bool WindowDriver::FindElementsInternal(
    const protocol::SearchStrategy& strategy,
    WeakElementVector* out_weak_ptrs) {
  DCHECK_EQ(base::MessageLoopProxy::current(), window_message_loop_);
  if (!window_) {
    return false;
  }
  switch (strategy.strategy()) {
    case protocol::SearchStrategy::kClassName: {
      scoped_refptr<dom::HTMLCollection> collection =
          window_->document()->GetElementsByClassName(strategy.parameter());
      if (collection) {
        for (uint32 i = 0; i < collection->length(); ++i) {
          out_weak_ptrs->push_back(base::AsWeakPtr(collection->Item(i).get()));
        }
      }
      break;
    }
    case protocol::SearchStrategy::kCssSelector: {
      scoped_refptr<dom::NodeList> node_list =
          window_->document()->QuerySelectorAll(strategy.parameter());
      if (node_list) {
        for (uint32 i = 0; i < node_list->length(); ++i) {
          scoped_refptr<dom::Element> element = node_list->Item(i)->AsElement();
          if (element) {
            out_weak_ptrs->push_back(base::AsWeakPtr(element.get()));
          }
        }
      }
      break;
    }
    default:
      NOTIMPLEMENTED();
  }
  return true;
}

}  // namespace webdriver
}  // namespace cobalt
