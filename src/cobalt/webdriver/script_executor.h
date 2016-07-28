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

#ifndef COBALT_WEBDRIVER_SCRIPT_EXECUTOR_H_
#define COBALT_WEBDRIVER_SCRIPT_EXECUTOR_H_

#if defined(ENABLE_WEBDRIVER)

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "cobalt/dom/element.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/opaque_handle.h"
#include "cobalt/script/script_object.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/webdriver/element_mapping.h"
#include "cobalt/webdriver/protocol/element_id.h"

namespace cobalt {
namespace webdriver {

// ScriptExecutor
class ScriptExecutor :
    public base::SupportsWeakPtr<ScriptExecutor>,
    public script::Wrappable {
 public:
  typedef script::CallbackFunction<std::string(
      const script::OpaqueHandleHolder*, const std::string&)>
      ExecuteFunctionCallback;
  typedef script::ScriptObject<ExecuteFunctionCallback>
      ExecuteFunctionCallbackHolder;

  explicit ScriptExecutor(ElementMapping* mapping)
      : element_mapping_(mapping) {}

  void set_execute_script_harness(
      const ExecuteFunctionCallbackHolder& callback);
  const ExecuteFunctionCallbackHolder* execute_script_harness();

  scoped_refptr<dom::Element> IdToElement(const std::string& id);
  std::string ElementToId(const scoped_refptr<dom::Element>& id);

  // Calls the function set to the executeScriptHarness property with the
  // provided |function_body| and |json_arguments|. Returns a JSON
  // serialized result string, of base::nullopt on execution failure.
  base::optional<std::string> Execute(
      const script::OpaqueHandleHolder* function_object,
      const std::string& json_arguments);

  DEFINE_WRAPPABLE_TYPE(ScriptExecutor);

 private:
  base::ThreadChecker thread_checker_;
  ElementMapping* element_mapping_;
  base::optional<ExecuteFunctionCallbackHolder::Reference> callback_;
};

}  // namespace webdriver
}  // namespace cobalt

#endif  // defined(ENABLE_WEBDRIVER)

#endif  // COBALT_WEBDRIVER_SCRIPT_EXECUTOR_H_
