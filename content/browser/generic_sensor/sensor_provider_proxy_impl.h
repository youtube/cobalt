// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GENERIC_SENSOR_SENSOR_PROVIDER_PROXY_IMPL_H_
#define CONTENT_BROWSER_GENERIC_SENSOR_SENSOR_PROVIDER_PROXY_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace content {

class RenderFrameHost;

// This proxy acts as a gatekeeper to the real sensor provider so that this
// proxy can intercept sensor requests and allow or deny them based on
// the permission statuses retrieved from a permission controller.
class SensorProviderProxyImpl final
    : public device::mojom::SensorProvider,
      public DocumentUserData<SensorProviderProxyImpl> {
 public:
  SensorProviderProxyImpl(const SensorProviderProxyImpl&) = delete;
  SensorProviderProxyImpl& operator=(const SensorProviderProxyImpl&) = delete;

  ~SensorProviderProxyImpl() override;

  void Bind(mojo::PendingReceiver<device::mojom::SensorProvider> receiver);

  // Allows tests to override how this class binds its backing SensorProvider
  // endpoint.
  using SensorProviderBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<device::mojom::SensorProvider>)>;
  static CONTENT_EXPORT void OverrideSensorProviderBinderForTesting(
      SensorProviderBinder binder);

 private:
  explicit SensorProviderProxyImpl(RenderFrameHost* render_frame_host);

  // SensorProvider implementation.
  void GetSensor(device::mojom::SensorType type,
                 GetSensorCallback callback) override;

  bool CheckFeaturePolicies(device::mojom::SensorType type) const;
  void OnPermissionRequestCompleted(device::mojom::SensorType type,
                                    GetSensorCallback callback,
                                    blink::mojom::PermissionStatus);
  void OnConnectionError();

  // Callbacks from |receiver_set_| are passed to |sensor_provider_| and so
  // the ReceiverSet should be destroyed first so that the callbacks are
  // invalidated before being discarded.
  mojo::Remote<device::mojom::SensorProvider> sensor_provider_;
  mojo::ReceiverSet<device::mojom::SensorProvider> receiver_set_;

  base::WeakPtrFactory<SensorProviderProxyImpl> weak_factory_{this};

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_GENERIC_SENSOR_SENSOR_PROVIDER_PROXY_IMPL_H_
