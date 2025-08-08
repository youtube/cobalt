// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/geolocation_context.h"

#include <utility>

#include "base/containers/cxx20_erase_vector.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/geolocation/geolocation_impl.h"
#include "url/origin.h"

namespace device {

GeolocationContext::GeolocationContext() = default;

GeolocationContext::~GeolocationContext() = default;

// static
void GeolocationContext::Create(
    mojo::PendingReceiver<mojom::GeolocationContext> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<GeolocationContext>(),
                              std::move(receiver));
}

void GeolocationContext::BindGeolocation(
    mojo::PendingReceiver<mojom::Geolocation> receiver,
    const GURL& requesting_url) {
  GeolocationImpl* impl =
      new GeolocationImpl(std::move(receiver), requesting_url, this);
  impls_.push_back(base::WrapUnique<GeolocationImpl>(impl));
  if (geoposition_override_) {
    impl->SetOverride(*geoposition_override_);
  } else {
    impl->StartListeningForUpdates();
  }
}

void GeolocationContext::OnPermissionRevoked(const url::Origin& origin) {
  base::EraseIf(impls_, [&origin](const auto& impl) {
    if (!origin.IsSameOriginWith(impl->url())) {
      return false;
    }
    // Invoke the position callback with kPermissionDenied before removing.
    impl->OnPermissionRevoked();
    return true;
  });
}

void GeolocationContext::OnConnectionError(GeolocationImpl* impl) {
  auto it =
      base::ranges::find(impls_, impl, &std::unique_ptr<GeolocationImpl>::get);
  DCHECK(it != impls_.end());
  impls_.erase(it);
}

void GeolocationContext::SetOverride(
    mojom::GeopositionResultPtr geoposition_result) {
  geoposition_override_ = std::move(geoposition_result);
  for (auto& impl : impls_) {
    impl->SetOverride(*geoposition_override_);
  }
}

void GeolocationContext::ClearOverride() {
  geoposition_override_.reset();
  for (auto& impl : impls_) {
    impl->ClearOverride();
  }
}

}  // namespace device
