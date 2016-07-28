/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WorkerScriptController_h
#define WorkerScriptController_h

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "wtf/OwnPtr.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/text/TextPosition.h"
#include <v8.h>

namespace blink {

class ErrorEvent;
class ExceptionState;
class ScriptSourceCode;
class WorkerGlobalScope;

class WorkerScriptController {
public:
    explicit WorkerScriptController(WorkerGlobalScope&);
    ~WorkerScriptController();

    bool isExecutionForbidden() const;
    bool isExecutionTerminating() const;
    void evaluate(const ScriptSourceCode&, RefPtrWillBeRawPtr<ErrorEvent>* = 0);

    // Prevents future JavaScript execution. See
    // scheduleExecutionTermination, isExecutionForbidden.
    void forbidExecution();

    // Used by WorkerThread:
    bool initializeContextIfNeeded();
    // Async request to terminate future JavaScript execution on the
    // worker thread. JavaScript evaluation exits with a
    // non-continuable exception and WorkerScriptController calls
    // forbidExecution to prevent further JavaScript execution. Use
    // forbidExecution()/isExecutionForbidden() to guard against
    // reentry into JavaScript.
    void scheduleExecutionTermination();

    // Used by WorkerGlobalScope:
    void rethrowExceptionFromImportedScript(PassRefPtrWillBeRawPtr<ErrorEvent>, ExceptionState&);
    void disableEval(const String&);
    // Send a notification about current thread is going to be idle.
    // Returns true if the embedder should stop calling idleNotification
    // until real work has been done.
    bool idleNotification() { return m_isolate->IdleNotification(1000); }

    // Used by Inspector agents:
    ScriptState* scriptState() { return m_scriptState.get(); }

    // Used by V8 bindings:
    v8::Local<v8::Context> context() { return m_scriptState ? m_scriptState->context() : v8::Local<v8::Context>(); }

private:
    class WorkerGlobalScopeExecutionState;

    bool isContextInitialized() { return m_scriptState && !!m_scriptState->perContextData(); }

    // Evaluate a script file in the current execution environment.
    ScriptValue evaluate(const String& script, const String& fileName, const TextPosition& scriptStartPosition);

    v8::Isolate* m_isolate;
    WorkerGlobalScope& m_workerGlobalScope;
    RefPtr<ScriptState> m_scriptState;
    RefPtr<DOMWrapperWorld> m_world;
    String m_disableEvalPending;
    bool m_executionForbidden;
    bool m_executionScheduledToTerminate;
    mutable Mutex m_scheduledTerminationMutex;

    // |m_globalScopeExecutionState| refers to a stack object
    // that evaluate() allocates; evaluate() ensuring that the
    // pointer reference to it is removed upon returning. Hence
    // kept as a bare pointer here, and not a Persistent with
    // Oilpan enabled; stack scanning will visit the object and
    // trace its on-heap fields.
    GC_PLUGIN_IGNORE("394615")
    WorkerGlobalScopeExecutionState* m_globalScopeExecutionState;
    OwnPtr<V8IsolateInterruptor> m_interruptor;
};

} // namespace blink

#endif // WorkerScriptController_h
