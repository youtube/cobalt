// Copyright 2014 Google Inc. All Rights Reserved.
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
#ifndef COBALT_SCRIPT_JAVASCRIPT_ENGINE_H_
#define COBALT_SCRIPT_JAVASCRIPT_ENGINE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/source_location.h"

namespace cobalt {
namespace script {

class GlobalEnvironment;

class JavaScriptEngine {
 public:
  struct Options {
    Options() : disable_jit(false), gc_threshold_bytes(1024*1024) {}

    // Default false. When set to true then the javascript engine should
    // disable the just-in-time compiler.
    bool disable_jit;

    // Determines the size of garbage collection threshold. After this many
    // bytes have been allocated, the garbage collector will run.
    // Default is 1MB (see default constructor).
    size_t gc_threshold_bytes;
  };

  typedef base::Callback<void(const base::SourceLocation& location,
                              const std::string& error_message)> ErrorHandler;

  // Initialize a JavaScript engine. The JavaScript engine should only be
  // accessed from the thread that called CreateEngine.
  // This function is defined per-implementation.
  static scoped_ptr<JavaScriptEngine> CreateEngine(
      const Options& options = Options());

  // Updates the memory usage and returns the total memory that is reserved
  // across all of the engines. This includes the part that is actually occupied
  // by JS objects, and the part that is not yet.
  // This function is defined per-implementation.
  static size_t UpdateMemoryStatsAndReturnReserved();

  // Create a new JavaScript global object proxy.
  virtual scoped_refptr<GlobalEnvironment> CreateGlobalEnvironment() = 0;

  // Kick off the engine's garbage collection synchronously.
  virtual void CollectGarbage() = 0;

  // Indicate to the JS heap that extra bytes have been allocated by some
  // Javascript object. This may mean collection needs to happen sooner.
  virtual void ReportExtraMemoryCost(size_t bytes) = 0;

  // Installs an ErrorHandler for listening to javascript errors.
  // Returns true if the error handler could be installed. False otherwise.
  virtual bool RegisterErrorHandler(ErrorHandler handler) = 0;

 protected:
  virtual ~JavaScriptEngine() {}
  friend class scoped_ptr<JavaScriptEngine>;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPT_ENGINE_H_
