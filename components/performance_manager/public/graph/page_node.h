// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PAGE_NODE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PAGE_NODE_H_

#include <ostream>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/function_ref.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/node.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/public/mojom/lifecycle.mojom.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace performance_manager {

class FrameNode;
class PageNodeObserver;

enum class PageType {
  // A browser tab.
  kTab,
  // An extension background page.
  kExtension,
  // Anything else.
  kUnknown,
};

// A PageNode represents the root of a FrameTree, or equivalently a WebContents.
// These may correspond to normal tabs, WebViews, Portals, Chrome Apps or
// Extensions.
class PageNode : public Node {
 public:
  using FrameNodeVisitor = base::FunctionRef<bool(const FrameNode*)>;
  using LifecycleState = mojom::LifecycleState;
  using Observer = PageNodeObserver;
  class ObserverDefaultImpl;

  // Reasons for which a frame can become the embedder of a page.
  enum class EmbeddingType {
    // Returned if this node doesn't have an embedder.
    kInvalid,
    // This page is a guest view. This can be many things (<webview>, <appview>,
    // etc) but is backed by the same inner/outer WebContents mechanism.
    kGuestView,
    // This page is a portal.
    kPortal,
  };

  // Returns a string for a PageNode::EmbeddingType enumeration.
  static const char* ToString(PageNode::EmbeddingType embedding_type);

  // Loading state of a page.
  enum class LoadingState {
    // No top-level document has started loading yet.
    kLoadingNotStarted,
    // A different top-level document is loading. The load started less than 5
    // seconds ago or the initial response was received.
    kLoading,
    // A different top-level document is loading. The load started more than 5
    // seconds ago and no response was received yet. Note: The state will
    // transition back to |kLoading| if a response is received.
    kLoadingTimedOut,
    // A different top-level document finished loading, but the page did not
    // reach CPU and network quiescence since then. Note: A page is considered
    // to have reached CPU and network quiescence after 1 minute, even if the
    // CPU and network are still busy - see page_load_tracker_decorator.h.
    kLoadedBusy,
    // The page reached CPU and network quiescence after loading the current
    // top-level document, or the load failed.
    kLoadedIdle,
  };

  // Returns a string for an enumeration value.
  static const char* ToString(PageType type);
  static const char* ToString(PageNode::LoadingState loading_state);

  // State of a page. Pages can be born in "kActive" or "kPrerendering" state.
  enum class PageState {
    // The page is a normal page, that is actively running. Can transition from
    // here to kBackForwardCache.
    kActive,

    // The page is a prerender page. It may do some initial loading but will
    // never fully run unless it is activated. Can transition from here to
    // kActive, or be destroyed.
    kPrerendering,

    // The page is in the back-forward cache. The page will be frozen during its
    // entire stay in the cache. Can transition from here to kActive or be
    // destroyed.
    kBackForwardCache,
  };

  static const char* ToString(PageNode::PageState page_state);

  PageNode();

  PageNode(const PageNode&) = delete;
  PageNode& operator=(const PageNode&) = delete;

  ~PageNode() override;

  // Returns the unique ID of the browser context that this page belongs to.
  virtual const std::string& GetBrowserContextID() const = 0;

  // Returns the opener frame node, if there is one. This may change over the
  // lifetime of this page. See "OnOpenerFrameNodeChanged".
  virtual const FrameNode* GetOpenerFrameNode() const = 0;

  // Returns the embedder frame node, if there is one. This may change over the
  // lifetime of this page. See "OnEmbedderFrameNodeChanged".
  virtual const FrameNode* GetEmbedderFrameNode() const = 0;

  // Returns the type of relationship this node has with its embedder, if it has
  // an embedder.
  virtual EmbeddingType GetEmbeddingType() const = 0;

  // Returns the type of the page.
  virtual PageType GetType() const = 0;

  // Returns true if this page is currently visible, false otherwise.
  // See PageNodeObserver::OnIsVisibleChanged.
  virtual bool IsVisible() const = 0;

  // Returns the time since the last visibility change. It is always well
  // defined as the visibility property is set at node creation.
  virtual base::TimeDelta GetTimeSinceLastVisibilityChange() const = 0;

  // Returns true if this page is currently audible, false otherwise.
  // See PageNodeObserver::OnIsAudibleChanged.
  virtual bool IsAudible() const = 0;

  // Returns the page's loading state.
  virtual LoadingState GetLoadingState() const = 0;

  // Returns the UKM source ID associated with the URL of the main frame of
  // this page.
  // See PageNodeObserver::OnUkmSourceIdChanged.
  virtual ukm::SourceId GetUkmSourceID() const = 0;

  // Returns the lifecycle state of this page. This is aggregated from the
  // lifecycle state of each frame in the frame tree. See
  // PageNodeObserver::OnPageLifecycleStateChanged.
  virtual LifecycleState GetLifecycleState() const = 0;

  // Returns true if at least one of the frame in this page is currently
  // holding a WebLock.
  virtual bool IsHoldingWebLock() const = 0;

