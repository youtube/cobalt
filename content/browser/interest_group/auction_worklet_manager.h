// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_AUCTION_WORKLET_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_AUCTION_WORKLET_MANAGER_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_types.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/subresource_url_builder.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/auction_shared_storage_host.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class NetworkAnonymizationKey;
}

namespace content {

class AuctionSharedStorageHost;
class RenderFrameHostImpl;
class SiteInstance;
class SubresourceUrlAuthorizations;
class SubresourceUrlBuilder;

// Per-frame manager of auction worklets. Manages creation and sharing of
// worklets. Worklets may be reused if they share URLs for scripts and trusted
// signals (and thus share an owner), share top frame origins, and are in the
// same frame. The frame requirement is needed by DevTools and URLLoaderFactory
// hooks, the others by the current worklet implementations.
//
// If a worklet fails to load or crashes, information about the error is
// broadcast to all consumers of the worklet. After a load failure or crash, the
// worklet will not be able to invoke any pending callbacks passed over the Mojo
// interface.
//
// AuctionWorkletManager does not implement any prioritization, apart from
// invoking callbacks that are sharing a worklet in FIFO order. The
// AuctionProcessManager handles prioritization for process creation. Once a
// process is created for a worklet, the worklet is created immediately.
class CONTENT_EXPORT AuctionWorkletManager {
 public:
  using WorkletType = AuctionProcessManager::WorkletType;

  // How many callbacks are dispatched at once, if splitting up notifications is
  // on.
  static const size_t kBatchSize = 10;

  // Types of fatal error that can prevent a worklet from all further execution.
  enum class FatalErrorType {
    kScriptLoadFailed,
    kWorkletCrash,
  };

  using FatalErrorCallback =
      base::OnceCallback<void(FatalErrorType fatal_error_type,
                              const std::vector<std::string>& errors)>;

  int GetFrameTreeNodeID();

  // Delegate class to allow dependency injection in tests. Note that passed in
  // URLLoaderFactories can crash and be restarted, so passing in raw pointers
  // would be problematic.
  class Delegate {
   public:
    // Returns the URLLoaderFactory of the associated frame. Used to load the
    // seller worklet scripts in the context of the parent frame, since unlike
    // other worklet types, it has no first party opt-in, and it's not a
    // cross-origin leak if the parent from knows its URL, since the parent
    // frame provided the URL in the first place.
    virtual network::mojom::URLLoaderFactory* GetFrameURLLoaderFactory() = 0;

    // Trusted URLLoaderFactory used to load bidder worklets, and seller scoring
    // signals.
    virtual network::mojom::URLLoaderFactory* GetTrustedURLLoaderFactory() = 0;

    // Preconnects a single uncredentialed socket with the provided parameters.
    virtual void PreconnectSocket(
        const GURL& url,
        const net::NetworkAnonymizationKey& network_anonymization_key) = 0;

    // Get containing frame. (Passed to debugging hooks, and also used to get
    // the renderer process ID for subresource loading).
    virtual RenderFrameHostImpl* GetFrame() = 0;

    // Returns the SiteInstance representing the frame running the auction.
    virtual scoped_refptr<SiteInstance> GetFrameSiteInstance() = 0;

    // Returns the ClientSecurityState associated with the frame, for use in
    // bidder worklet and signals fetches.
    virtual network::mojom::ClientSecurityStatePtr GetClientSecurityState() = 0;
  };

  // Internal class that owns and creates worklets. It also tracks pending
  // requests that the worklet will be provided upon creation. Refcounted and
  // owned by one or more worklet handles, rather than the
  // AuctionWorkletManager.
  class WorkletOwner;

  // Enough information to uniquely ID a worklet. If these fields match for two
  // worklets (and they're loaded in the same frame, as this class is
  // frame-scoped), the worklets can use the same Mojo Worklet object.
  struct CONTENT_EXPORT WorkletKey {
    WorkletKey(WorkletType type,
               const GURL& script_url,
               const absl::optional<GURL>& wasm_url,
               const absl::optional<GURL>& signals_url,
               bool needs_cors_for_additional_bid,
               absl::optional<uint16_t> experiment_group_id);
    WorkletKey(const WorkletKey&);
    WorkletKey(WorkletKey&&);
    ~WorkletKey();

