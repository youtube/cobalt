// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_HOST_H_

#include "third_party/blink/public/mojom/portal/portal.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

struct BlinkTransferableMessage;
class ExecutionContext;
class LocalDOMWindow;
class ScriptValue;
class SecurityOrigin;
class PostMessageOptions;

class CORE_EXPORT PortalHost : public EventTarget,
                               public Supplement<LocalDOMWindow> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit PortalHost(LocalDOMWindow& window);

  void Trace(Visitor* visitor) const override;

  static const char kSupplementName[];
  static PortalHost& From(LocalDOMWindow& window);

  // EventTarget overrides
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  PortalHost* ToPortalHost() override;

  // Called immediately before dispatching the onactivate event.
  void OnPortalActivated();

  // idl implementation
  void postMessage(ScriptState* script_state,
                   const ScriptValue& message,
                   const PostMessageOptions* options,
                   ExceptionState& exception_state);
  EventListener* onmessage();
  void setOnmessage(EventListener* listener);
  EventListener* onmessageerror();
  void setOnmessageerror(EventListener* listener);

  void ReceiveMessage(BlinkTransferableMessage message,
                      scoped_refptr<const SecurityOrigin> source_origin);

 private:
  mojom::blink::PortalHost& GetPortalHostInterface();

  HeapMojoAssociatedRemote<mojom::blink::PortalHost> portal_host_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_HOST_H_
