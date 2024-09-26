// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_INTERNAL_H_
#define CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_INTERNAL_H_

// A collection of functions designed for use with content_shell based browser
// tests internal to the content/ module.
// Note: If a function here also works with browser_tests, it should be in
// the content public API.

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/navigation_type.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom-forward.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#include "third_party/blink/public/mojom/page/widget.mojom-test-utils.h"
#include "url/gurl.h"

namespace content {

class FrameTreeNode;
class RenderFrameHost;
class RenderFrameHostImpl;
class RenderWidgetHostImpl;
class Shell;
class SiteInstance;
class ToRenderFrameHost;

// Navigates the frame represented by |node| to |url|, blocking until the
// navigation finishes. Returns true if the navigation succeedd and the final
// URL matches |url|.
bool NavigateFrameToURL(FrameTreeNode* node, const GURL& url);

// Sets the DialogManager to proceed by default or not when showing a
// BeforeUnload dialog, and if it proceeds, what value to return.
void SetShouldProceedOnBeforeUnload(Shell* shell, bool proceed, bool success);

// Extends the ToRenderFrameHost mechanism to FrameTreeNodes.
RenderFrameHost* ConvertToRenderFrameHost(FrameTreeNode* frame_tree_node);

// Helper function to navigate a window to a |url|, using a browser-initiated
// navigation that will stay in the same BrowsingInstance.  Most
// browser-initiated navigations swap BrowsingInstances, but some tests need a
// navigation to swap processes for cross-site URLs (even outside of
// --site-per-process) while staying in the same BrowsingInstance.
[[nodiscard]] bool NavigateToURLInSameBrowsingInstance(Shell* window,
                                                       const GURL& url);

// Helper function to checks for a subframe navigation starting  in
// `start_site_instance` and results in an error page correctly transitions to
// `end_site_instance` based on whether error page isolation is enabled or not.
[[nodiscard]] bool IsExpectedSubframeErrorTransition(
    SiteInstance* start_site_instance,
    SiteInstance* end_site_instance);

// Creates an iframe with id |frame_id| and src set to |url|, and appends it to
// the main frame's document, waiting until the RenderFrameHostCreated
// notification is received by the browser. If |wait_for_navigation| is true,
// will also wait for the first navigation in the iframe to finish. Returns the
// RenderFrameHost of the iframe.
RenderFrameHost* CreateSubframe(WebContentsImpl* web_contents,
                                std::string frame_id,
                                const GURL& url,
                                bool wait_for_navigation);

// Returns the frames visited by |RenderFrameHostImpl::ForEachRenderFrameHost|
// in the same order.
std::vector<RenderFrameHostImpl*> CollectAllRenderFrameHosts(
    RenderFrameHostImpl* starting_rfh);
std::vector<RenderFrameHostImpl*>
CollectAllRenderFrameHostsIncludingSpeculative(
    RenderFrameHostImpl* starting_rfh);
// Returns the frames visited by |WebContentsImpl::ForEachRenderFrameHost|
// in the same order.
std::vector<RenderFrameHostImpl*> CollectAllRenderFrameHosts(
    WebContentsImpl* web_contents);
std::vector<RenderFrameHostImpl*>
CollectAllRenderFrameHostsIncludingSpeculative(WebContentsImpl* web_contents);

// Open a new popup passing no URL to window.open, which results in a blank page
// and only the initial entry. Returns the newly created shell. Also saves the
// reference to the opened window in the "last_opened_window" variable in JS.
Shell* OpenBlankWindow(WebContentsImpl* web_contents);

// Pop open a new window that navigates to |url|. Returns the newly created
// shell. Also saves the reference to the opened window in the
// "last_opened_window" variable in JS.
Shell* OpenWindow(WebContentsImpl* web_contents, const GURL& url);

// Creates compact textual representations of the state of the frame tree that
// is appropriate for use in assertions.
//
// The diagrams show frame tree structure, the SiteInstance of current frames,
// presence of pending frames, and the SiteInstances of any and all proxies.
// They look like this:
//
//        Site A (D pending) -- proxies for B C
//          |--Site B --------- proxies for A C
//          +--Site C --------- proxies for B A
//               |--Site A ---- proxies for B
//               +--Site A ---- proxies for B
//                    +--Site A -- proxies for B
//       Where A = http://127.0.0.1/
//             B = http://foo.com/ (no process)
//             C = http://bar.com/
//             D = http://next.com/
//
// SiteInstances are assigned single-letter names (A, B, C) which are remembered
// across invocations of the pretty-printer.
class FrameTreeVisualizer {
 public:
  FrameTreeVisualizer();

