// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_GLOBAL_FETCH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_GLOBAL_FETCH_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/fetch/request.h"

namespace blink {

class ExceptionState;
class LocalDOMWindow;
class NavigatorBase;
class RequestInit;
class ScriptState;
class WorkerGlobalScope;

class CORE_EXPORT GlobalFetch {
  STATIC_ONLY(GlobalFetch);

 public:
  class CORE_EXPORT ScopedFetcher : public GarbageCollectedMixin {
   public:
    virtual ~ScopedFetcher();

    virtual ScriptPromise Fetch(ScriptState*,
                                const V8RequestInfo*,
                                const RequestInit*,
                                ExceptionState&) = 0;

    // Returns the number of fetch() method calls in the associated execution
    // context.  This is used for metrics.
    virtual uint32_t FetchCount() const = 0;

    static ScopedFetcher* From(LocalDOMWindow&);
    static ScopedFetcher* From(WorkerGlobalScope&);
    static ScopedFetcher* From(NavigatorBase& navigator);

    void Trace(Visitor*) const override;
  };

  static ScriptPromise fetch(ScriptState* script_state,
                             LocalDOMWindow& window,
                             const V8RequestInfo* input,
                             const RequestInit* init,
                             ExceptionState& exception_state);
  static ScriptPromise fetch(ScriptState* script_state,
                             WorkerGlobalScope& worker,
                             const V8RequestInfo* input,
                             const RequestInit* init,
                             ExceptionState& exception_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_GLOBAL_FETCH_H_
