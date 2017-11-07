// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_V8C_ENTRY_SCOPE_H_
#define COBALT_SCRIPT_V8C_ENTRY_SCOPE_H_

#include "base/basictypes.h"
#include "cobalt/script/v8c/v8c_global_environment.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

// A wrapper around:
//   v8::Isolate::Scope isolate_scope(isolate);
//   v8::HandleScope handle_scope(isolate);
//   v8::Local<v8::Context> context =
//       V8cGlobalEnvironment::GetFromIsolate(isolate)->context();
//   v8::Context::Scope scope(context);
//
// In the case that you need the |v8::Context| after creating an |EntryScope|,
// it is recommended that you access it via |isolate->GetCurrentContext()|.
class EntryScope {
 public:
  explicit EntryScope(v8::Isolate* isolate)
      : isolate_scope_(isolate),
        handle_scope_(isolate),
        context_scope_(
            V8cGlobalEnvironment::GetFromIsolate(isolate)->context()) {}

 private:
  v8::Isolate::Scope isolate_scope_;
  v8::HandleScope handle_scope_;
  v8::Context::Scope context_scope_;

  DISALLOW_COPY_AND_ASSIGN(EntryScope);
};

// The same thing as |EntryScope|, only with a |v8::EscapableHandleScope|
// instead of a |v8::HandleScope|.
class EscapableEntryScope {
 public:
  explicit EscapableEntryScope(v8::Isolate* isolate)
      : isolate_scope_(isolate),
        handle_scope_(isolate),
        context_scope_(
            V8cGlobalEnvironment::GetFromIsolate(isolate)->context()) {}

  template <class T>
  v8::Local<T> Escape(v8::Local<T> value) {
    return handle_scope_.Escape(value);
  }

 private:
  v8::Isolate::Scope isolate_scope_;
  v8::EscapableHandleScope handle_scope_;
  v8::Context::Scope context_scope_;

  DISALLOW_COPY_AND_ASSIGN(EscapableEntryScope);
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_ENTRY_SCOPE_H_