  FrameTreeVisualizer(const FrameTreeVisualizer&) = delete;
  FrameTreeVisualizer& operator=(const FrameTreeVisualizer&) = delete;

  ~FrameTreeVisualizer();

  // Formats and returns a diagram for the provided FrameTreeNode.
  std::string DepictFrameTree(FrameTreeNode* root);

 private:
  // Assign or retrive the abbreviated short name (A, B, C) for a site instance.
  std::string GetName(SiteInstance* site_instance);

  // Elements are site instance ids. The index of the SiteInstance in the vector
  // determines the abbreviated name (0->A, 1->B) for that SiteInstance.
  std::vector<SiteInstanceId> seen_site_instance_ids_;
};

// Uses FrameTreeVisualizer to draw a text representation of the FrameTree that
// is appropriate for use in assertions. If you are going to depict multiple
// trees in a single test, you might want to construct a longer-lived instance
// of FrameTreeVisualizer as this will ensure consistent naming of the site
// instances across all calls.
std::string DepictFrameTree(FrameTreeNode& root);

// Uses window.open to open a popup from the frame |opener| with the specified
// |url|, |name| and window |features|. |expect_return_from_window_open| is used
// to indicate if the caller expects window.open() to return a non-null value.
// Waits for the navigation to |url| to finish and then returns the new popup's
// Shell.  Note that since this navigation to |url| is renderer-initiated, it
// won't cause a process swap unless used in --site-per-process mode.
Shell* OpenPopup(const ToRenderFrameHost& opener,
                 const GURL& url,
                 const std::string& name,
                 const std::string& features,
                 bool expect_return_from_window_open);

// Same as above, but with an empty |features| and
// |expect_return_from_window_open| assumed to be true..
Shell* OpenPopup(const ToRenderFrameHost& opener,
                 const GURL& url,
                 const std::string& name);

// Helper for mocking choosing a file via a file dialog.
class FileChooserDelegate : public WebContentsDelegate {
 public:
  // Constructs a WebContentsDelegate that mocks a file dialog.
  // The mocked file dialog will always reply that the user selected |file| or
  // |files|. |callback| is invoked when RunFileChooser() is called.
  FileChooserDelegate(const base::FilePath& file, base::OnceClosure callback);
  // |base_dir| must be set to the folder being uploaded in |kUploadFolder|
  // mode, and must be empty in all other modes.
  FileChooserDelegate(std::vector<base::FilePath> files,
                      const base::FilePath& base_dir,
                      base::OnceClosure callback);
  ~FileChooserDelegate() override;

  // Implementation of WebContentsDelegate::RunFileChooser.
  void RunFileChooser(RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;

  // The params passed to RunFileChooser.
  const blink::mojom::FileChooserParams& params() const { return *params_; }

 private:
  std::vector<base::FilePath> files_;
  const base::FilePath base_dir_;
  base::OnceClosure callback_;
  blink::mojom::FileChooserParamsPtr params_;
};

// This class is a TestNavigationManager that only monitors notifications within
// the given frame tree node.
class FrameTestNavigationManager : public TestNavigationManager {
 public:
  FrameTestNavigationManager(int frame_tree_node_id,
                             WebContents* web_contents,
                             const GURL& url);

  FrameTestNavigationManager(const FrameTestNavigationManager&) = delete;
  FrameTestNavigationManager& operator=(const FrameTestNavigationManager&) =
      delete;

