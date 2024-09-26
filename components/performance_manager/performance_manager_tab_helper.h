// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TAB_HELPER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TAB_HELPER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom-forward.h"
#include "components/performance_manager/web_contents_proxy_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-forward.h"

namespace performance_manager {

class FrameNodeImpl;
class PageNodeImpl;

// This tab helper maintains a page node, and its associated tree of frame nodes
// in the performance manager graph. It also sources a smattering of attributes
// into the graph, including visibility, title, and favicon bits.
// In addition it handles forwarding interface requests from the render frame
// host to the frame graph entity.
class PerformanceManagerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PerformanceManagerTabHelper>,
      public WebContentsProxyImpl {
 public:
  // Observer interface to be notified when a PerformanceManagerTabHelper is
  // being teared down.
  class DestructionObserver {
   public:
    virtual ~DestructionObserver() = default;
    virtual void OnPerformanceManagerTabHelperDestroying(
        content::WebContents*) = 0;
  };

  PerformanceManagerTabHelper(const PerformanceManagerTabHelper&) = delete;
  PerformanceManagerTabHelper& operator=(const PerformanceManagerTabHelper&) =
      delete;

  ~PerformanceManagerTabHelper() override;

  // Returns the PageNode associated with the primary page. This can change
  // during the WebContents lifetime.
  PageNodeImpl* primary_page_node() { return primary_page_->page_node.get(); }

  // Returns the PageNode assicated with the given RenderFrameHost, or nullptr
  // if it doesn't exist.
  PageNodeImpl* GetPageNodeForRenderFrameHost(content::RenderFrameHost* rfh);

  // Registers an observer that is notified when the PerformanceManagerTabHelper
  // is destroyed. Can only be set to non-nullptr if it was previously nullptr,
  // and vice-versa.
  void SetDestructionObserver(DestructionObserver* destruction_observer);

  // Must be invoked prior to detaching a PerformanceManagerTabHelper from its
  // WebContents.
  void TearDown();

  // WebContentsObserver overrides.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void OnAudioStateChanged(bool audible) override;
  void OnFrameAudioStateChanged(content::RenderFrameHost* render_frame_host,
                                bool is_audible) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void InnerWebContentsAttached(content::WebContents* inner_web_contents,
                                content::RenderFrameHost* render_frame_host,
                                bool is_full_page) override;
  void InnerWebContentsDetached(
      content::WebContents* inner_web_contents) override;
  void WebContentsDestroyed() override;
  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override;
  void AboutToBeDiscarded(content::WebContents* new_contents) override;

  // WebContentsProxyImpl overrides. Note that `LastNavigationId()` and
  // `LastNewDocNavigationId()` refer to navigations associated with the
  // primary page.
  content::WebContents* GetWebContents() const override;
  int64_t LastNavigationId() const override;
  int64_t LastNewDocNavigationId() const override;

  void BindDocumentCoordinationUnit(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::DocumentCoordinationUnit> receiver);

  // Retrieves the frame node associated with |render_frame_host|. Returns
  // nullptr if none exist for that frame.
  FrameNodeImpl* GetFrameNode(content::RenderFrameHost* render_frame_host);

  class Observer : public base::CheckedObserver {
   public:
    // Invoked when a frame node is about to be removed from the graph.
    virtual void OnBeforeFrameNodeRemoved(
        PerformanceManagerTabHelper* performance_manager,
        FrameNodeImpl* frame_node) = 0;
  };

  // Adds/removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class content::WebContentsUserData<PerformanceManagerTabHelper>;
  friend class PerformanceManagerRegistryImpl;
  friend class WebContentsProxyImpl;
  FRIEND_TEST_ALL_PREFIXES(PerformanceManagerFencedFrameBrowserTest,
                           FencedFrameDoesNotHaveParentFrameNode);

  explicit PerformanceManagerTabHelper(content::WebContents* web_contents);

  // Make CreateForWebContents private to restrict usage to
  // PerformanceManagerRegistry.
  using WebContentsUserData<PerformanceManagerTabHelper>::CreateForWebContents;

  void OnMainFrameNavigation(int64_t navigation_id, bool same_doc);

  // Data that is tracked per page.
  struct PageData {
    PageData();
    ~PageData();

    // The actual page node.
    std::unique_ptr<PageNodeImpl> page_node;

    // The frame tree node ID of the main frame of this PageNode. This is the
    // primary sort key for the PageNode, as it remains constant over its
    // lifetime.  It allows an abitrary RFH to be mapped to the appropriate
    // page via RFH::GetMainFrame()->GetFrameTreeNodeId().
    // TODO(crbug.com/1211368): This is not true under MPArch, because the
    // frame tree node ID of a prerendered RFH changes when it's activated.
    // (Also, until PM's MPArch support is finished, the "main" FrameNode for a
    // PageNode can change.) Fortunately `main_frame_tree_node_id` is currently
    // only used as a DCHECK that pages are not added twice to the `pages_`
    // set. Make `pages_` a simple list, or a set keyed on something else.
    int main_frame_tree_node_id = 0;

    // The UKM source ID for this page.
    ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;

    // Favicon and title are set when a page is loaded, we only want to send
    // signals to the page node about title and favicon update from the previous
    // title and favicon, thus we want to ignore the very first update since it
    // is always supposed to happen.
    bool first_time_favicon_set = false;
    bool first_time_title_set = false;

    // The last navigation ID that was committed to a main frame in this web
    // contents.
    int64_t last_navigation_id = 0;
    // Similar to the above, but for the last non same-document navigation
    // associated with this WebContents. This is always for a navigation that is
    // older or equal to |last_navigation_id_|.
    int64_t last_new_doc_navigation_id = 0;
  };

  // A transparent comparator for PageData. These are keyed by FrameTreeNodeId,
  // which is unique per Page.
  struct PageDataComparator {
    using is_transparent = void;

    bool operator()(const std::unique_ptr<PageData>& pd1,
                    const std::unique_ptr<PageData>& pd2) const {
      return pd1->main_frame_tree_node_id < pd2->main_frame_tree_node_id;
    }

    bool operator()(const std::unique_ptr<PageData>& pd1,
                    int main_frame_tree_node_id2) const {
      return pd1->main_frame_tree_node_id < main_frame_tree_node_id2;
    }

    bool operator()(int main_frame_tree_node_id1,
                    const std::unique_ptr<PageData>& pd2) const {
      return main_frame_tree_node_id1 < pd2->main_frame_tree_node_id;
    }
  };

  // Stores data related to all pages associated with this WebContents. Multiple
  // pages may exist due to things like BFCache, Portals, Prerendering, etc.
  // Exactly *one* page will be primary.
  base::flat_set<std::unique_ptr<PageData>, PageDataComparator> pages_;

  // Tracks the primary page associated with this WebContents.
  raw_ptr<PageData> primary_page_ = nullptr;

  // Maps from RenderFrameHost to the associated PM node. This is a single
  // map across all pages associated with this WebContents.
  std::map<content::RenderFrameHost*, std::unique_ptr<FrameNodeImpl>> frames_;

  raw_ptr<DestructionObserver> destruction_observer_ = nullptr;
  base::ObserverList<Observer, true, false> observers_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<PerformanceManagerTabHelper> weak_factory_{this};
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TAB_HELPER_H_
