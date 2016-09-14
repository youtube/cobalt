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
#ifndef COBALT_SCRIPT_JAVASCRIPT_ENGINE_H_
#define COBALT_SCRIPT_JAVASCRIPT_ENGINE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace cobalt {
namespace script {

class GlobalEnvironment;

class JavaScriptEngine {
 public:
  // Initialize a JavaScript engine. The JavaScript engine should only be
  // accessed from the thread that called CreateEngine.
  // This function is defined per-implementation.
  static scoped_ptr<JavaScriptEngine> CreateEngine();

  // Create a new JavaScript global object proxy.
  virtual scoped_refptr<GlobalEnvironment> CreateGlobalEnvironment() = 0;

  // Kick off the engine's garbage collection synchronously.
  virtual void CollectGarbage() = 0;

  // Indicate to the JS heap that extra bytes have been allocated by some
  // Javascript object. This may mean collection needs to happen sooner.
  virtual void ReportExtraMemoryCost(size_t bytes) = 0;

 protected:
  virtual ~JavaScriptEngine() {}
  friend class scoped_ptr<JavaScriptEngine>;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPT_ENGINE_H_