 private:
  // TestNavigationManager:
  bool ShouldMonitorNavigation(NavigationHandle* handle) override;

  // Notifications are filtered so only this frame is monitored.
  int filtering_frame_tree_node_id_;
};

// An observer that can wait for a specific URL to be committed in a specific
// frame.
// Note: it does not track the start of a navigation, unlike other observers.
class UrlCommitObserver : WebContentsObserver {
 public:
  explicit UrlCommitObserver(FrameTreeNode* frame_tree_node, const GURL& url);

  UrlCommitObserver(const UrlCommitObserver&) = delete;
  UrlCommitObserver& operator=(const UrlCommitObserver&) = delete;

  ~UrlCommitObserver() override;

  void Wait();

 private:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  // The id of the FrameTreeNode in which navigations are peformed.
  int frame_tree_node_id_;

  // The URL this observer is expecting to be committed.
  GURL url_;

  // The RunLoop used to spin the message loop.
  base::RunLoop run_loop_;
};

// Waits for a kill of the given RenderProcessHost and returns the
// BadMessageReason that caused a //content-triggerred kill.
//
// Example usage:
//   RenderProcessHostBadIpcMessageWaiter kill_waiter(render_process_host);
//   ... test code that triggers a renderer kill ...
//   EXPECT_EQ(bad_message::RFH_INVALID_ORIGIN_ON_COMMIT, kill_waiter.Wait());
//
// Tests that don't expect kills (e.g. tests where a renderer process exits
// normally, like RenderFrameHostManagerTest.ProcessExitWithSwappedOutViews)
// should use RenderProcessHostWatcher instead of
// RenderProcessHostBadIpcMessageWaiter.
class RenderProcessHostBadIpcMessageWaiter {
 public:
  explicit RenderProcessHostBadIpcMessageWaiter(
      RenderProcessHost* render_process_host);

  RenderProcessHostBadIpcMessageWaiter(
      const RenderProcessHostBadIpcMessageWaiter&) = delete;
  RenderProcessHostBadIpcMessageWaiter& operator=(
      const RenderProcessHostBadIpcMessageWaiter&) = delete;

  // Waits until the renderer process exits.  Returns the bad message that made
  // //content kill the renderer.  |absl::nullopt| is returned if the renderer
  // was killed outside of //content or exited normally.
  [[nodiscard]] absl::optional<bad_message::BadMessageReason> Wait();

 private:
  RenderProcessHostKillWaiter internal_waiter_;
};

class ShowPopupWidgetWaiter
    : public blink::mojom::PopupWidgetHostInterceptorForTesting {
 public:
  ShowPopupWidgetWaiter(WebContentsImpl* web_contents,
                        RenderFrameHostImpl* frame_host);

  ShowPopupWidgetWaiter(const ShowPopupWidgetWaiter&) = delete;
  ShowPopupWidgetWaiter& operator=(const ShowPopupWidgetWaiter&) = delete;

  ~ShowPopupWidgetWaiter() override;

  gfx::Rect last_initial_rect() const { return initial_rect_; }

  int last_routing_id() const { return routing_id_; }

  // Waits until a popup request is received.
  void Wait();

  // Stops observing new messages.
  void Stop();

 private:
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  void ShowPopupMenu(const gfx::Rect& bounds);
#endif

  // Callback bound for creating a popup widget.
  void DidCreatePopupWidget(RenderWidgetHostImpl* render_widget_host);

  // blink::mojom::PopupWidgetHostInterceptorForTesting:
  blink::mojom::PopupWidgetHost* GetForwardingInterface() override;
  void ShowPopup(const gfx::Rect& initial_rect,
                 const gfx::Rect& initial_anchor_rect,
                 ShowPopupCallback callback) override;

  base::RunLoop run_loop_;
  gfx::Rect initial_rect_;
  int32_t routing_id_ = MSG_ROUTING_NONE;
  int32_t process_id_ = 0;
  raw_ptr<RenderFrameHostImpl> frame_host_;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  raw_ptr<WebContentsImpl> web_contents_;
#endif
};

// A BrowserMessageFilter that drops a pre-specified message.
class DropMessageFilter : public BrowserMessageFilter {
 public:
  DropMessageFilter(uint32_t message_class, uint32_t drop_message_id);

