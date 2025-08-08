// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_PUBLIC_NETWORK_CONTEXT_GETTER_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_PUBLIC_NETWORK_CONTEXT_GETTER_H_

#include "base/functional/callback.h"

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace cast_streaming {

using NetworkContextGetter =
    base::RepeatingCallback<network::mojom::NetworkContext*()>;

// Sets the NetworkContextGetter for embedders that use the Network Service.
// This must be called before any call to
// ReceiverSession::SetCastStreamingReceiver() and must only be called once.
// If the NetworkContext crashes, any existing Cast Streaming Session will
// eventually terminate.
void SetNetworkContextGetter(NetworkContextGetter getter);

// Clears the NetworkContextGetter set above, if it has been set.
void ClearNetworkContextGetter();

// Checks whether the above context has been set.
bool HasNetworkContextGetter();

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_PUBLIC_NETWORK_CONTEXT_GETTER_H_