  // Returns true if at least one of the frame in this page is currently
  // holding an IndexedDB lock.
  virtual bool IsHoldingIndexedDBLock() const = 0;

  // Returns the navigation ID associated with the last committed navigation
  // event for the main frame of this page.
  // See PageNodeObserver::OnMainFrameNavigationCommitted.
  virtual int64_t GetNavigationID() const = 0;

  // Returns the MIME type of the contents associated with the last committed
  // navigation event for the main frame of this page.
  virtual const std::string& GetContentsMimeType() const = 0;

  // Returns "zero" if no navigation has happened, otherwise returns the time
  // since the last navigation commit.
  virtual base::TimeDelta GetTimeSinceLastNavigation() const = 0;

  // Returns the current main frame node (if there is one), otherwise returns
  // any of the potentially multiple main frames that currently exist. If there
  // are no main frames at the moment, returns nullptr.
  virtual const FrameNode* GetMainFrameNode() const = 0;

  // Visits the main frame nodes associated with this page. The iteration is
  // halted if the visitor returns false. Returns true if every call to the
  // visitor returned true, false otherwise.
  virtual bool VisitMainFrameNodes(const FrameNodeVisitor& visitor) const = 0;

  // Returns all of the main frame nodes, both current and otherwise. If there
  // are no main frames at the moment, returns the empty set. Note that this
  // incurs a full container copy of all main frame nodes. Please use
  // VisitMainFrameNodes when that makes sense.
  virtual const base::flat_set<const FrameNode*> GetMainFrameNodes() const = 0;

  // Returns the URL the main frame last committed a navigation to, or the
  // initial URL of the page before navigation. The latter case is distinguished
  // by a zero navigation ID.
  // See PageNodeObserver::OnMainFrameNavigationCommitted.
  virtual const GURL& GetMainFrameUrl() const = 0;

  // Indicates if at least one of the frames in the page has received some form
  // interactions.
  virtual bool HadFormInteraction() const = 0;

  // Indicates if at least one of the frames in the page has received
  // user-initiated edits. This is a superset of `HadFormInteraction()` that
  // also includes changes to `contenteditable` elements.
  virtual bool HadUserEdits() const = 0;

  // Returns the web contents associated with this page node. It is valid to
  // call this function on any thread but the weak pointer must only be
  // dereferenced on the UI thread.
  virtual const WebContentsProxy& GetContentsProxy() const = 0;

  // Indicates if there's a freezing vote for this page node. This has 3
  // possible values:
  //   - absl::nullopt: There's no active freezing vote for this page.
  //   - freezing::FreezingVoteValue::kCanFreeze: There's one or more positive
  //     freezing vote for this page and no negative vote.
  //   - freezing::FreezingVoteValue::kCannotFreeze: There's at least one
  //     negative freezing vote for this page.
  virtual const absl::optional<freezing::FreezingVote>& GetFreezingVote()
      const = 0;

  // Returns the current page state. See "PageNodeObserver::OnPageStateChanged".
  virtual PageState GetPageState() const = 0;

  virtual uint64_t EstimateResidentSetSize() const = 0;

  virtual uint64_t EstimatePrivateFootprintSize() const = 0;
};

// Pure virtual observer interface. Derive from this if you want to be forced to
// implement the entire interface.
class PageNodeObserver {
 public:
  using PageState = PageNode::PageState;
  using EmbeddingType = PageNode::EmbeddingType;

  PageNodeObserver();

  PageNodeObserver(const PageNodeObserver&) = delete;
  PageNodeObserver& operator=(const PageNodeObserver&) = delete;

  virtual ~PageNodeObserver();

  // Node lifetime notifications.

  // Called when a |page_node| is added to the graph. Observers must not make
  // any property changes or cause re-entrant notifications during the scope of
  // this call. Instead, make property changes via a separate posted task.
  virtual void OnPageNodeAdded(const PageNode* page_node) = 0;

  // Called before a |page_node| is removed from the graph. Observers must not
  // make any property changes or cause re-entrant notifications during the
  // scope of this call.
  virtual void OnBeforePageNodeRemoved(const PageNode* page_node) = 0;

  // Notifications of property changes.

  // Invoked when this page has been assigned an opener, had the opener
  // change, or had the opener removed. This happens when a page is opened
  // via window.open, or when that relationship is subsequently severed or
  // reparented.
  virtual void OnOpenerFrameNodeChanged(const PageNode* page_node,
                                        const FrameNode* previous_opener) = 0;

  // Invoked when this page has been assigned an embedder, had the embedder
  // change, or had the embedder removed. This can happen if a page is opened
  // via webviews, guestviews, portals, etc, or when that relationship is
  // subsequently severed or reparented.
  virtual void OnEmbedderFrameNodeChanged(
      const PageNode* page_node,
      const FrameNode* previous_embedder,
      EmbeddingType previous_embedder_type) = 0;

  // Invoked when the GetType property changes.
  virtual void OnTypeChanged(const PageNode* page_node,
                             PageType previous_type) = 0;

  // Invoked when the IsVisible property changes.
  virtual void OnIsVisibleChanged(const PageNode* page_node) = 0;