  DropMessageFilter(const DropMessageFilter&) = delete;
  DropMessageFilter& operator=(const DropMessageFilter&) = delete;

 protected:
  ~DropMessageFilter() override;

 private:
  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override;

  const uint32_t drop_message_id_;
};

// A BrowserMessageFilter that observes a message without handling it, and
// reports when it was seen.
class ObserveMessageFilter : public BrowserMessageFilter {
 public:
  ObserveMessageFilter(uint32_t message_class, uint32_t watch_message_id);

  ObserveMessageFilter(const ObserveMessageFilter&) = delete;
  ObserveMessageFilter& operator=(const ObserveMessageFilter&) = delete;

  bool has_received_message() { return received_; }

  // Spins a RunLoop until the message is observed.
  void Wait();

 protected:
  ~ObserveMessageFilter() override;

  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  void QuitWait();

  const uint32_t watch_message_id_;
  bool received_ = false;
  base::OnceClosure quit_closure_;
};

// This observer waits until WebContentsObserver::OnRendererUnresponsive
// notification.
class UnresponsiveRendererObserver : public WebContentsObserver {
 public:
  explicit UnresponsiveRendererObserver(WebContents* web_contents);

  UnresponsiveRendererObserver(const UnresponsiveRendererObserver&) = delete;
  UnresponsiveRendererObserver& operator=(const UnresponsiveRendererObserver&) =
      delete;

  ~UnresponsiveRendererObserver() override;

  RenderProcessHost* Wait(base::TimeDelta timeout = base::TimeDelta::Max());

 private:
  // WebContentsObserver:
  void OnRendererUnresponsive(RenderProcessHost* render_process_host) override;

  raw_ptr<RenderProcessHost> captured_render_process_host_ = nullptr;
  base::RunLoop run_loop_;
};

// Helper class that overrides the JavaScriptDialogManager of a WebContents
// to endlessly block on beforeunload.
class BeforeUnloadBlockingDelegate : public JavaScriptDialogManager,
                                     public WebContentsDelegate {
 public:
  explicit BeforeUnloadBlockingDelegate(WebContentsImpl* web_contents);

  BeforeUnloadBlockingDelegate(const BeforeUnloadBlockingDelegate&) = delete;
  BeforeUnloadBlockingDelegate& operator=(const BeforeUnloadBlockingDelegate&) =
      delete;

  ~BeforeUnloadBlockingDelegate() override;
  void Wait();

  // WebContentsDelegate

  JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source) override;

  // JavaScriptDialogManager

