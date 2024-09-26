// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MANAGED_DEVICE_NAVIGATOR_MANAGED_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MANAGED_DEVICE_NAVIGATOR_MANAGED_DATA_H_

#include "third_party/blink/public/mojom/device/device.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Navigator;
class ExecutionContext;
class ScriptPromiseResolver;
class ScriptPromise;
class ScriptState;

class MODULES_EXPORT NavigatorManagedData final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<NavigatorManagedData>,
      public Supplement<Navigator>,
      public mojom::blink::ManagedConfigurationObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // Web-based getter for navigator.managed.
  static NavigatorManagedData* managed(Navigator&);

  explicit NavigatorManagedData(Navigator&);
  NavigatorManagedData(const NavigatorManagedData&) = delete;
  NavigatorManagedData& operator=(const NavigatorManagedData&) = delete;

  void Trace(Visitor*) const override;

  // EventTargetWithInlineData:
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  void AddedEventListener(
      const AtomicString& event_type,
      RegisteredEventListener& registered_listener) override;
  void RemovedEventListener(
      const AtomicString& event_type,
      const RegisteredEventListener& registered_listener) override;

  // ScriptWrappable implementation.
  bool HasPendingActivity() const final;

  // Managed Configuration API:
  ScriptPromise getManagedConfiguration(ScriptState* script_state,
                                        Vector<String> keys);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(managedconfigurationchange,
                                  kManagedconfigurationchange)

  // Device Attributes API:
  ScriptPromise getDirectoryId(ScriptState* script_state);
  ScriptPromise getHostname(ScriptState* script_state);
  ScriptPromise getSerialNumber(ScriptState* script_state);
  ScriptPromise getAnnotatedAssetId(ScriptState* script_state);
  ScriptPromise getAnnotatedLocation(ScriptState* script_state);

 private:
  // ManagedConfigurationObserver:
  void OnConfigurationChanged() override;

  void OnConfigurationReceived(
      ScriptPromiseResolver* scoped_resolver,
      const absl::optional<HashMap<String, String>>& configurations);

  void OnAttributeReceived(ScriptState* script_state,
                           ScriptPromiseResolver* scoped_resolver,
                           mojom::blink::DeviceAttributeResultPtr result);

  // Lazily binds mojo interface.
  mojom::blink::DeviceAPIService* GetService();

  // Lazily binds mojo interface.
  mojom::blink::ManagedConfigurationService* GetManagedConfigurationService();

  void OnServiceConnectionError();
  void StopObserving();

  HeapMojoRemote<mojom::blink::DeviceAPIService> device_api_service_;
  HeapMojoRemote<mojom::blink::ManagedConfigurationService>
      managed_configuration_service_;

  HeapMojoReceiver<mojom::blink::ManagedConfigurationObserver,
                   NavigatorManagedData>
      configuration_observer_;
  HeapHashSet<Member<ScriptPromiseResolver>> pending_promises_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MANAGED_DEVICE_NAVIGATOR_MANAGED_DATA_H_
