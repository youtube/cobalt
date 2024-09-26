// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h"

#include <memory>
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

typedef HashMap<String, std::unique_ptr<V8DOMActivityLogger>>
    DOMActivityLoggerMapForMainWorld;
typedef HashMap<int,
                std::unique_ptr<V8DOMActivityLogger>,
                IntWithZeroKeyHashTraits<int>>
    DOMActivityLoggerMapForIsolatedWorld;

static DOMActivityLoggerMapForMainWorld& DomActivityLoggersForMainWorld() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(DOMActivityLoggerMapForMainWorld, map, ());
  return map;
}

static DOMActivityLoggerMapForIsolatedWorld&
DomActivityLoggersForIsolatedWorld() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(DOMActivityLoggerMapForIsolatedWorld, map, ());
  return map;
}

void V8DOMActivityLogger::LogMethod(const char* api_name,
                                    v8::FunctionCallbackInfo<v8::Value> info) {
  Vector<v8::Local<v8::Value>> loggerArgs;
  loggerArgs.ReserveInitialCapacity(info.Length());
  for (int i = 0; i < info.Length(); ++i) {
    loggerArgs.UncheckedAppend(info[i]);
  }
  LogMethod(api_name, info.Length(), loggerArgs.data());
}

void V8DOMActivityLogger::SetActivityLogger(
    int world_id,
    const String& extension_id,
    std::unique_ptr<V8DOMActivityLogger> logger) {
  if (world_id)
    DomActivityLoggersForIsolatedWorld().Set(world_id, std::move(logger));
  else
    DomActivityLoggersForMainWorld().Set(extension_id, std::move(logger));
}

V8DOMActivityLogger* V8DOMActivityLogger::ActivityLogger(
    int world_id,
    const String& extension_id) {
  if (world_id) {
    DOMActivityLoggerMapForIsolatedWorld& loggers =
        DomActivityLoggersForIsolatedWorld();
    DOMActivityLoggerMapForIsolatedWorld::iterator it = loggers.find(world_id);
    return it == loggers.end() ? nullptr : it->value.get();
  }

  if (extension_id.empty())
    return nullptr;

  DOMActivityLoggerMapForMainWorld& loggers = DomActivityLoggersForMainWorld();
  DOMActivityLoggerMapForMainWorld::iterator it = loggers.find(extension_id);
  return it == loggers.end() ? nullptr : it->value.get();
}

V8DOMActivityLogger* V8DOMActivityLogger::ActivityLogger(int world_id,
                                                         const KURL& url) {
  // extension ID is ignored for worldId != 0.
  if (world_id)
    return ActivityLogger(world_id, String());

  // To find an activity logger that corresponds to the main world of an
  // extension, we need to obtain the extension ID. Extension ID is a hostname
  // of a background page's URL.
  if (!CommonSchemeRegistry::IsExtensionScheme(url.Protocol().Ascii()))
    return nullptr;

  return ActivityLogger(world_id, url.Host());
}

V8DOMActivityLogger* V8DOMActivityLogger::CurrentActivityLogger() {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (!isolate->InContext())
    return nullptr;

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  V8PerContextData* context_data = ScriptState::From(context)->PerContextData();
  if (!context_data)
    return nullptr;

  return context_data->ActivityLogger();
}

V8DOMActivityLogger* V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorld(
    v8::Isolate* isolate) {
  if (!isolate->InContext())
    return nullptr;

  ScriptState* script_state = ScriptState::From(isolate->GetCurrentContext());
  if (!script_state->World().IsIsolatedWorld())
    return nullptr;

  V8PerContextData* context_data = script_state->PerContextData();
  if (!context_data)
    return nullptr;

  return context_data->ActivityLogger();
}

V8DOMActivityLogger*
V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorld() {
  return CurrentActivityLoggerIfIsolatedWorld(v8::Isolate::GetCurrent());
}

V8DOMActivityLogger*
V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorldForMainThread() {
  DCHECK(IsMainThread());
  if (DomActivityLoggersForIsolatedWorld().empty())
    return nullptr;
  return CurrentActivityLoggerIfIsolatedWorld(
      V8PerIsolateData::MainThreadIsolate());
}

}  // namespace blink
