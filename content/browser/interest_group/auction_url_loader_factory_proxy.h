// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_URL_LOADER_FACTORY_PROXY_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_URL_LOADER_FACTORY_PROXY_H_

#include <stdint.h>

#include "base/functional/callback_forward.h"
#include "base/strings/string_piece_forward.h"
#include "content/browser/interest_group/subresource_url_authorizations.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/isolation_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class NetworkAnonymizationKey;
}

namespace content {

// Proxy URLLoaderFactoryFactory, to limit the requests that an auction worklet
// can make.
class CONTENT_EXPORT AuctionURLLoaderFactoryProxy
    : public network::mojom::URLLoaderFactory {
 public:
  using GetUrlLoaderFactoryCallback =
      base::RepeatingCallback<network::mojom::URLLoaderFactory*()>;

  // Callback to preconnect a single socket with the given info.
  using PreconnectSocketCallback = base::OnceCallback<void(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key)>;

  // Passed in callbacks must be safe to call at any time during the lifetime of
  // the AuctionURLLoaderFactoryProxy.
  //
  // `get_frame_url_loader_factory` returns the URLLoaderFactory for the
  // associated RenderFrameHost. Only used for seller worklet scripts. Must be
  // safe to call at any point until `this` has been destroyed.
  //
  // `get_trusted_url_loader_factory` returns a trusted URLLoaderFactory. Used
  // for bidder worklet script and trusted selling signals fetches.
  //
  // `preconnect_socket_callback` is used to issue a preconnect if there's a
  // non-empty trusted signals URL. No preconnect is made for the JS and web
  // assembly, since they should be cacheable, and currently erring on the side
  // of not making unnecessary connections. Invoked immediately, if it's going
  // to be invoked at all.
  //
  // `frame_origin` is the origin of the frame running the auction. Used as the
  // initiator.
  //
  // `is_for_seller` indicates if this is for a seller or bidder workler.
  // Requests are configured differently. Seller requests use CORS and the
  // URLLoader from the renderer, while bidder requests use trusted browser
  // URLLoaderFactory, and don't use CORS, since they're same-site relative to
  // the page they were learned on.
  //
  // `client_security_state` is the ClientSecurityState to use for fetches for
  // bidder worklets. Ignored for seller worklets, since they use a
  // URLLoaderFactory with that information already attached.
  //
  // `script_url` is the Javascript URL for the worklet, `wasm_url` is a URL
  // for an optional WASM helper for the worklet, and `trusted_signals_url` is
  // the optional JSON url for additional input to the script. No other URLs may
  // be requested.
  AuctionURLLoaderFactoryProxy(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
      GetUrlLoaderFactoryCallback get_frame_url_loader_factory,
      GetUrlLoaderFactoryCallback get_trusted_url_loader_factory,
      PreconnectSocketCallback preconnect_socket_callback,
      const url::Origin& top_frame_origin,
      const url::Origin& frame_origin,
      absl::optional<int> renderer_process_id,
      bool is_for_seller,
      network::mojom::ClientSecurityStatePtr client_security_state,
      const GURL& script_url,
      const absl::optional<GURL>& wasm_url,
      const absl::optional<GURL>& trusted_signals_base_url);
  AuctionURLLoaderFactoryProxy(const AuctionURLLoaderFactoryProxy&) = delete;
  AuctionURLLoaderFactoryProxy& operator=(const AuctionURLLoaderFactoryProxy&) =
      delete;
  ~AuctionURLLoaderFactoryProxy() override;

  SubresourceUrlAuthorizations& subresource_url_authorizations() {
    return subresource_url_authorizations_;
  }

  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

 private:
  // Returns `url` could be a valid trusted signals URL. In particular,
  // 1) It needs to start with
  //     `<trusted_signals_base_url_>?hostname=<top_frame_origin>&keys=`.
  // 2) The rest of the URL has none of the following characters, in unescaped
  //     form: &, #, =.
  bool CouldBeTrustedSignalsUrl(const GURL& url) const;

  mojo::Receiver<network::mojom::URLLoaderFactory> receiver_;

  const GetUrlLoaderFactoryCallback get_frame_url_loader_factory_;
  const GetUrlLoaderFactoryCallback get_trusted_url_loader_factory_;

  // Manages the bundle subresource URLs that may be accessed by the worklet.
  SubresourceUrlAuthorizations subresource_url_authorizations_;

  const url::Origin top_frame_origin_;
  const url::Origin frame_origin_;
  const absl::optional<int> renderer_process_id_;
  const bool is_for_seller_;
  const network::mojom::ClientSecurityStatePtr client_security_state_;

  // IsolationInfo used for requests using the trusted URLLoaderFactory. A
  // Transient IsolationInfo for sellers, the bidder's IsolationInfo for
  // bidders.
  const net::IsolationInfo isolation_info_;

  const GURL script_url_;
  const absl::optional<GURL> wasm_url_;
  const absl::optional<GURL> trusted_signals_base_url_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_URL_LOADER_FACTORY_PROXY_H_
