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

#ifndef COBALT_WEBDRIVER_SCRIPT_EXECUTOR_H_
#define COBALT_WEBDRIVER_SCRIPT_EXECUTOR_H_

#if defined(ENABLE_WEBDRIVER)

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "cobalt/dom/element.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/webdriver/element_mapping.h"
#include "cobalt/webdriver/protocol/element_id.h"
#include "cobalt/webdriver/script_executor_params.h"
#include "cobalt/webdriver/script_executor_result.h"

namespace cobalt {
namespace webdriver {

// ScriptExecutor
class ScriptExecutor :
    public base::SupportsWeakPtr<ScriptExecutor>,
    public script::Wrappable {
 public:
  typedef script::CallbackFunction<void(
      const scoped_refptr<ScriptExecutorParams>&,
      const scoped_refptr<ScriptExecutorResult>&)> ExecuteFunctionCallback;
  typedef script::ScriptValue<ExecuteFunctionCallback>
      ExecuteFunctionCallbackHolder;

  // This can be called on any thread to preload the webdriver javascript code.
  // If this is not called, then it will be lazily loaded the first time a
  // ScriptExecutor is created, which will be the Web Module thread in most
  // cases.
  static void LoadExecutorSourceCode();

  // Create a new ScriptExecutor instance.
  static scoped_refptr<ScriptExecutor> Create(
      ElementMapping* element_mapping,
      const scoped_refptr<script::GlobalEnvironment>& global_environment);

  // Calls the function set to the executeScriptHarness property with the
  // provided |param|. The result of script execution will be passed to the
  // caller through |result_handler|.
  // Returns false on execution failure.
  bool Execute(const scoped_refptr<ScriptExecutorParams>& params,
               ScriptExecutorResult::ResultHandler* result_handler);

  // ScriptExecutor bindings implementation.
  //

  void set_execute_script_harness(
      const ExecuteFunctionCallbackHolder& callback);
  const ExecuteFunctionCallbackHolder* execute_script_harness();

  scoped_refptr<dom::Element> IdToElement(const std::string& id);
  std::string ElementToId(const scoped_refptr<dom::Element>& id);

  DEFINE_WRAPPABLE_TYPE(ScriptExecutor);

 private:
  explicit ScriptExecutor(ElementMapping* mapping)
      : element_mapping_(mapping) {}

  bool ExecuteInternal(
      const scoped_refptr<ScriptExecutorParams>& params,
      const scoped_refptr<ScriptExecutorResult>& result_handler);

  base::ThreadChecker thread_checker_;
  ElementMapping* element_mapping_;
  base::optional<ExecuteFunctionCallbackHolder::Reference> execute_callback_;
};

}  // namespace webdriver
}  // namespace cobalt

#endif  // defined(ENABLE_WEBDRIVER)

#endif  // COBALT_WEBDRIVER_SCRIPT_EXECUTOR_H_
