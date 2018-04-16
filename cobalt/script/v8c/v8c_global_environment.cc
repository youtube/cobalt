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

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/v8c/embedded_resources.h"
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
    : isolate_(isolate),
      destruction_helper_(isolate),
      wrapper_factory_(new WrapperFactory(isolate)),
      script_value_factory_(new V8cScriptValueFactory(isolate)) {
  TRACK_MEMORY_SCOPE("Javascript");
  TRACE_EVENT0("cobalt::script",
               "V8cGlobalEnvironment::V8cGlobalEnvironment()");
  wrapper_factory_.reset(new WrapperFactory(isolate));
  isolate_->SetData(kIsolateDataIndex, this);
  DCHECK(isolate_->GetData(kIsolateDataIndex) == this);

  isolate_->SetAllowCodeGenerationFromStringsCallback(
      AllowCodeGenerationFromStringsCallback);

  isolate_->SetAllowWasmCodeGenerationCallback(
      [](v8::Local<v8::Context> context, v8::Local<v8::String> source) {
        return false;
      });
}

V8cGlobalEnvironment::~V8cGlobalEnvironment() {
  TRACE_EVENT0("cobalt::script",
               "V8cGlobalEnvironment::~V8cGlobalEnvironment()");
  DCHECK(thread_checker_.CalledOnValidThread());
}

void V8cGlobalEnvironment::CreateGlobalObject() {
  TRACE_EVENT0("cobalt::script", "V8cGlobalEnvironment::CreateGlobalObject()");
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
    const scoped_refptr<SourceCode>& source_code,
    std::string* out_result_utf8) {
  TRACK_MEMORY_SCOPE("Javascript");
  TRACE_EVENT0("cobalt::script", "V8cGlobalEnvironment::EvaluateScript()");
  DCHECK(thread_checker_.CalledOnValidThread());

  EntryScope entry_scope(isolate_);
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();
  v8::TryCatch try_catch(isolate_);

  v8::Local<v8::Value> result;
  if (!EvaluateScriptInternal(source_code).ToLocal(&result)) {
    if (!try_catch.HasCaught()) {
      LOG(WARNING) << "Script evaluation failed with no JavaScript exception.";
      return false;
    }
    if (out_result_utf8) {
      *out_result_utf8 = ExceptionToString(try_catch);
    }
    return false;
  }

  if (out_result_utf8) {
    V8cExceptionState exception_state(isolate_);
    FromJSValue(isolate_, result, kNoConversionFlags, &exception_state,
                out_result_utf8);
  }

  return true;
}

bool V8cGlobalEnvironment::EvaluateScript(
    const scoped_refptr<SourceCode>& source_code,
    const scoped_refptr<Wrappable>& owning_object,
    base::optional<ValueHandleHolder::Reference>* out_value_handle) {
  TRACK_MEMORY_SCOPE("Javascript");
  TRACE_EVENT0("cobalt::script", "V8cGlobalEnvironment::EvaluateScript()");
  DCHECK(thread_checker_.CalledOnValidThread());

  EntryScope entry_scope(isolate_);
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();
  v8::TryCatch try_catch(isolate_);

  v8::Local<v8::Value> result;
  if (!EvaluateScriptInternal(source_code).ToLocal(&result)) {
    if (!try_catch.HasCaught()) {
      LOG(WARNING) << "Script evaluation failed with no JavaScript exception.";
    }
    return false;
  }

  if (out_value_handle) {
    V8cValueHandleHolder v8c_value_handle_holder(isolate_, result);
    out_value_handle->emplace(owning_object.get(), v8c_value_handle_holder);
  }

  return true;
}

std::vector<StackFrame> V8cGlobalEnvironment::GetStackTrace(int max_frames) {
  TRACE_EVENT0("cobalt::script", "V8cGlobalEnvironment::GetStackTrace()");
  DCHECK(thread_checker_.CalledOnValidThread());

  // cobalt::script treats |max_frames| being set to 0 as "the entire stack",
  // while V8 interprets the frame count being set to 0 as "give me 0 frames",
  // so we have to translate between the two.
  const int kV8CallMaxFrameAmount = 4096;
  int v8_max_frames = (max_frames == 0) ? kV8CallMaxFrameAmount : max_frames;

  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::StackTrace> stack_trace =
      v8::StackTrace::CurrentStackTrace(isolate_, v8_max_frames);

  std::vector<StackFrame> result;
  for (int i = 0; i < stack_trace->GetFrameCount(); i++) {
    v8::Local<v8::StackFrame> stack_frame = stack_trace->GetFrame(i);
    result.emplace_back(
        stack_frame->GetLineNumber(), stack_frame->GetColumn(),
        *v8::String::Utf8Value(isolate_, stack_frame->GetFunctionName()),
        *v8::String::Utf8Value(isolate_, stack_frame->GetScriptName()));
  }

  return result;
}

