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
#ifndef SCRIPT_JAVASCRIPTCORE_JSC_ENGINE_H_
#define SCRIPT_JAVASCRIPTCORE_JSC_ENGINE_H_

#include "config.h"
#undef LOG

#include "base/threading/thread_checker.h"
#include "cobalt/script/javascript_engine.h"

#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalData.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

class JSCEngine : public JavaScriptEngine {
 public:
  JSCEngine();
  scoped_refptr<GlobalObjectProxy> CreateGlobalObject() OVERRIDE;
  void CollectGarbage() OVERRIDE;
  JSC::JSGlobalData* global_data() { return global_data_.get(); }

 private:
  RefPtr<JSC::JSGlobalData> global_data_;
  base::ThreadChecker thread_checker_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // SCRIPT_JAVASCRIPTCORE_JSC_ENGINE_H_