    WorkletType type;
    GURL script_url;
    absl::optional<GURL> wasm_url;
    absl::optional<GURL> signals_url;

    // `needs_cors_for_additional_bid` is set for buyer reporting for additional
    // bids; those need to perform a CORS check others don't.
    bool needs_cors_for_additional_bid;
    absl::optional<uint16_t> experiment_group_id;

    // Fast, non-cryptographic hash to count unique worklets for UKM.
    size_t GetHash() const;

    bool operator<(const WorkletKey& other) const;
  };

  // Class that tracks a request for a Worklet, and helps manage the lifetime of
  // the returned Worklet once the request receives one. Destroying the handle
  // will abort a pending request and potentially release any worklets or
  // processes it is keeping alive, so consumers should destroy these as soon as
  // they are no longer needed. Handles should not outlive the
  // AuctionWorkletManager.
  class CONTENT_EXPORT WorkletHandle : public base::CheckedObserver {
   public:
    WorkletHandle(const WorkletHandle&) = delete;
    WorkletHandle& operator=(const WorkletHandle&) = delete;
    ~WorkletHandle() override;

    // Authorizes subresource bundle subresource URLs that the worklet may
    // request as long as this WorkletHandle instance is live (refcounting
    // allows multiple WorkletHandle instances to authorize the same URLs).
    //
    // This must be called manually before the worklet is asked to do anything
    // involving fetching those subresources, but after the worklet is
    // available. Calls after the first one will be ignored.
    void AuthorizeSubresourceUrls(
        const SubresourceUrlBuilder& subresource_url_builder);

    // Retrieves the corresponding Worklet Mojo interface for the requested
    // worklet. Only the method corresponding to the worklet type `this` was
    // created with my be invoked. Once the worklet is created, will never
    // return null. Even in the case of process crash or load error, it will
    // return an interface with a broken pipe.
    //
    // Note that calls in the returned Mojo worklet cannot currently be
    // cancelled, so calls should always use weak pointers. Since there's no way
    // for pages to cancel auctions, anyways, and these are currently scoped
    // per-frame, supporting cancellation seems not useful (And the weak
    // pointers are probably not strictly necessary, since everything is torn
    // down before Mojo has a chance to invoke callbacks, but using weak
    // pointers still seems the safest thing to do).
    auction_worklet::mojom::BidderWorklet* GetBidderWorklet();
    auction_worklet::mojom::SellerWorklet* GetSellerWorklet();

    const SubresourceUrlAuthorizations&
    GetSubresourceUrlAuthorizationsForTesting();

   private:
    friend class AuctionWorkletManager;
    friend class WorkletOwner;

    // These are only created by AuctionWorkletManager.
    explicit WorkletHandle(scoped_refptr<WorkletOwner> worklet_owner,
                           base::OnceClosure worklet_available_callback,
                           FatalErrorCallback fatal_error_callback);

    // Both these methods are invoked by WorkletOwner, and call the
    // corresponding callback.
    void OnWorkletAvailable();
    void OnFatalError(FatalErrorType type,
                      const std::vector<std::string>& errors);

    // Returns true if `worklet_owner_` has created a worklet yet.
    bool worklet_created() const;

    scoped_refptr<WorkletOwner> worklet_owner_;

    base::OnceClosure worklet_available_callback_;
    FatalErrorCallback fatal_error_callback_;

    uint64_t seq_num_;
    bool authorized_subresources_ = false;
  };

  // `delegate` and `auction_process_manager` must outlive the created
  // AuctionWorkletManager.
  AuctionWorkletManager(AuctionProcessManager* auction_process_manager,
                        url::Origin top_window_origin,
                        url::Origin frame_origin,
                        Delegate* delegate);
  AuctionWorkletManager(const AuctionWorkletManager&) = delete;
  AuctionWorkletManager& operator=(const AuctionWorkletManager&) = delete;
  ~AuctionWorkletManager();

