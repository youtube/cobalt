// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/navigatorcontentutils/navigator_content_utils_client.h"

#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

NavigatorContentUtilsClient::NavigatorContentUtilsClient(LocalFrame* frame)
    : frame_(frame) {}

void NavigatorContentUtilsClient::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
}

void NavigatorContentUtilsClient::RegisterProtocolHandler(const String& scheme,
                                                          const KURL& url) {
  bool user_gesture = LocalFrame::HasTransientUserActivation(frame_);
  frame_->GetLocalFrameHostRemote().RegisterProtocolHandler(scheme, url,
                                                            user_gesture);
}

void NavigatorContentUtilsClient::UnregisterProtocolHandler(
    const String& scheme,
    const KURL& url) {
  bool user_gesture = LocalFrame::HasTransientUserActivation(frame_);
  frame_->GetLocalFrameHostRemote().UnregisterProtocolHandler(scheme, url,
                                                              user_gesture);
}

}  // namespace blink
