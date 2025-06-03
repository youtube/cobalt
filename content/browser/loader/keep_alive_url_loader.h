// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_KEEP_ALIVE_URL_LOADER_H_
#define CONTENT_BROWSER_LOADER_KEEP_ALIVE_URL_LOADER_H_

#include <stdint.h>
#include <queue>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/types/pass_key.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/mojom/loader/fetch_later.mojom.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace blink {
class URLLoaderThrottle;
}

namespace content {

class BrowserContext;
class KeepAliveURLLoaderService;
class PolicyContainerHost;

// A URLLoader for loading a fetch keepalive request via the browser process,
// including requests generated from the following JS API calls:
//   - fetch(..., {keepalive: true})
//   - navigator.sendBeacon(...)
//   - fetchLater(...)
//
// To load a keepalive request initiated by a renderer, this loader performs the
// following logic:
//
// 1. In ctor, stores request data sent from a renderer.
// 2. In `Start()`, asks the network service to start loading the request, and
//    then runs throttles to perform checks.
// 3. Handles request loading results from the network service, i.e. from the
//    remote of `loader_receiver_` (a mojom::URLLoaderClient):
//    A. If it is `OnReceiveRedirect()`, this loader performs checks and runs
//       throttles, and then asks the network service to proceed with redirects
//       without interacting with renderer. The redirect params are stored for
//       later use.
//    B. If it is `OnReceiveResponse()` or `OnComplete()`, this loader does not
//       process response. Instead, it calls `ForwardURLLoad()` to begin to
//       forward previously saved mojom::URLLoaderClient calls to the renderer,
//       if the renderer is still alive; Otherwise, terminating this loader.
//    C. If a throttle asynchronously asks to cancel the request, similar to B,
//       the previously stored calls will be forwarded to the renderer.
//    D. The renderer's response to `ForwardURLLoad()` may be any of
//       mojom::URLLoader calls, in which they should continue forwarding by
//       calling `ForwardURLLoader()` again.
//
// See the "Longer Redirect Chain" section of the Design Doc for an example
// call sequence diagram.
//
// This class must only be constructed by `KeepAliveURLLoaderService`.
//
// The lifetime of an instance is roughly equal to the lifetime of a keepalive
// request, which may surpass the initiator renderer's lifetime.
//
// Design Doc:
// https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY
class CONTENT_EXPORT KeepAliveURLLoader
    : public network::mojom::URLLoader,
      public network::mojom::URLLoaderClient,
      public blink::mojom::FetchLaterLoader {
 public:
  // A callback type to delete this loader immediately on triggered.
  using OnDeleteCallback = base::OnceCallback<void(void)>;
  // A callback type to return URLLoaderThrottles to be used by this loader.
  using URLLoaderThrottlesGetter = base::RepeatingCallback<
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>>(void)>;

  // Must only be constructed by a `KeepAliveURLLoaderService`.
  //
  // Note that calling ctor does not mean loading the request. `Start()` must
  // also be called subsequently.
  //
  // `resource_request` must be a keepalive request from a renderer.
  // `forwarding_client` should handle request loading results from the network
  // service if it is still connected.
  // `delete_callback` is a callback to delete this object.
  // `policy_container_host` must not be null.
  KeepAliveURLLoader(
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
      scoped_refptr<PolicyContainerHost> policy_container_host,
      BrowserContext* browser_context,
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles,
      base::PassKey<KeepAliveURLLoaderService>);
  ~KeepAliveURLLoader() override;

  // Not copyable.
  KeepAliveURLLoader(const KeepAliveURLLoader&) = delete;
  KeepAliveURLLoader& operator=(const KeepAliveURLLoader&) = delete;

  // Sets the callback to be invoked on errors which require closing the pipe.
  // Running `on_delete_callback` will immediately delete `this`.
  //
  // Not an argument to constructor because the Mojo ReceiverId needs to be
  // bound to the callback, but can only be obtained after creating `this`.
  // Must be called immediately after creating a KeepAliveLoader.
  void set_on_delete_callback(OnDeleteCallback on_delete_callback);

  // Kicks off loading the request, including prepare for requests, and setting
  // up communication with network service.
  // This method must only be called when `IsStarted()` is false.
  void Start();

  // Called when the receiver of URLLoader implemented by this is disconnected.
  void OnURLLoaderDisconnected();

  // Called when the `browser_context_` is shutting down.
  void Shutdown();

  base::WeakPtr<KeepAliveURLLoader> GetWeakPtr();

  // For testing only:
  // TODO(crbug.com/1427366): Figure out alt to not rely on this in test.
  class TestObserver : public base::RefCountedThreadSafe<TestObserver> {
   public:
    virtual void OnReceiveRedirectForwarded(KeepAliveURLLoader* loader) = 0;
    virtual void OnReceiveRedirectProcessed(KeepAliveURLLoader* loader) = 0;
    virtual void OnReceiveResponse(KeepAliveURLLoader* loader) = 0;
    virtual void OnReceiveResponseForwarded(KeepAliveURLLoader* loader) = 0;
    virtual void OnReceiveResponseProcessed(KeepAliveURLLoader* loader) = 0;
    virtual void OnComplete(
        KeepAliveURLLoader* loader,
        const network::URLLoaderCompletionStatus& completion_status) = 0;
    virtual void OnCompleteForwarded(
        KeepAliveURLLoader* loader,
        const network::URLLoaderCompletionStatus& completion_status) = 0;
    virtual void OnCompleteProcessed(
        KeepAliveURLLoader* loader,
        const network::URLLoaderCompletionStatus& completion_status) = 0;
    virtual void PauseReadingBodyFromNetProcessed(
        KeepAliveURLLoader* loader) = 0;
    virtual void ResumeReadingBodyFromNetProcessed(
        KeepAliveURLLoader* loader) = 0;

   protected:
    virtual ~TestObserver() = default;
    friend class base::RefCountedThreadSafe<TestObserver>;
  };
  void SetObserverForTesting(scoped_refptr<TestObserver> observer);

 private:
  // Returns true if request loading has been started, i.e. `Start()` has been
  // called. Otherwise, returns false by default.
  bool IsStarted() const;

  // Receives actions from renderer.
  // `network::mojom::URLLoader` overrides:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // Receives actions from network service.
  // `network::mojom::URLLoaderClient` overrides:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(
      const network::URLLoaderCompletionStatus& completion_status) override;