  // Computes the key for bidder worklet with given params.
  // RequestBidderWorklet(...) is RequestWorkletByKey(BidderWorkletKey(...))
  static WorkletKey BidderWorkletKey(
      const GURL& bidding_logic_url,
      const absl::optional<GURL>& wasm_url,
      const absl::optional<GURL>& trusted_bidding_signals_url,
      bool needs_cors_for_additional_bid,
      absl::optional<uint16_t> experiment_group_id);

  // Requests a worklet with the specified properties. The top frame origin and
  // debugging information are obtained from the Delegate's RenderFrameHost.
  //
  // The AuctionWorkletManager will handle requesting a process, hooking up
  // DevTools, and merging requests with the same parameters so they can share a
  // single worklet.
  //
  // Will invoke `worklet_available_callback` when the service pointer can
  // be retrieved from `handle`. Multiple instances of the callback can be
  // invoked synchronously right after each other, but none will be invoked from
  // within the Request... call itself. `worklet_available_callback` invocations
  // that refer to the same underlying worklet will be in the same order as the
  // Request... call, but those that don't share worklets are unordered with
  // respect to each other.
  //
  // `fatal_error_callback` is invoked in the case of a fatal error. It may be
  // invoked both if the worklet creation outright failed, or after a successful
  // creation that invoked `worklet_available_callback_`, so long as the
  // WorkletHandle lives. It is called to indicate the worklet failed to
  // load or crashed.  Callbacks from multiple calls may be invoked right after
  // another without returning to the event loop (but not within the Request...
  // call itself); but unlike for success callback there are no ordering
  // guarantees about ordering of failures corresponding to different Request...
  // calls.
  //
  // The callbacks should not delete the AuctionWorkletManager itself, but are
  // free to release any WorkletHandle they wish.
  void RequestBidderWorklet(
      const GURL& bidding_logic_url,
      const absl::optional<GURL>& wasm_url,
      const absl::optional<GURL>& trusted_bidding_signals_url,
      bool needs_cors_for_additional_bid,
      absl::optional<uint16_t> experiment_group_id,
      base::OnceClosure worklet_available_callback,
      FatalErrorCallback fatal_error_callback,
      std::unique_ptr<WorkletHandle>& out_worklet_handle);
  void RequestSellerWorklet(
      const GURL& decision_logic_url,
      const absl::optional<GURL>& trusted_scoring_signals_url,
      absl::optional<uint16_t> experiment_group_id,
      base::OnceClosure worklet_available_callback,
      FatalErrorCallback fatal_error_callback,
      std::unique_ptr<WorkletHandle>& out_worklet_handle);
  void RequestWorkletByKey(WorkletKey worklet_info,
                           base::OnceClosure worklet_available_callback,
                           FatalErrorCallback fatal_error_callback,
                           std::unique_ptr<WorkletHandle>& out_worklet_handle);

 private:
  void OnWorkletNoLongerUsable(WorkletOwner* worklet);

  mojo::PendingRemote<auction_worklet::mojom::AuctionSharedStorageHost>
  MaybeBindAuctionSharedStorageHost(RenderFrameHostImpl* auction_runner_rfh,
                                    const url::Origin& worklet_origin);

  // Accessors used by inner classes. Not strictly needed, but makes it clear
  // which fields they can access.
  AuctionProcessManager* auction_process_manager() {
    return auction_process_manager_;
  }
  const url::Origin& top_window_origin() const { return top_window_origin_; }
  const url::Origin& frame_origin() const { return frame_origin_; }
  Delegate* delegate() { return delegate_; }

  const raw_ptr<AuctionProcessManager> auction_process_manager_;
  const url::Origin top_window_origin_;
  const url::Origin frame_origin_;
  raw_ptr<Delegate> const delegate_;

  std::unique_ptr<AuctionSharedStorageHost> auction_shared_storage_host_;

  std::map<WorkletKey, WorkletOwner*> worklets_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_AUCTION_WORKLET_MANAGER_H_
