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

#include "cobalt/webdriver/element_driver.h"

#include "cobalt/cssom/property_value.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/webdriver/algorithms.h"
#include "cobalt/webdriver/keyboard.h"
#include "cobalt/webdriver/search.h"
#include "cobalt/webdriver/util/call_on_message_loop.h"

namespace cobalt {
namespace webdriver {
namespace {

std::string GetTagName(dom::Element* element) {
  DCHECK(element);
  return element->tag_name().c_str();
}

base::optional<std::string> GetAttribute(const std::string& attribute_name,
                                         dom::Element* element) {
  DCHECK(element);
  return element->GetAttribute(attribute_name);
}

std::string GetCssProperty(const std::string& property_name,
                           dom::Element* element) {
  DCHECK(element);
  DCHECK(element->node_document());
  element->node_document()->UpdateComputedStyles();

  scoped_refptr<dom::HTMLElement> html_element(element->AsHTMLElement());
  DCHECK(html_element);
  DCHECK(html_element->computed_style());
  cssom::PropertyKey property_key = cssom::GetPropertyKey(property_name);
  if (property_key != cssom::kNoneProperty) {
    scoped_refptr<cssom::PropertyValue> property_value =
        html_element->computed_style()->GetPropertyValue(property_key);
    if (property_value) {
      return property_value->ToString();
    }
  }
  return "";
}

}  // namespace

ElementDriver::ElementDriver(
    const protocol::ElementId& element_id,
    const base::WeakPtr<dom::Element>& element, ElementMapping* element_mapping,
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : element_id_(element_id),
      element_(element),
      element_mapping_(element_mapping),
      element_message_loop_(message_loop) {}

util::CommandResult<std::string> ElementDriver::GetTagName() {
  return util::CallWeakOnMessageLoopAndReturnResult(
      element_message_loop_,
      base::Bind(&ElementDriver::GetWeakElement, base::Unretained(this)),
      base::Bind(&::cobalt::webdriver::GetTagName),
      protocol::Response::kStaleElementReference);
}

util::CommandResult<std::string> ElementDriver::GetText() {
  return util::CallWeakOnMessageLoopAndReturnResult(
      element_message_loop_,
      base::Bind(&ElementDriver::GetWeakElement, base::Unretained(this)),
      base::Bind(&algorithms::GetElementText),
      protocol::Response::kStaleElementReference);
}

util::CommandResult<bool> ElementDriver::IsDisplayed() {
  return util::CallWeakOnMessageLoopAndReturnResult(
      element_message_loop_,
      base::Bind(&ElementDriver::GetWeakElement, base::Unretained(this)),
      base::Bind(&algorithms::IsDisplayed),
      protocol::Response::kStaleElementReference);
}

util::CommandResult<void> ElementDriver::SendKeys(const protocol::Keys& keys) {
  typedef util::CommandResult<void> CommandResult;

  // Translate the keys into KeyboardEvents. Reset modifiers.
  scoped_ptr<Keyboard::KeyboardEventVector> events(
      new Keyboard::KeyboardEventVector());
  Keyboard::TranslateToKeyEvents(keys.utf8_keys(), Keyboard::kReleaseModifiers,
                                 events.get());
  // Dispatch the keyboard events.
  return util::CallOnMessageLoop(
      element_message_loop_,
      base::Bind(&ElementDriver::SendKeysInternal, base::Unretained(this),
                 base::Passed(&events)));
}

util::CommandResult<protocol::ElementId> ElementDriver::FindElement(
    const protocol::SearchStrategy& strategy) {
  return util::CallOnMessageLoop(
      element_message_loop_,
      base::Bind(&ElementDriver::FindElementsInternal<protocol::ElementId>,
                 base::Unretained(this), strategy));
}

util::CommandResult<std::vector<protocol::ElementId> >
ElementDriver::FindElements(const protocol::SearchStrategy& strategy) {
  return util::CallOnMessageLoop(
      element_message_loop_,
      base::Bind(&ElementDriver::FindElementsInternal<ElementIdVector>,
                 base::Unretained(this), strategy));
}

util::CommandResult<bool> ElementDriver::Equals(
    const ElementDriver* other_element_driver) {
  return util::CallOnMessageLoop(
      element_message_loop_,
      base::Bind(&ElementDriver::EqualsInternal, base::Unretained(this),
                 other_element_driver));
}

util::CommandResult<base::optional<std::string> > ElementDriver::GetAttribute(
    const std::string& attribute_name) {
  return util::CallWeakOnMessageLoopAndReturnResult(
      element_message_loop_,
      base::Bind(&ElementDriver::GetWeakElement, base::Unretained(this)),
      base::Bind(&::cobalt::webdriver::GetAttribute, attribute_name),
      protocol::Response::kStaleElementReference);
}

util::CommandResult<std::string> ElementDriver::GetCssProperty(
    const std::string& property_name) {
  return util::CallWeakOnMessageLoopAndReturnResult(
      element_message_loop_,
      base::Bind(&ElementDriver::GetWeakElement, base::Unretained(this)),
      base::Bind(&::cobalt::webdriver::GetCssProperty, property_name),
      protocol::Response::kStaleElementReference);
}

dom::Element* ElementDriver::GetWeakElement() {
  DCHECK_EQ(base::MessageLoopProxy::current(), element_message_loop_);
  return element_.get();
}

util::CommandResult<void> ElementDriver::SendKeysInternal(
    scoped_ptr<Keyboard::KeyboardEventVector> events) {
  typedef util::CommandResult<void> CommandResult;
  DCHECK_EQ(base::MessageLoopProxy::current(), element_message_loop_);
  if (!element_) {
    return CommandResult(protocol::Response::kStaleElementReference);
  }
  // First ensure that the element is displayed, and return an error if not.
  if (!algorithms::IsDisplayed(element_.get())) {
    return CommandResult(protocol::Response::kElementNotVisible);
  }

  for (size_t i = 0; i < events->size(); ++i) {
    // Check each iteration in case the element was deleted as a result of
    // some action.
    if (!element_) {
      return CommandResult(protocol::Response::kStaleElementReference);
    }
    scoped_refptr<dom::KeyboardEvent> keyboard_event(
              new dom::KeyboardEvent((*events)[i]));

    element_->DispatchEvent(keyboard_event);
  }
  return CommandResult(protocol::Response::kSuccess);
}

// Shared logic between FindElement and FindElements.
template <typename T>
util::CommandResult<T> ElementDriver::FindElementsInternal(
    const protocol::SearchStrategy& strategy) {
  DCHECK_EQ(base::MessageLoopProxy::current(), element_message_loop_);
  typedef util::CommandResult<T> CommandResult;
  if (!element_) {
    return CommandResult(protocol::Response::kStaleElementReference);
  }
  return Search::FindElementsUnderNode<T>(strategy, element_.get(),
                                          element_mapping_);
}

util::CommandResult<bool> ElementDriver::EqualsInternal(
    const ElementDriver* other_element_driver) {
  DCHECK_EQ(base::MessageLoopProxy::current(), element_message_loop_);
  typedef util::CommandResult<bool> CommandResult;
  base::WeakPtr<dom::Element> other_element = other_element_driver->element_;
  if (!element_ || !other_element) {
    return CommandResult(protocol::Response::kStaleElementReference);
  }
  bool is_same_element = other_element.get() == element_.get();
  return CommandResult(is_same_element);
}

}  // namespace webdriver
}  // namespace cobalt