  // Invoked when the IsAudible property changes.
  virtual void OnIsAudibleChanged(const PageNode* page_node) = 0;

  // Invoked when the GetLoadingState property changes.
  virtual void OnLoadingStateChanged(const PageNode* page_node,
                                     PageNode::LoadingState previous_state) = 0;

  // Invoked when the UkmSourceId property changes.
  virtual void OnUkmSourceIdChanged(const PageNode* page_node) = 0;

  // Invoked when the PageLifecycleState property changes.
  virtual void OnPageLifecycleStateChanged(const PageNode* page_node) = 0;

  // Invoked when the IsHoldingWebLock property changes.
  virtual void OnPageIsHoldingWebLockChanged(const PageNode* page_node) = 0;

  // Invoked when the IsHoldingIndexedDBLock property changes.
  virtual void OnPageIsHoldingIndexedDBLockChanged(
      const PageNode* page_node) = 0;

  // Invoked when the MainFrameUrl property changes.
  virtual void OnMainFrameUrlChanged(const PageNode* page_node) = 0;

  // This is fired when a non-same document navigation commits in the main
  // frame. It indicates that the the |NavigationId| property and possibly the
  // |MainFrameUrl| properties have changed.
  virtual void OnMainFrameDocumentChanged(const PageNode* page_node) = 0;

  // Invoked when the HadFormInteraction property changes.
  virtual void OnHadFormInteractionChanged(const PageNode* page_node) = 0;

  // Invoked when the HadUserEdits property changes.
  virtual void OnHadUserEditsChanged(const PageNode* page_node) = 0;

  // Invoked when the page state changes. See `PageState` for the valid
  // transitions.
  virtual void OnPageStateChanged(const PageNode* page_node,
                                  PageState old_state) = 0;

  // Events with no property changes.

  // Fired when the tab title associated with a page changes. This property is
  // not directly reflected on the node.
  virtual void OnTitleUpdated(const PageNode* page_node) = 0;

  // Fired when the favicon associated with a page is updated. This property is
  // not directly reflected on the node.
  virtual void OnFaviconUpdated(const PageNode* page_node) = 0;

  // Fired after `new_page_node` is created but before `page_node` is deleted
  // from being discarded. See the equivalent function on `WebContentsObserver`
  // for more detail.
  virtual void OnAboutToBeDiscarded(const PageNode* page_node,
                                    const PageNode* new_page_node) = 0;

  // Called every time the aggregated freezing vote changes or gets invalidated.
  virtual void OnFreezingVoteChanged(
      const PageNode* page_node,
      absl::optional<freezing::FreezingVote> previous_vote) = 0;
};

// Default implementation of observer that provides dummy versions of each
// function. Derive from this if you only need to implement a few of the
// functions.
class PageNode::ObserverDefaultImpl : public PageNodeObserver {
 public:
  ObserverDefaultImpl();

  ObserverDefaultImpl(const ObserverDefaultImpl&) = delete;
  ObserverDefaultImpl& operator=(const ObserverDefaultImpl&) = delete;

  ~ObserverDefaultImpl() override;

  // PageNodeObserver implementation:
  void OnPageNodeAdded(const PageNode* page_node) override {}
  void OnBeforePageNodeRemoved(const PageNode* page_node) override {}
  void OnPageStateChanged(const PageNode* page_node,
                          PageState old_state) override {}
  void OnOpenerFrameNodeChanged(const PageNode* page_node,
                                const FrameNode* previous_opener) override {}
  void OnEmbedderFrameNodeChanged(
      const PageNode* page_node,
      const FrameNode* previous_embedder,
      EmbeddingType previous_embedding_type) override {}
  void OnTypeChanged(const PageNode* page_node,
                     PageType previous_type) override {}
  void OnIsVisibleChanged(const PageNode* page_node) override {}
  void OnIsAudibleChanged(const PageNode* page_node) override {}
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override {}
  void OnUkmSourceIdChanged(const PageNode* page_node) override {}
  void OnPageLifecycleStateChanged(const PageNode* page_node) override {}
  void OnPageIsHoldingWebLockChanged(const PageNode* page_node) override {}
  void OnPageIsHoldingIndexedDBLockChanged(const PageNode* page_node) override {
  }
  void OnMainFrameUrlChanged(const PageNode* page_node) override {}
  void OnMainFrameDocumentChanged(const PageNode* page_node) override {}
  void OnHadFormInteractionChanged(const PageNode* page_node) override {}
  void OnHadUserEditsChanged(const PageNode* page_node) override {}
  void OnTitleUpdated(const PageNode* page_node) override {}
  void OnFaviconUpdated(const PageNode* page_node) override {}
  void OnAboutToBeDiscarded(const PageNode* page_node,
                            const PageNode* new_page_node) override {}
  void OnFreezingVoteChanged(
      const PageNode* page_node,
      absl::optional<freezing::FreezingVote> previous_vote) override {}
};

// std::ostream support for PageNode::EmbeddingType.
std::ostream& operator<<(
    std::ostream& os,
    performance_manager::PageNode::EmbeddingType embedding_type);

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PAGE_NODE_H_
