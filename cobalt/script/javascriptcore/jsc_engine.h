/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_ENGINE_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_ENGINE_H_

#include "base/threading/thread_checker.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/javascriptcore/script_object_registry.h"
#include "third_party/WebKit/Source/JavaScriptCore/config.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalData.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

class JSCEngine : public JavaScriptEngine {
 public:
  JSCEngine();
  ~JSCEngine() OVERRIDE;
  scoped_refptr<GlobalEnvironment> CreateGlobalEnvironment() OVERRIDE;
  void CollectGarbage() OVERRIDE;
  void ReportExtraMemoryCost(size_t bytes) OVERRIDE;
  JSC::JSGlobalData* global_data() { return global_data_.get(); }
  ScriptObjectRegistry* script_object_registry() {
    return &script_object_registry_;
  }

 private:
  base::ThreadChecker thread_checker_;
  ScriptObjectRegistry script_object_registry_;
  RefPtr<JSC::JSGlobalData> global_data_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_ENGINE_H_