  void RunJavaScriptDialog(WebContents* web_contents,
                           RenderFrameHost* render_frame_host,
                           JavaScriptDialogType dialog_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override;

  void RunBeforeUnloadDialog(WebContents* web_contents,
                             RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override;

  bool HandleJavaScriptDialog(WebContents* web_contents,
                              bool accept,
                              const std::u16string* prompt_override) override;

  void CancelDialogs(WebContents* web_contents, bool reset_state) override {}

 private:
  raw_ptr<WebContentsImpl> web_contents_;

  DialogClosedCallback callback_;

  std::unique_ptr<base::RunLoop> run_loop_ = std::make_unique<base::RunLoop>();
};

// Captures various properties of the NavigationHandle on DidFinishNavigation.
// By default, captures the next navigation (either for a specific frame or
// any frame in the WebContents) and waits until the navigation completely
// loads. Can be configured to not wait for load to finish, and also to capture
// properties for multiple navigations, as we save the values in arrays.
class FrameNavigateParamsCapturer : public WebContentsObserver {
 public:
  // Observes navigation for any node in `contents`.
  explicit FrameNavigateParamsCapturer(WebContents* contents);
  // Observes navigation for the specified |node|.
  explicit FrameNavigateParamsCapturer(FrameTreeNode* node);
  ~FrameNavigateParamsCapturer() override;

  // Start waiting for |navigations_remaining_| navigations to finish (and for
  // load to finish if |wait_for_load_| is true).
  void Wait();

  // Sets the number of navigations to wait for.
  void set_navigations_remaining(int count) {
    DCHECK_GE(count, 0);
    navigations_remaining_ = count;
  }

  // Sets |wait_for_load_| to determine whether to stop waiting when we receive
  // DidFinishNavigation or DidStopLoading.
  void set_wait_for_load(bool wait_for_load) { wait_for_load_ = wait_for_load; }

  // Gets various captured parameters from the last navigation.
  // Must only be called when we only capture a single navigation.
  ui::PageTransition transition() const {
    EXPECT_EQ(1U, transitions_.size());
    return transitions_[0];
  }
  NavigationType navigation_type() const {
    EXPECT_EQ(1U, navigation_types_.size());
    return navigation_types_[0];
  }
  bool is_same_document() const {
    EXPECT_EQ(1U, is_same_documents_.size());
    return is_same_documents_[0];
  }
  bool is_renderer_initiated() const {
    EXPECT_EQ(1U, is_renderer_initiateds_.size());
    return is_renderer_initiateds_[0];
  }
  bool did_replace_entry() const {
    EXPECT_EQ(1U, did_replace_entries_.size());
    return did_replace_entries_[0];
  }
  bool has_user_gesture() const {
    EXPECT_EQ(1U, has_user_gestures_.size());
    return has_user_gestures_[0];
  }
  bool is_overriding_user_agent() const {
    EXPECT_EQ(1U, is_overriding_user_agents_.size());
    return is_overriding_user_agents_[0];
  }
  bool is_error_page() const {
    EXPECT_EQ(1U, is_error_pages_.size());
    return is_error_pages_[0];
  }

  // Gets various captured parameters from all observed navigations.
  const std::vector<ui::PageTransition>& transitions() { return transitions_; }
  const std::vector<GURL>& urls() { return urls_; }
  const std::vector<NavigationType>& navigation_types() {
    return navigation_types_;
  }
  const std::vector<bool>& is_same_documents() { return is_same_documents_; }
  const std::vector<bool>& did_replace_entries() {
    return did_replace_entries_;
  }
  const std::vector<bool>& has_user_gestures() { return has_user_gestures_; }
  const std::vector<bool>& is_overriding_user_agents() {
    return is_overriding_user_agents_;
  }
  const std::vector<bool>& is_error_pages() { return is_error_pages_; }

 private:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  void DidStopLoading() override;

  // The id of the FrameTreeNode whose navigations to observe. If this is not
  // set, then this FrameNavigateParamsCapturer observes all navigations that
  // happen in the observed WebContents.
  absl::optional<int> frame_tree_node_id_;

  // How many navigations remain to capture.
  int navigations_remaining_ = 1;

  // Whether to also wait for the load to complete.
  bool wait_for_load_ = true;

  // The saved properties of the NavigationHandle, captured on
  // DidFinishNavigation. When Wait() finishes, these arrays should contain
  // |navigations_remaining_|, as we always capture them for each navigations.
  std::vector<ui::PageTransition> transitions_;
  std::vector<GURL> urls_;
  std::vector<NavigationType> navigation_types_;
  std::vector<bool> is_same_documents_;
  std::vector<bool> did_replace_entries_;
  std::vector<bool> is_renderer_initiateds_;
  std::vector<bool> has_user_gestures_;
  std::vector<bool> is_overriding_user_agents_;
  std::vector<bool> is_error_pages_;

  base::RunLoop loop_;
};

// This observer keeps track of the number of created RenderFrameHosts.  Tests
// can use this to ensure that a certain number of child frames has been
// created after navigating (defaults to 1), and can also supply a callback to
// run on every RenderFrameCreated call.
class RenderFrameHostCreatedObserver : public WebContentsObserver {
 public:
  using OnRenderFrameHostCreatedCallback =
      base::RepeatingCallback<void(RenderFrameHost*)>;

  explicit RenderFrameHostCreatedObserver(WebContents* web_contents);

  RenderFrameHostCreatedObserver(WebContents* web_contents,
                                 int expected_frame_count);

  RenderFrameHostCreatedObserver(
      WebContents* web_contents,
      OnRenderFrameHostCreatedCallback on_rfh_created);

  ~RenderFrameHostCreatedObserver() override;

  RenderFrameHost* Wait();

  RenderFrameHost* last_rfh() { return last_rfh_; }

 private:
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;

  // The number of RenderFrameHosts to wait for.
  int expected_frame_count_ = 1;

  // The number of RenderFrameHosts that have been created.
  int frames_created_ = 0;

  // The RunLoop used to spin the message loop.
  base::RunLoop run_loop_;

  // The last RenderFrameHost created.
  raw_ptr<RenderFrameHost, DanglingUntriaged> last_rfh_ = nullptr;

  // The callback to call when a RenderFrameCreated call is observed.
  OnRenderFrameHostCreatedCallback on_rfh_created_;
};

// The standard DisabledReason used in testing. The functions below use this
// reason and tests will need to assert that it appears.
BackForwardCache::DisabledReason RenderFrameHostDisabledForTestingReason();
// Disable using the standard testing DisabledReason.
void DisableBFCacheForRFHForTesting(RenderFrameHost* render_frame_host);
void DisableBFCacheForRFHForTesting(GlobalRenderFrameHostId id);

// Changes the WebContents and active entry user agent override from
// DidStartNavigation().
class UserAgentInjector : public WebContentsObserver {
 public:
  UserAgentInjector(WebContents* web_contents, const std::string& user_agent)
      : UserAgentInjector(web_contents,
                          blink::UserAgentOverride::UserAgentOnly(user_agent),
                          true) {}

  UserAgentInjector(WebContents* web_contents,
                    const blink::UserAgentOverride& ua_override,
                    bool is_overriding_user_agent = true)
      : WebContentsObserver(web_contents),
        user_agent_override_(ua_override),
        is_overriding_user_agent_(is_overriding_user_agent) {}

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;

  void set_is_overriding_user_agent(bool is_overriding_user_agent) {
    is_overriding_user_agent_ = is_overriding_user_agent;
  }

  void set_user_agent_override(const std::string& user_agent) {
    user_agent_override_ = blink::UserAgentOverride::UserAgentOnly(user_agent);
  }

 private:
  blink::UserAgentOverride user_agent_override_;
  bool is_overriding_user_agent_ = true;
};

// Just like RenderFrameHostWrapper but holds and gives access to a
// RenderFrameHostImpl.
class RenderFrameHostImplWrapper : public RenderFrameHostWrapper {
 public:
  explicit RenderFrameHostImplWrapper(RenderFrameHost* rfh);

  // Returns the pointer or nullptr if the RFH has already been deleted.
  RenderFrameHostImpl* get() const;

  // Pointerish operators. Feel free to add more if you need them.
  RenderFrameHostImpl& operator*() const;
  RenderFrameHostImpl* operator->() const;
};

// Use this class to wait for all RenderFrameHosts in a WebContents that are
// inactive (pending deletion, stored in BackForwardCache, prerendered, etc) to
// be deleted. This will triggerBackForwardCache flushing and prerender
// cancellations..
class InactiveRenderFrameHostDeletionObserver : public WebContentsObserver {
 public:
  explicit InactiveRenderFrameHostDeletionObserver(WebContents* content);
  ~InactiveRenderFrameHostDeletionObserver() override;

  void Wait();

 private:
  void RenderFrameDeleted(RenderFrameHost*) override;

  void CheckCondition();

  std::unique_ptr<base::RunLoop> loop_;
  std::set<RenderFrameHost*> inactive_rfhs_;
};

class TestNavigationObserverInternal : public TestNavigationObserver {
 public:
  using TestNavigationObserver::TestNavigationObserver;
  ~TestNavigationObserverInternal() override = default;

  // TestNavigationObserver:
  void OnDidFinishNavigation(NavigationHandle* navigation_handle) override;
  // Return the NavigationType of the last navigation.
  NavigationType last_navigation_type() const { return last_navigation_type_; }

 private:
  NavigationType last_navigation_type_ = NAVIGATION_TYPE_UNKNOWN;
};

// Return the descendant of `rfh` found by selecting children according to
// `descendant_indices`. E.g. `DescendantRenderFrameHostImplAt(rfh, {0, 1}) will
// return the child at index 1 of the child at index 0 of `rfh`.
RenderFrameHostImpl* DescendantRenderFrameHostImplAt(
    const ToRenderFrameHost& adapter,
    std::vector<size_t> descendant_indices);

class EffectiveURLContentBrowserTestContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  explicit EffectiveURLContentBrowserTestContentBrowserClient(
      bool requires_dedicated_process);
  EffectiveURLContentBrowserTestContentBrowserClient(
      const GURL& url_to_modify,
      const GURL& url_to_return,
      bool requires_dedicated_process);
  ~EffectiveURLContentBrowserTestContentBrowserClient() override;

  // Adds effective URL translation from |url_to_modify| to |url_to_return|.
  void AddTranslation(const GURL& url_to_modify, const GURL& url_to_return);

 private:
  GURL GetEffectiveURL(BrowserContext* browser_context,
                       const GURL& url) override;
  bool DoesSiteRequireDedicatedProcess(BrowserContext* browser_context,
                                       const GURL& effective_site_url) override;

  EffectiveURLContentBrowserClientHelper helper_;
};

// Class that requests that all pages belonging to the provided site get loaded
// in a non-default StoragePartition.
class CustomStoragePartitionBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  explicit CustomStoragePartitionBrowserClient(const GURL& site_to_isolate);

