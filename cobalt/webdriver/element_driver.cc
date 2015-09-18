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

#include "cobalt/webdriver/util/call_on_message_loop.h"

namespace cobalt {
namespace webdriver {

ElementDriver::ElementDriver(
    const protocol::ElementId& element_id,
    const base::WeakPtr<dom::Element>& element,
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : element_id_(element_id),
      element_(element),
      element_message_loop_(message_loop) {}

std::string ElementDriver::GetTagName() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return util::CallOnMessageLoop(
      element_message_loop_,
      base::Bind(&ElementDriver::GetTagNameInternal, base::Unretained(this)));
}

std::string ElementDriver::GetTagNameInternal() {
  DCHECK_EQ(base::MessageLoopProxy::current(), element_message_loop_);
  // TODO(***REMOVED***): If the element was deleted, return StaleElementReference
  DCHECK(element_);
  return element_->tag_name();
}

}  // namespace webdriver
}  // namespace cobalt
