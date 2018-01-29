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

#include "cobalt/script/v8c/v8c_global_environment.h"

#include <algorithm>
#include <utility>

#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/script/v8c/v8c_script_value_factory.h"
#include "cobalt/script/v8c/v8c_source_code.h"
#include "cobalt/script/v8c/v8c_user_object_holder.h"
#include "cobalt/script/v8c/v8c_value_handle.h"
#include "nb/memory_scope.h"

namespace cobalt {
namespace script {
namespace v8c {
namespace {

std::string ExceptionToString(const v8::TryCatch& try_catch) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::String::Utf8Value exception(try_catch.Exception());
  v8::Local<v8::Message> message(try_catch.Message());

  std::string string;
  if (message.IsEmpty()) {
    string.append(base::StringPrintf("%s\n", *exception));
  } else {
    v8::String::Utf8Value filename(message->GetScriptOrigin().ResourceName());
    int linenum = message->GetLineNumber();
    int colnum = message->GetStartColumn();
    string.append(base::StringPrintf("%s:%i:%i %s\n", *filename, linenum,
                                     colnum, *exception));
    v8::String::Utf8Value sourceline(message->GetSourceLine());
    string.append(base::StringPrintf("%s\n", *sourceline));
  }
  return string;
}

}  // namespace

V8cGlobalEnvironment::V8cGlobalEnvironment(v8::Isolate* isolate)
    : garbage_collection_count_(0),
      environment_settings_(nullptr),
      last_error_message_(nullptr),
      eval_enabled_(false),
      isolate_(isolate) {
  TRACK_MEMORY_SCOPE("Javascript");
  wrapper_factory_.reset(new WrapperFactory(isolate));
  isolate_->SetData(kIsolateDataIndex, this);
  DCHECK(isolate_->GetData(kIsolateDataIndex) == this);

  // TODO: Change type of this member to scoped_ptr
  script_value_factory_ = new V8cScriptValueFactory(isolate);
}

V8cGlobalEnvironment::~V8cGlobalEnvironment() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO: Change type of this member to scoped_ptr
  delete script_value_factory_;
  script_value_factory_ = nullptr;

  // Run garbage collection right before we die, in order to give callbacks
  // that depend on us being alive one more chance to run.
  isolate_->LowMemoryNotification();
  isolate_->SetData(kIsolateDataIndex, nullptr);
}

void V8cGlobalEnvironment::CreateGlobalObject() {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK(thread_checker_.CalledOnValidThread());

  // Intentionally not an |EntryScope|, since the context doesn't exist yet.
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  context_.Reset(isolate_, context);
  v8::Context::Scope context_scope(context);

  EvaluateAutomatics();
}

bool V8cGlobalEnvironment::EvaluateScript(
    const scoped_refptr<SourceCode>& source_code, bool mute_errors,
    std::string* out_result_utf8) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK(thread_checker_.CalledOnValidThread());

  EntryScope entry_scope(isolate_);
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  V8cSourceCode* v8c_source_code =
      base::polymorphic_downcast<V8cSourceCode*>(source_code.get());
  const base::SourceLocation& source_location = v8c_source_code->location();

  v8::TryCatch try_catch(isolate_);
  v8::ScriptOrigin script_origin(
      v8::String::NewFromUtf8(isolate_, source_location.file_path.c_str(),
                              v8::NewStringType::kNormal)
          .ToLocalChecked(),
      v8::Integer::New(isolate_, source_location.line_number),
      v8::Integer::New(isolate_, source_location.column_number),
      v8::Boolean::New(isolate_, !mute_errors));
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8(isolate_, v8c_source_code->source_utf8().c_str(),
                              v8::NewStringType::kNormal)
          .ToLocalChecked();

  v8::MaybeLocal<v8::Script> maybe_script =
      v8::Script::Compile(context, source, &script_origin);
  v8::Local<v8::Script> script;
  if (!maybe_script.ToLocal(&script)) {
    if (out_result_utf8) {
      *out_result_utf8 = ExceptionToString(try_catch);
    }
    return false;
  }

  v8::MaybeLocal<v8::Value> maybe_result = script->Run(context);
  v8::Local<v8::Value> result;
  if (!maybe_result.ToLocal(&result)) {
    if (out_result_utf8) {
      *out_result_utf8 = ExceptionToString(try_catch);
    }
    return false;
  }

  if (out_result_utf8) {
    *out_result_utf8 = *v8::String::Utf8Value(isolate_, result);
  }

  return true;
}