  StoragePartitionConfig GetStoragePartitionConfigForSite(
      BrowserContext* browser_context,
      const GURL& site) override;

 private:
  GURL site_to_isolate_;
};

// Helper for BeginNavigationInCommitCallbackInterceptor. Declared separately
// to make it easier to friend.
//
// Defers an attempt to commit a navigation A in a speculative RenderFrameHost.
// This observer should be owned by a base::Closure that is used for a
// subsequent navigation B's `resume_commit_closure_` field.
//
// Navigation B will eventually be queued when it reaches ready to commit; since
// navigation A is still stuck in the pending commit state, navigation B will
// not be able to successfully select a RenderFrameHost.
//
// Navigation B will then assign an actual navigation continuation closure to
// its `resume_commit_closure_` field, which will destroy `this`. The destructor
// then resumes committing navigation A.
class ResumeCommitClosureSetObserver {
 public:
  explicit ResumeCommitClosureSetObserver(
      base::SafeRef<NavigationHandle> original_navigation,
      mojom::DidCommitProvisionalLoadParamsPtr original_params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr
          original_interface_params);

  ~ResumeCommitClosureSetObserver();

 private:
  base::SafeRef<NavigationHandle> original_navigation_;
  mojom::DidCommitProvisionalLoadParamsPtr original_params_;
  mojom::DidCommitProvisionalLoadInterfaceParamsPtr original_interface_params_;
};

// Helper that ignores a request from the renderer to commit a navigation and
// instead, begins another navigation to the specified `url` in
// `frame_tree_node`.
class BeginNavigationInCommitCallbackInterceptor
    : public RenderFrameHostImpl::CommitCallbackInterceptor {
 public:
  BeginNavigationInCommitCallbackInterceptor(FrameTreeNode* frame_tree_node,
                                             const GURL& url);

  bool WillProcessDidCommitNavigation(
      NavigationRequest* request,
      mojom::DidCommitProvisionalLoadParamsPtr* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override;

 private:
  class ObserverInstaller;

  const raw_ptr<FrameTreeNode> frame_tree_node_;
  const GURL url_;
};

}  // namespace content

#endif  // CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_INTERNAL_H_