void V8cGlobalEnvironment::AddRoot(Traceable* traceable) {
  V8cEngine::GetFromIsolate(isolate_)->heap_tracer()->AddRoot(traceable);
}

void V8cGlobalEnvironment::RemoveRoot(Traceable* traceable) {
  V8cEngine::GetFromIsolate(isolate_)->heap_tracer()->RemoveRoot(traceable);
}

void V8cGlobalEnvironment::PreventGarbageCollection(
    const scoped_refptr<Wrappable>& wrappable) {
  DCHECK(thread_checker_.CalledOnValidThread());

  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Object> wrapper = wrapper_factory_->GetWrapper(wrappable);
  WrapperPrivate::GetFromWrapperObject(wrapper)->IncrementRefCount();
}

void V8cGlobalEnvironment::AllowGarbageCollection(
    const scoped_refptr<Wrappable>& wrappable) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK(thread_checker_.CalledOnValidThread());

  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Object> wrapper = wrapper_factory_->GetWrapper(wrappable);
  WrapperPrivate::GetFromWrapperObject(wrapper)->DecrementRefCount();
}

void V8cGlobalEnvironment::DisableEval(const std::string& message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  EntryScope entry_scope(isolate_);
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();
  context->AllowCodeGenerationFromStrings(false);
  context->SetErrorMessageForCodeGenerationFromStrings(
      v8::String::NewFromUtf8(isolate_, message.c_str(),
                              v8::NewStringType::kNormal)
          .ToLocalChecked());
}

void V8cGlobalEnvironment::EnableEval() {
  DCHECK(thread_checker_.CalledOnValidThread());

  EntryScope entry_scope(isolate_);
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();
  context->AllowCodeGenerationFromStrings(true);
}

void V8cGlobalEnvironment::DisableJit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG(INFO) << "V8 version " << V8_MAJOR_VERSION << '.' << V8_MINOR_VERSION
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
  TRACE_EVENT0("cobalt::script", "V8cGlobalEnvironment::Bind()");
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
  return script_value_factory_.get();
}

V8cGlobalEnvironment::DestructionHelper::~DestructionHelper() {
  class ForceWeakVisitor : public v8::PersistentHandleVisitor {
   public:
    explicit ForceWeakVisitor(v8::Isolate* isolate) : isolate_(isolate) {}
    void VisitPersistentHandle(v8::Persistent<v8::Value>* value,
                               uint16_t class_id) override {
      if (class_id == WrapperPrivate::kClassId) {
        v8::Local<v8::Value> v = value->Get(isolate_);
        DCHECK(v->IsObject());
        WrapperPrivate* wrapper_private =
            WrapperPrivate::GetFromWrapperObject(v.As<v8::Object>());
        wrapper_private->ForceWeakForShutDown();
      }
    }

   private:
    v8::Isolate* isolate_;
  };

  TRACE_EVENT0("cobalt::script",
               "V8cGlobalEnvironment::DestructionHelper::~DestructionHelper()");

  {
    TRACE_EVENT0("cobalt::script",
                 "V8cGlobalEnvironment::DestructionHelper::~DestructionHelper::"
                 "ForceWeakVisitor");
    v8::HandleScope handle_scope(isolate_);
    ForceWeakVisitor force_weak_visitor(isolate_);
    isolate_->VisitHandlesWithClassIds(&force_weak_visitor);
  }

  isolate_->SetEmbedderHeapTracer(nullptr);
  isolate_->SetData(kIsolateDataIndex, nullptr);
  isolate_->LowMemoryNotification();
}

// static
bool V8cGlobalEnvironment::AllowCodeGenerationFromStringsCallback(
    v8::Local<v8::Context> context, v8::Local<v8::String> source) {
  V8cGlobalEnvironment* global_environment =
      V8cGlobalEnvironment::GetFromIsolate(context->GetIsolate());
  DCHECK(global_environment);
  if (!global_environment->report_eval_.is_null()) {
    global_environment->report_eval_.Run();
  }
  // This callback should only be called while code generation from strings is
  // not allowed from within V8, so this should always be false.  Note that
  // WebAssembly code generation will fall back to this callback if a
  // WebAssembly callback has not been explicitly set, however we *have* set
  // one.
  DCHECK_EQ(context->IsCodeGenerationFromStringsAllowed(), false);
  return context->IsCodeGenerationFromStringsAllowed();
}

