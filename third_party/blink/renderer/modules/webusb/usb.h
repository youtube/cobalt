// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBUSB_USB_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBUSB_USB_H_

#include "base/functional/function_ref.h"
#include "services/device/public/mojom/usb_manager.mojom-blink-forward.h"
#include "services/device/public/mojom/usb_manager_client.mojom-blink.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class DOMWrapperWorld;
class ExceptionState;
class NavigatorBase;
class ScriptState;
class USBDevice;
class USBDeviceRequestOptions;

class USB final : public EventTarget,
                  public Supplement<NavigatorBase>,
                  public ExecutionContextLifecycleObserver,
                  public device::mojom::blink::UsbDeviceManagerClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // Getter for navigator.usb
  static USB* usb(NavigatorBase&);

  explicit USB(NavigatorBase&);
  ~USB() override;

  // USB.idl
  ScriptPromise<IDLSequence<USBDevice>> getDevices(ScriptState*,
                                                   ExceptionState&);
  ScriptPromise<USBDevice> requestDevice(ScriptState*,
                                         const USBDeviceRequestOptions*,
                                         ExceptionState&);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connect, kConnect)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect, kDisconnect)

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // ExecutionContextLifecycleObserver overrides.
  void ContextDestroyed() override;

  // Gets or creates a USBDevice in the cache for the given `world`.
  // Used primarily during event dispatch (connect/disconnect) to ensure
  // the device is isolated to the target world, even when there is no
  // current V8 context.
  USBDevice* GetOrCreateDevice(DOMWrapperWorld& world,
                               device::mojom::blink::UsbDeviceInfoPtr);

  // Gets or creates a USBDevice in the cache for the world associated
  // with the given `ScriptState`.
  // Used when resolving promises (e.g. getDevices, requestDevice) where
  // a ScriptState is available.
  USBDevice* GetOrCreateDevice(ScriptState*,
                               device::mojom::blink::UsbDeviceInfoPtr);
  // Legacy fallback: gets or creates a device using the shared, non-isolated
  // cache (used when WebUSBWorldIsolatedCache is disabled).
  USBDevice* GetOrCreateDevice(device::mojom::blink::UsbDeviceInfoPtr);

  mojom::blink::WebUsbService* GetWebUsbService() const {
    return service_.get();
  }

  void ForgetDevice(const String& device_guid,
                    mojom::blink::WebUsbService::ForgetDeviceCallback callback);

  void OnGetDevices(ScriptPromiseResolver<IDLSequence<USBDevice>>*,
                    Vector<device::mojom::blink::UsbDeviceInfoPtr>);
  void OnGetPermission(ScriptPromiseResolver<USBDevice>*,
                       device::mojom::blink::UsbDeviceInfoPtr);

  // DeviceManagerClient implementation.
  void OnDeviceAdded(device::mojom::blink::UsbDeviceInfoPtr) override;
  void OnDeviceRemoved(device::mojom::blink::UsbDeviceInfoPtr) override;

  void OnServiceConnectionError();

  void Trace(Visitor*) const override;

 protected:
  // EventTarget protected overrides.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

 private:
  // Helper to execute `action` for each relevant DOMWrapperWorld.
  // - If `kWebUSBWorldIsolatedCache` is disabled, `action` is invoked once with
  //   `nullptr`.
  // - If enabled, it iterates over all initialized worlds (for Window) or the
  //   single worker world (for Workers).
  // `action` might not be invoked at all if the ExecutionContext is missing,
  // the frame is detached, or (for Window) if no worlds have initialized V8
  // contexts yet.
  void ForEachWorld(base::FunctionRef<void(DOMWrapperWorld*)> action);

  void EnsureServiceConnection();

  bool IsFeatureEnabled(ReportOptions) const;

  HeapMojoRemote<mojom::blink::WebUsbService> service_;
  HeapHashSet<Member<ScriptPromiseResolver<IDLSequence<USBDevice>>>>
      get_devices_requests_;
  HeapHashSet<Member<ScriptPromiseResolverBase>> get_permission_requests_;
  HeapMojoAssociatedReceiver<device::mojom::blink::UsbDeviceManagerClient, USB>
      client_receiver_;

  // USBDeviceCache wraps a map of GUIDs to WeakMembers of USBDevices.
  // It is used to keep track of USBDevice instances that have been created
  // for a specific V8 execution world, preventing cross-world leaks.
  class USBDeviceCache final : public GarbageCollected<USBDeviceCache> {
   public:
    void Trace(Visitor* visitor) const;
    HeapHashMap<String, WeakMember<USBDevice>>& DeviceCache() {
      return device_cache_;
    }

   private:
    HeapHashMap<String, WeakMember<USBDevice>> device_cache_;
  };

  HeapHashMap<String, WeakMember<USBDevice>>& GetOrCreateWorldDeviceCache(
      DOMWrapperWorld& world);

  // Used when `WebUSBWorldIsolatedCache` is enabled. Maps each DOMWrapperWorld
  // to its own USBDeviceCache, preventing cross-world leaks.
  HeapHashMap<WeakMember<DOMWrapperWorld>, Member<USBDeviceCache>>
      device_caches_;

  // Legacy fallback cache: used when `WebUSBWorldIsolatedCache` is disabled.
  // Device instances are shared across all worlds, which may lead to leaks.
  HeapHashMap<String, WeakMember<USBDevice>> device_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBUSB_USB_H_
