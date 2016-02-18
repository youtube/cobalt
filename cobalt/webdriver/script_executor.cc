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

#include "cobalt/webdriver/script_executor.h"

namespace cobalt {
namespace webdriver {

void ScriptExecutor::set_execute_script_harness(
    const ExecuteFunctionCallbackHolder& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  callback_.emplace(this, callback);
}

const ScriptExecutor::ExecuteFunctionCallbackHolder*
ScriptExecutor::execute_script_harness() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (callback_) {
    return &(callback_->referenced_object());
  } else {
    return NULL;
  }
}

base::optional<std::string> ScriptExecutor::Execute(
    const script::OpaqueHandleHolder* function_object,
    const std::string& json_arguments) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback_);

  ExecuteFunctionCallback::ReturnValue callback_result =
      callback_->value().Run(function_object, json_arguments);
  if (callback_result.exception) {
    return base::nullopt;
  } else {
    return callback_result.result;
  }
}

scoped_refptr<dom::Element> ScriptExecutor::IdToElement(const std::string& id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(element_mapping_);
  return element_mapping_->IdToElement(protocol::ElementId(id));
}

std::string ScriptExecutor::ElementToId(
    const scoped_refptr<dom::Element>& element) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(element_mapping_);
  return element_mapping_->ElementToId(element).id();
}

}  // namespace webdriver
}  // namespace cobalt
