/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "bindings/core/v8/V8ScriptRunner.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptStreamer.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8RecursionScope.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/ExecutionContext.h"
#include "core/fetch/CachedMetadata.h"
#include "core/fetch/ScriptResource.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/TraceEvent.h"

namespace blink {

namespace {

// In order to make sure all pending messages to be processed in
// v8::Function::Call, we don't call handleMaxRecursionDepthExceeded
// directly. Instead, we create a v8::Function of
// throwStackOverflowException and call it.
void throwStackOverflowException(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    V8ThrowException::throwRangeError(info.GetIsolate(), "Maximum call stack size exceeded.");
}

v8::Local<v8::Value> throwStackOverflowExceptionIfNeeded(v8::Isolate* isolate)
{
    if (V8PerIsolateData::from(isolate)->isHandlingRecursionLevelError()) {
        // If we are already handling a recursion level error, we should
        // not invoke v8::Function::Call.
        return v8::Undefined(isolate);
    }
    V8PerIsolateData::from(isolate)->setIsHandlingRecursionLevelError(true);
    v8::Local<v8::Value> result = v8::Function::New(isolate, throwStackOverflowException)->Call(v8::Undefined(isolate), 0, 0);
    V8PerIsolateData::from(isolate)->setIsHandlingRecursionLevelError(false);
    return result;
}

v8::Local<v8::Script> compileAndProduceCache(v8::Isolate* isolate, v8::Handle<v8::String> code, v8::ScriptOrigin origin, ScriptResource* resource, v8::ScriptCompiler::CompileOptions options, unsigned cacheTag, Resource::MetadataCacheType cacheType)
{
    v8::ScriptCompiler::Source source(code, origin);
    v8::Local<v8::Script> script = v8::ScriptCompiler::Compile(isolate, &source, options);
    const v8::ScriptCompiler::CachedData* cachedData = source.GetCachedData();
    if (resource && cachedData) {
        resource->clearCachedMetadata();
        resource->setCachedMetadata(
            cacheTag,
            reinterpret_cast<const char*>(cachedData->data),
            cachedData->length,
            cacheType);
    }
    return script;
}

v8::Local<v8::Script> compileAndConsumeCache(v8::Isolate* isolate, v8::Handle<v8::String> code, v8::ScriptOrigin origin, ScriptResource* resource, v8::ScriptCompiler::CompileOptions options, unsigned cacheTag)
{
    // Consume existing cache data:
    CachedMetadata* cachedMetadata = resource->cachedMetadata(cacheTag);
    v8::ScriptCompiler::CachedData* cachedData = new v8::ScriptCompiler::CachedData(
        reinterpret_cast<const uint8_t*>(cachedMetadata->data()),
        cachedMetadata->size(),
        v8::ScriptCompiler::CachedData::BufferNotOwned);
    v8::ScriptCompiler::Source source(code, origin, cachedData);
    return v8::ScriptCompiler::Compile(isolate, &source, options);
}

} // namespace

v8::Local<v8::Script> V8ScriptRunner::compileScript(const ScriptSourceCode& source, v8::Isolate* isolate, AccessControlStatus corsStatus, V8CacheOptions cacheOptions)
{
    return compileScript(v8String(isolate, source.source()), source.url(), source.startPosition(), source.resource(), source.streamer(), isolate, corsStatus, cacheOptions);
}

v8::Local<v8::Script> V8ScriptRunner::compileScript(v8::Handle<v8::String> code, const String& fileName, const TextPosition& scriptStartPosition, ScriptResource* resource, ScriptStreamer* streamer, v8::Isolate* isolate, AccessControlStatus corsStatus, V8CacheOptions cacheOptions)
{
    TRACE_EVENT1("v8", "v8.compile", "fileName", fileName.utf8());
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Compile");

    // NOTE: For compatibility with WebCore, ScriptSourceCode's line starts at
    // 1, whereas v8 starts at 0.
    v8::Handle<v8::String> name = v8String(isolate, fileName);
    v8::Handle<v8::Integer> line = v8::Integer::New(isolate, scriptStartPosition.m_line.zeroBasedInt());
    v8::Handle<v8::Integer> column = v8::Integer::New(isolate, scriptStartPosition.m_column.zeroBasedInt());
    v8::Handle<v8::Boolean> isSharedCrossOrigin = corsStatus == SharableCrossOrigin ? v8::True(isolate) : v8::False(isolate);
    v8::ScriptOrigin origin(name, line, column, isSharedCrossOrigin);

    v8::Local<v8::Script> script;
    unsigned cacheTag = 0;
    if (streamer) {
        // We don't stream scripts which don't have a Resource.
        ASSERT(resource);
        // Failed resources should never get this far.
        ASSERT(!resource->errorOccurred());
        ASSERT(streamer->isFinished());
        ASSERT(!streamer->streamingSuppressed());
        script = v8::ScriptCompiler::Compile(isolate, streamer->source(), code, origin);
        // Whether to produce the cached data or not is decided when the
        // streamer is started. Here we only need to get the data out.
        const v8::ScriptCompiler::CachedData* newCachedData = streamer->source()->GetCachedData();
        if (newCachedData) {
            resource->clearCachedMetadata();
            resource->setCachedMetadata(streamer->cachedDataType(), reinterpret_cast<const char*>(newCachedData->data), newCachedData->length);
        }
    } else if (!resource || !resource->url().protocolIsInHTTPFamily() || code->Length() < 1024) {
        v8::ScriptCompiler::Source source(code, origin);
        script = v8::ScriptCompiler::Compile(isolate, &source, v8::ScriptCompiler::kNoCompileOptions);
    } else {
        switch (cacheOptions) {
        case V8CacheOptionsParse:
            cacheTag = tagForParserCache();
            script = resource->cachedMetadata(cacheTag)
                ? compileAndConsumeCache(isolate, code, origin, resource, v8::ScriptCompiler::kConsumeParserCache, cacheTag)
                : compileAndProduceCache(isolate, code, origin, resource, v8::ScriptCompiler::kProduceParserCache, cacheTag, Resource::SendToPlatform);
            break;
        case V8CacheOptionsCode:
            cacheTag = tagForCodeCache();
            script = resource->cachedMetadata(cacheTag)
                ? compileAndConsumeCache(isolate, code, origin, resource, v8::ScriptCompiler::kConsumeCodeCache, cacheTag)
                : compileAndProduceCache(isolate, code, origin, resource, v8::ScriptCompiler::kProduceCodeCache, cacheTag, Resource::SendToPlatform);
            break;
        case V8CacheOptionsOff:
            // Previous behaviour was to always generate an in-memory parser
            // cache. We emulate this here.
            // FIXME: Determine whether this should get its own setting, so we
            //        can also have a true 'off'.
            cacheTag = tagForParserCache();
            script = resource->cachedMetadata(cacheTag)
                ? compileAndConsumeCache(isolate, code, origin, resource, v8::ScriptCompiler::kConsumeParserCache, cacheTag)
                : compileAndProduceCache(isolate, code, origin, resource, v8::ScriptCompiler::kProduceParserCache, cacheTag, Resource::CacheLocally);
            break;
        }
    }
    return script;
}

v8::Local<v8::Value> V8ScriptRunner::runCompiledScript(v8::Isolate* isolate, v8::Handle<v8::Script> script, ExecutionContext* context)
{
    if (script.IsEmpty())
        return v8::Local<v8::Value>();
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");
    TRACE_EVENT1("v8", "v8.run", "fileName", TRACE_STR_COPY(*v8::String::Utf8Value(script->GetUnboundScript()->GetScriptName())));

    if (V8RecursionScope::recursionLevel(isolate) >= kMaxRecursionDepth)
        return throwStackOverflowExceptionIfNeeded(isolate);

    RELEASE_ASSERT(!context->isIteratingOverObservers());

    // Run the script and keep track of the current recursion depth.
    v8::Local<v8::Value> result;
    {
        if (ScriptForbiddenScope::isScriptForbidden())
            return v8::Local<v8::Value>();
        V8RecursionScope recursionScope(isolate);
        result = script->Run();
    }

    if (result.IsEmpty())
        return v8::Local<v8::Value>();

    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Value> V8ScriptRunner::compileAndRunInternalScript(v8::Handle<v8::String> source, v8::Isolate* isolate, const String& fileName, const TextPosition& scriptStartPosition)
{
    v8::Handle<v8::Script> script = V8ScriptRunner::compileScript(source, fileName, scriptStartPosition, 0, 0, isolate);
    if (script.IsEmpty())
        return v8::Local<v8::Value>();

    TRACE_EVENT0("v8", "v8.run");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");
    V8RecursionScope::MicrotaskSuppression recursionScope(isolate);
    v8::Local<v8::Value> result = script->Run();
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Value> V8ScriptRunner::runCompiledInternalScript(v8::Isolate* isolate, v8::Handle<v8::Script> script)
{
    TRACE_EVENT0("v8", "v8.run");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");
    V8RecursionScope::MicrotaskSuppression recursionScope(isolate);
    v8::Local<v8::Value> result = script->Run();
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Value> V8ScriptRunner::callFunction(v8::Handle<v8::Function> function, ExecutionContext* context, v8::Handle<v8::Value> receiver, int argc, v8::Handle<v8::Value> args[], v8::Isolate* isolate)
{
    TRACE_EVENT0("v8", "v8.callFunction");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");

    if (V8RecursionScope::recursionLevel(isolate) >= kMaxRecursionDepth)
        return throwStackOverflowExceptionIfNeeded(isolate);

    RELEASE_ASSERT(!context->isIteratingOverObservers());

    if (ScriptForbiddenScope::isScriptForbidden())
        return v8::Local<v8::Value>();
    V8RecursionScope recursionScope(isolate);
    v8::Local<v8::Value> result = function->Call(receiver, argc, args);
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Value> V8ScriptRunner::callInternalFunction(v8::Handle<v8::Function> function, v8::Handle<v8::Value> receiver, int argc, v8::Handle<v8::Value> args[], v8::Isolate* isolate)
{
    TRACE_EVENT0("v8", "v8.callFunction");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");
    V8RecursionScope::MicrotaskSuppression recursionScope(isolate);
    v8::Local<v8::Value> result = function->Call(receiver, argc, args);
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Value> V8ScriptRunner::callAsFunction(v8::Isolate* isolate, v8::Handle<v8::Object> object, v8::Handle<v8::Value> receiver, int argc, v8::Handle<v8::Value> args[])
{
    TRACE_EVENT0("v8", "v8.callFunction");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");

    V8RecursionScope::MicrotaskSuppression recursionScope(isolate);
    v8::Local<v8::Value> result = object->CallAsFunction(receiver, argc, args);
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Object> V8ScriptRunner::instantiateObject(v8::Isolate* isolate, v8::Handle<v8::ObjectTemplate> objectTemplate)
{
    TRACE_EVENT0("v8", "v8.newInstance");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");

    V8RecursionScope::MicrotaskSuppression scope(isolate);
    v8::Local<v8::Object> result = objectTemplate->NewInstance();
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Object> V8ScriptRunner::instantiateObject(v8::Isolate* isolate, v8::Handle<v8::Function> function, int argc, v8::Handle<v8::Value> argv[])
{
    TRACE_EVENT0("v8", "v8.newInstance");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");

    V8RecursionScope::MicrotaskSuppression scope(isolate);
    v8::Local<v8::Object> result = function->NewInstance(argc, argv);
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Object> V8ScriptRunner::instantiateObjectInDocument(v8::Isolate* isolate, v8::Handle<v8::Function> function, ExecutionContext* context, int argc, v8::Handle<v8::Value> argv[])
{
    TRACE_EVENT0("v8", "v8.newInstance");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("v8", "V8Execution");
    if (ScriptForbiddenScope::isScriptForbidden())
        return v8::Local<v8::Object>();
    V8RecursionScope scope(isolate);
    v8::Local<v8::Object> result = function->NewInstance(argc, argv);
    crashIfV8IsDead();
    return result;
}

unsigned V8ScriptRunner::tagForParserCache()
{
    return StringHash::hash(v8::V8::GetVersion()) * 2;
}

unsigned V8ScriptRunner::tagForCodeCache()
{
    return StringHash::hash(v8::V8::GetVersion()) * 2 + 1;
}

} // namespace blink
