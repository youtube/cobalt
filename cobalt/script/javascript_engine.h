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

// https://webplatform.github.io/docs/apis/timing/properties/memory/
struct HeapStatistics {
  size_t total_heap_size;
  size_t used_heap_size;
};

class JavaScriptEngine {
 public:
  struct Options {
    Options() : disable_jit(false), gc_threshold_bytes(1024*1024) {}

    // Default false. When set to true then the JavaScript engine should
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

  // Create a new JavaScript global object proxy.
  virtual scoped_refptr<GlobalEnvironment> CreateGlobalEnvironment() = 0;

  // Kick off the engine's garbage collection synchronously.
  virtual void CollectGarbage() = 0;

  // Indicate to the JavaScript heap that extra bytes have been allocated by
  // some Javascript object.  The engine will take advantage of this
  // information to the best of its ability.
  virtual void AdjustAmountOfExternalAllocatedMemory(int64_t bytes) = 0;

  // Installs an ErrorHandler for listening to JavaScript errors.
  // Returns true if the error handler could be installed. False otherwise.
  virtual bool RegisterErrorHandler(ErrorHandler handler) = 0;

  // Adjusts the memory threshold to force garbage collection.
  virtual void SetGcThreshold(int64_t bytes) = 0;

  // Get the current |HeapStatistics| measurements for this engine.  Note that
  // engines will implement this to the best of their ability, but will likely
  // be unable to provide perfectly accurate values.
  virtual HeapStatistics GetHeapStatistics() = 0;

 protected:
  virtual ~JavaScriptEngine() {}
  friend class scoped_ptr<JavaScriptEngine>;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPT_ENGINE_H_
