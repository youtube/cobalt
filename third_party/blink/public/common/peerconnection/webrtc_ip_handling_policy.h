// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PEERCONNECTION_WEBRTC_IP_HANDLING_POLICY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PEERCONNECTION_WEBRTC_IP_HANDLING_POLICY_H_

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// This is the default behavior of Chrome. Currently, WebRTC has the right to
// enumerate all interfaces and bind them to discover public interfaces.
BLINK_COMMON_EXPORT extern const char kWebRTCIPHandlingDefault[];

// WebRTC should only use the default route used by http. This also exposes the
// associated default private address. Default route is the route chosen by the
// OS on a multi-homed endpoint.
BLINK_COMMON_EXPORT extern const char
    kWebRTCIPHandlingDefaultPublicAndPrivateInterfaces[];

// WebRTC should only use the default route used by http. This doesn't expose
// any local addresses.
BLINK_COMMON_EXPORT extern const char
    kWebRTCIPHandlingDefaultPublicInterfaceOnly[];

// WebRTC should only use TCP to contact peers or servers unless the proxy
// server supports UDP. This doesn't expose any local addresses either.
BLINK_COMMON_EXPORT extern const char kWebRTCIPHandlingDisableNonProxiedUdp[];

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PEERCONNECTION_WEBRTC_IP_HANDLING_POLICY_H_