  // `blink::mojom::FetchLaterLoader` overrides:
  void SendNow() override;
  void Cancel() override;

  // Whether `OnReceiveResponse()` has been called.
  bool HasReceivedResponse() const;

  // Forwards the stored chain of redriects, response, completion status to the
  // renderer that initiates this loader, such that the renderer knows what URL
  // the response come from when parsing the response.
  //
  // This method must be called when `IsRendererConnected()` is true.
  // This method may be called more than one time until it deletes `this`.
  // WARNING: Calling this method may result in the deletion of `this`.
  // See also the "Proposed Call Sequences After Migration" section in
  // https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY/edit?pli=1#heading=h.d006i46pmq9
  void ForwardURLLoad();

  // Tells if `ForwardURLLoad()` has ever been called.
  bool IsForwardURLLoadStarted() const;

  // Tells if this loader is still able to forward actions to the
  // URLLoaderClient in renderer.
  bool IsRendererConnected() const;

  // Tells if this loader is constructed for a FetchLater request.
  bool IsFetchLater() const;

  // Returns net::OK to allow following the redirect. Otherwise, returns
  // corresponding error code.
  net::Error WillFollowRedirect(const net::RedirectInfo& redirect_info) const;

  // Called when `loader_receiver_`, Browser<-Network pipe, is disconnected.
  void OnNetworkConnectionError();

  // Called when `forwarding_client_`, Browser->Renderer pipe, is disconnected.
  void OnForwardingClientDisconnected();

  // Called when `disconnected_loader_timer_` is fired.
  void OnDisconnectedLoaderTimerFired();

  void DeleteSelf();

  // The ID to identify the request being loaded by this loader.
  const int32_t request_id_;

  // A bitfield of the options of the request being loaded.
  // See services/network/public/mojom/url_loader_factory.mojom.
  const uint32_t options_;

  // The request to be loaded by this loader.
  // Set in the constructor and updated when redirected.
  network::ResourceRequest resource_request_;

  // Browser -> Network connection:
  //
  // Connects to the receiver network::URLLoader implemented in the network
  // service that performs actual request loading.
  mojo::Remote<network::mojom::URLLoader> loader_;
  // Browser <- Network connection:
  //
  // Receives the result of the request loaded by `loader_` from the network
  // service.
  mojo::Receiver<network::mojom::URLLoaderClient> loader_receiver_{this};

  // Browser -> Renderer connection:
  //
  // Connects to the receiver URLLoaderClient implemented in the renderer.
  // It is the client that this loader may forward the URLLoader response from
  // the network service, i.e. message received by `loader_receiver_`, to.
  // It may be disconnected if the renderer is dead. In such case, subsequent
  // URLLoader response may be handled in browser.
  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  // Browser <- Renderer connection:
  // Timer used for triggering cleaning up `this` after the receiver is
  // disconnected from the remote of URLLoader in the renderer.
  base::OneShotTimer disconnected_loader_timer_;

  // The NetworkTrafficAnnotationTag for the request being loaded.
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  // A refptr to the URLLoaderFactory implementation that can actually create a
  // URLLoader. An extra refptr is required here to support deferred loading.
  scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory_;

  struct StoredURLLoad;
  // Stores the chain of redriects, response, and completion status, such that
  // they can be forwarded to renderer after handled in browser.
  // See also `ForwardURLLoad()`.
  std::unique_ptr<StoredURLLoad> stored_url_load_;

  // A refptr to keep the `PolicyContainerHost` from the RenderFrameHost that
  // initiates this loader alive until `this` is destroyed.
  // It is never null.
  scoped_refptr<PolicyContainerHost> policy_container_host_;

  // The BrowserContext that initiates this loader.
  // It is ensured to outlive this because it owns KeepAliveURLLoaderService
  // which owns this loader.
  const raw_ptr<BrowserContext> browser_context_;

  // TODO(crbug.com/1356128): Remove custom throttle logic here with blink's.
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles_;

  // Tells if this loader has been started or not.
  bool is_started_ = false;

  // A callback to delete this loader object and clean up resource.
  OnDeleteCallback on_delete_callback_;

  // Records the initial request URL to help veryfing redirect request.
  const GURL initial_url_;
  // Records the latest URL to help veryfing redirect request.
  GURL last_url_;

  class ThrottleEntry;
  class ThrottleDelegate;
  // Maintains a list of `blink::URLLoaderThrottle` created by content and
  // content embedder, which will be prepared to run in case this loader has to
  // handle redirects in-browser.
  std::vector<std::unique_ptr<ThrottleEntry>> throttle_entries_;

  // Counts the total number when this loader is requested by throttle to pause
  // reading body.
  size_t paused_reading_body_from_net_count_ = 0;

  // For testing only:
  // Not owned.
  scoped_refptr<TestObserver> observer_for_testing_ = nullptr;

  // Must be the last field.
  base::WeakPtrFactory<KeepAliveURLLoader> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_KEEP_ALIVE_URL_LOADER_H_