bool V8cGlobalEnvironment::EvaluateScript(
    const scoped_refptr<SourceCode>& source_code,
    const scoped_refptr<Wrappable>& owning_object, bool mute_errors,
    base::optional<ValueHandleHolder::Reference>* out_value_handle) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK(thread_checker_.CalledOnValidThread());

  EntryScope entry_scope(isolate_);
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  V8cSourceCode* v8c_source_code =
      base::polymorphic_downcast<V8cSourceCode*>(source_code.get());
  const base::SourceLocation& source_location = v8c_source_code->location();

  v8::TryCatch try_catch(isolate_);
  v8::ScriptOrigin script_origin(
      v8::String::NewFromUtf8(isolate_, source_location.file_path.c_str(),
                              v8::NewStringType::kNormal)
          .ToLocalChecked(),
      v8::Integer::New(isolate_, source_location.line_number),
      v8::Integer::New(isolate_, source_location.column_number),
      v8::Boolean::New(isolate_, !mute_errors));
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8(isolate_, v8c_source_code->source_utf8().c_str(),
                              v8::NewStringType::kNormal)
          .ToLocalChecked();

  v8::MaybeLocal<v8::Script> maybe_script =
      v8::Script::Compile(context, source, &script_origin);
  v8::Local<v8::Script> script;
  if (!maybe_script.ToLocal(&script)) {
    return false;
  }

  v8::MaybeLocal<v8::Value> maybe_result = script->Run(context);
  v8::Local<v8::Value> result;
  if (!maybe_result.ToLocal(&result)) {
    return false;
  }

  if (out_value_handle) {
    V8cValueHandleHolder v8c_value_handle_holder(isolate_, result);
    out_value_handle->emplace(owning_object.get(), v8c_value_handle_holder);
  }

  return true;
}

std::vector<StackFrame> V8cGlobalEnvironment::GetStackTrace(int max_frames) {
  DCHECK(thread_checker_.CalledOnValidThread());
  v8::HandleScope handle_scope(isolate_);
  std::vector<StackFrame> result;
  v8::Local<v8::StackTrace> stack_trace =
      v8::StackTrace::CurrentStackTrace(isolate_, max_frames);
  for (int i = 0; i < stack_trace->GetFrameCount(); i++) {
    v8::Local<v8::StackFrame> stack_frame = stack_trace->GetFrame(i);
    result.emplace_back(
        stack_frame->GetLineNumber(), stack_frame->GetColumn(),
        *v8::String::Utf8Value(isolate_, stack_frame->GetFunctionName()),
        *v8::String::Utf8Value(isolate_, stack_frame->GetScriptName()));
  }
  return result;
}

void V8cGlobalEnvironment::PreventGarbageCollection(
    const scoped_refptr<Wrappable>& wrappable) {
  DCHECK(thread_checker_.CalledOnValidThread());

  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Object> wrapper = wrapper_factory_->GetWrapper(wrappable);
  DCHECK(WrapperPrivate::HasWrapperPrivate(wrapper));

  // Attempt to insert a |Wrappable*| -> wrapper mapping into
  // |kept_alive_objects_|...
  auto insert_result =
      kept_alive_objects_.insert({wrappable.get(), {{isolate_, wrapper}, 1}});
  // ...and if it was already there, just increment the count.
  if (!insert_result.second) {
    insert_result.first->second.count++;
  }
}

void V8cGlobalEnvironment::AllowGarbageCollection(
    const scoped_refptr<Wrappable>& wrappable) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK(thread_checker_.CalledOnValidThread());

  auto it = kept_alive_objects_.find(wrappable.get());
  DCHECK(it != kept_alive_objects_.end());
  it->second.count--;
  DCHECK_GE(it->second.count, 0);
  if (it->second.count == 0) {
    kept_alive_objects_.erase(it);
  }
}

void V8cGlobalEnvironment::DisableEval(const std::string& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  eval_disabled_message_.emplace(message);
  eval_enabled_ = false;
}

void V8cGlobalEnvironment::EnableEval() {
  DCHECK(thread_checker_.CalledOnValidThread());
  eval_disabled_message_ = base::nullopt;
  eval_enabled_ = true;
}

void V8cGlobalEnvironment::DisableJit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SB_DLOG(INFO)
      << "V8 version " << V8_MAJOR_VERSION << '.' << V8_MINOR_VERSION
      << "can only be run with JIT enabled, ignoring |DisableJit| call.";
}

void V8cGlobalEnvironment::SetReportEvalCallback(
    const base::Closure& report_eval) {
  DCHECK(thread_checker_.CalledOnValidThread());
  report_eval_ = report_eval;
}

void V8cGlobalEnvironment::SetReportErrorCallback(
    const ReportErrorCallback& report_error_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  report_error_callback_ = report_error_callback;
}

void V8cGlobalEnvironment::Bind(const std::string& identifier,
                                const scoped_refptr<Wrappable>& impl) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK(impl);

  EntryScope entry_scope(isolate_);
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  v8::Local<v8::Object> wrapper = wrapper_factory_->GetWrapper(impl);
  v8::Local<v8::Object> global_object = context->Global();

  v8::Maybe<bool> set_result = global_object->Set(
      context,
      v8::String::NewFromUtf8(isolate_, identifier.c_str(),
                              v8::NewStringType::kInternalized)
          .ToLocalChecked(),
      wrapper);
  DCHECK(set_result.FromJust());
}

ScriptValueFactory* V8cGlobalEnvironment::script_value_factory() {
  DCHECK(script_value_factory_);
  return script_value_factory_;
}

void V8cGlobalEnvironment::EvaluateAutomatics() {
  // TODO: Maybe add fetch and stream polyfills.  Investigate what V8 has to
  // natively offer first.
  NOTIMPLEMENTED();
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
