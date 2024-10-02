// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/per_isolate_data.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "gin/public/gin_embedders.h"
#include "gin/v8_foreground_task_runner.h"
#include "gin/v8_foreground_task_runner_with_locker.h"
#include "v8/include/v8-isolate.h"

using v8::ArrayBuffer;
using v8::Eternal;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::FunctionTemplate;
using v8::ObjectTemplate;

namespace gin {

PerIsolateData::PerIsolateData(
    Isolate* isolate,
    ArrayBuffer::Allocator* allocator,
    IsolateHolder::AccessMode access_mode,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : isolate_(isolate), allocator_(allocator) {
  isolate_->SetData(kEmbedderNativeGin, this);

  DCHECK(task_runner);
  if (access_mode == IsolateHolder::kUseLocker) {
    task_runner_ = std::make_shared<V8ForegroundTaskRunnerWithLocker>(
        isolate, task_runner);
  } else {
    task_runner_ = std::make_shared<V8ForegroundTaskRunner>(task_runner);
  }
}

PerIsolateData::~PerIsolateData() = default;

PerIsolateData* PerIsolateData::From(Isolate* isolate) {
  return static_cast<PerIsolateData*>(isolate->GetData(kEmbedderNativeGin));
}

void PerIsolateData::SetObjectTemplate(WrapperInfo* info,
                                       Local<ObjectTemplate> templ) {
  object_templates_[info] = Eternal<ObjectTemplate>(isolate_, templ);
}

void PerIsolateData::SetFunctionTemplate(WrapperInfo* info,
                                         Local<FunctionTemplate> templ) {
  function_templates_[info] = Eternal<FunctionTemplate>(isolate_, templ);
}

v8::Local<v8::ObjectTemplate> PerIsolateData::GetObjectTemplate(
    WrapperInfo* info) {
  ObjectTemplateMap::iterator it = object_templates_.find(info);
  if (it == object_templates_.end())
    return v8::Local<v8::ObjectTemplate>();
  return it->second.Get(isolate_);
}

v8::Local<v8::FunctionTemplate> PerIsolateData::GetFunctionTemplate(
    WrapperInfo* info) {
  FunctionTemplateMap::iterator it = function_templates_.find(info);
  if (it == function_templates_.end())
    return v8::Local<v8::FunctionTemplate>();
  return it->second.Get(isolate_);
}

void PerIsolateData::SetIndexedPropertyInterceptor(
    WrappableBase* base,
    IndexedPropertyInterceptor* interceptor) {
  indexed_interceptors_[base] = interceptor;
}

void PerIsolateData::SetNamedPropertyInterceptor(
    WrappableBase* base,
    NamedPropertyInterceptor* interceptor) {
  named_interceptors_[base] = interceptor;
}

void PerIsolateData::ClearIndexedPropertyInterceptor(
    WrappableBase* base,
    IndexedPropertyInterceptor* interceptor) {
  IndexedPropertyInterceptorMap::iterator it = indexed_interceptors_.find(base);
  if (it != indexed_interceptors_.end())
    indexed_interceptors_.erase(it);
  else
    NOTREACHED();
}

void PerIsolateData::ClearNamedPropertyInterceptor(
    WrappableBase* base,
    NamedPropertyInterceptor* interceptor) {
  NamedPropertyInterceptorMap::iterator it = named_interceptors_.find(base);
  if (it != named_interceptors_.end())
    named_interceptors_.erase(it);
  else
    NOTREACHED();
}

IndexedPropertyInterceptor* PerIsolateData::GetIndexedPropertyInterceptor(
    WrappableBase* base) {
  IndexedPropertyInterceptorMap::iterator it = indexed_interceptors_.find(base);
  if (it != indexed_interceptors_.end())
    return it->second;
  else
    return NULL;
}

NamedPropertyInterceptor* PerIsolateData::GetNamedPropertyInterceptor(
    WrappableBase* base) {
  NamedPropertyInterceptorMap::iterator it = named_interceptors_.find(base);
  if (it != named_interceptors_.end())
    return it->second;
  else
    return NULL;
}

void PerIsolateData::EnableIdleTasks(
    std::unique_ptr<V8IdleTaskRunner> idle_task_runner) {
  task_runner_->EnableIdleTasks(std::move(idle_task_runner));
}

}  // namespace gin
