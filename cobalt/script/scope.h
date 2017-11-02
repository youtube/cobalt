// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_SCOPE_H_
#define COBALT_SCRIPT_SCOPE_H_

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "cobalt/script/value_handle.h"

namespace cobalt {
namespace script {

// Opaque type that encapsulates a scope object, as used by the JavaScript
// debugger and devtools. Each call frame has a scope chain: a list of the
// individual scopes valid at that call frame. The scope objects are only
// guaranteed to be valid while the script debugger is paused in that call
// frame.
// https://developer.chrome.com/devtools/docs/protocol/1.1/debugger#type-CallFrame
// https://developer.chrome.com/devtools/docs/protocol/1.1/debugger#type-Scope
class Scope {
 public:
  // Possible values returned by |GetType|.
  // One of the set of enumerated strings defined here:
  // https://developer.chrome.com/devtools/docs/protocol/1.1/debugger#type-Scope
  enum Type { kCatch, kClosure, kGlobal, kLocal, kWith };

  virtual ~Scope() {}

  // An opaque handle to the underlying JavaScript object encapsulating the
  // scope.
  virtual const ValueHandleHolder* GetObject() = 0;

  // The type of the scope object. One of the enumerated values in |Type|.
  // Devtools expects a string, which can be obtained from the
  // |TypeToString| method.
  virtual Type GetType() = 0;

  // The next entry in the scope chain.
  virtual scoped_ptr<Scope> GetNext() = 0;

  // Converts an enumerated type value to the string expected by devtools.
  static const char* TypeToString(Type type) {
    static const char kCatchString[] = "catch";
    static const char kClosureString[] = "closure";
    static const char kGlobalString[] = "global";
    static const char kLocalString[] = "local";
    static const char kWithString[] = "with";

    switch (type) {
      case kCatch:
        return kCatchString;
      case kClosure:
        return kClosureString;
      case kGlobal:
        return kGlobalString;
      case kLocal:
        return kLocalString;
      case kWith:
        return kWithString;
      default: {
        NOTREACHED();
        return kLocalString;
      }
    }
  }
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_SCOPE_H_