v8::MaybeLocal<v8::Value> V8cGlobalEnvironment::EvaluateScriptInternal(
    const scoped_refptr<SourceCode>& source_code) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK(thread_checker_.CalledOnValidThread());

  // Note that we expect an |EntryScope| and |v8::TryCatch| to have been set
  // up by our caller.
  V8cSourceCode* v8c_source_code =
      base::polymorphic_downcast<V8cSourceCode*>(source_code.get());
  const base::SourceLocation& source_location = v8c_source_code->location();

  v8::Local<v8::String> resource_name;
  if (!v8::String::NewFromUtf8(isolate_, source_location.file_path.c_str(),
                               v8::NewStringType::kNormal)
           .ToLocal(&resource_name)) {
    // Technically possible, but whoa man should this never happen.
    LOG(WARNING) << "Failed to convert source location file path \""
                 << source_location.file_path << "\" to a V8 UTF-8 string.";
    return {};
  }

  // Note that |v8::ScriptOrigin| offsets are 0-based, whereas
  // |SourceLocation| line/column numbers are 1-based, so subtract 1 to
  // translate between the two.
  v8::ScriptOrigin script_origin(
      /*resource_name=*/resource_name,
      /*resource_line_offset=*/
      v8::Integer::New(isolate_, source_location.line_number - 1),
      /*resource_column_offset=*/
      v8::Integer::New(isolate_, source_location.column_number - 1),
      /*resource_is_shared_cross_origin=*/
      v8::Boolean::New(isolate_, !v8c_source_code->is_muted()));

  v8::Local<v8::String> source;
  if (!v8::String::NewFromUtf8(isolate_, v8c_source_code->source_utf8().c_str(),
                               v8::NewStringType::kNormal)
           .ToLocal(&source)) {
    LOG(WARNING) << "Failed to convert source code to V8 UTF-8 string.";
    return {};
  }

  v8::Local<v8::Context> context = isolate_->GetCurrentContext();
  v8::Local<v8::Script> script;
  {
    TRACE_EVENT0("cobalt::script", "v8::Script::Compile()");
    if (!v8::Script::Compile(context, source, &script_origin)
             .ToLocal(&script)) {
      LOG(WARNING) << "Failed to compile script.";
      return {};
    }
  }

  v8::Local<v8::Value> result;
  {
    TRACE_EVENT0("cobalt::script", "v8::Script::Run()");
    if (!script->Run(context).ToLocal(&result)) {
      LOG(WARNING) << "Failed to run script.";
      return {};
    }
  }

  return result;
}

void V8cGlobalEnvironment::EvaluateEmbeddedScript(const unsigned char* data,
                                                  size_t size,
                                                  const char* filename) {
  TRACE_EVENT1("cobalt::script", "V8cGlobalEnvironment::EvaluateEmbeddedScript",
               "filename", filename);
  TRACK_MEMORY_SCOPE("Javascript");
  std::string source(reinterpret_cast<const char*>(data), size);
  scoped_refptr<SourceCode> source_code =
      new V8cSourceCode(source, base::SourceLocation(filename, 1, 1));
  std::string result;
  bool success = EvaluateScript(source_code, &result);
  if (!success) {
    DLOG(FATAL) << result;
  }
}

void V8cGlobalEnvironment::EvaluateAutomatics() {
  TRACE_EVENT0("cobalt::script", "V8cGlobalEnvironment::EvaluateAutomatics()");
  EvaluateEmbeddedScript(
      V8cEmbeddedResources::byte_length_queuing_strategy_js,
      sizeof(V8cEmbeddedResources::byte_length_queuing_strategy_js),
      "byte_length_queuing_strategy.js");
  EvaluateEmbeddedScript(
      V8cEmbeddedResources::count_queuing_strategy_js,
      sizeof(V8cEmbeddedResources::count_queuing_strategy_js),
      "count_queuing_strategy.js");
  EvaluateEmbeddedScript(V8cEmbeddedResources::readable_stream_js,
                         sizeof(V8cEmbeddedResources::readable_stream_js),
                         "readable_stream.js");
  EvaluateEmbeddedScript(V8cEmbeddedResources::fetch_js,
                         sizeof(V8cEmbeddedResources::fetch_js), "fetch.js");
}

bool V8cGlobalEnvironment::HasInterfaceData(int key) const {
  DCHECK_GE(key, 0);
  if (key >= cached_interface_data_.size()) {
    return false;
  }
  return !cached_interface_data_[key].IsEmpty();
}

v8::Local<v8::FunctionTemplate> V8cGlobalEnvironment::GetInterfaceData(
    int key) const {
  DCHECK(HasInterfaceData(key));
  return cached_interface_data_[key].Get(isolate_);
}

void V8cGlobalEnvironment::AddInterfaceData(
    int key, v8::Local<v8::FunctionTemplate> function_template) {
  DCHECK(!HasInterfaceData(key));
  if (key >= cached_interface_data_.size()) {
    cached_interface_data_.resize(key + 1);
  }
  DCHECK(!HasInterfaceData(key));
  cached_interface_data_[key].Set(isolate_, function_template);
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
