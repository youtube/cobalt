// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of SafeBrowsing for WebSockets. This code runs inside the
// render process, calling the interface defined in safe_browsing.mojom to
// communicate with the SafeBrowsing service.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_WEBSOCKET_SB_HANDSHAKE_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_WEBSOCKET_SB_HANDSHAKE_THROTTLE_H_

#include <memory>

#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/common/safe_browsing_url_checker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "url/gurl.h"

namespace safe_browsing {

class WebSocketSBHandshakeThrottle : public blink::WebSocketHandshakeThrottle,
                                     public mojom::UrlCheckNotifier {
 public:
  WebSocketSBHandshakeThrottle(mojom::SafeBrowsing* safe_browsing,
                               int render_frame_id);

  WebSocketSBHandshakeThrottle(const WebSocketSBHandshakeThrottle&) = delete;
  WebSocketSBHandshakeThrottle& operator=(const WebSocketSBHandshakeThrottle&) =
      delete;

  ~WebSocketSBHandshakeThrottle() override;

  void ThrottleHandshake(const blink::WebURL& url,
                         blink::WebSocketHandshakeThrottle::OnCompletion
                             completion_callback) override;

 private:
  enum class State {
    kInitial,
    kStarted,
    kSafe,
    kBlocked,
    kNotSupported,
  };

  // mojom::UrlCheckNotifier implementation.
  void OnCompleteCheck(bool proceed,
                       bool showed_interstitial,
                       bool did_perform_real_time_check,
                       bool did_check_allowlist) override;

  void OnCheckResult(
      mojo::PendingReceiver<mojom::UrlCheckNotifier> slow_check_notifier,
      bool proceed,
      bool showed_interstitial,
      bool did_perform_real_time_check,
      bool did_check_allowlist);
  void OnMojoDisconnect();

  const int render_frame_id_;
  GURL url_;
  blink::WebSocketHandshakeThrottle::OnCompletion completion_callback_;
  mojo::Remote<mojom::SafeBrowsingUrlChecker> url_checker_;
  mojom::SafeBrowsing* safe_browsing_;
  std::unique_ptr<mojo::Receiver<mojom::UrlCheckNotifier>> notifier_receiver_;

  // |state_| is used to validate that events happen in the right order. It
  // isn't used to control the behaviour of the class.
  State state_ = State::kInitial;

  base::WeakPtrFactory<WebSocketSBHandshakeThrottle> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_WEBSOCKET_SB_HANDSHAKE_THROTTLE_H_
