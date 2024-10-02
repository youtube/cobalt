// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_REQUEST_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/presentation/presentation_promise_property.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExceptionState;
class V8UnionPresentationSourceOrUSVString;

// Implements the PresentationRequest interface from the Presentation API from
// which websites can start or join presentation connections.
class MODULES_EXPORT PresentationRequest final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<PresentationRequest>,
      public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PresentationRequest(ExecutionContext*, const Vector<KURL>&);
  ~PresentationRequest() override = default;

  static PresentationRequest* Create(ExecutionContext*,
                                     const String& url,
                                     ExceptionState&);
  static PresentationRequest* Create(
      ExecutionContext*,
      const HeapVector<Member<V8UnionPresentationSourceOrUSVString>>& sources,
      ExceptionState&);

  // EventTarget implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ScriptWrappable implementation.
  bool HasPendingActivity() const final;

  ScriptPromise start(ScriptState*, ExceptionState&);
  ScriptPromise reconnect(ScriptState*, const String& id, ExceptionState&);
  ScriptPromise getAvailability(ScriptState*, ExceptionState&);

  const Vector<KURL>& Urls() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(connectionavailable, kConnectionavailable)

  void Trace(Visitor*) const override;

 protected:
  // EventTarget implementation.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

 private:
  Member<PresentationAvailabilityProperty> availability_property_;
  Vector<KURL> urls_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_REQUEST_H_
