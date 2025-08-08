// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/frame_navigation_entry.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/renderer_cancellation_throttle.h"
#include "content/browser/site_info.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/common/frame_messages.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "content/test/render_document_feature.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/page_state/page_state_serialization.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "ui/display/display_util.h"

namespace content {
namespace {

using testing::ElementsAre;
using testing::HasSubstr;
using testing::IsEmpty;
using InitialNavigationEntryState =
    NavigationEntryImpl::InitialNavigationEntryState;

const char kAddNamedFrameScript[] =
    "var f = document.createElement('iframe');"
    "f.name = 'foo-frame-name';"
    "document.body.appendChild(f);";
const char kRemoveFrameScript[] =
    "var f = document.querySelector('iframe');"
    "f.parentNode.removeChild(f);";

const char kAddEmptyFrameScript[] =
    "let iframe = document.createElement('iframe');"
    "document.body.appendChild(iframe);";

const char kAddFrameWithSrcScript[] =
    "let iframe = document.createElement('iframe');"
    "iframe.src = $1;"
    "document.body.appendChild(iframe);";

class DataURLOriginToCommitObserver : public WebContentsObserver {
 public:
  explicit DataURLOriginToCommitObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  absl::optional<url::Origin> origin_to_commit() { return origin_to_commit_; }

 private:
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    origin_to_commit_ = static_cast<NavigationRequest*>(navigation_handle)
                            ->commit_params()
                            .origin_to_commit;
  }

  absl::optional<url::Origin> origin_to_commit_;
};

}  // namespace

class NavigationControllerBrowserTestBase : public ContentBrowserTest {
 public:
  NavigationControllerBrowserTestBase() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kQueueNavigationsWhileWaitingForCommit,
          {{"queueing_level", "full"}}}},
        {});
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kExposeInternalsForTesting);
  }

  WebContentsImpl* contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  void LoadDataWithBaseURL(const GURL& base_url,
                           const std::string& data,
                           const GURL& history_url,
                           const std::string& title,
                           bool use_load_data_as_string_with_base_url) {
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    TitleWatcher title_watcher(shell()->web_contents(),
                               base::UTF8ToUTF16(title));
    if (use_load_data_as_string_with_base_url) {
#if BUILDFLAG(IS_ANDROID)
      shell()->LoadDataAsStringWithBaseURL(history_url, data, base_url);
#else
      NOTREACHED();
#endif
    } else {
      shell()->LoadDataWithBaseURL(history_url, data, base_url);
    }
    same_tab_observer.Wait();
    std::u16string actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(title, base::UTF16ToUTF8(actual_title));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class NavigationControllerBrowserTest
    : public NavigationControllerBrowserTestBase,
      public ::testing::WithParamInterface<
          std::tuple<std::string /* render_document_level */,
                     bool /* enable_back_forward_cache*/>> {
 public:
  NavigationControllerBrowserTest() {
    InitAndEnableRenderDocumentFeature(&feature_list_for_render_document_,
                                       std::get<0>(GetParam()));
    InitBackForwardCacheFeature(&feature_list_for_back_forward_cache_,
                                std::get<1>(GetParam()));
  }

  // Provides meaningful param names instead of /0, /1, ...
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto [render_document_level, enable_back_forward_cache] = info.param;
    return base::StringPrintf(
        "%s_%s",
        GetRenderDocumentLevelNameForTestParams(render_document_level).c_str(),
        enable_back_forward_cache ? "BFCacheEnabled" : "BFCacheDisabled");
  }

  RenderFrameHostImpl* current_main_frame_host() {
    return static_cast<RenderFrameHostImpl*>(contents()->GetPrimaryMainFrame());
  }

 protected:
  // TODO(bokan): There's one test whose result depends on whether the
  // FractionalScrollOffsets feature is enabled in Blink's
  // RuntimeEnabledFeatures. Since there's just one, we can determine the
  // status of the feature using script, rather than plumbing this state across
  // the browser/renderer boundary. This can be removed once the feature is
  // shipped to stable and the flag removed. https://crbug.com/414283.
  bool IsFractionalScrollOffsetsEnabled() {
    std::string script =
        "internals.runtimeFlags.fractionalScrollOffsetsEnabled";
    return EvalJs(shell(), script).ExtractBool();
  }

  // Creates a form and submits it to |form_submit_url|. Returns the POST ID of
  // the submitted form POST data.
  int64_t CreateAndSubmitForm(const GURL& form_submit_url) {
    NavigationControllerImpl& controller =
        static_cast<NavigationControllerImpl&>(contents()->GetController());
    TestNavigationObserver form_nav_observer(contents());
    EXPECT_TRUE(ExecJs(contents(),
                       JsReplace(R"(var form = document.createElement('form');
                                 form.method = 'POST';
                                 form.action = $1;
                                 document.body.appendChild(form);
                                 form.submit();)",
                                 form_submit_url)));
    form_nav_observer.Wait();
    EXPECT_EQ(form_submit_url, contents()->GetLastCommittedURL());
    const int64_t form_post_id =
        controller.GetLastCommittedEntry()->GetPostID();
    EXPECT_NE(-1, form_post_id);
    EXPECT_TRUE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ("POST", contents()->GetPrimaryMainFrame()->last_http_method());
    return form_post_id;
  }

  void ReplaceState(FrameTreeNode* node,
                    const std::string& state,
                    GURL url = GURL()) {
    FrameNavigateParamsCapturer capturer(node);
    capturer.set_wait_for_load(false);
    if (url.is_empty()) {
      EXPECT_TRUE(
          ExecJs(node, JsReplace("history.replaceState($1, '')", state)));
    } else {
      EXPECT_TRUE(ExecJs(
          node, JsReplace("history.replaceState($1, '', $2)", state, url)));
    }
    capturer.Wait();
    EXPECT_TRUE(capturer.is_same_document());
  }

 private:
  base::test::ScopedFeatureList feature_list_for_render_document_;
  base::test::ScopedFeatureList feature_list_for_back_forward_cache_;
};

// Base class for tests that need to supply modifications to EmbeddedTestServer
// which are required to be complete before it is started.
class NavigationControllerBrowserTestNoServer
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string /* render_document_level */,
                     bool /* enable_back_forward_cache*/>> {
 public:
  NavigationControllerBrowserTestNoServer() {
    InitAndEnableRenderDocumentFeature(&feature_list_for_render_document_,
                                       std::get<0>(GetParam()));
    InitBackForwardCacheFeature(&feature_list_for_back_forward_cache_,
                                std::get<1>(GetParam()));
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
  }

  WebContentsImpl* contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 private:
  base::test::ScopedFeatureList feature_list_for_render_document_;
  base::test::ScopedFeatureList feature_list_for_back_forward_cache_;
};

// Ensure that tests can navigate subframes cross-site in both default mode and
// --site-per-process, but that they only go cross-process in the latter.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, LoadCrossSiteSubframe) {
  // Load a main frame with a subframe.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  // Use NavigateToURLFromRenderer to go cross-site in the subframe.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), foo_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We should only have swapped processes in --site-per-process.
  bool cross_process = root->current_frame_host()->GetProcess() !=
                       root->child_at(0)->current_frame_host()->GetProcess();
  EXPECT_EQ(AreAllSitesIsolatedForTesting(), cross_process);
}

// Expects the referrer URL saved in `entry` is `url`, and that the referrer
// uses the default referrer policy.
void ExpectReferrerWithDefaultPolicy(FrameNavigationEntry* entry,
                                     const GURL& url) {
  EXPECT_EQ(url, entry->referrer().url);
  EXPECT_EQ(blink::ReferrerUtils::MojoReferrerPolicyResolveDefault(
                network::mojom::ReferrerPolicy::kDefault),
            blink::ReferrerUtils::MojoReferrerPolicyResolveDefault(
                entry->referrer().policy));
}

// Same as above, but takes in a NavigationEntry instead.
void ExpectReferrerWithDefaultPolicy(NavigationEntry* entry, const GURL& url) {
  ExpectReferrerWithDefaultPolicy(
      static_cast<NavigationEntryImpl*>(entry)->root_node()->frame_entry.get(),
      url);
}

class LoadDataWithBaseURLWithPossiblyEmptyURLsBrowserTest
    : public NavigationControllerBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<std::string /* render_document_level */,
                     bool /* use_load_data_as_string_with_base_url */,
                     bool /* base_url_empty */,
                     bool /* history_url_empty */>> {
 public:
  LoadDataWithBaseURLWithPossiblyEmptyURLsBrowserTest() {
    InitAndEnableRenderDocumentFeature(&feature_list_for_render_document_,
                                       std::get<0>(GetParam()));
  }

  // Provides meaningful param names instead of /0, /1, ...
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto [render_document_level, use_load_data_as_string_with_base_url,
          base_url_empty, history_url_empty] = info.param;
    return base::StringPrintf(
        "%s_%s_%s_%s",
        GetRenderDocumentLevelNameForTestParams(render_document_level).c_str(),
        use_load_data_as_string_with_base_url ? "AsString" : "Normal",
        base_url_empty ? "BaseURLEmpty" : "BaseURLSet",
        history_url_empty ? "HistoryURLEmpty" : "HistoryURLSet");
  }

 protected:
  bool use_load_data_as_string_with_base_url() {
    return std::get<1>(GetParam());
  }
  bool base_url_empty() { return std::get<2>(GetParam()); }
  bool history_url_empty() { return std::get<3>(GetParam()); }

  // Helper struct to combine all URLs involved for LoadDataWithBaseURL
  // navigations.
  struct URLsForLoadDataWithBaseURL {
    GURL supplied_base_url;
    GURL commit_url;
    GURL virtual_url;
    GURL history_url_for_data_url;
    GURL document_url;
  };

  void CheckURLsMatchExpectationsForLoadDataWithBaseURL(
      NavigationEntryImpl* entry,
      RenderFrameHostImpl* rfh,
      URLsForLoadDataWithBaseURL& urls,
      bool is_renderer_initiated_same_document_navigation = false) {
    EXPECT_EQ(urls.document_url, rfh->last_document_url_in_renderer());
    // The URL in the session history entry will use the commit URL.
    EXPECT_EQ(urls.commit_url, entry->GetURL());
    // Regardless of whether the supplied base URL and history URL are actually
    // used in the renderer or not, they will be saved in the entry.
    EXPECT_EQ(urls.supplied_base_url, entry->GetBaseURLForDataURL());
    EXPECT_EQ(urls.history_url_for_data_url, entry->GetHistoryURLForDataURL());
    EXPECT_EQ(urls.virtual_url, entry->GetVirtualURL());
    // We should use `commit_url` instead of `supplied_base_url` as the original
    // url of this navigation entry, because `supplied_base_url` is only used
    // for resolving relative paths in the data, or enforcing same origin policy
    // (not to actually load the initial contents of the page).
    EXPECT_EQ(urls.commit_url, entry->GetOriginalRequestURL());

    // For cross-document or browser-initiated navigations, the redirect chain
    // contains the document URL.
    // TODO(https://crbug.com/1171237): Should we use the commit URL instead?
    if (!is_renderer_initiated_same_document_navigation) {
      EXPECT_EQ(1u, entry->GetRedirectChain().size());
      EXPECT_EQ(urls.document_url, entry->GetRedirectChain()[0]);
    }
  }

  URLsForLoadDataWithBaseURL GetURLsForLoadDataWithBaseURL(
      const GURL& commit_url,
      const GURL& supplied_base_url,
      const GURL& supplied_history_url) {
    URLsForLoadDataWithBaseURL result;
    result.supplied_base_url = supplied_base_url;
    // The "commit URL" will always be set to the URL used for commit (the data
    // URL/header).
    result.commit_url = commit_url;
    // The virtual URL (the URL shown in the address bar) will be the supplied
    // history url (*not* the renderer history URL), unless it's empty, in which
    // case we'll fall back to the entry URL (see
    // NavigationEntry::GetVirtualURL()).
    result.virtual_url = supplied_history_url.is_empty() ? result.commit_url
                                                         : supplied_history_url;
    // The "history URL for data URL" in the NavigationEntry will always be set
    // to the virtual URL, unless the base URL is empty, in which case it
    // returns an empty url(see NavigationEntry::GetHistoryURLForDataURL())
    result.history_url_for_data_url =
        supplied_base_url.is_empty() ? GURL() : result.virtual_url;

    // The "document URL" will be set to the base URL, unless the navigation is
    // not treated as a loadDataWithBaseURL navigation or the base URL is empty,
    // in which case it will use the commit URL.
    result.document_url =
        !supplied_base_url.is_empty() ? supplied_base_url : result.commit_url;
    return result;
  }

 private:
  base::test::ScopedFeatureList feature_list_for_render_document_;
};

// Verifies that the base, history, and data URLs for LoadDataWithBaseURL end up
// in the expected parts of the NavigationEntry in each stage of navigation, and
// that we don't kill the renderer on reload.  See https://crbug.com/522567.
// Having the base/history URLs as empty might affect things like which URLs are
// used in the NavigationEntries.
IN_PROC_BROWSER_TEST_P(LoadDataWithBaseURLWithPossiblyEmptyURLsBrowserTest,
                       LoadDataWithBaseURLThenReload) {
#if !BUILDFLAG(IS_ANDROID)
  // LoadDataAsStringWithBaseURL is only supported on Android.
  if (use_load_data_as_string_with_base_url())
    return;
#endif
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;

  const std::string data_header = "data:text/html;charset=utf-8,";
  const std::string title = "foo";
  const std::string data = base::StringPrintf(
      "<html><head><title>%s</title></head><body>foo</body></html>",
      title.c_str());
  const GURL data_url = GURL(data_header + data);
  // `base_url_for_data_url` in LoadURLParams.
  const GURL supplied_base_url =
      base_url_empty() ? GURL() : GURL("http://baseurl");
  // `history_url_for_data_url` in LoadURLParams.
  const GURL supplied_history_url =
      history_url_empty() ? GURL() : GURL("http://historyurl");
  // `url` in LoadURLParams.
  const GURL commit_url =
      use_load_data_as_string_with_base_url() ? GURL(data_header) : data_url;
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  DataURLOriginToCommitObserver data_observer(shell()->web_contents());

  // 1) Load data, but don't commit yet.
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
  if (use_load_data_as_string_with_base_url()) {
#if BUILDFLAG(IS_ANDROID)
    shell()->LoadDataAsStringWithBaseURL(supplied_history_url, data,
                                         supplied_base_url);
#else
    NOTREACHED();
#endif
  } else {
    shell()->LoadDataWithBaseURL(supplied_history_url, data, supplied_base_url);
  }

  // Verify the pending NavigationEntry.
  NavigationEntryImpl* pending_entry = controller.GetPendingEntry();
  URLsForLoadDataWithBaseURL urls = GetURLsForLoadDataWithBaseURL(
      commit_url, supplied_base_url, supplied_history_url);

  // The URL of the entry will always be set to the URL used for commit.
  EXPECT_EQ(urls.commit_url, pending_entry->GetURL());
  EXPECT_EQ(urls.supplied_base_url, pending_entry->GetBaseURLForDataURL());
  EXPECT_EQ(urls.virtual_url, pending_entry->GetVirtualURL());
  EXPECT_EQ(urls.history_url_for_data_url,
            pending_entry->GetHistoryURLForDataURL());

  // 2) Let the navigation commit.
  same_tab_observer.Wait();

  // Verify the last committed NavigationEntry has correct HTTP status code
  // and URLs.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  RenderFrameHostImpl* current_rfh =
      static_cast<RenderFrameHostImpl*>(contents()->GetPrimaryMainFrame());

  {
    SCOPED_TRACE(testing::Message() << " Testing case 2.");
    CheckURLsMatchExpectationsForLoadDataWithBaseURL(entry, current_rfh, urls);
  }

  // data: URL loads always have HTTP status code 200.
  EXPECT_EQ(200, current_rfh->last_http_status_code());
  // If there is no base URL, then the URL is loaded like a regular data: URL,
  // for which origin_to_commit should be set.
  if (base_url_empty()) {
    EXPECT_TRUE(data_observer.origin_to_commit().has_value());
  } else {
    EXPECT_FALSE(data_observer.origin_to_commit().has_value());
  }

  // Verify that the page is not classified as an error page.
  EXPECT_EQ(PAGE_TYPE_NORMAL, entry->GetPageType());
  EXPECT_FALSE(current_rfh->IsErrorDocument());

  // The original request URL for loadDataWithBaseURL navigations will be the
  // URL used for commit (the data URL/header).
  EXPECT_EQ(entry->GetOriginalRequestURL(), urls.commit_url);

  // No referrer since this is a browser-initiated navigation.
  ExpectReferrerWithDefaultPolicy(entry, GURL());

  // 3) Now reload and make sure the renderer isn't killed.
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  current_rfh =
      static_cast<RenderFrameHostImpl*>(contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(current_rfh->IsRenderFrameLive());

  // Verify the last committed NavigationEntry and all the URLs hasn't changed.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* reload_entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(reload_entry, entry);
  {
    SCOPED_TRACE(testing::Message() << " Testing case 3.");
    CheckURLsMatchExpectationsForLoadDataWithBaseURL(reload_entry, current_rfh,
                                                     urls);
  }

  // 4) Now do a same-URL navigation, where all the URLs are the exact same as
  // before. This will be turned into a reload by
  // ShouldTreatNavigationAsReload() in navigation_controller_impl.cc.
  LoadDataWithBaseURL(supplied_base_url, data, supplied_history_url, title,
                      use_load_data_as_string_with_base_url());

  // Verify the last committed NavigationEntry and all the URLs hasn't changed.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* same_url_entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(reload_entry, same_url_entry);

  {
    SCOPED_TRACE(testing::Message() << " Testing case 4.");
    CheckURLsMatchExpectationsForLoadDataWithBaseURL(
        same_url_entry,
        static_cast<RenderFrameHostImpl*>(contents()->GetPrimaryMainFrame()),
        urls);
  }
}

// Similar to LoadDataWithBaseURLThenRendererInitiatedSameDocument but tests
// back navigation after doing some renderer-initiated same-document
// navigations.
IN_PROC_BROWSER_TEST_P(
    LoadDataWithBaseURLWithPossiblyEmptyURLsBrowserTest,
    HistoryNavigationWhenLoadDataWithBaseURLWithSameDocumentNavigation) {
#if !BUILDFLAG(IS_ANDROID)
  // LoadDataAsStringWithBaseURL is only supported on Android.
  if (use_load_data_as_string_with_base_url())
    return;
#endif
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;
  // Renderer-initiated navigations to data: URL on a main frame will be
  // blocked. If the base URL is empty, the renderer will use the data: URL as
  // the base & document URL, and same-document navigations initiated by the
  // renderer will try to do a main frame navigation to a data: URL, which will
  // be blocked. So, return early instead.
  if (base_url_empty())
    return;

  const std::string data_header = "data:text/html;charset=utf-8,";
  const std::string title = "foo";
  const std::string data = base::StringPrintf(
      "<html><head><title>%s</title></head><body>foo</body></html>",
      title.c_str());
  const GURL data_url = GURL(data_header + data);
  // `base_url_for_data_url` in LoadURLParams.
  const GURL supplied_base_url =
      base_url_empty() ? GURL() : GURL("http://baseurl");
  // `history_url_for_data_url` in LoadURLParams.
  const GURL supplied_history_url =
      history_url_empty() ? GURL() : GURL("http://historyurl");
  // `url` in LoadURLParams.
  const GURL commit_url =
      use_load_data_as_string_with_base_url() ? GURL(data_header) : data_url;
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  LoadDataWithBaseURL(supplied_base_url, data, supplied_history_url, title,
                      use_load_data_as_string_with_base_url());

  URLsForLoadDataWithBaseURL urls = GetURLsForLoadDataWithBaseURL(
      commit_url, supplied_base_url, supplied_history_url);

  CheckURLsMatchExpectationsForLoadDataWithBaseURL(
      controller.GetLastCommittedEntry(),
      static_cast<RenderFrameHostImpl*>(contents()->GetPrimaryMainFrame()),
      urls);

  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  // 2) Do a same-document navigation via pushState. The URLs will stay the
  // same (actually the base/document URL should have changed, but the browser
  // has no way of knowing that).
  {
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    EXPECT_TRUE(ExecJs(shell(), "history.pushState({},'foo1', '#bar1')"));
    capturer.Wait();
    EXPECT_TRUE(capturer.is_same_document());

    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    CheckURLsMatchExpectationsForLoadDataWithBaseURL(
        entry,
        static_cast<RenderFrameHostImpl*>(contents()->GetPrimaryMainFrame()),
        urls, true /* is_renderer_initiated_same_document_navigation */);
    scoped_refptr<FrameNavigationEntry> frame_entry =
        entry->root_node()->frame_entry.get();
    if (entry->GetBaseURLForDataURL().is_empty()) {
      // when the base URL is empty, the navigation is treated like a normal
      // data: URL navigation, so the origin is opaque.
      EXPECT_TRUE(frame_entry->committed_origin()->opaque());
    } else {
      // loadDataWithBaseURL() navigations uses the base URL's origin.
      EXPECT_EQ(url::Origin::Create(supplied_base_url),
                frame_entry->committed_origin());
    }
  }

  // 3) Do a same-document navigation via pushState. The URLs will stay the
  // same (actually the base/document URL should have changed, but the browser
  // has no way of knowing that).
  {
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    EXPECT_TRUE(ExecJs(shell(), "history.pushState({},'foo2', '#bar2')"));
    capturer.Wait();
    EXPECT_TRUE(capturer.is_same_document());

    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    CheckURLsMatchExpectationsForLoadDataWithBaseURL(
        entry,
        static_cast<RenderFrameHostImpl*>(contents()->GetPrimaryMainFrame()),
        urls, true /* is_renderer_initiated_same_document_navigation */);
    scoped_refptr<FrameNavigationEntry> frame_entry =
        entry->root_node()->frame_entry.get();
    if (entry->GetBaseURLForDataURL().is_empty()) {
      // when the base URL is empty, the navigation is treated like a normal
      // data: URL navigation, so the origin is opaque.
      EXPECT_TRUE(frame_entry->committed_origin()->opaque());
    } else {
      // loadDataWithBaseURL() navigations uses the base URL's origin.
      EXPECT_EQ(url::Origin::Create(supplied_base_url),
                frame_entry->committed_origin());
    }
  }

  // 4) Do a history navigation via GoBack. GoBack needs to be successful.
  {
    FrameNavigateParamsCapturer capturer(root);
    controller.GoBack();
    capturer.Wait();
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
    scoped_refptr<FrameNavigationEntry> frame_entry =
        entry->root_node()->frame_entry.get();
    if (entry->GetBaseURLForDataURL().is_empty()) {
      // when the base URL is empty, the navigation is treated like a normal
      // data: URL navigation, so the origin is opaque.
      EXPECT_TRUE(frame_entry->committed_origin()->opaque());
    } else {
      // loadDataWithBaseURL() navigations uses the base URL's origin.
      EXPECT_EQ(url::Origin::Create(supplied_base_url),
                frame_entry->committed_origin());
    }
  }
}

// Similar to LoadDataWithBaseURLThenReload but tests renderer-initiated
// same-document navigations after LoadDataWithBaseURL instead.
IN_PROC_BROWSER_TEST_P(LoadDataWithBaseURLWithPossiblyEmptyURLsBrowserTest,
                       LoadDataWithBaseURLThenRendererInitiatedSameDocument) {
#if !BUILDFLAG(IS_ANDROID)
  // LoadDataAsStringWithBaseURL is only supported on Android.
  if (use_load_data_as_string_with_base_url())
    return;
#endif
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;
  // Renderer-initiated navigations to data: URL on a main frame will be
  // blocked. If the base URL is empty, the renderer will use the data: URL as
  // the base & document URL, and same-document navigations initiated by the
  // renderer will try to do a main frame navigation to a data: URL, which will
  // be blocked. So, return early instead.
  if (base_url_empty())
    return;

  const std::string data_header = "data:text/html;charset=utf-8,";
  const std::string title = "foo";
  const std::string data = base::StringPrintf(
      "<html><head><title>%s</title></head><body>foo</body></html>",
      title.c_str());
  const GURL data_url = GURL(data_header + data);
  // `base_url_for_data_url` in LoadURLParams.
  const GURL supplied_base_url =
      base_url_empty() ? GURL() : GURL("http://baseurl");
  // `history_url_for_data_url` in LoadURLParams.
  const GURL supplied_history_url =
      history_url_empty() ? GURL() : GURL("http://historyurl");
  // `url` in LoadURLParams.
  const GURL commit_url =
      use_load_data_as_string_with_base_url() ? GURL(data_header) : data_url;
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  LoadDataWithBaseURL(supplied_base_url, data, supplied_history_url, title,
                      use_load_data_as_string_with_base_url());

  URLsForLoadDataWithBaseURL urls = GetURLsForLoadDataWithBaseURL(
      commit_url, supplied_base_url, supplied_history_url);

  CheckURLsMatchExpectationsForLoadDataWithBaseURL(
      controller.GetLastCommittedEntry(),
      static_cast<RenderFrameHostImpl*>(contents()->GetPrimaryMainFrame()),
      urls);

  // 2) Make a same-document navigation by navigating to the document URL but
  // adding a fragment in the end. No URLs change on the browser side (actually
  // the base/document URL should have changed, but the browser has no way of
  // knowing that).
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  {
    SCOPED_TRACE(testing::Message() << " Testing case 2.");
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    GURL document_url_with_fragment = GURL(urls.document_url.spec() + "#foo");
    EXPECT_TRUE(ExecJs(
        shell(), JsReplace("location.href = $1;", document_url_with_fragment)));
    capturer.Wait();
    EXPECT_TRUE(capturer.is_same_document());
    CheckURLsMatchExpectationsForLoadDataWithBaseURL(
        controller.GetLastCommittedEntry(),
        static_cast<RenderFrameHostImpl*>(contents()->GetPrimaryMainFrame()),
        urls, true /* is_renderer_initiated_same_document_navigation */);
  }

  // 3) Do a same-document navigation via pushState. The URLs will stay the
  // same (actually the base/document URL should have changed, but the browser
  // has no way of knowing that).
  {
    SCOPED_TRACE(testing::Message() << " Testing case 3.");
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    EXPECT_TRUE(ExecJs(shell(), "history.pushState({},'foo', 'bar')"));
    capturer.Wait();
    EXPECT_TRUE(capturer.is_same_document());

    CheckURLsMatchExpectationsForLoadDataWithBaseURL(
        controller.GetLastCommittedEntry(),
        static_cast<RenderFrameHostImpl*>(contents()->GetPrimaryMainFrame()),
        urls, true /* is_renderer_initiated_same_document_navigation */);
  }
}

// Similar to LoadDataWithBaseURLThenReload but tests browser-initiated
// same-document navigations after LoadDataWithBaseURL instead.
IN_PROC_BROWSER_TEST_P(LoadDataWithBaseURLWithPossiblyEmptyURLsBrowserTest,
                       LoadDataWithBaseURLThenBrowserInitiatedSameDocument) {
#if !BUILDFLAG(IS_ANDROID)
  // LoadDataAsStringWithBaseURL is only supported on Android.
  if (use_load_data_as_string_with_base_url())
    return;
#endif
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;

  const std::string data_header = "data:text/html;charset=utf-8,";
  const std::string title = "foo";
  const std::string data = base::StringPrintf(
      "<html><head><title>%s</title></head><body>foo</body></html>",
      title.c_str());
  const GURL data_url = GURL(data_header + data);
  // `base_url_for_data_url` in LoadURLParams.
  const GURL supplied_base_url =
      base_url_empty() ? GURL()
                       : embedded_test_server()->GetURL("/title1.html");
  // `history_url_for_data_url` in LoadURLParams.
  const GURL supplied_history_url =
      history_url_empty() ? GURL()
                          : embedded_test_server()->GetURL("/title2.html");
  // `url` in LoadURLParams.
  const GURL commit_url =
      use_load_data_as_string_with_base_url() ? GURL(data_header) : data_url;
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Do a LoadDataWithBaseURL navigation.
  LoadDataWithBaseURL(supplied_base_url, data, supplied_history_url, title,
                      use_load_data_as_string_with_base_url());

  URLsForLoadDataWithBaseURL urls = GetURLsForLoadDataWithBaseURL(
      commit_url, supplied_base_url, supplied_history_url);
  CheckURLsMatchExpectationsForLoadDataWithBaseURL(
      controller.GetLastCommittedEntry(),
      static_cast<RenderFrameHostImpl*>(contents()->GetPrimaryMainFrame()),
      urls);

  // 1) Try to make a same-document navigation from the browser after
  // LoadDataWithBaseURL by navigating to the commit URL + "#foo". This will
  // start out as same-document but might be restarted as cross-document if it
  // doesn't match the document URL. All cases will just end up being a regular
  // data: URL commit (either same-document or cross-document).
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  {
    SCOPED_TRACE(testing::Message() << " Testing case 1. ");

    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    GURL commit_url_with_fragment = GURL(commit_url.spec() + "#foo");
    NavigationController::LoadURLParams params(commit_url_with_fragment);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    contents()->GetController().LoadURLWithParams(params);
    capturer.Wait();
    EXPECT_EQ(urls.document_url == urls.commit_url,
              capturer.is_same_document());

    // Update the URL expectations for the regular data: URL commit.
    urls.commit_url = commit_url_with_fragment;
    urls.document_url = commit_url_with_fragment;
    urls.history_url_for_data_url = GURL();
    urls.supplied_base_url = GURL();
    // If the navigation is classified as a same document, it might use the
    // previous history URL as the virtual URL, probably unintentionally copied
    // from the previous NavigationEntry.
    urls.virtual_url = (capturer.is_same_document() && !history_url_empty())
                           ? supplied_history_url
                           : commit_url_with_fragment;

    CheckURLsMatchExpectationsForLoadDataWithBaseURL(
        controller.GetLastCommittedEntry(),
        static_cast<RenderFrameHostImpl*>(contents()->GetPrimaryMainFrame()),
        urls);
  }

  // Do another LoadDataWithBaseURL navigation.
  LoadDataWithBaseURL(supplied_base_url, data, supplied_history_url, title,
                      use_load_data_as_string_with_base_url());
  urls = GetURLsForLoadDataWithBaseURL(commit_url, supplied_base_url,
                                       supplied_history_url);
  CheckURLsMatchExpectationsForLoadDataWithBaseURL(
      controller.GetLastCommittedEntry(),
      static_cast<RenderFrameHostImpl*>(contents()->GetPrimaryMainFrame()),
      urls);

  // 2) Try to make a same-document navigation from the browser after
  // LoadDataWithBaseURL by navigating to the document URL + "#foo". This will
  // not be classified as same-document by the browser (unless the document URL
  // is the same as the commit URL, e.g. if the base URL is empty) because the
  // browser only compares against the document URL when deciding whether a
  // navigation is same-document or not.
  {
    SCOPED_TRACE(testing::Message() << " Testing case 2. ");

    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    GURL document_url_with_fragment = GURL(urls.document_url.spec() + "#foo");
    NavigationController::LoadURLParams params(document_url_with_fragment);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    contents()->GetController().LoadURLWithParams(params);
    capturer.Wait();
    EXPECT_EQ(urls.document_url == urls.commit_url,
              capturer.is_same_document());

    // Update the URL expectations for the regular commit.
    urls.commit_url = document_url_with_fragment;
    urls.document_url = document_url_with_fragment;
    urls.history_url_for_data_url = GURL();
    urls.supplied_base_url = GURL();
    // If the navigation is classified as a same document, it might use the
    // previous history URL as the virtual URL, probably unintentionally copied
    // from the previous NavigationEntry.
    urls.virtual_url = (capturer.is_same_document() && !history_url_empty())
                           ? supplied_history_url
                           : document_url_with_fragment;

    CheckURLsMatchExpectationsForLoadDataWithBaseURL(
        controller.GetLastCommittedEntry(),
        static_cast<RenderFrameHostImpl*>(contents()->GetPrimaryMainFrame()),
        urls);
  }
}

// Test history navigations on an iframe within a document loaded with
// loadDataWithBaseURL.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       LoadDataWithBaseURLThenHistoryNavigationInSubframe) {
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;

  GURL subframe_url_1(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/simple_page.html"));
  GURL subframe_url_2(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/simple_page_1.html"));
  const std::string data =
      base::StringPrintf("<html><body><iframe src=\"%s\"/></body></html>",
                         subframe_url_1.spec().c_str());

  // `base_url_for_data_url` in LoadURLParams.
  const GURL base_url = GURL("http://baseurl");
  // `history_url_for_data_url` in LoadURLParams.
  const GURL history_url = GURL("http://historyurl");
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  {
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    shell()->LoadDataWithBaseURL(history_url, data, base_url);
    same_tab_observer.Wait();

    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    scoped_refptr<FrameNavigationEntry> subframe_entry =
        entry->root_node()->children[0]->frame_entry.get();
    // The subframe's origin is based on the iframe's URL, unlike the main frame
    // which uses the base URL's origin in some cases.
    EXPECT_EQ(url::Origin::Create(subframe_url_1),
              subframe_entry->committed_origin());
    EXPECT_EQ(subframe_url_1, subframe_entry->url());
  }

  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  // 1) Do a same-document navigation via pushState in iframe.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    std::string script = "history.pushState({}, 'page 1', '#bar1')";
    EXPECT_TRUE(ExecJs(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(capturer.is_same_document());

    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    scoped_refptr<FrameNavigationEntry> main_frame_entry =
        entry->root_node()->frame_entry.get();
    // loadDataWithBaseURL() navigations uses the base URL's origin.
    EXPECT_EQ(url::Origin::Create(base_url),
              main_frame_entry->committed_origin());

    scoped_refptr<FrameNavigationEntry> subframe_entry =
        entry->root_node()->children[0]->frame_entry.get();
    // The subframe's origin is based on the iframe's URL, unlike the main frame
    // which uses the base URL's origin in some cases.
    EXPECT_EQ(url::Origin::Create(subframe_url_1),
              subframe_entry->committed_origin());
    EXPECT_EQ(subframe_url_1.spec() + "#bar1", subframe_entry->url());
  }

  // 2) Do a cross-document navigation on iframe.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), subframe_url_2));
    capturer.Wait();
    EXPECT_FALSE(capturer.is_same_document());

    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    scoped_refptr<FrameNavigationEntry> main_frame_entry =
        entry->root_node()->frame_entry.get();
    // loadDataWithBaseURL() navigations uses the base URL's origin.
    EXPECT_EQ(url::Origin::Create(base_url),
              main_frame_entry->committed_origin());

    scoped_refptr<FrameNavigationEntry> subframe_entry =
        entry->root_node()->children[0]->frame_entry.get();
    // The subframe's origin is based on the iframe's URL, unlike the main frame
    // which uses the base URL's origin in some cases.
    EXPECT_EQ(url::Origin::Create(subframe_url_2),
              subframe_entry->committed_origin());
    EXPECT_EQ(subframe_url_2, subframe_entry->url());
  }

  // 3) Do a history navigation via GoBack. GoBack needs to be successful.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    std::string script = "history.back()";
    EXPECT_TRUE(ExecJs(root->child_at(0), script));
    capturer.Wait();
    EXPECT_FALSE(capturer.is_same_document());
    EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    scoped_refptr<FrameNavigationEntry> main_frame_entry =
        entry->root_node()->frame_entry.get();
    // loadDataWithBaseURL() navigations uses the base URL's origin.
    EXPECT_EQ(url::Origin::Create(base_url),
              main_frame_entry->committed_origin());

    // Check the subframe origin is restored correctly.
    scoped_refptr<FrameNavigationEntry> subframe_entry =
        entry->root_node()->children[0]->frame_entry.get();
    EXPECT_EQ(url::Origin::Create(subframe_url_1),
              subframe_entry->committed_origin());
    EXPECT_EQ(subframe_url_1.spec() + "#bar1", subframe_entry->url());
  }
}

// Verify which page loads when going back to a LoadDataWithBaseURL entry.
// See https://crbug.com/612196.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       LoadDataWithBaseURLTitleAfterBack) {
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;

  const GURL base_url("http://baseurl");
  const GURL history_url(
      embedded_test_server()->GetURL("/navigation_controller/form.html"));
  const std::string data1 = "<html><title>One</title><body>foo</body></html>";
  const GURL data_url1 = GURL("data:text/html;charset=utf-8," + data1);

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  {
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    shell()->LoadDataWithBaseURL(history_url, data1, base_url);
    same_tab_observer.Wait();
  }

  // Verify the last committed NavigationEntry.
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, entry->GetVirtualURL());
  EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
  EXPECT_EQ(data_url1, entry->GetURL());
  EXPECT_EQ("http://baseurl", EvalJs(shell(), "self.origin"));
  EXPECT_EQ(
      url::Origin::Create(base_url),
      shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  // Navigate again to a different data URL.
  const std::string data2 = "<html><title>Two</title><body>bar</body></html>";
  const GURL data_url2 = GURL("data:text/html;charset=utf-8," + data2);
  {
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    // Load data, not loaddatawithbaseurl.
    EXPECT_TRUE(NavigateToURL(shell(), data_url2));
    same_tab_observer.Wait();
  }
  url::Origin data_origin =
      shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  EXPECT_TRUE(data_origin.opaque());
  EXPECT_EQ(url::SchemeHostPort(),
            data_origin.GetTupleOrPrecursorTupleIfOpaque());

  // Go back.
  TestNavigationObserver back_load_observer(shell()->web_contents());
  controller.GoBack();
  back_load_observer.Wait();

  // Check title.  We should load the data URL when going back.
  EXPECT_EQ("One", base::UTF16ToUTF8(shell()->web_contents()->GetTitle()));

  // Verify the last committed NavigationEntry.
  NavigationEntryImpl* back_entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, back_entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, back_entry->GetVirtualURL());
  EXPECT_EQ(history_url, back_entry->GetHistoryURLForDataURL());
  EXPECT_EQ(data_url1, back_entry->GetOriginalRequestURL());
  EXPECT_EQ(data_url1, back_entry->GetURL());

  EXPECT_EQ(
      data_url1,
      shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(
      url::Origin::Create(base_url),
      shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       CrossDomainResourceRequestLoadDataWithBaseUrl) {
  const GURL base_url("foobar://");
  const GURL history_url("http://historyurl");
  const std::string data = "<html><body></body></html>";
  const GURL data_url = GURL("data:text/html;charset=utf-8," + data);

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Load data and commit.
  {
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    shell()->LoadDataWithBaseURL(history_url, data, base_url);
    same_tab_observer.Wait();
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
    EXPECT_EQ(history_url, entry->GetVirtualURL());
    EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
    EXPECT_EQ(data_url, entry->GetURL());
  }

  // Now make an XHR request and check that the renderer isn't killed.
  std::string script =
      "var url = 'http://www.example.com';\n"
      "var xhr = new XMLHttpRequest();\n"
      "xhr.open('GET', url);\n"
      "xhr.send();\n";
  EXPECT_TRUE(ExecJs(shell()->web_contents(), script));
  // The renderer may not be killed immediately (if it is indeed killed), so
  // reload, block and verify its liveness.
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  EXPECT_TRUE(
      shell()->web_contents()->GetPrimaryMainFrame()->IsRenderFrameLive());
}

class LoadDataWithBaseURLBrowserTest
    : public NavigationControllerBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<std::string /* render_document_level */,
                     bool /* use_load_data_as_string_with_base_url */>> {
 public:
  LoadDataWithBaseURLBrowserTest() {
    InitAndEnableRenderDocumentFeature(&feature_list_for_render_document_,
                                       std::get<0>(GetParam()));
  }

  // Provides meaningful param names instead of /0, /1, ...
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto [render_document_level, use_load_data_as_string_with_base_url] =
        info.param;
    return base::StringPrintf(
        "%s_%s",
        GetRenderDocumentLevelNameForTestParams(render_document_level).c_str(),
        use_load_data_as_string_with_base_url ? "AsString" : "Normal");
  }

 protected:
  bool use_load_data_as_string_with_base_url() {
    return std::get<1>(GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_for_render_document_;
};

// Tests that navigating with LoadDataWithBaseURL succeeds even when the base
// URL given is invalid.
IN_PROC_BROWSER_TEST_P(LoadDataWithBaseURLBrowserTest,
                       LoadDataWithInvalidBaseURL) {
#if !BUILDFLAG(IS_ANDROID)
  // LoadDataAsStringWithBaseURL is only supported on Android.
  if (use_load_data_as_string_with_base_url())
    return;
#endif
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;

  const GURL base_url("http://");  // Invalid.
  const GURL history_url("http://historyurl");
  const std::string title = "invalid_base_url";
  const std::string data = base::StringPrintf(
      "<html><head><title>%s</title></head><body>foo</body></html>",
      title.c_str());
  LoadDataWithBaseURL(base_url, data, history_url, title,
                      use_load_data_as_string_with_base_url());

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  // What the base URL ends up being is really implementation defined, as
  // using an invalid base URL is already undefined behavior.
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());

  const GURL original_committed_url =
      contents()->GetPrimaryMainFrame()->GetLastCommittedURL();

  {
    // Make a same-document navigation via history.pushState.
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    EXPECT_TRUE(ExecJs(shell(), "history.pushState('', 'test', '#foo')"));
    same_tab_observer.Wait();
  }

  // Verify that the same-document navigation succeeds.
  EXPECT_EQ(2, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(original_committed_url.spec() + "#foo",
            contents()->GetPrimaryMainFrame()->GetLastCommittedURL());

  // Verify that the page is not classified as an error page.
  EXPECT_EQ(PAGE_TYPE_NORMAL, entry->GetPageType());
  EXPECT_FALSE(contents()->GetPrimaryMainFrame()->IsErrorDocument());
}

// ContentBrowserClient that blocks normal commits to any URL in
// VerifyDidCommitParams.
class BlockAllCommitContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  // Any visit to any URL will be blocked by VerifyDidCommitParams, except if
  // the checks are skipped (e.g. loadDataWithBaseURL).
  BlockAllCommitContentBrowserClient() = default;

  BlockAllCommitContentBrowserClient(
      const BlockAllCommitContentBrowserClient&) = delete;
  BlockAllCommitContentBrowserClient& operator=(
      const BlockAllCommitContentBrowserClient&) = delete;

  bool CanCommitURL(RenderProcessHost* process_host,
                    const GURL& site_url) override {
    return false;
  }
};

// Tests that navigating with LoadDataWithBaseURL succeeds when the data URL is
// typically blocked by an embedder, because it bypasses the renderer check.
IN_PROC_BROWSER_TEST_P(LoadDataWithBaseURLBrowserTest,
                       LoadDataWithBlockedDataURL) {
#if !BUILDFLAG(IS_ANDROID)
  // LoadDataAsStringWithBaseURL is only supported on Android.
  if (use_load_data_as_string_with_base_url())
    return;
#endif
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;

  BlockAllCommitContentBrowserClient content_browser_client;
  const GURL base_url = embedded_test_server()->GetURL("/title1.html");
  const GURL history_url("http://historyurl");
  const std::string title = "blocked_url";
  const std::string data = base::StringPrintf(
      "<html><head><title>%s</title></head><body>foo</body></html>",
      title.c_str());
  LoadDataWithBaseURL(base_url, data, history_url, title,
                      use_load_data_as_string_with_base_url());

  const GURL data_url = GURL("data:text/html;charset=utf-8," + data);
  const GURL commit_url = use_load_data_as_string_with_base_url()
                              ? GURL("data:text/html;charset=utf-8,")
                              : data_url;

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(commit_url,
            contents()->GetPrimaryMainFrame()->GetLastCommittedURL());

  // Verify that the page is not classified as an error page.
  EXPECT_EQ(PAGE_TYPE_NORMAL, entry->GetPageType());
  EXPECT_FALSE(contents()->GetPrimaryMainFrame()->IsErrorDocument());

  {
    // Make a same-document navigation via history.pushState.
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    EXPECT_TRUE(ExecJs(shell(), "history.pushState('', 'test', '#foo')"));
    same_tab_observer.Wait();
  }

  // Verify the last committed NavigationEntry.
  EXPECT_EQ(2, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(commit_url,
            contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  {
    // Go back.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    controller.GoBack();
    back_load_observer.Wait();
  }

  // Verify the last committed NavigationEntry.
  EXPECT_EQ(2, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(commit_url,
            contents()->GetPrimaryMainFrame()->GetLastCommittedURL());

  // Verify that the page is not classified as an error page.
  EXPECT_EQ(PAGE_TYPE_NORMAL, entry->GetPageType());
  EXPECT_FALSE(contents()->GetPrimaryMainFrame()->IsErrorDocument());

  {
    // Make a same-document navigation via fragment navigation.
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    EXPECT_TRUE(ExecJs(shell(), "location.href = '#bar';"));
    same_tab_observer.Wait();
  }

  // Verify the last committed NavigationEntry.
  EXPECT_EQ(2, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(commit_url,
            contents()->GetPrimaryMainFrame()->GetLastCommittedURL());

  // Verify that the page is not classified as an error page.
  EXPECT_EQ(PAGE_TYPE_NORMAL, entry->GetPageType());
  EXPECT_FALSE(contents()->GetPrimaryMainFrame()->IsErrorDocument());
}

// Tests that a LoadDataWithBaseURL to a blocked data URL and an invalid
// base URL results in a renderer kill, different from the
// LoadDataWithBlockedDataURL test above, because the navigation won't bypass
// the checks since the base URL is invalid.
IN_PROC_BROWSER_TEST_P(LoadDataWithBaseURLBrowserTest,
                       LoadDataWithBlockedDataURLAndInvalidBaseURL) {
#if !BUILDFLAG(IS_ANDROID)
  // LoadDataAsStringWithBaseURL is only supported on Android.
  if (use_load_data_as_string_with_base_url())
    return;
#endif
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;

  const GURL base_url("http://");  // Invalid.
  EXPECT_TRUE(!base_url.is_valid());
  const GURL history_url("http://historyurl");
  const std::string title = "invalid_base_url";
  const std::string data = base::StringPrintf(
      "<html><head><title>%s</title></head><body>foo</body></html>",
      title.c_str());

  BlockAllCommitContentBrowserClient content_browser_client;
  RenderProcessHostBadIpcMessageWaiter kill_waiter(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());

  if (use_load_data_as_string_with_base_url()) {
#if BUILDFLAG(IS_ANDROID)
    shell()->LoadDataAsStringWithBaseURL(history_url, data, base_url);
#else
    NOTREACHED();
#endif
  } else {
    shell()->LoadDataWithBaseURL(history_url, data, base_url);
  }

  // The renderer got killed because the ContentBrowserClient blocks the
  // navigation.
  EXPECT_EQ(bad_message::RFH_CAN_COMMIT_URL_BLOCKED, kill_waiter.Wait());
}

// Checks that a browser-initiated same-document navigation after a javascript:
// URL navigation on a page which has a valid base URL preserves the base URL.
IN_PROC_BROWSER_TEST_P(
    LoadDataWithBaseURLBrowserTest,
    LoadDataWithBaseURLThenJavaScriptURLThenSameDocumentNavigation) {
#if !BUILDFLAG(IS_ANDROID)
  // LoadDataAsStringWithBaseURL is only supported on Android.
  if (use_load_data_as_string_with_base_url())
    return;
#endif
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;

  const GURL base_url("http://baseurl");
  const GURL history_url("http://history");
  const std::string title = "foo";
  const std::string data_header = "data:text/html;charset=utf-8,";
  const std::string data = base::StringPrintf(
      "<html><head><title>%s</title></head><body>foo</body></html>",
      title.c_str());
  const GURL data_url = GURL(data_header + data);
  const GURL commit_url =
      use_load_data_as_string_with_base_url() ? GURL(data_header) : data_url;

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  LoadDataWithBaseURL(base_url, data, history_url, title,
                      use_load_data_as_string_with_base_url());

  // Verify the last committed NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, entry->GetVirtualURL());
  EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
  EXPECT_EQ(commit_url, entry->GetURL());
  EXPECT_EQ(commit_url,
            contents()->GetPrimaryMainFrame()->GetLastCommittedURL());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  const int64_t start_dsn = controller.GetLastCommittedEntry()
                                ->GetFrameEntry(root)
                                ->document_sequence_number();

  // Do a javascript: URL "navigation", which will create a new document but
  // won't send anything to the browser.
  EXPECT_TRUE(ExecJs(root, R"(window.location = 'javascript:"foo"';)"));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(
      use_load_data_as_string_with_base_url() ? GURL(data_header) : data_url,
      root->current_url());
  EXPECT_EQ("foo", EvalJs(shell(), "document.body.innerHTML"));
  EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
  EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
  EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                           ->GetFrameEntry(root)
                           ->document_sequence_number());

  {
    // Make a same-document navigation via history.pushState.
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    EXPECT_TRUE(ExecJs(shell(), "history.pushState('', 'test', '#foo')"));
    same_tab_observer.Wait();
  }

  // Verify the last committed NavigationEntry has the same URLs as it would
  // if the javascript: URL commit never happened.
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_NE(entry, controller.GetLastCommittedEntry());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, entry->GetVirtualURL());
  EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
  EXPECT_EQ(commit_url, entry->GetURL());
  EXPECT_EQ(commit_url,
            contents()->GetPrimaryMainFrame()->GetLastCommittedURL().spec());
  EXPECT_EQ(start_dsn, entry->GetFrameEntry(root)->document_sequence_number());

  {
    // Go back.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    controller.GoBack();
    back_load_observer.Wait();
  }

  // Verify that we navigated back to the correct NavigationEntry.
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_NE(entry, controller.GetLastCommittedEntry());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, entry->GetVirtualURL());
  EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
  EXPECT_EQ(commit_url,
            contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(start_dsn, entry->GetFrameEntry(root)->document_sequence_number());
}

// Tests that LoadDataWithBaseURL navigations that failed will commit an error
// page and correctly classified as an error page navigation.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ErrorPageFromLoadDataWithBaseURL) {
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // WebView or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;

  const GURL base_url("http://baseurl");
  const GURL history_url("http://historyurl");
  const std::string data = "<html><body></body></html>";
  const GURL data_url = GURL("data:text/html;charset=utf-8," + data);

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Set up an URLLoaderInterceptor which will cause all navigations to fail.
  auto url_loader_interceptor = std::make_unique<URLLoaderInterceptor>(
      base::BindRepeating([](URLLoaderInterceptor::RequestParams* params) {
        network::URLLoaderCompletionStatus status;
        status.error_code = net::ERR_NOT_IMPLEMENTED;
        params->client->OnComplete(status);
        return true;
      }));

  // Do a LoadDataWithBaseURL navigation that will fail and commit an error
  // page instead.
  {
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    shell()->LoadDataWithBaseURL(history_url, data, base_url);
    same_tab_observer.Wait();

    EXPECT_FALSE(same_tab_observer.last_navigation_succeeded());
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();

    // Verify that the page is classified as an error page.
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_TRUE(contents()->GetPrimaryMainFrame()->IsErrorDocument());

    EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
    EXPECT_EQ(history_url, entry->GetVirtualURL());
    EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
    EXPECT_EQ(data_url, entry->GetURL());
  }
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       NavigateFromLoadDataWithBaseURL) {
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;

  const GURL base_url("http://baseurl");
  const GURL history_url("http://historyurl");
  const std::string data = "<html><body></body></html>";
  const GURL data_url = GURL("data:text/html;charset=utf-8," + data);

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Load data and commit.
  {
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    shell()->LoadDataWithBaseURL(history_url, data, base_url);
    same_tab_observer.Wait();
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
    EXPECT_EQ(history_url, entry->GetVirtualURL());
    EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
    EXPECT_EQ(data_url, entry->GetURL());
  }

  // TODO(boliu): Add test for same document fragment navigation. See
  // crbug.com/561034.

  // Navigate with Javascript.
  {
    GURL navigate_url = embedded_test_server()->GetURL("/title1.html");
    std::string script = JsReplace("document.location = $1", navigate_url);
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    EXPECT_TRUE(ExecJs(shell(), script));
    same_tab_observer.Wait();
    EXPECT_EQ(2, controller.GetEntryCount());
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(entry->GetBaseURLForDataURL().is_empty());
    EXPECT_TRUE(entry->GetHistoryURLForDataURL().is_empty());
    EXPECT_EQ(navigate_url, entry->GetVirtualURL());
    EXPECT_EQ(navigate_url, entry->GetURL());
  }
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FragmentNavigateFromLoadDataWithBaseURL) {
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;

  const GURL base_url("http://baseurl");
  const GURL history_url("http://historyurl");
  const std::string data =
      "<html><body>"
      "  <p id=\"frag\">"
      "    <a id=\"fraglink\" href=\"#frag\">same document nav</a>"
      "  </p>"
      "</body></html>";

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Load data and commit.
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
#if BUILDFLAG(IS_ANDROID)
  shell()->LoadDataAsStringWithBaseURL(history_url, data, base_url);
#else
  shell()->LoadDataWithBaseURL(history_url, data, base_url);
#endif
  same_tab_observer.Wait();
  EXPECT_EQ(1, controller.GetEntryCount());
  const GURL data_url = controller.GetLastCommittedEntry()->GetURL();

  // Perform a fragment navigation using a javascript: URL (which doesn't lead
  // to a commit).
  GURL js_url("javascript:document.location = '#frag';");
  EXPECT_FALSE(NavigateToURL(shell(), js_url));
  EXPECT_EQ(2, controller.GetEntryCount());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
  EXPECT_EQ(history_url, entry->GetVirtualURL());
  EXPECT_EQ(data_url, entry->GetURL());

  // Passes if renderer is still alive.
  EXPECT_TRUE(ExecJs(shell(), "console.log('Success');"));
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, UniqueIDs) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_link_to_load_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  ASSERT_EQ(1, controller.GetEntryCount());

  // Use JavaScript to click the link and load the iframe.
  std::string script = "document.getElementById('link').click()";
  EXPECT_TRUE(ExecJs(shell(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ASSERT_EQ(2, controller.GetEntryCount());

  // Unique IDs should... um... be unique.
  ASSERT_NE(controller.GetEntryAtIndex(0)->GetUniqueID(),
            controller.GetEntryAtIndex(1)->GetUniqueID());
}

// Ensures that RenderFrameHosts end up with the correct nav_entry_id() after
// navigations.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, UniqueIDsOnFrames) {
  NavigationController& controller = shell()->web_contents()->GetController();

  // Load a main frame with an about:blank subframe.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  // The main frame's nav_entry_id should match the last committed entry.
  int unique_id = controller.GetLastCommittedEntry()->GetUniqueID();
  EXPECT_EQ(unique_id, root->current_frame_host()->nav_entry_id());

  // The about:blank iframe should have inherited the same nav_entry_id.
  EXPECT_EQ(unique_id, root->child_at(0)->current_frame_host()->nav_entry_id());

  // Use NavigateToURLFromRenderer to go cross-site in the subframe.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), foo_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The unique ID should have stayed the same for the auto-subframe navigation,
  // since the new page replaces the initial about:blank page in the subframe.
  EXPECT_EQ(unique_id, controller.GetLastCommittedEntry()->GetUniqueID());
  EXPECT_EQ(unique_id, root->current_frame_host()->nav_entry_id());
  EXPECT_EQ(unique_id, root->child_at(0)->current_frame_host()->nav_entry_id());

  // Navigating in the subframe again should create a new entry.
  GURL foo_url2(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), foo_url2));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  int unique_id2 = controller.GetLastCommittedEntry()->GetUniqueID();
  EXPECT_NE(unique_id, unique_id2);

  // The unique ID should have updated for the current RenderFrameHost in both
  // frames, not just the subframe.
  EXPECT_EQ(unique_id2, root->current_frame_host()->nav_entry_id());
  EXPECT_EQ(unique_id2,
            root->child_at(0)->current_frame_host()->nav_entry_id());
}

// This test used to make sure that a scheme used to prevent spoofs didn't ever
// interfere with navigations. We switched to a different scheme, so now this is
// just a test to make sure we can still navigate once we prune the history
// list.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       DontIgnoreBackAfterNavEntryLimit) {
  NavigationController& controller = shell()->web_contents()->GetController();

  // The default (50) makes this test too slow and leads to flakes
  // (https://crbug.com/1167300).
  NavigationControllerImpl::set_max_entry_count_for_testing(10);

  const int kMaxEntryCount =
      static_cast<int>(NavigationControllerImpl::max_entry_count());

  // Load up to the max count, all entries should be there.
  for (int url_index = 0; url_index < kMaxEntryCount; ++url_index) {
    GURL url(base::StringPrintf("data:text/html,page%d", url_index));
    EXPECT_TRUE(NavigateToURL(shell(), url));
  }

  EXPECT_EQ(controller.GetEntryCount(), kMaxEntryCount);

  // Navigate twice more.
  for (int url_index = kMaxEntryCount; url_index < kMaxEntryCount + 2;
       ++url_index) {
    GURL url(base::StringPrintf("data:text/html,page%d", url_index));
    EXPECT_TRUE(NavigateToURL(shell(), url));
  }

  // We expect page0 and page1 to be gone.
  EXPECT_EQ(kMaxEntryCount, controller.GetEntryCount());
  EXPECT_EQ(GURL("data:text/html,page2"),
            controller.GetEntryAtIndex(0)->GetURL());

  // Now try to go back. This should not hang.
  ASSERT_TRUE(controller.CanGoBack());
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // This should have successfully gone back.
  EXPECT_EQ(GURL(base::StringPrintf("data:text/html,page%d", kMaxEntryCount)),
            controller.GetLastCommittedEntry()->GetURL());
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       DontIgnoreRendererInitiatedBackAfterNavEntryLimit) {
  NavigationController& controller = shell()->web_contents()->GetController();

  // The default (50) makes this test too slow and leads to flakes
  // (https://crbug.com/1167300).
  NavigationControllerImpl::set_max_entry_count_for_testing(10);

  const int kMaxEntryCount =
      static_cast<int>(NavigationControllerImpl::max_entry_count());

  // Load up to the max count, all entries should be there.
  for (int url_index = 0; url_index < kMaxEntryCount; ++url_index) {
    GURL url(base::StringPrintf("data:text/html,page%d", url_index));
    EXPECT_TRUE(NavigateToURL(shell(), url));
  }

  EXPECT_EQ(controller.GetEntryCount(), kMaxEntryCount);

  // Navigate twice more.
  for (int url_index = kMaxEntryCount; url_index < kMaxEntryCount + 2;
       ++url_index) {
    GURL url(base::StringPrintf("data:text/html,page%d", url_index));
    EXPECT_TRUE(NavigateToURL(shell(), url));
  }

  // We expect page0 and page1 to be gone.
  EXPECT_EQ(kMaxEntryCount, controller.GetEntryCount());
  EXPECT_EQ(GURL("data:text/html,page2"),
            controller.GetEntryAtIndex(0)->GetURL());

  // Now try to go back. This should not hang.
  ASSERT_TRUE(controller.CanGoBack());
  {
    // Renderer-initiated-back.
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.back()"));
    capturer.Wait();
  }

  // This should have successfully gone back.
  EXPECT_EQ(GURL(base::StringPrintf("data:text/html,page%d", kMaxEntryCount)),
            controller.GetLastCommittedEntry()->GetURL());
}

namespace {

// Does a renderer-initiated location.replace navigation to |url|, replacing the
// current entry.
bool RendererLocationReplace(Shell* shell, const GURL& url) {
  WebContents* web_contents = shell->web_contents();
  WaitForLoadStop(web_contents);
  TestNavigationManager navigation_manager(web_contents, url);
  const GURL& current_url =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedURL();
  // Execute script in an isolated world to avoid causing a Trusted Types
  // violation due to eval.
  EXPECT_TRUE(ExecJs(shell, JsReplace("window.location.replace($1)", url),
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
  // Observe pending entry if it's not a same-document navigation. We can't
  // observe same-document navigations because it might finish in the renderer,
  // only telling the browser side at the end.
  if (!current_url.EqualsIgnoringRef(url)) {
    EXPECT_TRUE(navigation_manager.WaitForRequestStart());
    EXPECT_TRUE(
        NavigationRequest::From(navigation_manager.GetNavigationHandle())
            ->common_params()
            .should_replace_current_entry);
  }
  EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());
  if (!IsLastCommittedEntryOfPageType(web_contents, PAGE_TYPE_NORMAL))
    return false;
  return web_contents->GetLastCommittedURL() == url;
}

}  // namespace

// When loading a new page to replace an old page in the history list, make sure
// that the browser and renderer agree, and that both get it right.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       CorrectLengthWithCurrentItemReplacement) {
  NavigationController& controller = shell()->web_contents()->GetController();

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/simple_page.html")));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, EvalJs(shell(), "history.length"));

  EXPECT_TRUE(RendererLocationReplace(
      shell(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, EvalJs(shell(), "history.length"));

  // Now create two more entries and go back, to test replacing an entry without
  // pruning the forward history.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, EvalJs(shell(), "history.length"));

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title3.html")));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(3, EvalJs(shell(), "history.length"));

  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(controller.CanGoForward());

  EXPECT_TRUE(RendererLocationReplace(
      shell(), embedded_test_server()->GetURL("/simple_page.html?page1b")));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(3, EvalJs(shell(), "history.length"));
  EXPECT_TRUE(controller.CanGoForward());

  // Note that there's no way to access the renderer's notion of the history
  // offset via JavaScript. Checking just the history length, though, is enough;
  // if the replacement failed, there would be a new history entry and thus an
  // incorrect length.
}

// When spawning a new page from a WebUI page, make sure that the browser and
// renderer agree about the length of the history list, and that both get it
// right.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       CorrectLengthWithNewTabNavigatingFromWebUI) {
  GURL web_ui_page(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));
  EXPECT_TRUE(NavigateToURL(shell(), web_ui_page));
  EXPECT_EQ(
      BINDINGS_POLICY_WEB_UI,
      shell()->web_contents()->GetPrimaryMainFrame()->GetEnabledBindings());

  ShellAddedObserver observer;
  GURL page_url = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html");
  // Execute script in an isolated world to avoid causing a Trusted Types
  // violation due to eval.
  EXPECT_TRUE(ExecJs(shell(), JsReplace("window.open($1, '_blank');", page_url),
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
  Shell* shell2 = observer.GetShell();
  EXPECT_TRUE(WaitForLoadStop(shell2->web_contents()));

  EXPECT_EQ(1, shell2->web_contents()->GetController().GetEntryCount());
  // Execute script in an isolated world to avoid causing a Trusted Types
  // violation due to eval.
  EXPECT_EQ(1, EvalJs(shell2, "history.length", EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                      /*world_id=*/1));

  // Again, as above, there's no way to access the renderer's notion of the
  // history offset via JavaScript. Checking just the history length, again,
  // will have to suffice.
}

namespace {

// Test that going back in a subframe on a loadDataWithBaseURL page doesn't
// crash.  See https://crbug.com/768575.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       NavigateBackInChildOfLoadDataWithBaseURL) {
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;

  GURL iframe_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));

  const GURL base_url("http://baseurl");
  const GURL history_url("http://historyurl");
  std::string data =
      "<html><body>"
      "  <p>"
      "    <iframe src=\"";
  data += iframe_url.spec();
  data +=
      "\" />"
      "  </p>"
      "</body></html>";

  // Load data and commit.
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
#if BUILDFLAG(IS_ANDROID)
  shell()->LoadDataAsStringWithBaseURL(history_url, data, base_url);
#else
  shell()->LoadDataWithBaseURL(history_url, data, base_url);
#endif
  same_tab_observer.Wait();

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0u);

  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    std::string script = "document.getElementById('thelink').click()";
    EXPECT_TRUE(ExecJs(child, script));
    observer.Wait();
  }

  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    shell()->web_contents()->GetController().GoBack();
    observer.Wait();
  }

  // Passes if renderer is still alive.
  EXPECT_TRUE(ExecJs(shell(), "console.log('Success');"));
}

class LoadCommittedCapturer : public WebContentsObserver {
 public:
  // Observes the load commit for the specified |node|.
  explicit LoadCommittedCapturer(FrameTreeNode* node)
      : WebContentsObserver(
            WebContents::FromRenderFrameHost(node->current_frame_host())),
        frame_tree_node_id_(node->frame_tree_node_id()) {}

  // Observes the load commit for the next created frame in the specified
  // |web_contents|.
  explicit LoadCommittedCapturer(WebContents* web_contents)
      : WebContentsObserver(web_contents), frame_tree_node_id_(0) {}

  void Wait() { loop_.Run(); }

  ui::PageTransition transition_type() const { return transition_type_; }

 private:
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(render_frame_host);

    // Don't pay attention to pending delete RenderFrameHosts in the main frame,
    // which might happen in a race if a cross-process navigation happens
    // quickly.
    if (rfh->IsPendingDeletion()) {
      DLOG(INFO) << "Skipping pending delete RFH: "
                 << rfh->GetSiteInstance()->GetSiteURL();
      return;
    }

    // If this object was not created with a specified frame tree node, then use
    // the first created active RenderFrameHost.  Once a node is selected, there
    // shouldn't be any other frames being created.
    int frame_tree_node_id = rfh->frame_tree_node()->frame_tree_node_id();
    DCHECK(frame_tree_node_id_ == 0 ||
           frame_tree_node_id_ == frame_tree_node_id);
    frame_tree_node_id_ = frame_tree_node_id;
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted())
      return;

    DCHECK_NE(0, frame_tree_node_id_);
    if (navigation_handle->GetRenderFrameHost()->GetFrameTreeNodeId() !=
        frame_tree_node_id_) {
      return;
    }

    transition_type_ = navigation_handle->GetPageTransition();
    if (!web_contents()->IsLoading()) {
      loop_.Quit();
    }
  }

  void DidStopLoading() override { loop_.Quit(); }

  // The id of the FrameTreeNode whose navigations to observe.
  int frame_tree_node_id_;

  // The transition_type of the last navigation.
  ui::PageTransition transition_type_;

  base::RunLoop loop_;
};

}  // namespace

// Some pages create a popup, then write an iframe into it. This used to cause a
// subframe navigation without having any committed entry. Such navigations
// used to just get thrown on the ground, but we shouldn't crash.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, SubframeOnEmptyPage) {
  // Navigate to a page to force the renderer process to start.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Pop open a new blank window.
  Shell* new_shell = OpenBlankWindow(contents());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  EXPECT_TRUE(new_shell->web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->IsInitialEntry());

  // Make a new iframe in it.
  {
    LoadCommittedCapturer capturer(new_shell->web_contents());
    std::string script =
        JsReplace(kAddFrameWithSrcScript, "data:text/html,<p>some page</p>");
    EXPECT_TRUE(ExecJs(new_root, script));
    capturer.Wait();
  }
  ASSERT_EQ(1U, new_root->child_count());
  ASSERT_NE(nullptr, new_root->child_at(0));

  // The subframe navigation should not be ignored, and we will create a
  // FrameNavigationEntry for the new iframe.
  EXPECT_TRUE(
      static_cast<NavigationEntryImpl*>(
          new_shell->web_contents()->GetController().GetLastCommittedEntry())
          ->GetFrameEntry(new_root->child_at(0)));

  // Navigate it cross-site.
  GURL frame_url = embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html");
  {
    LoadCommittedCapturer capturer(new_root->child_at(0));
    std::string script = JsReplace("location.assign($1);", frame_url);
    EXPECT_TRUE(ExecJs(new_root->child_at(0), script));
    capturer.Wait();
  }

  // The subframe navigation should not ignored, and we will create a
  // FrameNavigationEntry for the new iframe.
  EXPECT_TRUE(
      static_cast<NavigationEntryImpl*>(
          new_shell->web_contents()->GetController().GetLastCommittedEntry())
          ->GetFrameEntry(new_root->child_at(0)));

  // A nested iframe with a cross-site URL should also be able to commit.
  GURL grandchild_url(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(new_shell->web_contents());
    EXPECT_TRUE(ExecJs(new_root->child_at(0),
                       JsReplace(kAddFrameWithSrcScript, grandchild_url)));
    capturer.Wait();
  }
  ASSERT_EQ(1U, new_root->child_at(0)->child_count());
  EXPECT_EQ(grandchild_url, new_root->child_at(0)->child_at(0)->current_url());
  // The subframe navigation should not be ignored, and we will create a
  // FrameNavigationEntry for the new grandchild iframe.
  EXPECT_EQ(
      grandchild_url,
      static_cast<NavigationEntryImpl*>(
          new_shell->web_contents()->GetController().GetLastCommittedEntry())
          ->GetFrameEntry(new_root->child_at(0)->child_at(0))
          ->url());
}

// Test that the renderer is not killed after an auto subframe navigation if the
// main frame appears to change its origin due to a document.write on an
// about:blank page.  See https://crbug.com/613732.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       OriginChangeAfterDocumentWrite) {
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Pop open a new window to `url2`.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(root, "var w = window.open('" + url2.spec() + "');"));
  Shell* new_shell = new_shell_observer.GetShell();
  ASSERT_NE(new_shell->web_contents(), shell()->web_contents());
  EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();

  // Navigate the new window to about:blank. Note that we didn't directly
  // window.open() to about:blank which because otherwise the about:blank page
  // will always get replaced on the next navigation.
  GURL blank_url(url::kAboutBlankURL);
  {
    TestNavigationObserver nav_observer(new_shell->web_contents());
    EXPECT_TRUE(ExecJs(new_shell, "location.href = 'about:blank';"));
    nav_observer.Wait();
    EXPECT_EQ(2, new_shell->web_contents()->GetController().GetEntryCount());
    EXPECT_EQ(blank_url, new_root->current_url());
    EXPECT_EQ(blank_url,
              new_root->current_frame_host()->last_document_url_in_renderer());
  }

  // Make a new iframe with src=`url2` in it using document.write from the
  // opener.
  {
    LoadCommittedCapturer capturer(new_shell->web_contents());
    std::string html = "<iframe src='" + url2.spec() + "'></iframe>";
    std::string script = JsReplace(
        "w.document.write($1);"
        "w.document.close();",
        html);
    EXPECT_TRUE(ExecJs(root->current_frame_host(), script));
    capturer.Wait();
  }
  ASSERT_EQ(1U, new_root->child_count());
  // The main frame stays at "about:blank", but due to document.write(), its
  // "document URL" is set to the opener URL.
  EXPECT_EQ(blank_url, new_root->current_url());
  EXPECT_EQ(url1,
            new_root->current_frame_host()->last_document_url_in_renderer());
  EXPECT_EQ(url2, new_root->child_at(0)->current_url());

  // Navigate the subframe to `url3`.
  GURL url3 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html");
  {
    LoadCommittedCapturer capturer(new_root->child_at(0));
    std::string script = "location.href = '" + url3.spec() + "';";
    EXPECT_TRUE(ExecJs(new_root->child_at(0), script));
    capturer.Wait();
  }
  EXPECT_EQ(blank_url, new_root->current_url());
  EXPECT_EQ(url1,
            new_root->current_frame_host()->last_document_url_in_renderer());
  EXPECT_EQ(url3, new_root->child_at(0)->current_url());
  EXPECT_EQ(3, new_shell->web_contents()->GetController().GetEntryCount());

  // Do a replaceState() in the main frame, which changes the URL from
  // about:blank to a URL with the opener's origin, because replaceState uses
  // the "document URL" when resolving relative URLs.
  {
    LoadCommittedCapturer capturer(new_root);
    std::string script = "history.replaceState({}, 'foo', 'foo');";
    EXPECT_TRUE(ExecJs(new_root, script));
    capturer.Wait();
  }
  GURL replace_state_url(embedded_test_server()->GetURL("/foo"));
  EXPECT_EQ(replace_state_url, new_root->current_url());
  EXPECT_EQ(replace_state_url,
            new_root->current_frame_host()->last_document_url_in_renderer());
  EXPECT_EQ(url3, new_root->child_at(0)->current_url());

  // Go back in the subframe.  Note that in the past before we share FNEs
  // between NavigationEntries (and update all affected NavigationEntry on
  // replaceState etc) the main frame's URL looked like a cross-origin change
  // from a web URL to about:blank, but now it's consistently
  // `replace_state_url` everywhere.
  {
    TestNavigationObserver observer(new_shell->web_contents(), 1);
    new_shell->web_contents()->GetController().GoBack();
    observer.Wait();
    EXPECT_EQ(replace_state_url, new_shell->web_contents()
                                     ->GetController()
                                     .GetLastCommittedEntry()
                                     ->GetURL());
    EXPECT_EQ(replace_state_url, new_root->current_url());
    EXPECT_EQ(replace_state_url,
              new_root->current_frame_host()->last_document_url_in_renderer());
    EXPECT_EQ(url2, new_root->child_at(0)->current_url());
  }
  EXPECT_TRUE(new_root->current_frame_host()->IsRenderFrameLive());

  // Go forward in the subframe.  Note that in the past before we share FNEs
  // between NavigationEntries (and update all affected NavigationEntry on
  // replaceState etc) the main frame's URL looked like a cross-origin change
  // from a web URL to about:blank, but now it's consistently
  // `replace_state_url` everywhere.
  {
    TestNavigationObserver observer(new_shell->web_contents(), 1);
    new_shell->web_contents()->GetController().GoForward();
    observer.Wait();
    EXPECT_EQ(replace_state_url, new_shell->web_contents()
                                     ->GetController()
                                     .GetLastCommittedEntry()
                                     ->GetURL());
    EXPECT_EQ(replace_state_url, new_root->current_url());
    EXPECT_EQ(url3, new_root->child_at(0)->current_url());
  }
  EXPECT_TRUE(new_root->current_frame_host()->IsRenderFrameLive());
}

// Test that a frame's url is correctly updated after a document.open() from
// another frame.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       DocumentOpenFromSibling) {
  // Open a page with iframe.
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe_simple.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(1U, root->child_count());

  GURL first_frame_url = root->child_at(0)->current_url();
  GURL second_frame_url(embedded_test_server()->GetURL("/title2.html"));
  // Make a new iframe.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(
        ExecJs(root, JsReplace(kAddFrameWithSrcScript, second_frame_url)));
    capturer.Wait();
  }
  EXPECT_EQ(2U, root->child_count());
  // Call document.open() on the first iframe from the second iframe.
  EXPECT_TRUE(ExecJs(
      root->child_at(1),
      "parent.document.getElementById(\"frame\").contentDocument.open();"));
  // Run script in the iframes to ensure that the URL update is already sent
  // to the browser before continuing.
  EXPECT_TRUE(ExecJs(root->child_at(1), "true"));
  EXPECT_TRUE(ExecJs(root->child_at(0), "true"));

  RenderFrameHostImpl* child0 = root->child_at(0)->current_frame_host();
  RenderFrameHostImpl* child1 = root->child_at(1)->current_frame_host();
  // The value of "last committed URL" in child0 is not updated, but "last
  // document URL" is updated correctly to be the same as the child1's URL.
  EXPECT_EQ(first_frame_url, child0->GetLastCommittedURL());
  EXPECT_EQ(second_frame_url, child0->last_document_url_in_renderer());
  EXPECT_EQ(second_frame_url, child1->GetLastCommittedURL());
  EXPECT_EQ(second_frame_url, child1->last_document_url_in_renderer());
}

// Test that a frame's url is correctly updated after a document.open() from
// an about:blank frame.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       DocumentOpenFromAboutBlank) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe_simple.html"));
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();

  // Make a new about:blank iframe that will document.open() its sibling.
  {
    LoadCommittedCapturer capturer(contents());
    EXPECT_EQ("done", EvalJs(root->current_frame_host(), R"(
      new Promise(async resolve => {
        const blank_iframe = document.createElement('iframe');
        await new Promise(resolve => {
          blank_iframe.onload = resolve;
          document.body.appendChild(blank_iframe);
        });

        let script = document.createElement('script');
        script.text = `
          const sibling = parent.document.getElementById("frame")
          sibling.contentDocument.open();
        `;
        blank_iframe.contentDocument.body.appendChild(script);
        resolve("done");
      })
    )"));
    capturer.Wait();
  }
  // Run script in the iframes to ensure that the URL update is already sent
  // to the browser before continuing.
  EXPECT_TRUE(ExecJs(root->child_at(1), "true"));
  EXPECT_TRUE(ExecJs(root->child_at(0), "true"));

  ASSERT_EQ(2U, root->child_count());
  RenderFrameHostImpl* child0 = root->child_at(0)->current_frame_host();
  RenderFrameHostImpl* child1 = root->child_at(1)->current_frame_host();
  // The value of "last committed URL" in child0 is not updated, but "last
  // document URL" is updated correctly to "about:blank".
  EXPECT_EQ(frame_url, child0->GetLastCommittedURL());
  EXPECT_EQ(GURL(url::kAboutBlankURL), child0->last_document_url_in_renderer());
  EXPECT_EQ(GURL(url::kAboutBlankURL), child1->GetLastCommittedURL());
  EXPECT_EQ(GURL(url::kAboutBlankURL), child1->last_document_url_in_renderer());
}

// Test that a frame's url is partially updated after a document.open() from
// an about:srcdoc frame.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       DocumentOpenFromSrcdoc) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe_simple.html"));
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();

  // Make a new about:srcdoc iframe that will document.open() its sibling.
  {
    LoadCommittedCapturer capturer(contents());
    std::string script =
        "let origin = document.createElement('iframe');"
        "origin.srcdoc = '<script>parent.document.getElementById(\"frame\")"
        ".contentDocument.open();</script>';"
        "document.body.appendChild(origin);";
    EXPECT_TRUE(ExecJs(root->current_frame_host(), script));
    capturer.Wait();
  }
  // Run script in the iframes to ensure that the URL update is already sent
  // to the browser before continuing.
  EXPECT_TRUE(ExecJs(root->child_at(1), "true"));
  EXPECT_TRUE(ExecJs(root->child_at(0), "true"));

  ASSERT_EQ(2U, root->child_count());
  RenderFrameHostImpl* child0 = root->child_at(0)->current_frame_host();
  RenderFrameHostImpl* child1 = root->child_at(1)->current_frame_host();
  // The value of "last committed URL" in child0 is not updated, but "last
  // document URL" is updated correctly to "about:srdoc".
  EXPECT_EQ(frame_url, child0->GetLastCommittedURL());
  EXPECT_EQ(GURL("about:srcdoc"), child0->last_document_url_in_renderer());
  EXPECT_EQ(GURL("about:srcdoc"), child1->GetLastCommittedURL());
  EXPECT_EQ(GURL("about:srcdoc"), child1->last_document_url_in_renderer());
}

// Test that a frame's url is partially updated after a document.open() from
// a blob: url.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       DocumentOpenFromBloblIframe) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe_simple.html"));
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();

  // Make a new blob iframe that will document.open() its sibling.
  {
    LoadCommittedCapturer capturer(contents());
    std::string script =
        "let origin = document.createElement('iframe');"
        "let blob = new Blob(['<script>"
        "parent.document.getElementById(\"frame\").contentDocument.open();"
        "</script>'], { type: 'text/html' });"
        "origin.src = URL.createObjectURL(blob);"
        "document.body.appendChild(origin);";
    EXPECT_TRUE(ExecJs(root->current_frame_host(), script));
    capturer.Wait();
  }
  // Run script in the iframes to ensure that the URL update is already sent
  // to the browser before continuing.
  EXPECT_TRUE(ExecJs(root->child_at(1), "true"));
  EXPECT_TRUE(ExecJs(root->child_at(0), "true"));

  ASSERT_EQ(2U, root->child_count());
  RenderFrameHostImpl* child0 = root->child_at(0)->current_frame_host();
  RenderFrameHostImpl* child1 = root->child_at(1)->current_frame_host();
  // The value of "last committed URL" in child0 is not updated, but "last
  // document URL" is updated correctly to the blob URL.
  EXPECT_EQ(frame_url, child0->GetLastCommittedURL());
  EXPECT_TRUE(child0->last_document_url_in_renderer().SchemeIsBlob());
  EXPECT_TRUE(child1->GetLastCommittedURL().SchemeIsBlob());
  EXPECT_TRUE(child1->last_document_url_in_renderer().SchemeIsBlob());
  EXPECT_EQ(child0->last_document_url_in_renderer(),
            child1->last_document_url_in_renderer());
}

// Test the last committed, document, and history URLs in various cases.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, RendererURLs) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe_simple.html"));
  GURL iframe_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  // 1) Navigate to `url1`. All three URLs in the main frame should be set to
  // `url_1`, and `frame_url` in the iframe.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(url1, root->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(url1, root->current_frame_host()->last_document_url_in_renderer());
  FrameTreeNode* iframe = root->child_at(0);
  EXPECT_EQ(iframe_url, iframe->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(iframe_url,
            iframe->current_frame_host()->last_document_url_in_renderer());

  // 2) Do a document.open() on the iframe from the main frame.
  EXPECT_TRUE(ExecJs(
      root, "document.getElementById(\"frame\").contentDocument.open();"));
  // Run script in both the main frame and the iframe to ensure that the URL
  // update is already sent to the browser before continuing.
  EXPECT_TRUE(ExecJs(root, "true"));
  EXPECT_TRUE(ExecJs(iframe, "true"));
  // THe iframe's document & history URL is updated to `url1`.
  EXPECT_EQ(iframe_url, iframe->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(url1,
            iframe->current_frame_host()->last_document_url_in_renderer());

  // 3) Do a same-document navigation to `url_1_fragment` on the iframe (Note
  // that this is a same-document navigation because the iframe's document URL
  // is now `url1`).
  GURL url_1_fragment(url1.spec() + "#foo");
  {
    FrameNavigateParamsCapturer capturer(iframe);
    capturer.set_wait_for_load(false);
    EXPECT_TRUE(
        ExecJs(iframe, JsReplace("location.href = '#foo';", url_1_fragment)));
    capturer.Wait();
    EXPECT_TRUE(capturer.is_same_document());
  }
  // All three URLs in the iframe should be updated to `url1_fragment`.
  EXPECT_EQ(url_1_fragment,
            iframe->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(url_1_fragment,
            iframe->current_frame_host()->last_document_url_in_renderer());

  // 4) Do a navigation to `url404`, which wil result in an error page.
  // The document URL will be set to the kUnreachableWebDataURL.
  GURL url404(embedded_test_server()->GetURL("/empty404.html"));
  EXPECT_FALSE(NavigateToURL(shell(), url404));
  EXPECT_EQ(url404, root->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            root->current_frame_host()->last_document_url_in_renderer());

  // 5) Do a same-document pushState on an error page without changing the URL
  // (otherwise it will result in an origin mismatch error). The URLs will stay
  // the same.
  {
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    EXPECT_TRUE(ExecJs(shell(), "history.pushState('', '')"));
    capturer.Wait();
    EXPECT_TRUE(capturer.is_same_document());
  }
  EXPECT_EQ(url404, root->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            root->current_frame_host()->last_document_url_in_renderer());
}

// Tests various cases of replacements caused by error pages.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, ErrorPageReplacement) {
  NavigationController& controller = shell()->web_contents()->GetController();
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&net::URLRequestFailedJob::AddUrlHandler));

  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_EQ(1, controller.GetEntryCount());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1) Navigate to a page that fails to load. It must result in an error page,
  // the NEW_ENTRY navigation type, and an addition to the history list.
  GURL error_url = embedded_test_server()->GetURL("/close-socket");
  {
    FrameNavigateParamsCapturer capturer(root);
    NavigateFrameToURL(root, error_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_EQ(2, controller.GetEntryCount());
  }

  // 2) Navigate again to the same URL. It results in an error page, the
  // NEW_ENTRY navigation type, does replacement, and no addition to the
  // history list. The navigation initially got converted into a reload by the
  // browser but since it failed and doesn't have a valid PageState, it does
  // replacement instead.
  {
    FrameNavigateParamsCapturer capturer(root);
    NavigateFrameToURL(root, error_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_EQ(2, controller.GetEntryCount());
  }

  // 3) Navigate to a different URL (|url2|) that fails to load. It adds a new
  // entry.
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  {
    // Set up an URLLoaderInterceptor which will cause the next navigation to
    // fail.
    auto url_loader_interceptor = std::make_unique<URLLoaderInterceptor>(
        base::BindRepeating([](URLLoaderInterceptor::RequestParams* params) {
          network::URLLoaderCompletionStatus status;
          status.error_code = net::ERR_NOT_IMPLEMENTED;
          params->client->OnComplete(status);
          return true;
        }));

    FrameNavigateParamsCapturer capturer(root);
    NavigateFrameToURL(root, url2);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_FALSE(capturer.did_replace_entry());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  }

  // 4) Go back to the previous error page. Since this still results in an error
  // page, it's classified as EXISTING_ENTRY, like a normal history navigation.
  {
    FrameNavigateParamsCapturer capturer(root);
    controller.GoBack();
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_FALSE(capturer.did_replace_entry());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  }

  // 5) Go forward to |url2|. This navigation will succeed, and since the final
  // SiteInstance is different from the history entry's (due to error page
  // isolation), it will be classified as NEW_ENTRY with replacement.
  {
    FrameNavigateParamsCapturer capturer(root);
    controller.GoForward();
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_NE(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  }

  // 6) Make a new entry ...
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_EQ(4, controller.GetEntryCount());

  // 7) ... and replace it with a failed load.
  {
    FrameNavigateParamsCapturer capturer(root);
    RendererLocationReplace(shell(), error_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_EQ(4, controller.GetEntryCount());
  }

  // 8) Make a new web ui page to force a process swap ...
  GURL web_ui_page(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));
  EXPECT_TRUE(NavigateToURL(shell(), web_ui_page));
  EXPECT_EQ(5, controller.GetEntryCount());

  // 9) ... and replace it with a failed load. (It is NEW_ENTRY for the reason
  // noted above.)
  {
    FrameNavigateParamsCapturer capturer(root);
    RendererLocationReplace(shell(), error_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_EQ(5, controller.GetEntryCount());
  }
}

// Tests various cases of replacements caused by error pages on subframes.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ErrorPageReplacementSubframe) {
  NavigationController& controller = shell()->web_contents()->GetController();
  // Navigate to a page with an iframe.
  GURL main_frame_url = embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));
  EXPECT_EQ(1, controller.GetEntryCount());

  GURL error_url = embedded_test_server()->GetURL("/close-socket");
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&net::URLRequestFailedJob::AddUrlHandler));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1) Navigate the subframe to a page that fails to load. It must result in an
  // error page, the NEW_SUBFRAME navigation type, and an addition to the
  // history list.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    NavigateFrameToURL(root->child_at(0), error_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
    EXPECT_EQ(2, controller.GetEntryCount());
  }

  // 2) Navigate the subframe again to the same URL. It results in an error
  // page, the AUTO_SUBFRAME navigation type, does replacement, and no addition
  // to the history list. This is because the navigation got converted into a
  // reload and doesn't have a valid PageState, but since it fails,
  // it does replacement instead of reload.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    NavigateFrameToURL(root->child_at(0), error_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_EQ(2, controller.GetEntryCount());
  }

  // 3) Navigate the subframe to a different URL (|url2|) that fails to load. It
  // adds a new entry.
  GURL url2 = embedded_test_server()->GetURL("/title1.html");
  {
    // Set up an URLLoaderInterceptor which will cause the next navigation to
    // fail.
    auto url_loader_interceptor = std::make_unique<URLLoaderInterceptor>(
        base::BindRepeating([](URLLoaderInterceptor::RequestParams* params) {
          network::URLLoaderCompletionStatus status;
          status.error_code = net::ERR_NOT_IMPLEMENTED;
          params->client->OnComplete(status);
          return true;
        }));

    FrameNavigateParamsCapturer capturer(root->child_at(0));
    NavigateFrameToURL(root->child_at(0), url2);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  }

  // 4) Go back to the previous error page on the subframe. Since this still
  // results in an error page, it's classified as AUTO_SUBFRAME and won't do
  // replacement, like a normal subframe history navigation.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    controller.GoBack();
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  }

  // 5) Go forward to |url2|. This navigation will succeed, and since the final
  // SiteInstance is the same as the entry, it will be classified as
  // AUTO_SUBFRAME and won't do replacement, like a normal subframe history
  // navigation. Note this is different from the main frame case, where the
  // navigation will do a replacement instead, because the SiteInstance would be
  // different due to error page isolation on main frames.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    controller.GoForward();
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  }

  // 6) Navigate the subframe to the same URL but fail to load again. It will
  // be classified as AUTO_SUBFRAME and do replacement.
  {
    // Set up an URLLoaderInterceptor which will cause the next navigation to
    // fail.
    auto url_loader_interceptor = std::make_unique<URLLoaderInterceptor>(
        base::BindRepeating([](URLLoaderInterceptor::RequestParams* params) {
          network::URLLoaderCompletionStatus status;
          status.error_code = net::ERR_NOT_IMPLEMENTED;
          params->client->OnComplete(status);
          return true;
        }));

    FrameNavigateParamsCapturer capturer(root->child_at(0));
    NavigateFrameToURL(root->child_at(0), url2);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  }
}

// Various tests for navigation type classifications. TODO(avi): It's rather
// bogus that the same info is in two different enums; http://crbug.com/453555.

// Verify that navigations for NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY are
// correctly classified.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       NavigationTypeClassification_NewEntry) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  {
    // Simple load.
    FrameNavigateParamsCapturer capturer(root);
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/page_with_links.html"));
    NavigateFrameToURL(root, frame_url);
    capturer.Wait();
    // TODO(avi,creis): Why is this (and quite a few others below) a "link"
    // transition? Lots of these transitions should be cleaned up.
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());
  }

  NavigationEntryImpl* previous_entry = controller.GetLastCommittedEntry();
  scoped_refptr<FrameNavigationEntry> previous_root_entry =
      previous_entry->root_node()->frame_entry.get();

  {
    // Load via a fragment link click.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "document.getElementById('fraglink').click()";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());

    // The FrameNavigationEntry and NavigationEntry are not reused.
    EXPECT_NE(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
    previous_entry = controller.GetLastCommittedEntry();
    previous_root_entry = previous_entry->root_node()->frame_entry.get();
  }

  {
    // Load via link click.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "document.getElementById('thelink').click()";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());

    // The FrameNavigationEntry and NavigationEntry are not reused.
    EXPECT_NE(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
    previous_entry = controller.GetLastCommittedEntry();
    previous_root_entry = previous_entry->root_node()->frame_entry.get();
  }

  {
    // location.assign().
    FrameNavigateParamsCapturer capturer(root);
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    std::string script = JsReplace("location.assign($1);", frame_url);
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());

    // The FrameNavigationEntry and NavigationEntry are not reused.
    EXPECT_NE(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
    previous_entry = controller.GetLastCommittedEntry();
    previous_root_entry = previous_entry->root_node()->frame_entry.get();
  }

  {
    // history.pushState().
    FrameNavigateParamsCapturer capturer(root);
    std::string script =
        "history.pushState({}, 'page 1', 'simple_page_1.html')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());

    // The FrameNavigationEntry and NavigationEntry are not reused.
    EXPECT_NE(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
    previous_entry = controller.GetLastCommittedEntry();
    previous_root_entry = previous_entry->root_node()->frame_entry.get();
  }

  // location.replace().
  {
    FrameNavigateParamsCapturer capturer(root);
    GURL frame_url(embedded_test_server()->GetURL(
        "foo.com", "/navigation_controller/simple_page_1.html"));
    std::string script = JsReplace("location.replace($1);", frame_url);
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_FALSE(capturer.is_same_document());

    // The FrameNavigationEntry and NavigationEntry are not reused.
    EXPECT_NE(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
  }
}

// Verify that navigations for NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY are
// correctly classified.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       NavigationTypeClassification_ExistingEntry) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  {
    // Back from the browser side.
    FrameNavigateParamsCapturer capturer(root);
    controller.GoBack();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());
  }

  {
    // Forward from the browser side.
    FrameNavigateParamsCapturer capturer(root);
    controller.GoForward();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());
  }

  {
    // Back from the renderer side.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.back()"));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());
  }

  {
    // Forward from the renderer side.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.forward()"));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());
  }

  {
    // Back from the renderer side via history.go().
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.go(-1)"));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());
  }

  {
    // Forward from the renderer side via history.go().
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.go(1)"));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());
  }

  // Replace history.state to "foo".
  ReplaceState(root, "foo");
  EXPECT_EQ("foo", EvalJs(root, "history.state"));
  NavigationEntryImpl* previous_entry = controller.GetLastCommittedEntry();

  {
    // Reload the tab from the browser side.
    FrameNavigateParamsCapturer capturer(root);
    controller.Reload(ReloadType::NORMAL, false);
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_RELOAD));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());

    // We reused the last committed entry for this navigation.
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(root, "history.state"));

    previous_entry = controller.GetLastCommittedEntry();
  }

  {
    // Reload the frame from the browser side.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetPrimaryMainFrame()->Reload();
    capturer.Wait();
    // We're classifying this as EXISTING_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());

    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_RELOAD));

    // We reused the last committed entry for this navigation.
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(root, "history.state"));

    previous_entry = controller.GetLastCommittedEntry();
  }

  {
    // Reload from the renderer side.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "location.reload()"));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());

    // We reused the last committed entry for this navigation.
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(root, "history.state"));

    previous_entry = controller.GetLastCommittedEntry();
  }

  // Set up an URLLoaderInterceptor which will cause all navigations to fail.
  auto url_loader_interceptor = std::make_unique<URLLoaderInterceptor>(
      base::BindRepeating([](URLLoaderInterceptor::RequestParams* params) {
        network::URLLoaderCompletionStatus status;
        status.error_code = net::ERR_NOT_IMPLEMENTED;
        params->client->OnComplete(status);
        return true;
      }));

  // Reload the tab (browser-initiated), but this time we hit a network error
  // and end up in an error page.
  {
    TestNavigationObserver reload_observer(shell()->web_contents());
    FrameNavigateParamsCapturer capturer(root);
    shell()->Reload();
    capturer.Wait();
    EXPECT_FALSE(reload_observer.last_navigation_succeeded());

    // We're classifying this as EXISTING_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_RELOAD));

    // We reused the last committed entry for this navigation.
    // TODO(https://crbug.com/1188956): This should replace the last committed
    // entry instead.
    EXPECT_FALSE(capturer.did_replace_entry());
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(previous_entry, entry);
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    previous_entry = entry;
  }

  // Reload the tab and hit the same network error again.
  {
    TestNavigationObserver reload_observer(shell()->web_contents());
    FrameNavigateParamsCapturer capturer(root);
    shell()->Reload();
    capturer.Wait();
    EXPECT_FALSE(reload_observer.last_navigation_succeeded());

    // We're classifying this as EXISTING_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_RELOAD));

    // We reused the last committed entry for this navigation.
    // TODO(https://crbug.com/1188956): This should replace the last committed
    // entry instead.
    EXPECT_FALSE(capturer.did_replace_entry());
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(previous_entry, entry);
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    previous_entry = entry;
  }

  // Reset the  URLLoaderInterceptor so future navigations will succeed.
  url_loader_interceptor.reset();

  {
    // Reload the tab successfully after a failed navigation.
    TestNavigationObserver reload_observer(shell()->web_contents());
    FrameNavigateParamsCapturer capturer(root);
    shell()->Reload();
    capturer.Wait();
    EXPECT_TRUE(reload_observer.last_navigation_succeeded());

    // We're classifying this as NEW_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_RELOAD));

    // We replaced the last committed entry with a new entry for this
    // navigation.
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());

    // We lost the history.state value from before the failed navigation.
    EXPECT_EQ(nullptr, EvalJs(root, "history.state"));
    previous_entry = controller.GetLastCommittedEntry();
  }

  {
    // location.replace().
    FrameNavigateParamsCapturer capturer(root);
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    std::string script = JsReplace("location.replace($1);", frame_url);
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_FALSE(capturer.is_same_document());

    // We replaced the last committed entry with a new entry for this
    // navigation.
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());

    previous_entry = controller.GetLastCommittedEntry();
  }

  // Now, various same document navigations.

  scoped_refptr<FrameNavigationEntry> previous_root_entry =
      previous_entry->root_node()->frame_entry.get();

  {
    // Same-document location.replace().
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "location.replace('#foo')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    // Different from cross-document replacement which would be classified as
    // NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, same-document replacement is
    // classified as NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_TRUE(capturer.is_same_document());

    // The FrameNavigationEntry and NavigationEntry are reused.
    EXPECT_EQ(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    previous_entry = controller.GetLastCommittedEntry();
    previous_root_entry = previous_entry->root_node()->frame_entry.get();
  }

  {
    // history.replaceState().
    FrameNavigateParamsCapturer capturer(root);
    std::string script =
        "history.replaceState({}, 'page 2', 'simple_page_2.html')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    // Different from history.pushState() which would be classified as
    // NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, replaceState() will be classified
    // as NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_TRUE(capturer.is_same_document());

    // The FrameNavigationEntry and NavigationEntry are reused.
    EXPECT_EQ(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());

    previous_entry = controller.GetLastCommittedEntry();
  }

  // Back and forward across a fragment navigation.

  GURL url_links(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_links));
  std::string script = "document.getElementById('fraglink').click()";
  EXPECT_TRUE(ExecJs(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  {
    // Back.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
  }

  {
    // Forward.
    FrameNavigateParamsCapturer capturer(root);
    controller.GoForward();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_FORWARD_BACK)));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
  }

  // Back and forward across a pushState-created navigation.

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  script = "history.pushState({}, 'page 2', 'simple_page_2.html')";
  EXPECT_TRUE(ExecJs(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  {
    // Back.
    FrameNavigateParamsCapturer capturer(root);
    controller.GoBack();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
  }

  {
    // Forward.
    FrameNavigateParamsCapturer capturer(root);
    controller.GoForward();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_FORWARD_BACK)));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
  }
}

// Verify that navigations to the same URL are correctly classified as
// EXISTING_ENTRY (if it becomes a reload) or NEW_ENTRY (if it becomes a
// replacement navigation).
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       NavigationTypeClassification_ExistingEntrySameURL) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(1, controller.GetEntryCount());
  // Replace history.state to "foo".
  ReplaceState(root, "foo");
  EXPECT_EQ("foo", EvalJs(root, "history.state"));

  NavigationEntryImpl* previous_entry = controller.GetLastCommittedEntry();

  {
    // Navigate to the same URL (browser-initiated).
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url1));
    capturer.Wait();
    // The navigation got converted into a reload, and we're classifying this as
    // EXISTING_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());

    // Ensure the pending entry was cleared after commit.
    EXPECT_FALSE(shell()->web_contents()->GetController().GetPendingEntry());

    // We reuse the last committed entry for this navigation.
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(1, controller.GetEntryCount());

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(root, "history.state"));

    previous_entry = controller.GetLastCommittedEntry();
  }

  {
    // Navigate to the same URL (renderer-initiated).
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(NavigateToURLFromRenderer(root, url1));
    capturer.Wait();
    // We're classifying this as NEW_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());

    // The navigation replaced the previously committed entry with a new entry.
    // This differs than the browser-initiated case's behavior, but it's OK.
    // The renderer-initiated navigation follows the spec  at
    // https://html.spec.whatwg.org/C/#navigating-across-documents:hh-replace-3,
    // while the browser-initiated version got converted into a reload.
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(1, controller.GetEntryCount());

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(root, "history.state"));

    previous_entry = controller.GetLastCommittedEntry();
  }

  {
    //  Navigate to the same URL (renderer-initiated) with location.replace.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, JsReplace("location.replace($1);", url1)));
    capturer.Wait();
    // We're classifying this as NEW_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());

    // The navigation replaced the previously committed entry with a new entry.
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(1, controller.GetEntryCount());

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(root, "history.state"));

    previous_entry = controller.GetLastCommittedEntry();
  }

  auto url_loader_interceptor = std::make_unique<URLLoaderInterceptor>(
      base::BindRepeating([](URLLoaderInterceptor::RequestParams* params) {
        network::URLLoaderCompletionStatus status;
        status.error_code = net::ERR_NOT_IMPLEMENTED;
        params->client->OnComplete(status);
        return true;
      }));

  {
    // Navigate to the same URL (renderer-initiated), but this time we hit a
    // network error and end up in an error page. This will result in
    // replacement as it's a same-url navigation.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_FALSE(NavigateToURLFromRenderer(shell(), url1));
    capturer.Wait();
    // We're classifying this as NEW_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());

    // The navigation replaced the previously committed entry.
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(1, controller.GetEntryCount());

    previous_entry = controller.GetLastCommittedEntry();
  }

  {
    // Navigate to the same URL (through explicit reload), and hit a network
    // error again. This will reload the previous entry.
    FrameNavigateParamsCapturer capturer(root);
    shell()->Reload();
    capturer.Wait();
    // We're classifying this as EXISTING_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());

    // The navigation reused the previously committed error page entry.
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(1, controller.GetEntryCount());

    previous_entry = controller.GetLastCommittedEntry();
  }

  {
    // Navigate to the same URL (browser-initiated), and hit a network error
    // again. The navigation initially got converted into a reload by the
    // browser but since it failed and doesn't have a valid PageState, it does
    // replacement instead.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_FALSE(NavigateToURL(shell(), url1));
    capturer.Wait();
    // We're classifying this as NEW_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());

    // The navigation replaced the previously committed entry.
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(1, controller.GetEntryCount());

    previous_entry = controller.GetLastCommittedEntry();
  }
  // Stop failing future navigations.
  url_loader_interceptor.reset();

  {
    // Navigate successfully to the same URL (browser-initiated) after a failed
    // navigation.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url1));
    capturer.Wait();
    // We're classifying this as NEW_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());

    // The navigation added a new entry.
    // TODO(https://crbug.com/1188956): This should replace the last committed
    // entry instead.
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(2, controller.GetEntryCount());

    // We lost the history.state value from before the failed navigation.
    EXPECT_EQ(nullptr, EvalJs(root, "history.state"));
    previous_entry = controller.GetLastCommittedEntry();
  }

  // Replace history.state to "foo".
  ReplaceState(root, "foo");
  EXPECT_EQ("foo", EvalJs(root, "history.state"));
  previous_entry = controller.GetLastCommittedEntry();

  {
    // Navigate to the same URL (browser-initiated) with LoadURLParams, setting
    // should_replace_current_entry to true. The browser should not convert this
    // navigation to do a reload, and should continue to do replacement.
    FrameNavigateParamsCapturer capturer(root);
    NavigationController::LoadURLParams params(url1);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    params.should_replace_current_entry = true;
    contents()->GetController().LoadURLWithParams(params);
    capturer.Wait();
    // We're classifying this as NEW_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());

    // The navigation replaced the previously committed entry with a new entry.
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(2, controller.GetEntryCount());

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(root, "history.state"));
    previous_entry = controller.GetLastCommittedEntry();
  }

  {
    // Navigate to the same URL (browser-initiated) with LoadURLParams, setting
    // should_replace_current_entry to true, but hit a network error. The
    // browser will convert this navigation to do a reload, but ended up doing
    // replacement instead.
    url_loader_interceptor = std::make_unique<URLLoaderInterceptor>(
        base::BindRepeating([](URLLoaderInterceptor::RequestParams* params) {
          network::URLLoaderCompletionStatus status;
          status.error_code = net::ERR_NOT_IMPLEMENTED;
          params->client->OnComplete(status);
          return true;
        }));

    FrameNavigateParamsCapturer capturer(root);
    NavigationController::LoadURLParams params(url1);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    params.should_replace_current_entry = true;
    contents()->GetController().LoadURLWithParams(params);
    capturer.Wait();

    // We're classifying this as NEW_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());

    // The navigation replaced the previously committed entry with a new entry
    // because the navigation resulted in an error page.
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(2, controller.GetEntryCount());

    url_loader_interceptor.reset();
  }
}

// Verify that navigations to the same WebUI URL are correctly classified as
// EXISTING_ENTRY (it becomes a reload).
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    NavigationTypeClassification_ExistingEntrySameURL_WebUI) {
  // Navigate to a WebUI page.
  GURL web_ui_url(std::string(kChromeUIScheme) + "://" +
                  std::string(kChromeUIGpuHost));
  EXPECT_TRUE(NavigateToURL(shell(), web_ui_url));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(1, controller.GetEntryCount());

  NavigationEntryImpl* previous_entry = controller.GetLastCommittedEntry();

  {
    // Navigate to the same Web UI URL (browser-initiated).
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), web_ui_url));
    capturer.Wait();
    // The navigation got converted into a reload, and we're classifying this as
    // EXISTING_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());

    // Ensure the pending entry was cleared after commit.
    EXPECT_FALSE(shell()->web_contents()->GetController().GetPendingEntry());

    // We reuse the last committed entry for this navigation.
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(1, controller.GetEntryCount());

    previous_entry = controller.GetLastCommittedEntry();
  }

  {
    // Navigate to the same WebUI URL (renderer-initiated). This will go through
    // the "browser-initiated" path in the browser (OpenURL instead of
    // BeginNavigation). This would behave like a normal renderer-initiated
    // navigation if https://crbug.com/883549 were resolved.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(contents(), JsReplace("location.href = $1", web_ui_url),
                       EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
    capturer.Wait();
    // The navigation got converted into a reload, and we're classifying this as
    // EXISTING_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());

    // We reuse the last committed entry for this navigation.
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(1, controller.GetEntryCount());
  }
}

// Verify that navigations to the same URL are correctly classified as
// EXISTING_ENTRY even after redirects.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    NavigationTypeClassification_ExistingEntrySameURLRedirect) {
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  GURL redirect_to_url2(
      embedded_test_server()->GetURL("/server-redirect?" + url2.spec()));
  // Navigate to |url_1|.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(1, controller.GetEntryCount());
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents())->GetPrimaryFrameTree().root();

  // Change the current URL to |redirect_to_url2| with replaceState, which
  // will stay in the same document and won't trigger an actual load to
  // |redirect_to_url2|.
  ReplaceState(root, "", redirect_to_url2);
  NavigationEntryImpl* previous_entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(redirect_to_url2, previous_entry->GetURL());

  {
    // 1) Do a browser-initiated navigation to |redirect_to_url2|.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), redirect_to_url2, url2));
    capturer.Wait();

    // Since it started out as a browser-initiated same-URL navigation, it got
    // converted to a reload and classified as
    // NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY and reuses the previous entry,
    // even though it ended up at a different URL than before.
    // TODO(https://crbug.com/1247185): Perhaps this should be classified as
    // NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY with replacement instead, to be
    // consistent with reloads that redirected cross-site.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_FALSE(capturer.did_replace_entry());

    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(url2, controller.GetLastCommittedEntry()->GetURL());
    EXPECT_EQ(1, controller.GetEntryCount());
  }

  // Change the current URL to |redirect_to_url2| with replaceState again.
  ReplaceState(root, "", redirect_to_url2);
  previous_entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(redirect_to_url2, previous_entry->GetURL());
  EXPECT_EQ(1, controller.GetEntryCount());

  {
    // 2) Do a renderer-initiated navigation to |redirect_to_url2|.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(NavigateToURLFromRenderer(root, redirect_to_url2, url2));
    capturer.Wait();

    // Since it started out as a renderer-initiated same-URL navigation, it got
    // converted to a replacement navigation, so it's classified as
    // NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());

    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(url2, controller.GetLastCommittedEntry()->GetURL());
    EXPECT_EQ(1, controller.GetEntryCount());
  }

  // Change the current URL to |redirect_to_url2| with replaceState again.
  ReplaceState(root, "", redirect_to_url2);
  previous_entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(redirect_to_url2, previous_entry->GetURL());
  EXPECT_EQ(1, controller.GetEntryCount());

  {
    // 3) Do a form submission to |redirect_to_url2|.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(contents(),
                       JsReplace(R"(var form = document.createElement('form');
                                 form.method = 'POST';
                                 form.action = $1;
                                 document.body.appendChild(form);
                                 form.submit();)",
                                 redirect_to_url2)));
    capturer.Wait();

    // It started out as a same-URL navigation, but since it was a POST
    // navigation, it didn't get converted to replacement.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_FALSE(capturer.did_replace_entry());

    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(url2, controller.GetLastCommittedEntry()->GetURL());
    EXPECT_EQ(2, controller.GetEntryCount());

    // The POST data should not be saved in the final NavigationEntry.
    EXPECT_FALSE(controller.GetLastCommittedEntry()->GetHasPostData());
  }
}

// Verify that navigations to the same URL that has a fragment part (#foo) are
// correctly classified as EXISTING_ENTRY.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    NavigationTypeClassification_ExistingEntrySameURLWithFragment) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html#foo"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  // Replace history.state to "foo".
  ReplaceState(root, "foo");
  EXPECT_EQ("foo", EvalJs(root, "history.state"));

  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* previous_entry = controller.GetLastCommittedEntry();

  {
    // Navigate to the same URL (browser-initiated).
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url1));
    capturer.Wait();
    // We're classifying this as EXISTING_ENTRY because the navigation got
    // converted into a reload.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());

    // Since we did a reload, it's not classified as a same-document navigation.
    EXPECT_FALSE(capturer.is_same_document());

    // We reuse the last committed entry for this navigation.
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(1, controller.GetEntryCount());

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(root, "history.state"));

    previous_entry = controller.GetLastCommittedEntry();
  }

  {
    // Navigate to the same URL (renderer-initiated).
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(NavigateToURLFromRenderer(root, url1));
    capturer.Wait();

    // We did a same-document navigation.
    EXPECT_TRUE(capturer.is_same_document());

    // We're reusing the previous entry and classifying this as EXISTING_ENTRY
    // with replacement.
    // TODO(rakina): did_replace_entry should be false since we're not actually
    // doing any replacement.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(1, controller.GetEntryCount());

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(root, "history.state"));
  }
}

// Verify that reloading a page with url anchor scrolls to correct position.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, ReloadWithUrlAnchor) {
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/reload-with-url-anchor.html#center-element"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  double window_scroll_y = EvalJs(shell(), "window.scrollY").ExtractDouble();

  // TODO(bokan): The floor hack below can go
  // away once FractionalScrolLOffsets ships. The reason it's required is that,
  // at certain device scale factors, the given CSS pixel scroll value may land
  // between physical pixels. Without the feature Blink will truncate to the
  // nearest physical pixel so the expectation must account for that. When the
  // feature is enabled, Blink stores the fractional offset so the truncation
  // is unnecessary. https://crbug.com/414283.
  bool fractional_scroll_offsets_enabled = IsFractionalScrollOffsetsEnabled();

  // The 'center-element' y-position is 2000px. 2000px is an arbitrary value.
  double expected_window_scroll_y = 2000;
  if (!fractional_scroll_offsets_enabled) {
    float device_scale_factor = shell()
                                    ->web_contents()
                                    ->GetRenderWidgetHostView()
                                    ->GetDeviceScaleFactor();
    expected_window_scroll_y =
        floor(device_scale_factor * expected_window_scroll_y) /
        device_scale_factor;
  }
  EXPECT_FLOAT_EQ(expected_window_scroll_y, window_scroll_y);

  // Reload.
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  window_scroll_y = EvalJs(shell(), "window.scrollY").ExtractDouble();
  EXPECT_FLOAT_EQ(expected_window_scroll_y, window_scroll_y);
}

// Verify that reloading a page with url anchor and scroll scrolls to correct
// position.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ReloadWithUrlAnchorAndScroll) {
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/reload-with-url-anchor.html#center-element"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // The 'center-element' y-position is 2000px. This script scrolls the view
  // 100px below this element. 2000px and 100px are arbitrary values.
  std::string script_scroll_down = "window.scroll(0, 2100)";
  EXPECT_TRUE(ExecJs(shell(), script_scroll_down));

  double window_scroll_y = EvalJs(shell(), "window.scrollY").ExtractDouble();

  // TODO(bokan): The floor hack below can go
  // away once FractionalScrolLOffsets ships. The reason it's required is that,
  // at certain device scale factors, the given CSS pixel scroll value may land
  // between physical pixels. Without the feature Blink will truncate to the
  // nearest physical pixel so the expectation must account for that. When the
  // feature is enabled, Blink stores the fractional offset so the truncation
  // is unnecessary. https://crbug.com/414283.
  bool fractional_scroll_offsets_enabled = IsFractionalScrollOffsetsEnabled();

  double expected_window_scroll_y = 2100;
  if (!fractional_scroll_offsets_enabled) {
    float device_scale_factor = shell()
                                    ->web_contents()
                                    ->GetRenderWidgetHostView()
                                    ->GetDeviceScaleFactor();
    expected_window_scroll_y =
        floor(device_scale_factor * expected_window_scroll_y) /
        device_scale_factor;
  }
  EXPECT_FLOAT_EQ(expected_window_scroll_y, window_scroll_y);

  // Reload.
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  window_scroll_y = EvalJs(shell(), "window.scrollY").ExtractDouble();
  EXPECT_FLOAT_EQ(expected_window_scroll_y, window_scroll_y);
}

// Verify that empty GURL navigations are not classified as EXISTING_ENTRY.
// See https://crbug.com/534980.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       NavigationTypeClassification_EmptyGURL) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(1, controller.GetEntryCount());
  {
    // Load an (invalid) empty GURL.  Blink will treat this as an inert commit,
    // but we don't want it to show up as EXISTING_ENTRY.
    FrameNavigateParamsCapturer capturer(root);
    NavigateFrameToURL(root, GURL());
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());

    // The navigation should add a new entry to the session history, and not
    // do any entry replacement.
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(2, controller.GetEntryCount());
  }
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       RendererInitiatedNavigationToEmptyGURLFails) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Trying to navigate to an empty URL from the renderer will fail.
  EXPECT_FALSE(NavigateToURLFromRenderer(root, GURL()));
}

// Verify that navigations for NAVIGATION_TYPE_NEW_SUBFRAME and
// NAVIGATION_TYPE_AUTO_SUBFRAME are properly classified.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       NavigationTypeClassification_NewAndAutoSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  {
    // Initial load.
    LoadCommittedCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  {
    // Simple load.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
  }

  {
    // Back.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
  }

  {
    // Forward.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
  }

  {
    // Simple load.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/page_with_links.html"));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
  }

  {
    // Load via a fragment link click.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    std::string script = "document.getElementById('fraglink').click()";
    EXPECT_TRUE(ExecJs(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
  }

  {
    // location.assign().
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    std::string script = JsReplace("location.assign($1);", frame_url);
    EXPECT_TRUE(ExecJs(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
  }

  {
    // location.replace().
    LoadCommittedCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    std::string script = JsReplace("location.replace($1);", frame_url);
    EXPECT_TRUE(ExecJs(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  {
    // history.pushState().
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    std::string script =
        "history.pushState({}, 'page 1', 'simple_page_1.html')";
    EXPECT_TRUE(ExecJs(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
  }

  {
    // history.replaceState().
    LoadCommittedCapturer capturer(root->child_at(0));
    std::string script =
        "history.replaceState({}, 'page 2', 'simple_page_2.html')";
    EXPECT_TRUE(ExecJs(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  {
    // Reload.
    LoadCommittedCapturer capturer(root->child_at(0));
    EXPECT_TRUE(ExecJs(root->child_at(0), "location.reload()"));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  {
    // Create an iframe.
    LoadCommittedCapturer capturer(shell()->web_contents());
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, frame_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }
}

// Captures LoadCommittedDetails to compare against expectations.
class LoadCommittedDetailsObserver : public WebContentsObserver {
 public:
  explicit LoadCommittedDetailsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void Wait() { loop_.Run(); }

  const LoadCommittedDetails& load_details() { return load_details_; }

 private:
  void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override {
    load_details_ = load_details;
    loop_.Quit();
  }

  LoadCommittedDetails load_details_;

  base::RunLoop loop_;
};

// Tests for navigations that happen after initial empty document loads on an
// iframe/opened window. This class is parameterized by both RenderDocumentHost
// mode and by whether it would do renderer vs browser initiated navigations.
class InitialEmptyDocNavigationControllerBrowserTest
    : public NavigationControllerBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<std::string, bool /* renderer_initiated */>> {
 public:
  InitialEmptyDocNavigationControllerBrowserTest() {
    InitAndEnableRenderDocumentFeature(&feature_list_for_render_document_,
                                       std::get<0>(GetParam()));
  }

  // Provides meaningful param names instead of /0, /1, ...
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto [render_document_level, renderer_initiated] = info.param;
    return base::StringPrintf(
        "%s_%s",
        GetRenderDocumentLevelNameForTestParams(render_document_level).c_str(),
        renderer_initiated ? "RendererInitiated" : "BrowserInitiated");
  }

 protected:
  bool renderer_initiated() { return std::get<1>(GetParam()); }

  // Navigates |node| to |url| then checks if its navigation type is
  // |navigation_type| and whether other related properties are consistent with
  // the type. Whether the navigation is renderer-initiated or not depends on
  // the renderer vs browser initiated parameter of this test class.
  void NavigateSubframeAndCheckNavigationType(WebContentsImpl* web_contents,
                                              FrameTreeNode* node,
                                              std::string frame_id,
                                              const GURL& url,
                                              NavigationType expected_type) {
    DCHECK(!node->IsMainFrame());
    FrameNavigateParamsCapturer capturer(node);
    if (renderer_initiated()) {
      EXPECT_TRUE(NavigateIframeToURL(web_contents, frame_id, url));
    } else {
      EXPECT_TRUE(NavigateFrameToURL(node, url));
    }
    capturer.Wait();

    EXPECT_EQ(expected_type, capturer.navigation_type());

    if (expected_type == NAVIGATION_TYPE_AUTO_SUBFRAME) {
      EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
          capturer.transition(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
      // |did_replace_entry| is true because the history item in the renderer
      // replaced the initial empty document.
      EXPECT_TRUE(capturer.did_replace_entry());

    } else {
      EXPECT_EQ(expected_type, NAVIGATION_TYPE_NEW_SUBFRAME);
      EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
          capturer.transition(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
      EXPECT_FALSE(capturer.did_replace_entry());
    }
  }

  // Navigates |web_contents| to |url| then check its navigation type and
  // whether other related properties are consistent with the type. Whether the
  // navigation is renderer-initiated or not depends on the renderer vs browser
  // initiated parameter of this test class.
  void NavigateWindowAndCheck(WebContentsImpl* web_contents,
                              const GURL& url,
                              bool wait_for_previous_navigations = true,
                              bool expect_same_document = false,
                              NavigationType expected_navigation_type =
                                  NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY,
                              bool expect_replace = true) {
    FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
    LoadCommittedDetailsObserver load_details_observer(web_contents);
    FrameNavigateParamsCapturer capturer(root);
    if (renderer_initiated()) {
      EXPECT_TRUE(NavigateToURLFromRenderer(web_contents, url));
    } else {
      // Do a browser-initiated navigation. In cases where there's a previous
      // navigation that hasn't finished and won't finish (e.g. navigations to
      // /hung), we can't use NavigateToURL(), because it will wait for the
      // previous navigation to finish first. So, use LoadURLWithParams()
      // directly in those cases.
      if (!wait_for_previous_navigations) {
        NavigationController::LoadURLParams params(url);
        params.transition_type = ui::PageTransitionFromInt(
            ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
        web_contents->GetController().LoadURLWithParams(params);
      } else {
        // Otherwise, just use NavigateToURL().
        EXPECT_TRUE(NavigateToURL(web_contents, url));
      }
    }
    capturer.Wait();
    load_details_observer.Wait();

    EXPECT_EQ(expected_navigation_type, capturer.navigation_type());
    EXPECT_EQ(expect_replace, capturer.did_replace_entry());

    // Check both NavigationHandle and LoadCommittedDetails for whether this was
    // considered same-document, as these have diverged in the past.
    // See https://crbug.com/1193134.
    EXPECT_EQ(expect_same_document, capturer.is_same_document());
    EXPECT_EQ(expect_same_document,
              load_details_observer.load_details().is_same_document);
  }

 private:
  base::test::ScopedFeatureList feature_list_for_render_document_;
};

// Test various navigation cases on newly-created subframes that have only
// loaded the initial empty document (but might have done other navigations that
// stay in the initial empty document), to see if the initial empty documents
// get replaced/not replaced.
// TODO(https://crbug.com/1215096): Most of these cases are not actually running
// on the initial empty document because they have committed the synchronous
// non-initial about:blank document. Update these tests or remove the
// synchronous navigation entirely.
IN_PROC_BROWSER_TEST_P(InitialEmptyDocNavigationControllerBrowserTest,
                       NavigateNewSubframe) {
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  GURL no_commit_url(embedded_test_server()->GetURL("/page204.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  EXPECT_EQ(1, controller.GetEntryCount());

  int subframe_index = 0;
  int expected_entry_count = 1;

  // 1) Navigate to |url_2| on a new subframe that hasn't done any navigation.
  {
    SCOPED_TRACE(testing::Message() << " Testing case 1.");

    // Create the "child1" subframe without navigating it.
    CreateSubframe(contents(), "child1", GURL(),
                   false /* wait_for_navigation */);

    // The new subframe should be on the initial empty document.
    FrameTreeNode* new_subframe = root->child_at(subframe_index);
    EXPECT_TRUE(new_subframe->is_on_initial_empty_document());

    // Do a navigation on the "child1" subframe to |url_2|.
    // The navigation is still classified as "auto", so we didn't append a new
    // NavigationEntry, and instead updated the current NavigationEntry.
    NavigateSubframeAndCheckNavigationType(contents(), new_subframe, "child1",
                                           url_2,
                                           NAVIGATION_TYPE_AUTO_SUBFRAME);
    EXPECT_EQ(expected_entry_count, controller.GetEntryCount());
  }

  // 2) Navigate to |url_2| on a new subframe that has done a navigation to
  // about:blank and a same-document navigation to about:blank#foo.
  {
    SCOPED_TRACE(testing::Message() << " Testing case 2.");

    // Create the "child2" subframe with src set to about:blank, navigating it
    // there.
    CreateSubframe(contents(), "child2", GURL("about:blank"),
                   true /* wait_for_navigation */);
    subframe_index++;
    EXPECT_EQ(expected_entry_count, controller.GetEntryCount());

    // The new subframe should be on the initial empty document.
    FrameTreeNode* new_subframe = root->child_at(subframe_index);
    EXPECT_TRUE(new_subframe->is_on_initial_empty_document());

    // Do a navigation on the "child2" subframe to about:blank#foo, creating a
    // same-document navigation. The navigation will do a replacement and get
    // classified as "auto", so we won't append a new NavigationEntry, and
    // instead update the current NavigationEntry.
    NavigateSubframeAndCheckNavigationType(contents(), new_subframe, "child2",
                                           GURL("about:blank#foo"),
                                           NAVIGATION_TYPE_AUTO_SUBFRAME);
    EXPECT_EQ(expected_entry_count, controller.GetEntryCount());

    // The subframe should still be on the initial empty document.
    EXPECT_TRUE(new_subframe->is_on_initial_empty_document());

    // Do a navigation on the "child2" subframe to |url_2|.
    NavigateSubframeAndCheckNavigationType(contents(), new_subframe, "child2",
                                           url_2,
                                           NAVIGATION_TYPE_AUTO_SUBFRAME);
    // The navigation is still classified as "auto", so we didn't append a new
    // NavigationEntry, and instead updated the current NavigationEntry.
    EXPECT_EQ(expected_entry_count, controller.GetEntryCount());

    // The subframe should no longer be on the initial empty document.
    EXPECT_FALSE(new_subframe->is_on_initial_empty_document());
  }

  // 3) Navigate to |url_2| on a new subframe that has done a navigation to a
  // data: URL.
  {
    SCOPED_TRACE(testing::Message() << " Testing case 3.");

    // Create the "child3" subframe with src set to a data: URL, navigating it
    // there.
    CreateSubframe(contents(), "child3", GURL("data:text/html,foo"),
                   true /* wait_for_navigation */);
    subframe_index++;
    EXPECT_EQ(expected_entry_count, controller.GetEntryCount());

    // The new subframe should not be on the initial empty document.
    FrameTreeNode* new_subframe = root->child_at(subframe_index);
    EXPECT_FALSE(new_subframe->is_on_initial_empty_document());

    // Do a navigation on the "child3" subframe to |url_2|.
    // The navigation is classified as a new navigation, and appended a new
    // NavigationEntry.
    NavigateSubframeAndCheckNavigationType(contents(), new_subframe, "child3",
                                           url_2, NAVIGATION_TYPE_NEW_SUBFRAME);
    expected_entry_count++;
    EXPECT_EQ(expected_entry_count, controller.GetEntryCount());
  }

  // 4) Navigate to |url_2| on a new subframe that has started a navigation to
  // a URL that never committed.
  // TODO(https://crbug.com/1311616): The browser-initiated case is flaky so we
  // only test this for the renderer-initiated case. Figure out how to test the
  // browser-initiated case in a non-flaky way.
  if (renderer_initiated()) {
    SCOPED_TRACE(testing::Message() << " Testing case 4.");

    // Create the "child4" subframe with src set to a URL that never commits.
    CreateSubframe(contents(), "child4", no_commit_url,
                   false /* wait_for_navigation */);
    subframe_index++;
    EXPECT_EQ(expected_entry_count, controller.GetEntryCount());

    // The new subframe should be on the initial empty document.
    FrameTreeNode* new_subframe = root->child_at(subframe_index);
    EXPECT_TRUE(new_subframe->is_on_initial_empty_document());

    // Do a navigation on the "child4" subframe to |url_2|.
    // The navigation is still classified as "auto", so we didn't append a new
    // NavigationEntry, and instead updated the current NavigationEntry.
    NavigateSubframeAndCheckNavigationType(contents(), new_subframe, "child4",
                                           url_2,
                                           NAVIGATION_TYPE_AUTO_SUBFRAME);
    EXPECT_EQ(expected_entry_count, controller.GetEntryCount());

    // The subframe should no longer be on the initial empty document.
    EXPECT_FALSE(new_subframe->is_on_initial_empty_document());
  }

  // 5) Navigate to |url_2| on a new subframe that has done a document.open().
  {
    SCOPED_TRACE(testing::Message() << " Testing case 5.");

    // Create the "child5" subframe.
    CreateSubframe(contents(), "child5", GURL(),
                   false /* wait_for_navigation */);
    subframe_index++;
    EXPECT_EQ(expected_entry_count, controller.GetEntryCount());

    // The new subframe should be on the initial empty document.
    FrameTreeNode* new_subframe = root->child_at(subframe_index);
    EXPECT_EQ(GURL("about:blank"), new_subframe->current_url());
    EXPECT_TRUE(new_subframe->is_on_initial_empty_document());

    // Do a document.open() on it.
    EXPECT_TRUE(ExecJs(shell(), R"(
          var iframeDoc = document.getElementById("child5").contentDocument;
          iframeDoc.open();
          iframeDoc.write("foo");
          iframeDoc.close();
      )"));

    // Ensure the URL update from the document.open() above has finished before
    // continuing by waiting for a renderer round-trip to run this script.
    EXPECT_TRUE(ExecJs(new_subframe, "true"));

    // The document.open() changed the iframe's URL in the renderer to be the
    // same as the main frame's URL, but doesn't actually commit a navigation.
    EXPECT_EQ(GURL("about:blank"),
              new_subframe->current_frame_host()->GetLastCommittedURL());
    EXPECT_EQ(
        url_1,
        new_subframe->current_frame_host()->last_document_url_in_renderer());
    // The frame lost its "initial empty document" status.
    EXPECT_FALSE(new_subframe->is_on_initial_empty_document());

    // Do a navigation on the "child5" subframe to |url_2|.
    // The navigation is classified as a new navigation, and appended a new
    // NavigationEntry.
    NavigateSubframeAndCheckNavigationType(contents(), new_subframe, "child5",
                                           url_2, NAVIGATION_TYPE_NEW_SUBFRAME);
    expected_entry_count++;
    EXPECT_EQ(expected_entry_count, controller.GetEntryCount());

    // The frame should still be marked as no longer being on the initial empty
    // document.
    EXPECT_FALSE(new_subframe->is_on_initial_empty_document());
  }

  // 6) Navigate to |url_2| on a new subframe that has done a navigation to
  // a javascript: url that replaces the document.
  {
    SCOPED_TRACE(testing::Message() << " Testing case 6.");

    // Create the "child6" subframe and set it to a javascript: URL.
    RenderFrameHost* child6 =
        CreateSubframe(contents(), "child6", GURL("javascript:'foo'"),
                       false /* wait_for_navigation */);
    subframe_index++;
    EXPECT_EQ(expected_entry_count, controller.GetEntryCount());

    // The new subframe should be on the initial empty document.
    FrameTreeNode* new_subframe = root->child_at(subframe_index);
    EXPECT_TRUE(new_subframe->is_on_initial_empty_document());

    // The subframe that was just created loads javascript which generates
    // DidStopLoading(). RenderFrameHostImpl::DidStopLoading()
    // early outs in this case as `is_loading_` is false. Wait for this before
    // continuing. If we did not wait, then the navigation occurring after
    // this would set `is_loading_` to true, so that the DidStopLoading() for
    // about:blank would be processed, which this test does not expect
    // (NavigateSubframeAndCheckNavigationType() thinks the navigation failed).
    // TODO: ideally the renderer wouldn't send DidStopLoading() in this case.
    base::RunLoop run_loop;
    static_cast<RenderFrameHostImpl*>(child6)
        ->set_did_stop_loading_callback_for_testing(run_loop.QuitClosure());
    run_loop.Run();

    // Do a navigation on the "child6" subframe to |url_2|.
    // The navigation is still classified as "auto", so we didn't append a new
    // NavigationEntry, and instead updated the current NavigationEntry.
    NavigateSubframeAndCheckNavigationType(contents(), new_subframe, "child6",
                                           url_2,
                                           NAVIGATION_TYPE_AUTO_SUBFRAME);
    EXPECT_EQ(expected_entry_count, controller.GetEntryCount());

    // The subframe should no longer be on the initial empty document.
    EXPECT_FALSE(new_subframe->is_on_initial_empty_document());
  }

  // 7) Navigate to a non-initial/synchronous about:blank and then |url_2| on
  // a new subframe.
  {
    SCOPED_TRACE(testing::Message() << " Testing case 7.");

    // Create the "child7" subframe with src set to about:blank, navigating it
    // there.
    CreateSubframe(contents(), "child7", GURL("about:blank"),
                   true /* wait_for_navigation */);
    subframe_index++;
    EXPECT_EQ(expected_entry_count, controller.GetEntryCount());

    // The new subframe should be on the initial empty document.
    FrameTreeNode* new_subframe = root->child_at(subframe_index);
    EXPECT_TRUE(new_subframe->is_on_initial_empty_document());

    // Do a navigation on the "child7" subframe to about:blank?foo, creating a
    // new about:blank document that is neither the initial about:blank nor the
    // synchronously committed about:blank document, so it's not treated as the
    // initial empty document. The navigation will do a replacement and get
    // classified as "auto", so we won't append a new NavigationEntry, and
    // instead update the current NavigationEntry.
    NavigateSubframeAndCheckNavigationType(contents(), new_subframe, "child7",
                                           GURL("about:blank?foo"),
                                           NAVIGATION_TYPE_AUTO_SUBFRAME);
    EXPECT_EQ(expected_entry_count, controller.GetEntryCount());

    // The subframe should no longer be on the the initial empty document.
    EXPECT_FALSE(new_subframe->is_on_initial_empty_document());

    // Do a navigation on the "child7" subframe to |url_2|.
    NavigateSubframeAndCheckNavigationType(contents(), new_subframe, "child7",
                                           url_2, NAVIGATION_TYPE_NEW_SUBFRAME);
    // The navigation is classified as a new navigation, and appended a new
    // NavigationEntry.
    expected_entry_count++;
    EXPECT_EQ(expected_entry_count, controller.GetEntryCount());

    // The subframe is not on the initial empty document.
    EXPECT_FALSE(new_subframe->is_on_initial_empty_document());
  }
}

// Test various navigation cases on newly-created windows that have only loaded
// the initial empty document (but might have done other navigations that stay
// in the initial empty document), to see if the initial empty documents get
// replaced/not replaced.
// TODO(rakina): Most of these cases are not actually running on the initial
// empty document because they have committed the synchronous non-initial
// about:blank document. Update these tests or remove the synchronous
// navigation entirely.
IN_PROC_BROWSER_TEST_P(InitialEmptyDocNavigationControllerBrowserTest,
                       NavigateNewWindow) {
  GURL main_window_url(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  GURL hung_url(embedded_test_server()->GetURL("/hung"));
  EXPECT_TRUE(NavigateToURL(shell(), main_window_url));
  EXPECT_TRUE(ExecJs(contents(), "var last_opened_window = null;"));

  // 1) Navigate to |url_2| on a new window that hasn't done any navigation.
  {
    SCOPED_TRACE(testing::Message() << " Testing case 1.");

    // Create a new blank window.
    Shell* new_shell = OpenBlankWindow(contents());
    WebContentsImpl* new_contents =
        static_cast<WebContentsImpl*>(new_shell->web_contents());
    NavigationControllerImpl& controller = new_contents->GetController();
    EXPECT_TRUE(!controller.GetLastCommittedEntry() ||
                controller.GetLastCommittedEntry()->IsInitialEntry());

    // Navigating the window to |url_2| will be classified as NEW_ENTRY and will
    // replace the previous entry if it exists.
    NavigateWindowAndCheck(new_contents, url_2);
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_FALSE(controller.GetLastCommittedEntry()->IsInitialEntry());
  }

  // 2) Navigate to about:blank on a new window that hasn't done any navigation.
  {
    SCOPED_TRACE(testing::Message() << " Testing case 2.");

    // Create a new blank window.
    Shell* new_shell = OpenBlankWindow(contents());
    WebContentsImpl* new_contents =
        static_cast<WebContentsImpl*>(new_shell->web_contents());
    NavigationControllerImpl& controller = new_contents->GetController();
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntry* last_entry = controller.GetLastCommittedEntry();

    // The window should be on the initial empty document and initial
    // NavigationEntry.
    FrameTreeNode* new_root = new_contents->GetPrimaryFrameTree().root();
    EXPECT_TRUE(new_root->is_on_initial_empty_document());
    EXPECT_TRUE(last_entry->IsInitialEntry());

    // Navigating the window to about:blank will be classified as NEW_ENTRY
    // and will replace the initial entry.
    NavigateWindowAndCheck(new_contents, GURL("about:blank"),
                           true /* wait_for_previous_navigations */,
                           false /* expect_same_document */,
                           NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY,
                           true /* expect_replace */);
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_NE(last_entry, controller.GetLastCommittedEntry());
    last_entry = controller.GetLastCommittedEntry();

    // The window is no longer on the initial empty document and initial
    // NavigationEntry.
    EXPECT_FALSE(new_root->is_on_initial_empty_document());
    EXPECT_FALSE(last_entry->IsInitialEntry());

    // Navigating the window to about:blank#foo will be classified as a same-
    // document NEW_ENTRY, and will add a new entry.
    NavigateWindowAndCheck(new_contents, GURL("about:blank#foo"),
                           true /* wait_for_previous_navigations */,
                           true /* expect_same_document */,
                           NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY,
                           false /* expect_replace */);
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_NE(last_entry, controller.GetLastCommittedEntry());

    // The window is still marked as no longer being on the initial empty
    // document.
    EXPECT_FALSE(new_root->is_on_initial_empty_document());
  }

  // 3) Navigate to about:blank#foo on a new window that hasn't done any
  // navigation.
  {
    SCOPED_TRACE(testing::Message() << " Testing case 3.");

    // Create a new blank window.
    Shell* new_shell = OpenBlankWindow(contents());
    WebContentsImpl* new_contents =
        static_cast<WebContentsImpl*>(new_shell->web_contents());
    NavigationControllerImpl& controller = new_contents->GetController();
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntry* last_entry = controller.GetLastCommittedEntry();

    // The window should be on the initial empty document and initial
    // NavigationEntry.
    FrameTreeNode* new_root = new_contents->GetPrimaryFrameTree().root();
    EXPECT_TRUE(new_root->is_on_initial_empty_document());
    EXPECT_TRUE(last_entry->IsInitialEntry());

    // Navigating the window to about:blank#foo will be classified as a same-
    // document replacement, and will be classified as EXISTING_ENTRY and modify
    // the last NavigationEntry for browser-initiated navigations.
    NavigateWindowAndCheck(new_contents, GURL("about:blank#foo"),
                           true /* wait_for_previous_navigations */,
                           true /* expect_same_document */,
                           NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY);
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_EQ(last_entry, controller.GetLastCommittedEntry());

    // The window should still be on the initial empty document and initial
    // NavigationEntry.
    EXPECT_TRUE(new_root->is_on_initial_empty_document());
    EXPECT_TRUE(last_entry->IsInitialEntry());
  }

  // 4) Navigate to |url_2| on a new window that initially loaded about:blank
  // and has done a same-document navigation to about:blank#foo.
  {
    SCOPED_TRACE(testing::Message() << " Testing case 4.");

    // Create a new window with URL set to about:blank.
    Shell* new_shell = OpenWindow(contents(), GURL("about:blank"));
    WebContentsImpl* new_contents =
        static_cast<WebContentsImpl*>(new_shell->web_contents());
    NavigationControllerImpl& controller = new_contents->GetController();
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntryImpl* last_entry = controller.GetLastCommittedEntry();

    // The window should be on the initial empty document and initial
    // NavigationEntry.
    FrameTreeNode* new_root = new_contents->GetPrimaryFrameTree().root();
    EXPECT_TRUE(new_root->is_on_initial_empty_document());
    EXPECT_TRUE(last_entry->IsInitialEntry());

    // Do a navigation on the window to about:blank#foo, creating a
    // same-document navigation.
    NavigateWindowAndCheck(new_contents, GURL("about:blank#foo"),
                           true /* wait_for_previous_navigations */,
                           true /* expect_same_document */,
                           NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
                           true /* expect_replace */);
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_EQ(last_entry, controller.GetLastCommittedEntry());
    // Check that we did a same-document navigation (the DSN stays the same).
    EXPECT_EQ(last_entry->GetMainFrameDocumentSequenceNumber(),
              controller.GetLastCommittedEntry()
                  ->GetMainFrameDocumentSequenceNumber());
    last_entry = controller.GetLastCommittedEntry();

    // The window should still be on the initial empty document and initial
    // NavigationEntry.
    EXPECT_TRUE(new_root->is_on_initial_empty_document());
    EXPECT_TRUE(last_entry->IsInitialEntry());

    // Navigating the window to |url_2| will be classified as NEW_ENTRY and will
    // replace the previous entry.
    NavigateWindowAndCheck(
        new_contents, url_2, true /* wait_for_previous_navigations */,
        false /* expect_same_document */, NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY,
        true /* expect_replace */);
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_NE(last_entry, controller.GetLastCommittedEntry());
    last_entry = controller.GetLastCommittedEntry();

    // The window is no longer on the initial empty document and initial
    // NavigationEntry.
    EXPECT_FALSE(new_root->is_on_initial_empty_document());
    EXPECT_FALSE(last_entry->IsInitialEntry());
  }

  // 5) Navigate to |url_2| on a new window that has started a navigation to
  // a URL that never committed.
  {
    SCOPED_TRACE(testing::Message() << " Testing case 5.");

    // Create a new window with URL set to a URL that never commits, which will
    // stay on the initial NavigationEntry.
    Shell* new_shell = OpenWindow(contents(), hung_url);
    WebContentsImpl* new_contents =
        static_cast<WebContentsImpl*>(new_shell->web_contents());
    NavigationControllerImpl& controller = new_contents->GetController();
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntryImpl* last_entry = controller.GetLastCommittedEntry();

    // The window should be on the initial empty document and initial
    // NavigationEntry.
    FrameTreeNode* new_root = new_contents->GetPrimaryFrameTree().root();
    EXPECT_TRUE(new_root->is_on_initial_empty_document());
    EXPECT_TRUE(last_entry->IsInitialEntry());

    // Navigate to |url_2|, and ensure that we won't wait for the |hung_url|
    // navigation to finish. The initial NavigationEntry will be replaced.
    NavigateWindowAndCheck(new_contents, url_2,
                           false /* wait_for_previous_navigations */);
    EXPECT_EQ(1, controller.GetEntryCount());
    last_entry = controller.GetLastCommittedEntry();

    // The window is no longer on the initial empty document and initial
    // NavigationEntry.
    EXPECT_FALSE(new_root->is_on_initial_empty_document());
    EXPECT_FALSE(last_entry->IsInitialEntry());
  }

  // 6) Navigate to |url_2| on a new window that has done a document.open().
  {
    SCOPED_TRACE(testing::Message() << " Testing case 6.");

    // Create a new blank window.
    Shell* new_shell = OpenBlankWindow(contents());
    WebContentsImpl* new_contents =
        static_cast<WebContentsImpl*>(new_shell->web_contents());
    NavigationControllerImpl& controller = new_contents->GetController();
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntryImpl* last_entry = controller.GetLastCommittedEntry();

    // The window should be on the initial empty document and initial
    // NavigationEntry.
    FrameTreeNode* new_root = new_contents->GetPrimaryFrameTree().root();
    EXPECT_TRUE(new_root->is_on_initial_empty_document());
    EXPECT_TRUE(last_entry->IsInitialEntry());

    // Do a document.open() on the blank window.
    TestNavigationObserver nav_observer(new_contents);
    EXPECT_TRUE(ExecJs(contents(), R"(
      last_opened_window.document.open();
          last_opened_window.document.write("foo");
          last_opened_window.document.close();
    )"));

    // Ensure the URL update from the document.open() above has finished before
    // continuing by waiting for a renderer round-trip to run this script.
    EXPECT_TRUE(ExecJs(new_contents, "true"));

    // The document.open() changed the window's URL in the renderer to be the
    // same as the main tab's URL, but doesn't actually commit a navigation.
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_EQ(GURL("about:blank"),
              new_contents->GetPrimaryMainFrame()->GetLastCommittedURL());
    EXPECT_EQ(
        main_window_url,
        new_contents->GetPrimaryMainFrame()->last_document_url_in_renderer());

    // The window lost its "initial empty document" status but is still on the
    // initial NavigationEntry.
    EXPECT_FALSE(new_root->is_on_initial_empty_document());
    EXPECT_TRUE(last_entry->IsInitialEntry());

    // Navigating the window to |url_2| will be classified as NEW_ENTRY and will
    // replace the previous entry, because it was still on the initial
    // NavigationEntry.
    NavigateWindowAndCheck(
        new_contents, url_2, true /* wait_for_previous_navigations */,
        false /* expect_same_document */, NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY,
        true /* expect_replace */);
    EXPECT_EQ(1, controller.GetEntryCount());
    last_entry = controller.GetLastCommittedEntry();
    // The window is no longer on the initial NavigationEntry.
    EXPECT_FALSE(last_entry->IsInitialEntry());
  }

  // 7) Navigate to |url_2| on a new window that has navigated to a javascript:
  // URL that replaced the initial empty document.
  {
    SCOPED_TRACE(testing::Message() << " Testing case 7.");

    // Create a new window with URL set to a javascript: URL that replaces the
    // document, which will stay on the initial NavigationEntry.
    Shell* new_shell = OpenWindow(contents(), GURL("javascript:'foo'"));
    WebContentsImpl* new_contents =
        static_cast<WebContentsImpl*>(new_shell->web_contents());
    NavigationControllerImpl& controller = new_contents->GetController();
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntryImpl* last_entry = controller.GetLastCommittedEntry();

    // The window should be on the initial empty document and initial
    // NavigationEntry.
    FrameTreeNode* new_root = new_contents->GetPrimaryFrameTree().root();
    EXPECT_TRUE(new_root->is_on_initial_empty_document());
    EXPECT_TRUE(last_entry->IsInitialEntry());

    // Navigating the window to |url_2| will be classified as NEW_ENTRY and will
    // replace the initial entry.
    NavigateWindowAndCheck(new_contents, url_2);
    EXPECT_EQ(1, controller.GetEntryCount());
    last_entry = controller.GetLastCommittedEntry();

    // The window is no longer on the initial empty document and initial
    // NavigationEntry.
    EXPECT_FALSE(new_root->is_on_initial_empty_document());
    EXPECT_FALSE(last_entry->IsInitialEntry());
  }
}

// Test a same-document navigation in a new window's initial empty document to
// ensure it is classified correctly.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       SameDocumentInInitialEmptyDocument) {
  GURL main_window_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_window_url));

  // Create a new blank window that will stay on the initial NavigationEntry.
  Shell* new_shell = OpenBlankWindow(contents());
  WebContentsImpl* new_contents =
      static_cast<WebContentsImpl*>(new_shell->web_contents());
  NavigationControllerImpl& controller = new_contents->GetController();
  FrameTreeNode* root = new_contents->GetPrimaryFrameTree().root();
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_TRUE(controller.GetLastCommittedEntry());

  {
    // Do a same-document navigation.
    LoadCommittedDetailsObserver load_details_observer(new_contents);
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(contents(), "last_opened_window.location.hash='foo';"));
    capturer.Wait();
    load_details_observer.Wait();

    // The pushState will be classified as NEW_ENTRY and will add a new entry.
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_TRUE(controller.GetLastCommittedEntry());

    // Check both NavigationHandle and LoadCommittedDetails for whether this was
    // considered same-document, as these have diverged in the past.
    // See https://crbug.com/1193134.
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_TRUE(load_details_observer.load_details().is_same_document);
  }
}

// Tests that the initial NavigationEntry retains its "initial" status until the
// first cross-document main frame navigation that is neither a reload nor the
// synchronous about:blank navigation. Tests both the "not for synchronous
// about:blank" and "for synchronous about:blank" cases. Also tests the fact
// that reloads on the initial entry that is not for synchronous about:blank
// will fail.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       InitialNavigationEntryStatus_MainFrame) {
  GURL main_window_url(embedded_test_server()->GetURL("/title1.html"));
  GURL no_commit_url(embedded_test_server()->GetURL("/page204.html"));
  GURL cross_document_url(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_window_url));

  // 1) Main frame same-document navigation. Note that we don't test the
  // non-synchronous about:blank case because we can't do same-document
  // navigation if no navigation had committed.
  {
    // Pop open a new window to an empty URL, which will stay at the initial
    // NavigationEntry (but marked as "for synchronous about:blank").
    Shell* new_shell = OpenWindow(contents(), GURL());
    WebContentsImpl* new_contents =
        static_cast<WebContentsImpl*>(new_shell->web_contents());
    NavigationControllerImpl& controller = new_contents->GetController();
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntryImpl* last_entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kInitialForSynchronousAboutBlank,
              last_entry->initial_navigation_entry_state());
    EXPECT_EQ(GURL("about:blank"), last_entry->GetURL());

    // Navigate the new window to a same-document URL.
    GURL same_document_url("about:blank#foo");
    FrameTreeNode* root = new_contents->GetPrimaryFrameTree().root();
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(NavigateToURL(new_contents, same_document_url));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_TRUE(capturer.is_same_document());

    // We reused the previous NavigationEntry and it keeps its "initial" status.
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_EQ(last_entry, controller.GetLastCommittedEntry());
    last_entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kInitialForSynchronousAboutBlank,
              last_entry->initial_navigation_entry_state());
    EXPECT_EQ(same_document_url, last_entry->GetURL());
  }

  // 2) Main frame cross-document navigation.
  {
    // 2a: "For synchronous about:blank" case.
    // Pop open a new window to an empty URL, which will stay at the initial
    // NavigationEntry.
    Shell* new_shell = OpenWindow(contents(), GURL());
    WebContentsImpl* new_contents =
        static_cast<WebContentsImpl*>(new_shell->web_contents());
    NavigationControllerImpl& controller = new_contents->GetController();
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntryImpl* last_entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kInitialForSynchronousAboutBlank,
              last_entry->initial_navigation_entry_state());
    EXPECT_EQ(GURL("about:blank"), last_entry->GetURL());

    // Navigate the new window to a cross-document URL.
    FrameTreeNode* root = new_contents->GetPrimaryFrameTree().root();
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(NavigateToURL(new_contents, cross_document_url));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_FALSE(capturer.is_same_document());

    // We replaced the previous NavigationEntry and it's no longer on the
    // initial NavigationEntry.
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_NE(last_entry, controller.GetLastCommittedEntry());
    last_entry = controller.GetLastCommittedEntry();
    EXPECT_FALSE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kNonInitial,
              last_entry->initial_navigation_entry_state());
    EXPECT_EQ(cross_document_url, last_entry->GetURL());
  }
  {
    // 2b: "Not for synchronous about:blank" case.
    // Pop open a new window which won't commit a navigation, so it will stay at
    // the initial NavigationEntry.
    Shell* new_shell = OpenWindow(contents(), no_commit_url);
    WebContentsImpl* new_contents =
        static_cast<WebContentsImpl*>(new_shell->web_contents());
    // Stop the navigation so that it won't affect future navigations.
    new_contents->Stop();
    NavigationControllerImpl& controller = new_contents->GetController();
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntryImpl* last_entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kInitialNotForSynchronousAboutBlank,
              last_entry->initial_navigation_entry_state());
    EXPECT_EQ(GURL(), last_entry->GetURL());

    // Navigate the new window to a cross-document URL.
    FrameTreeNode* root = new_contents->GetPrimaryFrameTree().root();
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(NavigateToURL(new_contents, cross_document_url));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_FALSE(capturer.is_same_document());

    // We replaced the previous NavigationEntry and it's no longer on the
    // initial NavigationEntry.
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_NE(last_entry, controller.GetLastCommittedEntry());
    last_entry = controller.GetLastCommittedEntry();
    EXPECT_FALSE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kNonInitial,
              last_entry->initial_navigation_entry_state());
    EXPECT_EQ(cross_document_url, last_entry->GetURL());
  }

  // 3) Main frame reload (browser-initiated).
  {
    // 3a: "For synchronous about:blank" case.
    // Pop open a new window to an empty URL, which will stay at the initial
    // NavigationEntry.
    Shell* new_shell = OpenWindow(contents(), GURL());
    WebContentsImpl* new_contents =
        static_cast<WebContentsImpl*>(new_shell->web_contents());
    NavigationControllerImpl& controller = new_contents->GetController();
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntryImpl* last_entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kInitialForSynchronousAboutBlank,
              last_entry->initial_navigation_entry_state());
    EXPECT_EQ(GURL("about:blank"), last_entry->GetURL());

    FrameTreeNode* root = new_contents->GetPrimaryFrameTree().root();
    // Reload the new window from the browser. This will succeed because the
    // initial NavigationEntry is for the synchronous about:blank commit.
    FrameNavigateParamsCapturer capturer(root);
    root->current_frame_host()->Reload();
    capturer.Wait();

    // We reused the previous NavigationEntry and it keeps its "initial" status.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_FALSE(capturer.is_same_document());

    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_EQ(last_entry, controller.GetLastCommittedEntry());
    last_entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kInitialForSynchronousAboutBlank,
              last_entry->initial_navigation_entry_state());
    EXPECT_EQ(GURL("about:blank"), last_entry->GetURL());
  }

  {
    // 3b: "Not for synchronous about:blank" case.
    // Pop open a new window which won't commit a navigation, so it will stay at
    // the initial NavigationEntry.
    Shell* new_shell = OpenWindow(contents(), no_commit_url);
    WebContentsImpl* new_contents =
        static_cast<WebContentsImpl*>(new_shell->web_contents());
    // Stop the navigation so that it won't affect future navigations.
    new_contents->Stop();
    NavigationControllerImpl& controller = new_contents->GetController();
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntryImpl* last_entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kInitialNotForSynchronousAboutBlank,
              last_entry->initial_navigation_entry_state());
    EXPECT_EQ(GURL(), last_entry->GetURL());

    FrameTreeNode* root = new_contents->GetPrimaryFrameTree().root();
    // Reload the new window from the browser. This will succeed because the
    // initial NavigationEntry is for the synchronous about:blank commit.
    FrameNavigateParamsCapturer capturer(root);
    root->current_frame_host()->Reload();

    // Assert that there's no pending NavigationEntry or NavigationRequest,
    // which means no reload had started.
    EXPECT_FALSE(root->navigation_request());
    EXPECT_FALSE(new_contents->GetController().GetPendingEntry());

    // To ensure that the reload navigation got ignored, trigger another
    // navigation from the renderer. This new navigation would normally get
    // ignored or at least happen later on if a reload is already in progress.
    EXPECT_TRUE(
        ExecJs(root, "location.href = '" + cross_document_url.spec() + "';"));
    capturer.Wait();

    // Assert that the cross-document navigation succeeds and the initial
    // NavigationEntry retains its status.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_FALSE(capturer.is_same_document());

    // We replaced the previous NavigationEntry and it's no longer on the
    // initial NavigationEntry.
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_NE(last_entry, controller.GetLastCommittedEntry());
    last_entry = controller.GetLastCommittedEntry();
    EXPECT_FALSE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kNonInitial,
              last_entry->initial_navigation_entry_state());
    EXPECT_EQ(cross_document_url, last_entry->GetURL());
  }

  // 4) Main frame reload (renderer-initiated). Note that we don't test the
  // "not for synchronous about:blank case here" because even if the reload
  // happens, it won't commit because it will navigate to `no_commit_url`.
  {
    // Pop open a new window to an empty URL, which will stay at the initial
    // NavigationEntry.
    Shell* new_shell = OpenWindow(contents(), GURL());
    WebContentsImpl* new_contents =
        static_cast<WebContentsImpl*>(new_shell->web_contents());
    NavigationControllerImpl& controller = new_contents->GetController();
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntryImpl* last_entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kInitialForSynchronousAboutBlank,
              last_entry->initial_navigation_entry_state());
    EXPECT_EQ(GURL("about:blank"), last_entry->GetURL());

    FrameTreeNode* root = new_contents->GetPrimaryFrameTree().root();
    // Reload from the renderer.
    FrameNavigateParamsCapturer capturer(root);
    ASSERT_TRUE(ExecJs(root, "location.reload()"));
    capturer.Wait();

    // We reused the previous NavigationEntry and it keeps its "initial" status.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_FALSE(capturer.is_same_document());

    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_EQ(last_entry, controller.GetLastCommittedEntry());
    last_entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kInitialForSynchronousAboutBlank,
              last_entry->initial_navigation_entry_state());
    EXPECT_EQ(GURL("about:blank"), last_entry->GetURL());
  }
}

// Tests that the initial NavigationEntry keeps its "initial" status on subframe
// navigations and when restoring/copying history state to another
// NavigationController. Also tests the fact that reloads on the initial entry
// that is not for synchronous about:blank will fail.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       InitialNavigationEntryStatus_SubframeAndRestore) {
  GURL main_window_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  GURL no_commit_url(embedded_test_server()->GetURL("/page204.html"));

  GURL subframe_url(embedded_test_server()->GetURL("/title1.html"));
  GURL subframe_url_2(embedded_test_server()->GetURL("/title2.html"));
  GURL subframe_url_3(embedded_test_server()->GetURL("/title3.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_window_url));
  // Pop open a new window which won't commit a navigation, so it will stay at
  // the initial NavigationEntry.
  Shell* new_shell = OpenWindow(contents(), no_commit_url);
  WebContentsImpl* new_contents =
      static_cast<WebContentsImpl*>(new_shell->web_contents());
  // Stop the navigation so that it won't affect future navigations.
  new_contents->Stop();
  NavigationControllerImpl& controller = new_contents->GetController();
  NavigationEntryImpl* last_entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_TRUE(last_entry->IsInitialEntry());
  EXPECT_EQ(GURL(), last_entry->GetURL());

  // 1) Subframe creation.
  {
    // Create a subframe and navigate it to `subframe_url`.
    CreateSubframe(new_contents, "child_frame", subframe_url,
                   true /* wait_for_navigation */);

    // The initial NavigationEntry is reused and keeps its "initial" status
    // and added a FrameNavigationEntry for the new subframe.
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_EQ(last_entry, controller.GetLastCommittedEntry());
    EXPECT_TRUE(last_entry->IsInitialEntry());
    ASSERT_EQ(1U, last_entry->root_node()->children.size());
    EXPECT_EQ(subframe_url,
              last_entry->root_node()->children[0]->frame_entry->url());
    EXPECT_EQ(InitialNavigationEntryState::kInitialNotForSynchronousAboutBlank,
              last_entry->initial_navigation_entry_state());

    EXPECT_EQ(GURL(), last_entry->GetURL());
  }

  FrameTreeNode* root = new_contents->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // 2) Subframe cross-document navigations.
  {
    // Navigate the subframe to a cross-document URL (browser-initiated).
    {
      FrameNavigateParamsCapturer capturer(child);
      NavigateFrameToURL(child, subframe_url_2);
      capturer.Wait();
      EXPECT_TRUE(capturer.did_replace_entry());
      EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
      EXPECT_EQ(subframe_url_2, child->current_url());
    }

    // The last committed entry retains the "initial NavigationEntry" status
    // and has an updated FrameNavigationEntry for the subframe.
    EXPECT_EQ(1, controller.GetEntryCount());
    last_entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kInitialNotForSynchronousAboutBlank,
              last_entry->initial_navigation_entry_state());

    EXPECT_EQ(GURL(), last_entry->GetURL());
    EXPECT_EQ(subframe_url_2,
              last_entry->root_node()->children[0]->frame_entry->url());

    {
      FrameNavigateParamsCapturer capturer(child);
      EXPECT_TRUE(
          ExecJs(child, "location.href = '" + subframe_url_3.spec() + "';"));
      capturer.Wait();
      EXPECT_TRUE(capturer.did_replace_entry());
      EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
      EXPECT_EQ(subframe_url_3, child->current_url());
    }

    // The last committed entry retains the "initial NavigationEntry" status.
    EXPECT_EQ(1, controller.GetEntryCount());
    last_entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kInitialNotForSynchronousAboutBlank,
              last_entry->initial_navigation_entry_state());

    EXPECT_EQ(GURL(), last_entry->GetURL());
    EXPECT_EQ(subframe_url_3,
              last_entry->root_node()->children[0]->frame_entry->url());
  }

  // 3) Subframe reload (renderer-initiated).
  {
    // Reload the subframe from the renderer.
    FrameNavigateParamsCapturer capturer(child);
    ASSERT_TRUE(ExecJs(child, "location.reload()"));
    capturer.Wait();
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
    EXPECT_EQ(subframe_url_3, child->current_url());

    // The last committed entry retains the "initial NavigationEntry" status.
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_EQ(last_entry, controller.GetLastCommittedEntry());
    EXPECT_TRUE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kInitialNotForSynchronousAboutBlank,
              last_entry->initial_navigation_entry_state());

    EXPECT_EQ(GURL(), last_entry->GetURL());
    EXPECT_EQ(subframe_url_3,
              last_entry->root_node()->children[0]->frame_entry->url());
  }

  // 4) Subframe reload (browser-initiated).
  {
    // Try to reload the subframe from the browser. This will get ignored
    // because the initial NavigationEntry is for the initial empty document
    // (not for the synchronous about:blank commit) and its URL is empty.
    child->current_frame_host()->Reload();

    // Assert that there's no pending NavigationEntry or NavigationRequest,
    // which means no reload had started.
    EXPECT_FALSE(root->navigation_request());
    EXPECT_FALSE(child->navigation_request());
    EXPECT_FALSE(new_contents->GetController().GetPendingEntry());

    // The last committed entry retains the "initial NavigationEntry" status.
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_EQ(last_entry, controller.GetLastCommittedEntry());
    EXPECT_TRUE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kInitialNotForSynchronousAboutBlank,
              last_entry->initial_navigation_entry_state());

    // To ensure that the reload navigation got ignored, trigger another
    // navigation from the renderer. This new navigation would normally get
    // ignored or at least happen later on if a reload is already in progress.
    FrameNavigateParamsCapturer capturer(child);
    EXPECT_TRUE(
        ExecJs(child, "location.href = '" + subframe_url_2.spec() + "';"));
    capturer.Wait();

    // Assert that the cross-document navigation succeeds and the initial
    // NavigationEntry retains its status.
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
    EXPECT_EQ(subframe_url_2, child->current_url());

    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_EQ(last_entry, controller.GetLastCommittedEntry());
    last_entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(last_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kInitialNotForSynchronousAboutBlank,
              last_entry->initial_navigation_entry_state());

    EXPECT_EQ(GURL(), last_entry->GetURL());
    EXPECT_EQ(subframe_url_2,
              last_entry->root_node()->children[0]->frame_entry->url());
  }

  // 6) Restore/copying history state into a new NavigationController.
  {
    // Restore the initial NavigationEntry in a new tab.
    Shell* restore_shell =
        Shell::CreateNewWindow(controller.GetBrowserContext(),
                               GURL::EmptyGURL(), nullptr, gfx::Size());
    NavigationControllerImpl& restore_controller =
        static_cast<NavigationControllerImpl&>(
            restore_shell->web_contents()->GetController());
    restore_controller.CopyStateFrom(&controller, false /* needs_reload */);

    EXPECT_EQ(1, restore_controller.GetEntryCount());
    EXPECT_EQ(0, restore_controller.GetLastCommittedEntryIndex());
    NavigationEntryImpl* restored_entry =
        restore_controller.GetLastCommittedEntry();

    // The entry keep its "initial" status, and retains its
    // FrameNavigationEntries.
    EXPECT_TRUE(restored_entry->IsInitialEntry());
    EXPECT_EQ(InitialNavigationEntryState::kInitialNotForSynchronousAboutBlank,
              restored_entry->initial_navigation_entry_state());

    EXPECT_EQ(GURL(), restored_entry->root_node()->frame_entry->url());
    ASSERT_EQ(1U, restored_entry->root_node()->children.size());
    EXPECT_EQ(subframe_url_2,
              restored_entry->root_node()->children[0]->frame_entry->url());
  }
}

// Verify that navigations caused by client-side redirects are correctly
// classified.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       NavigationTypeClassification_ClientSideRedirect) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(1, controller.GetEntryCount());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  {
    // Load the redirecting page.
    // Two navigations will happen: The original navigation to the
    // client-redirector URL, then to the URL we got redirected to.
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_navigations_remaining(2);
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/client_redirect.html"));
    NavigateFrameToURL(root, frame_url);
    capturer.Wait();

    ASSERT_EQ(2U, capturer.transitions().size());
    ASSERT_EQ(2U, capturer.navigation_types().size());
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transitions()[0], ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY,
              capturer.navigation_types()[0]);
    EXPECT_FALSE(capturer.did_replace_entries()[0]);
    // The transition used for the second navigation indicates that it is a
    // client-side redirect.
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transitions()[1],
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY,
              capturer.navigation_types()[1]);
    // The client-side redirect results in the replacement of the previous
    // entry.
    EXPECT_TRUE(capturer.did_replace_entries()[1]);
    EXPECT_EQ(2, controller.GetEntryCount());
  }

  {
    GURL fragment_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html#foo"));
    FrameNavigateParamsCapturer capturer(root);
    // Renderer-initiated fragment navigation with location.href
    EXPECT_TRUE(ExecJs(contents(), "location.href = '#foo'"));
    capturer.Wait();
    EXPECT_EQ(fragment_url, contents()->GetLastCommittedURL());
    ASSERT_EQ(1U, capturer.transitions().size());
    // The transition used for the renderer-initiated fragment navigation
    // indicates that it is not a client side redirect since it does
    // not do replacement.
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transitions()[0], ui::PAGE_TRANSITION_LINK));
    EXPECT_FALSE(capturer.did_replace_entries()[0]);
    EXPECT_EQ(3, controller.GetEntryCount());
  }

  {
    GURL fragment_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html#foobar"));
    FrameNavigateParamsCapturer capturer(root);
    // Renderer-initiated fragment navigation with location.replace
    EXPECT_TRUE(ExecJs(contents(), "location.replace('#foobar');"));
    capturer.Wait();
    EXPECT_EQ(fragment_url, contents()->GetLastCommittedURL());
    ASSERT_EQ(1U, capturer.transitions().size());
    // The transition used for the renderer-initiated fragment navigation
    // indicates that it is a client-side redirect that results
    // in entry replacement.
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transitions()[0],
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_TRUE(capturer.did_replace_entries()[0]);
    EXPECT_EQ(3, controller.GetEntryCount());
  }

  {
    // History API same-document navigation through history.pushState.
    GURL push_state_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html#bar"));
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(
        ExecJs(shell(), "history.pushState({}, '', 'simple_page_1.html#bar')"));
    capturer.Wait();
    EXPECT_EQ(push_state_url, contents()->GetLastCommittedURL());
    ASSERT_EQ(1U, capturer.transitions().size());
    // The transition used for the History API same-document navigation
    // indicates that it is not a client redirect since it doesn't do
    // replacement.
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transitions()[0],
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_FALSE(capturer.did_replace_entries()[0]);
    EXPECT_EQ(4, controller.GetEntryCount());
  }

  {
    // History API same-document navigation through history.replaceState.
    GURL replace_state_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html#bar2"));
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(
        shell(), "history.replaceState({}, '', 'simple_page_1.html#bar2')"));
    capturer.Wait();
    EXPECT_EQ(replace_state_url, contents()->GetLastCommittedURL());
    ASSERT_EQ(1U, capturer.transitions().size());
    // The transition used for the History API same-document navigation
    // with history.replaceState indicates that it is a client-side redirect
    // that results in entry replacement.
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transitions()[0],
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_TRUE(capturer.did_replace_entries()[0]);
    EXPECT_EQ(4, controller.GetEntryCount());
  }

  {
    GURL fragment_url_2(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html#baz"));
    // Browser-initiated fragment navigation.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), fragment_url_2));
    capturer.Wait();
    ASSERT_EQ(1U, capturer.transitions().size());
    // The transition used for the browser-initiated fragment navigation does
    // not indicate a client-side redirect.
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transitions()[0],
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_FALSE(capturer.did_replace_entries()[0]);
    EXPECT_EQ(5, controller.GetEntryCount());
  }
}

// Verify that the LoadCommittedDetails::is_same_document value is properly set
// for non same document navigations.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       LoadCommittedDetails_IsSameDocument) {
  GURL links_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  EXPECT_TRUE(NavigateToURL(shell(), links_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  {
    // Do a fragment link click.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "document.getElementById('fraglink').click()";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
  }

  {
    // Do a non-fragment link click.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "document.getElementById('thelink').click()";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());
  }

  // Second verse, same as the first. (But in a subframe.)

  GURL iframe_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), iframe_url));

  root = static_cast<WebContentsImpl*>(shell()->web_contents())
             ->GetPrimaryFrameTree()
             .root();

  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), links_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  {
    // Do a fragment link click.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    std::string script = "document.getElementById('fraglink').click()";
    EXPECT_TRUE(ExecJs(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
  }

  {
    // Do a non-fragment link click.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    std::string script = "document.getElementById('thelink').click()";
    EXPECT_TRUE(ExecJs(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());
  }
}

// Verify the tree of FrameNavigationEntries after initial about:blank commits
// in subframes should keep its "initial empty document" status.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_BlankAutoSubframe) {
  GURL about_blank_url(url::kAboutBlankURL);
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  url::Origin main_origin =
      root->current_frame_host()->GetLastCommittedOrigin();
  EXPECT_EQ(url::Origin::Create(main_url), main_origin);

  // 1. Create a iframe with no URL.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, kAddEmptyFrameScript));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // Check last committed NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  scoped_refptr<FrameNavigationEntry> root_entry =
      entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());
  EXPECT_EQ(main_origin, root_entry->committed_origin());
  EXPECT_FALSE(root_entry->initiator_origin().has_value());

  // Verify subframe entries.  The entry should now have one blank subframe
  // FrameNavigationEntry and should be on the initial empty document.
  ASSERT_EQ(1U, entry->root_node()->children.size());
  scoped_refptr<FrameNavigationEntry> frame_entry =
      entry->root_node()->children[0]->frame_entry.get();
  EXPECT_EQ(about_blank_url, frame_entry->url());
  EXPECT_EQ(main_origin, frame_entry->committed_origin());
  ASSERT_TRUE(frame_entry->initiator_origin().has_value());
  EXPECT_EQ(main_origin, frame_entry->initiator_origin().value());
  EXPECT_TRUE(root->child_at(0)->is_on_initial_empty_document());

  // 1a. A nested iframe with no URL should also create a subframe entry but not
  // count as a real load.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root->child_at(0), kAddEmptyFrameScript));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // Verify subframe entries.  The nested entry should have one blank subframe
  // FrameNavigationEntry and should be on the initial empty document.
  ASSERT_EQ(1U, entry->root_node()->children[0]->children.size());
  frame_entry = entry->root_node()->children[0]->children[0]->frame_entry.get();
  EXPECT_EQ(about_blank_url, frame_entry->url());
  EXPECT_EQ(main_origin, frame_entry->committed_origin());
  ASSERT_TRUE(frame_entry->initiator_origin().has_value());
  EXPECT_EQ(main_origin, frame_entry->initiator_origin().value());
  EXPECT_TRUE(root->child_at(0)->child_at(0)->is_on_initial_empty_document());

  // 2. Create another iframe with an explicit about:blank URL.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, "about:blank")));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // Check last committed NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());

  // Verify subframe entries.  The new entry should have one blank subframe
  // FrameNavigationEntry and should be on the initial empty document.
  ASSERT_EQ(2U, entry->root_node()->children.size());
  frame_entry = entry->root_node()->children[1]->frame_entry.get();
  EXPECT_EQ(about_blank_url, frame_entry->url());
  EXPECT_EQ(main_origin, frame_entry->committed_origin());
  ASSERT_TRUE(frame_entry->initiator_origin().has_value());
  EXPECT_EQ(main_origin, frame_entry->initiator_origin().value());
  EXPECT_TRUE(root->child_at(1)->is_on_initial_empty_document());

  // 3. A real same-site navigation in the nested iframe should be AUTO.
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(root->child_at(0)->child_at(0));
    std::string script = JsReplace(
        "var frames = document.getElementsByTagName('iframe');"
        "frames[0].src = $1;",
        frame_url);
    EXPECT_TRUE(ExecJs(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // Check last committed NavigationEntry.  It should have replaced the previous
  // frame entry in the original NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());

  // The entry should still have one nested subframe FrameNavigationEntry.
  ASSERT_EQ(1U, entry->root_node()->children[0]->children.size());
  frame_entry = entry->root_node()->children[0]->children[0]->frame_entry.get();
  EXPECT_EQ(frame_url, frame_entry->url());
  EXPECT_EQ(url::Origin::Create(frame_url), frame_entry->committed_origin());
  ASSERT_TRUE(frame_entry->initiator_origin().has_value());
  EXPECT_EQ(main_origin, frame_entry->initiator_origin().value());
  EXPECT_TRUE(root->child_at(0)->is_on_initial_empty_document());
  EXPECT_FALSE(root->child_at(0)->child_at(0)->is_on_initial_empty_document());
  EXPECT_TRUE(root->child_at(1)->is_on_initial_empty_document());

  // 4. A real cross-site navigation in the second iframe should be AUTO.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(root->child_at(1));
    std::string script = JsReplace(
        "var frames = document.getElementsByTagName('iframe');"
        "frames[1].src = $1;",
        foo_url);
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // Check last committed NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());

  // The entry should still have two subframe FrameNavigationEntries.
  ASSERT_EQ(2U, entry->root_node()->children.size());
  frame_entry = entry->root_node()->children[1]->frame_entry.get();
  EXPECT_EQ(foo_url, frame_entry->url());
  EXPECT_EQ(url::Origin::Create(foo_url), frame_entry->committed_origin());
  ASSERT_TRUE(frame_entry->initiator_origin().has_value());
  EXPECT_EQ(main_origin, frame_entry->initiator_origin().value());
  EXPECT_TRUE(root->child_at(0)->is_on_initial_empty_document());
  EXPECT_FALSE(root->child_at(0)->child_at(0)->is_on_initial_empty_document());
  EXPECT_FALSE(root->child_at(1)->is_on_initial_empty_document());
  EXPECT_EQ(frame_entry->committed_origin(),
            root->child_at(1)->current_frame_host()->GetLastCommittedOrigin());

  // 5. A new navigation to about:blank in the nested frame should count as a
  // real load, since that frame has already committed a real load and this is
  // not the initial blank page.
  {
    LoadCommittedCapturer capturer(root->child_at(0)->child_at(0));
    std::string script =
        "var frames = document.getElementsByTagName('iframe');"
        "frames[0].src = 'about:blank';";
    EXPECT_TRUE(ExecJs(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
  }

  // This should have created a new NavigationEntry.
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_NE(entry, controller.GetLastCommittedEntry());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // Verify subframe entries are shared between the NavigationEntries.
  ASSERT_EQ(2U, entry->root_node()->children.size());
  EXPECT_EQ(entry->root_node()->frame_entry.get(),
            entry2->root_node()->frame_entry.get());
  EXPECT_EQ(entry->root_node()->children[0]->frame_entry.get(),
            entry2->root_node()->children[0]->frame_entry.get());
  EXPECT_EQ(entry->root_node()->children[1]->frame_entry.get(),
            entry2->root_node()->children[1]->frame_entry.get());
  EXPECT_NE(entry->root_node()->children[0]->children[0]->frame_entry.get(),
            entry2->root_node()->children[0]->children[0]->frame_entry.get());
  frame_entry =
      entry2->root_node()->children[0]->children[0]->frame_entry.get();
  EXPECT_EQ(about_blank_url, frame_entry->url());
  EXPECT_EQ(main_origin, frame_entry->committed_origin());
  ASSERT_TRUE(frame_entry->initiator_origin().has_value());
  EXPECT_EQ(main_origin, frame_entry->initiator_origin().value());
  EXPECT_TRUE(root->child_at(0)->is_on_initial_empty_document());
  EXPECT_FALSE(root->child_at(0)->child_at(0)->is_on_initial_empty_document());
  EXPECT_FALSE(root->child_at(1)->is_on_initial_empty_document());

  // Check the end result of the frame tree.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site A ------- proxies for B\n"
        "   |    +--Site A -- proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://127.0.0.1/\n"
        "      B = http://foo.com/",
        DepictFrameTree(*root));
  }
}

// Verify the tree of FrameNavigationEntries when a nested iframe commits inside
// the initial blank page of a loading iframe.  Prevents regression of
// https://crbug.com/600743.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_SlowNestedAutoSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1. Create a iframe with a URL that doesn't commit.
  GURL slow_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  TestNavigationManager subframe_delayer(shell()->web_contents(), slow_url);
  std::string script_template =
      "var iframe = document.createElement('iframe');"
      "iframe.src = $1;"
      "document.body.appendChild(iframe);";

  EXPECT_TRUE(ExecJs(root, JsReplace(script_template, slow_url)));
  EXPECT_TRUE(subframe_delayer.WaitForRequestStart());

  // Stop the request so that we can wait for load stop below, without ending up
  // with a commit for this frame.
  shell()->web_contents()->Stop();

  // 2. A nested iframe with a cross-site URL should be able to commit.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(ExecJs(root->child_at(0), JsReplace(script_template, foo_url)));
  WaitForLoadStopWithoutSuccessCheck(shell()->web_contents());

  // TODO(creis): Check subframe entries once we create them in this case.
  // See https://crbug.com/608402.
  EXPECT_EQ(foo_url, root->child_at(0)->child_at(0)->current_url());
}

// Verify that history.pushState() does not replace the pending entry.
// https://crbug.com/900036.
// TODO(crbug.com/926009): Fix and re-enable this test.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       DISABLED_PushStatePreservesPendingEntry) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1. Start loading an URL that doesn't commit.
  GURL stalled_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  // Have the user decide to go to a different page which is very slow.
  TestNavigationManager stalled_navigation(shell()->web_contents(),
                                           stalled_url);
  controller.LoadURL(stalled_url, Referrer(), ui::PAGE_TRANSITION_LINK,
                     std::string());
  EXPECT_TRUE(stalled_navigation.WaitForRequestStart());

  // That should be the pending entry.
  NavigationEntryImpl* entry = controller.GetPendingEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(stalled_url, entry->GetURL());

  {
    // Now the existing page uses history.pushState() while the pending entry
    // for the other navigation still exists.
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    std::string script = "history.pushState({}, '', 'pushed')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
  }

  // This should not replace the pending entry.
  entry = controller.GetPendingEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(stalled_url, entry->GetURL());
}

// Verify that history.replaceState() does not replace the pending entry.
// https://crbug.com/900036.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ReplaceStatePreservesPendingEntry) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1. Start loading an URL that doesn't commit.
  GURL stalled_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  // Have the user decide to go to a different page which is very slow.
  TestNavigationManager stalled_navigation(shell()->web_contents(),
                                           stalled_url);
  controller.LoadURL(stalled_url, Referrer(), ui::PAGE_TRANSITION_LINK,
                     std::string());
  EXPECT_TRUE(stalled_navigation.WaitForRequestStart());

  // That should be the pending entry.
  NavigationEntryImpl* entry = controller.GetPendingEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(stalled_url, entry->GetURL());

  {
    // Now the existing page uses history.replaceState() while the pending entry
    // for the other navigation still exists.
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    std::string script = "history.replaceState({}, '', 'replaced')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
  }

  // This should not replace the pending entry.
  entry = controller.GetPendingEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(stalled_url, entry->GetURL());
}

// Verify the tree of FrameNavigationEntries when a nested iframe commits inside
// the initial blank page of an iframe with no committed entry.  Prevents
// regression of https://crbug.com/600743.
// Flaky test: See https://crbug.com/610801
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    DISABLED_FrameNavigationEntry_NoCommitNestedAutoSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1. Create a iframe with a URL that doesn't commit.
  GURL no_commit_url(embedded_test_server()->GetURL("/nocontent"));
  {
    std::string script =
        "var iframe = document.createElement('iframe');"
        "iframe.src = '" +
        no_commit_url.spec() +
        "';"
        "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecJs(root, script));
  }
  EXPECT_EQ(GURL(), root->child_at(0)->current_url());

  // 2. A nested iframe with a cross-site URL should be able to commit.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(
        ExecJs(root->child_at(0), JsReplace(kAddFrameWithSrcScript, foo_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // TODO(creis): Check subframe entries once we create them in this case.
  // See https://crbug.com/608402.
  EXPECT_EQ(foo_url, root->child_at(0)->child_at(0)->current_url());
}

// Verify the tree of FrameNavigationEntries when a nested iframe commits after
// doing same document back navigation, in which case its parent might not have
// been in the NavigationEntry.  Prevents regression of
// https://crbug.com/600743.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_BackNestedAutoSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1. Perform same document navigation.
  {
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "history.pushState({}, 'foo', 'foo')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
  }

  // 2. Create an iframe.
  GURL child_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, child_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // 3. Perform same document back navigation.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }

  // 4. A nested iframe with a cross-site URL should be able to commit.
  GURL grandchild_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root->child_at(0),
                       JsReplace(kAddFrameWithSrcScript, grandchild_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // TODO(creis): Check subframe entries once we create them in this case.
  // See https://crbug.com/608402.
  EXPECT_EQ(grandchild_url, root->child_at(0)->child_at(0)->current_url());
}

// Verify that an iframe is not reloaded after going back same-document to a
// NavigationEntry before the iframe was created, and then forward again.
// Prevents regression of https://crbug.com/1301985.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_BackForwardAcrossSubframeCreation) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1. Perform a same-document navigation.
  {
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "history.pushState({}, 'foo', 'foo')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
  }

  // 2. Create an iframe to about:blank and inject content.
  GURL blank_url(url::kAboutBlankURL);
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, blank_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    EXPECT_TRUE(ExecJs(root->child_at(0), "foo=3;"));
  }

  // 3. Perform a same-document back navigation, to a NavigationEntry before the
  // iframe existed.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }

  // 4. Perform a same-document forward navigation. Wait for all navigations to
  // stop, in case subframes are navigated as well.
  {
    TestNavigationObserver forward_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    forward_load_observer.Wait();
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }

  // Ensure the injected content is still present in the iframe to verify that
  // it was not reloaded. See https://crbug.com/1301985.
  EXPECT_EQ(3, EvalJs(root->child_at(0), "foo"));
}

// Verify that an iframe ends up at the correct URL if going forward from a
// NavigationEntry created before it existed to one where its URL must change.
// See https://crbug.com/1301985 and
// FrameNavigationEntry_BackForwardAcrossSubframeCreation.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_BackForwardAcrossSubframeCreationCrossDocument) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1. Perform a same-document navigation.
  {
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "history.pushState({}, 'foo', 'foo')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
  }

  // 2. Create an iframe to about:blank and inject content.
  GURL child_url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, child_url1)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    EXPECT_TRUE(ExecJs(root->child_at(0), "foo=3;"));
  }

  // 3. Navigate the iframe to another document.
  GURL child_url2(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/page_with_links.html"));
  {
    LoadCommittedCapturer capturer(root->child_at(0));
    std::string script = JsReplace("frames[0].location = $1;", child_url2);
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
  }
  EXPECT_EQ(child_url2, root->child_at(0)->current_url());

  // 4. Perform a cross-document back navigation within the iframe.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  EXPECT_EQ(child_url1, root->child_at(0)->current_url());

  // 5. Perform a same-document back navigation, to a NavigationEntry before the
  // iframe existed.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  EXPECT_EQ(child_url1, root->child_at(0)->current_url());

  // 6. Go forward two entries (cross-document in the iframe) and wait for all
  // navigations to stop. Checking the URL in this case ensures that we do not
  // incorrectly skip navigating the iframe if it needs to be navigated.
  {
    TestNavigationObserver forward_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoToOffset(2);
    forward_load_observer.Wait();
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  }
  EXPECT_EQ(child_url2, root->child_at(0)->current_url());
}

// Verify that the main frame can navigate a grandchild frame to about:blank,
// even if GetFrameEntry might not find the corresponding FrameNavigationEntry
// due to https://crbug.com/608402.  Prevents regression of
// https://crbug.com/1054209.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_BackSameDocumentThenNestedBlank) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1. Perform same document navigation.
  {
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "history.pushState({}, 'foo', 'foo')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
  }

  // 2. Create an iframe.
  GURL child_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, child_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // 3. Create a cross-process nested iframe.
  GURL grandchild_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root->child_at(0),
                       JsReplace(kAddFrameWithSrcScript, grandchild_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // 4. Perform same document back navigation.  The subframes are still present
  // but the previous NavigationEntry doesn't have a record of them (per
  // https://crbug.com/608402).
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }

  // 5. Navigate the nested iframe to about:blank in the main frame's process.
  // This should end up in the main frame's process (without crashing).
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0)->child_at(0));
    std::string script = "frames[0][0].location.href = 'about:blank'";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());
  }

  // TODO(creis): Check subframe entries once we create them in this case.
  // See https://crbug.com/608402.
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            root->child_at(0)->child_at(0)->current_url());
  EXPECT_EQ(
      root->current_frame_host()->GetSiteInstance(),
      root->child_at(0)->child_at(0)->current_frame_host()->GetSiteInstance());
}

// Verify the tree of FrameNavigationEntries when a nested iframe commits after
// its parent changes its name, in which case we might not find the parent
// FrameNavigationEntry.  Prevents regression of https://crbug.com/600743.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_RenameNestedAutoSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1. Create an iframe.
  GURL child_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, child_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // 2. Change the iframe's name.
  EXPECT_TRUE(ExecJs(root->child_at(0), "window.name = 'foo';"));

  // 3. A nested iframe with a cross-site URL should be able to commit.
  GURL bar_url(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(
        ExecJs(root->child_at(0), JsReplace(kAddFrameWithSrcScript, bar_url)));

    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // TODO(creis): Check subframe entries once we create them in this case.
  // See https://crbug.com/608402.
  EXPECT_EQ(bar_url, root->child_at(0)->child_at(0)->current_url());
}

IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_CrossProcessSameDocumentNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  // 1. Create a cross-process iframe.
  GURL child_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, child_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // 2. Create a grandchild iframe (also cross-process).
  GURL grandchild_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root->child_at(0),
                       JsReplace(kAddFrameWithSrcScript, grandchild_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  NavigationEntryImpl* entry_before_nav = controller.GetLastCommittedEntry();
  scoped_refptr<FrameNavigationEntry> main_entry_before_nav =
      entry_before_nav->GetFrameEntry(root);
  scoped_refptr<FrameNavigationEntry> child_entry_before_nav =
      entry_before_nav->GetFrameEntry(root->child_at(0));
  scoped_refptr<FrameNavigationEntry> grandchild_entry_before_nav =
      entry_before_nav->GetFrameEntry(root->child_at(0)->child_at(0));
  ASSERT_NE(main_entry_before_nav, nullptr);
  ASSERT_NE(child_entry_before_nav, nullptr);
  ASSERT_NE(grandchild_entry_before_nav, nullptr);

  // 4. The main frame navigates the child same-document from a different
  // process.
  GURL child_fragment_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html#fragment"));
  {
    LoadCommittedCapturer capturer(root->child_at(0));
    std::string script =
        JsReplace("frames[0].location = $1;", child_fragment_url);
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
  }

  NavigationEntryImpl* entry_after_nav = controller.GetLastCommittedEntry();
  // Only the navigated frame's FrameNavigationEntry should change.
  EXPECT_EQ(main_entry_before_nav, entry_after_nav->GetFrameEntry(root));
  EXPECT_NE(child_entry_before_nav,
            entry_after_nav->GetFrameEntry(root->child_at(0)));
  EXPECT_EQ(child_fragment_url,
            entry_after_nav->GetFrameEntry(root->child_at(0))->url());
  EXPECT_EQ(grandchild_entry_before_nav,
            entry_after_nav->GetFrameEntry(root->child_at(0)->child_at(0)));
}

// Verify the tree of FrameNavigationEntries after NAVIGATION_TYPE_AUTO_SUBFRAME
// commits.
// TODO(creis): Test updating entries for history auto subframe navigations.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_AutoSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1. Create a same-site iframe.
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, frame_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // Check last committed NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  scoped_refptr<FrameNavigationEntry> root_entry =
      entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());
  EXPECT_FALSE(root_entry->initiator_origin().has_value());
  EXPECT_FALSE(controller.GetPendingEntry());

  // The entry should now have a subframe FrameNavigationEntry.
  ASSERT_EQ(1U, entry->root_node()->children.size());
  scoped_refptr<FrameNavigationEntry> frame_entry =
      entry->root_node()->children[0]->frame_entry.get();
  EXPECT_EQ(frame_url, frame_entry->url());
  ASSERT_TRUE(frame_entry->initiator_origin().has_value());
  EXPECT_EQ(url::Origin::Create(main_url),
            frame_entry->initiator_origin().value());
  EXPECT_FALSE(root->child_at(0)->is_on_initial_empty_document());

  // 2. Create a second, initially cross-site iframe.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, foo_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // The last committed NavigationEntry shouldn't have changed.
  EXPECT_EQ(1, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());
  EXPECT_FALSE(root_entry->initiator_origin().has_value());
  EXPECT_FALSE(controller.GetPendingEntry());

  // The entry should now have 2 subframe FrameNavigationEntries.
  ASSERT_EQ(2U, entry->root_node()->children.size());
  frame_entry = entry->root_node()->children[1]->frame_entry.get();
  EXPECT_EQ(foo_url, frame_entry->url());
  ASSERT_TRUE(frame_entry->initiator_origin().has_value());
  EXPECT_EQ(url::Origin::Create(main_url),
            frame_entry->initiator_origin().value());
  EXPECT_FALSE(root->child_at(1)->is_on_initial_empty_document());

  // 3. Create a nested iframe in the second subframe.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(
        ExecJs(root->child_at(1), JsReplace(kAddFrameWithSrcScript, foo_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // The last committed NavigationEntry shouldn't have changed.
  EXPECT_EQ(1, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());
  EXPECT_FALSE(root_entry->initiator_origin().has_value());

  // The entry should now have 2 subframe FrameNavigationEntries.
  ASSERT_EQ(2U, entry->root_node()->children.size());
  ASSERT_EQ(1U, entry->root_node()->children[1]->children.size());
  frame_entry = entry->root_node()->children[1]->children[0]->frame_entry.get();
  EXPECT_EQ(foo_url, frame_entry->url());
  ASSERT_TRUE(frame_entry->initiator_origin().has_value());
  EXPECT_EQ(url::Origin::Create(foo_url),
            frame_entry->initiator_origin().value());

  // 4. Create a third iframe on the same site as the second.  This ensures that
  // the commit type is correct even when the subframe process already exists.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, foo_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // The last committed NavigationEntry shouldn't have changed.
  EXPECT_EQ(1, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());
  EXPECT_FALSE(root_entry->initiator_origin().has_value());

  // The entry should now have 3 subframe FrameNavigationEntries.
  ASSERT_EQ(3U, entry->root_node()->children.size());
  frame_entry = entry->root_node()->children[2]->frame_entry.get();
  EXPECT_EQ(foo_url, frame_entry->url());
  ASSERT_TRUE(frame_entry->initiator_origin().has_value());
  EXPECT_EQ(url::Origin::Create(main_url),
            frame_entry->initiator_origin().value());

  // 5. Create a nested iframe on the original site (A-B-A).
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    FrameTreeNode* child = root->child_at(2);
    EXPECT_TRUE(ExecJs(child, JsReplace(kAddFrameWithSrcScript, frame_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // The last committed NavigationEntry shouldn't have changed.
  EXPECT_EQ(1, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());
  EXPECT_FALSE(root_entry->initiator_origin().has_value());

  // There should be a corresponding FrameNavigationEntry.
  ASSERT_EQ(1U, entry->root_node()->children[2]->children.size());
  frame_entry = entry->root_node()->children[2]->children[0]->frame_entry.get();
  EXPECT_EQ(frame_url, frame_entry->url());
  ASSERT_TRUE(frame_entry->initiator_origin().has_value());
  EXPECT_EQ(url::Origin::Create(foo_url),
            frame_entry->initiator_origin().value());

  // Check the end result of the frame tree.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site A ------- proxies for B\n"
        "   |--Site B ------- proxies for A\n"
        "   |    +--Site B -- proxies for A\n"
        "   +--Site B ------- proxies for A\n"
        "        +--Site A -- proxies for B\n"
        "Where A = http://127.0.0.1/\n"
        "      B = http://foo.com/",
        DepictFrameTree(*root));
  }
}

// Verify the tree of FrameNavigationEntries after NAVIGATION_TYPE_NEW_SUBFRAME
// commits.
// Disabled due to flakes; see https://crbug.com/646836.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_NewSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1. Create a same-site iframe.
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, frame_url)));
    capturer.Wait();
  }
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();

  // 2. Navigate in the subframe same-site.
  GURL frame_url2(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url2));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
  }

  // We should have created a new NavigationEntry with the same main frame URL.
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();
  EXPECT_NE(entry, entry2);
  EXPECT_EQ(main_url, entry2->GetURL());
  scoped_refptr<FrameNavigationEntry> root_entry2 =
      entry2->root_node()->frame_entry.get();
  EXPECT_EQ(entry->root_node()->frame_entry.get(), root_entry2);
  EXPECT_EQ(main_url, root_entry2->url());
  EXPECT_FALSE(root_entry2->initiator_origin().has_value());

  // The entry should have a new FrameNavigationEntries for the subframe.
  ASSERT_EQ(1U, entry2->root_node()->children.size());
  EXPECT_NE(entry->root_node()->children[0]->frame_entry.get(),
            entry2->root_node()->children[0]->frame_entry.get());
  EXPECT_EQ(frame_url2, entry2->root_node()->children[0]->frame_entry->url());

  // 3. Create a second, initially cross-site iframe.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, foo_url)));
    capturer.Wait();
  }

  // 4. Create a nested same-site iframe in the second subframe, wait for it to
  // commit, then navigate it again cross-site.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(
        ExecJs(root->child_at(1), JsReplace(kAddFrameWithSrcScript, foo_url)));
    capturer.Wait();
  }
  GURL bar_url(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/simple_page_1.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(1)->child_at(0));
    RenderFrameDeletedObserver deleted_observer(
        root->child_at(1)->child_at(0)->current_frame_host());
    EXPECT_TRUE(
        NavigateToURLFromRenderer(root->child_at(1)->child_at(0), bar_url));
    // Wait for the RenderFrame to go away, if this will be cross-process.
    if (AreAllSitesIsolatedForTesting())
      deleted_observer.WaitUntilDeleted();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
  }

  // We should have created a new NavigationEntry with the same main frame URL.
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry3 = controller.GetLastCommittedEntry();
  EXPECT_NE(entry, entry3);
  EXPECT_EQ(main_url, entry3->GetURL());
  scoped_refptr<FrameNavigationEntry> root_entry3 =
      entry3->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry3->url());
  EXPECT_EQ(root_entry2, root_entry3);

  // The entry should still have FrameNavigationEntries for all 3 subframes.
  ASSERT_EQ(2U, entry3->root_node()->children.size());
  EXPECT_EQ(entry2->root_node()->children[0]->frame_entry.get(),
            entry3->root_node()->children[0]->frame_entry.get());
  EXPECT_EQ(entry2->root_node()->children[1]->frame_entry.get(),
            entry3->root_node()->children[1]->frame_entry.get());
  EXPECT_NE(entry2->root_node()->children[1]->children[0]->frame_entry.get(),
            entry3->root_node()->children[1]->children[0]->frame_entry.get());
  EXPECT_EQ(foo_url, entry3->root_node()->children[1]->frame_entry->url());
  ASSERT_EQ(1U, entry3->root_node()->children[1]->children.size());
  EXPECT_EQ(bar_url,
            entry3->root_node()->children[1]->children[0]->frame_entry->url());

  // 6. Navigate the second subframe cross-site, clearing its existing subtree.
  GURL baz_url(embedded_test_server()->GetURL(
      "baz.com", "/navigation_controller/simple_page_1.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(1));
    RenderFrameDeletedObserver deleted_observer(
        root->child_at(1)->current_frame_host());
    std::string script =
        "var frames = document.getElementsByTagName('iframe');"
        "frames[1].src = '" +
        baz_url.spec() + "';";
    EXPECT_TRUE(ExecJs(root, script));
    // Wait for the RenderFrame to go away, if this will be cross-process.
    if (AreAllSitesIsolatedForTesting())
      deleted_observer.WaitUntilDeleted();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
  }

  // We should have created a new NavigationEntry with the same main frame URL.
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry4 = controller.GetLastCommittedEntry();
  EXPECT_NE(entry, entry4);
  EXPECT_EQ(main_url, entry4->GetURL());
  scoped_refptr<FrameNavigationEntry> root_entry4 =
      entry4->root_node()->frame_entry.get();
  EXPECT_EQ(root_entry3, root_entry4);
  EXPECT_EQ(main_url, root_entry4->url());

  // The entry should still have FrameNavigationEntries for all 3 subframes.
  ASSERT_EQ(2U, entry4->root_node()->children.size());
  EXPECT_EQ(entry3->root_node()->children[0]->frame_entry.get(),
            entry4->root_node()->children[0]->frame_entry.get());
  EXPECT_EQ(baz_url, entry4->root_node()->children[1]->frame_entry->url());
  ASSERT_EQ(0U, entry4->root_node()->children[1]->children.size());

  // Check the end result of the frame tree.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site A ------- proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://127.0.0.1/\n"
        "      B = http://baz.com/",
        DepictFrameTree(*root));
  }
}

// Ensure that we don't crash when navigating subframes after same document
// navigations.  See https://crbug.com/522193.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_SubframeAfterSameDocument) {
  // 1. Start on a page with a subframe.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  // Navigate to a real page in the subframe, so that the next navigation will
  // be MANUAL_SUBFRAME.
  GURL subframe_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(root->child_at(0));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), subframe_url));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // 2. Same document navigation in the main frame.
  std::string push_script = "history.pushState({}, 'page 2', 'page_2.html')";
  EXPECT_TRUE(ExecJs(root, push_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The entry should have a FrameNavigationEntry for the subframe.
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  ASSERT_EQ(1U, entry->root_node()->children.size());
  EXPECT_EQ(subframe_url, entry->root_node()->children[0]->frame_entry->url());

  // 3. Add a nested subframe.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root->child_at(0),
                       JsReplace(kAddFrameWithSrcScript, subframe_url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // The entry should have a FrameNavigationEntry for the subframe.
  entry = controller.GetLastCommittedEntry();
  ASSERT_EQ(1U, entry->root_node()->children.size());
  EXPECT_EQ(subframe_url, entry->root_node()->children[0]->frame_entry->url());
  ASSERT_EQ(1U, entry->root_node()->children[0]->children.size());
  EXPECT_EQ(subframe_url,
            entry->root_node()->children[0]->children[0]->frame_entry->url());
}

// Verify the tree of FrameNavigationEntries after back/forward navigations in a
// cross-site subframe.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_SubframeBackForward) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1. Create a same-site iframe.
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, frame_url)));
    capturer.Wait();
  }
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();

  // 2. Navigate in the subframe cross-site.
  GURL frame_url2(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/page_with_links.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url2));
    capturer.Wait();
  }
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // 3. Navigate in the subframe cross-site again.
  GURL frame_url3(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/page_with_links.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url3));
    capturer.Wait();
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry3 = controller.GetLastCommittedEntry();
  EXPECT_EQ(entry1->root_node()->frame_entry.get(),
            entry2->root_node()->frame_entry.get());
  EXPECT_EQ(entry2->root_node()->frame_entry.get(),
            entry3->root_node()->frame_entry.get());
  EXPECT_NE(entry1->root_node()->children[0]->frame_entry.get(),
            entry2->root_node()->children[0]->frame_entry.get());
  EXPECT_NE(entry1->root_node()->children[0]->frame_entry.get(),
            entry3->root_node()->children[0]->frame_entry.get());
  EXPECT_NE(entry2->root_node()->children[0]->frame_entry.get(),
            entry3->root_node()->children[0]->frame_entry.get());

  // 4. Go back in the subframe.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry2, controller.GetLastCommittedEntry());

  // The entry should have a FrameNavigationEntry for the subframe.
  ASSERT_EQ(1U, entry2->root_node()->children.size());
  EXPECT_EQ(frame_url2, entry2->root_node()->children[0]->frame_entry->url());

  // 5. Go back in the subframe again to the parent page's site.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry1, controller.GetLastCommittedEntry());

  // The entry should have a FrameNavigationEntry for the subframe.
  ASSERT_EQ(1U, entry1->root_node()->children.size());
  EXPECT_EQ(frame_url, entry1->root_node()->children[0]->frame_entry->url());

  // 6. Go forward in the subframe cross-site.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry2, controller.GetLastCommittedEntry());

  // The entry should have a FrameNavigationEntry for the subframe.
  ASSERT_EQ(1U, entry2->root_node()->children.size());
  EXPECT_EQ(frame_url2, entry2->root_node()->children[0]->frame_entry->url());

  // 7. Go forward in the subframe again, cross-site.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry3, controller.GetLastCommittedEntry());

  // The entry should have a FrameNavigationEntry for the subframe.
  ASSERT_EQ(1U, entry3->root_node()->children.size());
  EXPECT_EQ(frame_url3, entry3->root_node()->children[0]->frame_entry->url());
}

// Verify the tree of FrameNavigationEntries after subframes are recreated in
// history navigations, including nested frames.  The history will look like:
// 1. initial_url
// 2. main_url_a (data_url)
// 3. main_url_a (frame_url_b (data_url))
// 4. main_url_a (frame_url_b (frame_url_c))
// 5. main_url_d
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_RecreatedSubframeBackForward) {
  // 1. Start on a page with no frames.
  GURL initial_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(initial_url, root->current_url());
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();
  EXPECT_EQ(0U, entry1->root_node()->children.size());

  // 2. Navigate to a page with a data URL iframe.
  GURL main_url_a(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/page_with_data_iframe.html"));
  GURL data_url("data:text/html,Subframe");
  EXPECT_TRUE(NavigateToURL(shell(), main_url_a));
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // The entry should have a FrameNavigationEntry for the data subframe.
  ASSERT_EQ(1U, entry2->root_node()->children.size());
  EXPECT_EQ(data_url, entry2->root_node()->children[0]->frame_entry->url());
  EXPECT_EQ(entry2->root_node()
                ->frame_entry->committed_origin()
                ->GetTupleOrPrecursorTupleIfOpaque(),
            entry2->root_node()
                ->children[0]
                ->frame_entry->committed_origin()
                ->GetTupleOrPrecursorTupleIfOpaque());
  ASSERT_TRUE(entry2->root_node()
                  ->children[0]
                  ->frame_entry->initiator_origin()
                  .has_value());
  EXPECT_EQ(url::Origin::Create(main_url_a),
            entry2->root_node()
                ->children[0]
                ->frame_entry->initiator_origin()
                .value());

  // 3. Navigate the iframe cross-site to a page with a nested iframe.
  GURL frame_url_b(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/page_with_data_iframe.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url_b));
    capturer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->child_at(0)->current_url());

  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry3 = controller.GetLastCommittedEntry();

  // The entry should have a FrameNavigationEntry for the b.com subframe.
  ASSERT_EQ(1U, entry3->root_node()->children.size());
  ASSERT_EQ(1U, entry3->root_node()->children[0]->children.size());
  EXPECT_EQ(entry2->root_node()->frame_entry.get(),
            entry3->root_node()->frame_entry.get());
  EXPECT_NE(entry2->root_node()->children[0]->frame_entry.get(),
            entry3->root_node()->children[0]->frame_entry.get());
  EXPECT_EQ(frame_url_b, entry3->root_node()->children[0]->frame_entry->url());
  EXPECT_EQ(data_url,
            entry3->root_node()->children[0]->children[0]->frame_entry->url());
  EXPECT_EQ(entry3->root_node()
                ->children[0]
                ->frame_entry->committed_origin()
                ->GetTupleOrPrecursorTupleIfOpaque(),
            entry3->root_node()
                ->children[0]
                ->children[0]
                ->frame_entry->committed_origin()
                ->GetTupleOrPrecursorTupleIfOpaque());

  // 4. Navigate the nested iframe cross-site.
  GURL frame_url_c(embedded_test_server()->GetURL(
      "c.com", "/navigation_controller/simple_page_2.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0)->child_at(0));
    EXPECT_TRUE(
        NavigateToURLFromRenderer(root->child_at(0)->child_at(0), frame_url_c));
    capturer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());
  EXPECT_EQ(frame_url_c, root->child_at(0)->child_at(0)->current_url());

  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry4 = controller.GetLastCommittedEntry();

  // The entry should have FrameNavigationEntries for the subframes.
  ASSERT_EQ(1U, entry4->root_node()->children.size());
  ASSERT_EQ(1U, entry4->root_node()->children[0]->children.size());
  EXPECT_EQ(entry3->root_node()->frame_entry.get(),
            entry4->root_node()->frame_entry.get());
  EXPECT_EQ(entry3->root_node()->children[0]->frame_entry.get(),
            entry4->root_node()->children[0]->frame_entry.get());
  EXPECT_NE(entry3->root_node()->children[0]->children[0]->frame_entry.get(),
            entry4->root_node()->children[0]->children[0]->frame_entry.get());
  EXPECT_EQ(frame_url_b, entry4->root_node()->children[0]->frame_entry->url());
  EXPECT_EQ(frame_url_c,
            entry4->root_node()->children[0]->children[0]->frame_entry->url());

  // Remember the DSNs for later.
  int64_t root_dsn =
      entry4->root_node()->frame_entry->document_sequence_number();
  int64_t frame_b_dsn =
      entry4->root_node()->children[0]->frame_entry->document_sequence_number();
  int64_t frame_c_dsn = entry4->root_node()
                            ->children[0]
                            ->children[0]
                            ->frame_entry->document_sequence_number();

  // 5. Navigate main frame cross-site, destroying the frames.
  GURL main_url_d(embedded_test_server()->GetURL(
      "d.com", "/navigation_controller/simple_page_2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url_d));
  ASSERT_EQ(0U, root->child_count());
  EXPECT_EQ(main_url_d, root->current_url());

  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(4, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry5 = controller.GetLastCommittedEntry();
  EXPECT_EQ(0U, entry5->root_node()->children.size());

  // 6. Go back, recreating the iframe and its nested iframe.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(1U, root->child_at(0)->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());
  EXPECT_EQ(frame_url_c, root->child_at(0)->child_at(0)->current_url());

  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry4, controller.GetLastCommittedEntry());

  // The main frame should not have changed its DSN.
  EXPECT_EQ(root_dsn,
            entry4->root_node()->frame_entry->document_sequence_number());

  // The entry should have FrameNavigationEntries for the subframes.
  ASSERT_EQ(1U, entry4->root_node()->children.size());
  ASSERT_EQ(1U, entry4->root_node()->children[0]->children.size());
  EXPECT_EQ(frame_url_b, entry4->root_node()->children[0]->frame_entry->url());
  EXPECT_EQ(frame_url_c,
            entry4->root_node()->children[0]->children[0]->frame_entry->url());
  // The subframes should not have changed their DSNs.
  // See https://crbug.com/628286.
  EXPECT_EQ(frame_b_dsn, entry4->root_node()
                             ->children[0]
                             ->frame_entry->document_sequence_number());
  EXPECT_EQ(frame_c_dsn, entry4->root_node()
                             ->children[0]
                             ->children[0]
                             ->frame_entry->document_sequence_number());

  // Inject a JS value so that we can check for it later.
  EXPECT_TRUE(ExecJs(root, "foo=3;"));

  // 7. Go back again, to the data URL in the nested iframe.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(1U, root->child_at(0)->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->child_at(0)->current_url());

  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry3, controller.GetLastCommittedEntry());

  // The entry should have FrameNavigationEntries for the subframes.
  ASSERT_EQ(1U, entry3->root_node()->children.size());
  ASSERT_EQ(1U, entry3->root_node()->children[0]->children.size());
  EXPECT_EQ(frame_url_b, entry3->root_node()->children[0]->frame_entry->url());
  EXPECT_EQ(data_url,
            entry3->root_node()->children[0]->children[0]->frame_entry->url());
  EXPECT_EQ(entry3->root_node()
                ->children[0]
                ->frame_entry->committed_origin()
                ->GetTupleOrPrecursorTupleIfOpaque(),
            entry3->root_node()
                ->children[0]
                ->children[0]
                ->frame_entry->committed_origin()
                ->GetTupleOrPrecursorTupleIfOpaque());

  // Verify that we did not reload the main frame. See https://crbug.com/586234.
  EXPECT_EQ(3, EvalJs(root, "foo"));

  // 8. Go back again, to the data URL in the first subframe.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry2, controller.GetLastCommittedEntry());

  // The entry should have a FrameNavigationEntry for the subframe.
  ASSERT_EQ(1U, entry2->root_node()->children.size());
  EXPECT_EQ(data_url, entry2->root_node()->children[0]->frame_entry->url());
  EXPECT_EQ(entry2->root_node()
                ->frame_entry->committed_origin()
                ->GetTupleOrPrecursorTupleIfOpaque(),
            entry2->root_node()
                ->children[0]
                ->frame_entry->committed_origin()
                ->GetTupleOrPrecursorTupleIfOpaque());
  ASSERT_TRUE(entry2->root_node()
                  ->children[0]
                  ->frame_entry->initiator_origin()
                  .has_value());
  EXPECT_EQ(url::Origin::Create(main_url_a),
            entry2->root_node()
                ->children[0]
                ->frame_entry->initiator_origin()
                .value());

  // 9. Go back again, to the initial main frame page.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  ASSERT_EQ(0U, root->child_count());
  EXPECT_EQ(initial_url, root->current_url());

  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry1, controller.GetLastCommittedEntry());
  EXPECT_EQ(0U, entry1->root_node()->children.size());

  // 10. Go forward multiple entries and verify the correct subframe URLs load.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoToOffset(2);
    back_load_observer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->child_at(0)->current_url());

  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry3, controller.GetLastCommittedEntry());

  // The entry should have FrameNavigationEntries for the subframes.
  ASSERT_EQ(1U, entry3->root_node()->children.size());
  EXPECT_EQ(frame_url_b, entry3->root_node()->children[0]->frame_entry->url());
  EXPECT_EQ(data_url,
            entry3->root_node()->children[0]->children[0]->frame_entry->url());
  EXPECT_EQ(entry3->root_node()
                ->children[0]
                ->frame_entry->committed_origin()
                ->GetTupleOrPrecursorTupleIfOpaque(),
            entry3->root_node()
                ->children[0]
                ->children[0]
                ->frame_entry->committed_origin()
                ->GetTupleOrPrecursorTupleIfOpaque());
}

// Verify that we navigate to the fallback (original) URL if a subframe's
// FrameNavigationEntry can't be found during a history navigation.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_SubframeHistoryFallback) {
  // 1. Start on a page with a data URL iframe.
  GURL main_url_a(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/page_with_data_iframe.html"));
  GURL data_url("data:text/html,Subframe");
  EXPECT_TRUE(NavigateToURL(shell(), main_url_a));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();

  // The entry should have a FrameNavigationEntry for the data subframe.
  ASSERT_EQ(1U, entry1->root_node()->children.size());
  EXPECT_EQ(data_url, entry1->root_node()->children[0]->frame_entry->url());

  // 2. Navigate the iframe cross-site.
  GURL frame_url_b(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/simple_page_1.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url_b));
    capturer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // The entry should have a FrameNavigationEntry for the b.com subframe.
  ASSERT_EQ(1U, entry2->root_node()->children.size());
  EXPECT_EQ(entry1->root_node()->frame_entry.get(),
            entry2->root_node()->frame_entry.get());
  EXPECT_NE(entry1->root_node()->children[0]->frame_entry.get(),
            entry2->root_node()->children[0]->frame_entry.get());
  EXPECT_EQ(frame_url_b, entry2->root_node()->children[0]->frame_entry->url());

  // 3. Navigate main frame cross-site, destroying the frames.
  GURL main_url_c(embedded_test_server()->GetURL(
      "c.com", "/navigation_controller/simple_page_2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url_c));
  ASSERT_EQ(0U, root->child_count());
  EXPECT_EQ(main_url_c, root->current_url());

  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry3 = controller.GetLastCommittedEntry();
  EXPECT_EQ(0U, entry3->root_node()->children.size());

  // Force the subframe entry to have the wrong name, so that it isn't found
  // when we go back.
  entry2->root_node()->children[0]->frame_entry->set_frame_unique_name("wrong");

  // With BackForwardCache page is restored from cache instead of getting
  // recreated on history navigation, disable back/forward cache to force a
  // reload and a URL fetch.
  DisableBackForwardCacheForTesting(contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // 4. Go back, recreating the iframe. The subframe entry won't be found, and
  // we should fall back to the default URL.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry2, controller.GetLastCommittedEntry());

  // The entry should have both the stale FrameNavigationEntry with the old
  // name and the new FrameNavigationEntry for the fallback navigation.
  ASSERT_EQ(2U, entry2->root_node()->children.size());
  EXPECT_EQ(frame_url_b, entry2->root_node()->children[0]->frame_entry->url());
  EXPECT_EQ(data_url, entry2->root_node()->children[1]->frame_entry->url());
}

// Allows waiting until an URL with a data scheme commits in any frame.
class DataUrlCommitObserver : public WebContentsObserver {
 public:
  explicit DataUrlCommitObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void Wait() { loop_.Run(); }

 private:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->HasCommitted() &&
        !navigation_handle->IsErrorPage() &&
        navigation_handle->GetURL().scheme() == "data") {
      loop_.Quit();
    }
  }

  base::RunLoop loop_;
};

// Verify that dynamically generated iframes load properly during a history
// navigation if no history item can be found for them.
// See https://crbug.com/649345.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_DynamicSubframeHistoryFallback) {
  // 1. Start on a page with a script-generated iframe.  The iframe has a
  // dynamic name, starts at about:blank, and gets navigated to a dynamic data
  // URL as the page is loading.
  GURL main_url_a(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/dynamic_iframe.html"));
  {
    // Wait until the data URL has committed, even if load stop happens after
    // about:blank load.
    DataUrlCommitObserver data_observer(shell()->web_contents());
    EXPECT_TRUE(NavigateToURL(shell(), main_url_a));
    data_observer.Wait();
  }
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ("data", root->child_at(0)->current_url().scheme());

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();

  // The entry should have a FrameNavigationEntry for the data subframe.
  ASSERT_EQ(1U, entry1->root_node()->children.size());
  EXPECT_EQ("data",
            entry1->root_node()->children[0]->frame_entry->url().scheme());

  // 2. Navigate main frame cross-site, destroying the frames.
  GURL main_url_b(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/simple_page_2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url_b));
  ASSERT_EQ(0U, root->child_count());
  EXPECT_EQ(main_url_b, root->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();
  EXPECT_EQ(0U, entry2->root_node()->children.size());

  // With BackForwardCache page is restored from cache instead of getting
  // recreated on history navigation, disable back/forward cache to force a
  // reload and a URL fetch.
  DisableBackForwardCacheForTesting(contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // 3. Go back, recreating the iframe.  The subframe will have a new name this
  // time, so we won't find a history item for it.  We should let the new data
  // URL be loaded into it, rather than clobbering it with an about:blank page.
  {
    // Wait until the data URL has committed, even if load stop happens first.
    DataUrlCommitObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ("data", root->child_at(0)->current_url().scheme());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry1, controller.GetLastCommittedEntry());

  // There should be only 1 FNE, because when the child frame is dynamically
  // created or recreated from javascript, it's FNE will be removed when the
  // frame is removed.
  ASSERT_EQ(1U, entry1->root_node()->children.size());
  EXPECT_EQ("data",
            entry1->root_node()->children[0]->frame_entry->url().scheme());

  // The iframe commit should have been classified AUTO_SUBFRAME and not
  // NEW_SUBFRAME, so we should still be able to go forward.
  EXPECT_TRUE(shell()->web_contents()->GetController().CanGoForward());
}

// Verify that we don't clobber any content injected into the initial blank page
// if we go back to an about:blank subframe.  See https://crbug.com/626416.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_RecreatedBlankSubframe) {
  // 1. Start on a page that injects content into an about:blank iframe.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/inject_into_blank_iframe.html"));
  GURL blank_url(url::kAboutBlankURL);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(blank_url, root->child_at(0)->current_url());

  // Verify that the parent was able to script the iframe.
  EXPECT_EQ("Injected text",
            EvalJs(root->child_at(0), "document.body.innerHTML"));

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();

  // The entry should have a FrameNavigationEntry for the blank subframe.
  ASSERT_EQ(1U, entry->root_node()->children.size());
  scoped_refptr<FrameNavigationEntry> child_frame_entry =
      entry->root_node()->children[0]->frame_entry.get();
  EXPECT_EQ(blank_url, child_frame_entry->url());

  // 2. Navigate the main frame, destroying the frames.
  GURL main_url_2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url_2));
  ASSERT_EQ(0U, root->child_count());
  EXPECT_EQ(main_url_2, root->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // 3. Go back, recreating the iframe.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    controller.GoBack();
    back_load_observer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(blank_url, root->child_at(0)->current_url());

  // Verify that the parent was able to script the iframe.
  EXPECT_EQ("Injected text",
            EvalJs(root->child_at(0), "document.body.innerHTML"));

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());

  // The entry should have the same FrameNavigationEntry for the blank subframe.
  ASSERT_EQ(1U, entry->root_node()->children.size());
  EXPECT_EQ(child_frame_entry,
            entry->root_node()->children[0]->frame_entry.get());
  EXPECT_EQ(blank_url, entry->root_node()->children[0]->frame_entry->url());
}

// Verify that we correctly load nested iframes injected into a page if we go
// back and recreate them.  Also confirm that form values are not restored for
// forms injected into about:blank pages.  See https://crbug.com/657896.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_RecreatedInjectedBlankSubframe) {
  // The test assumes the previous iframe gets deleted after navigation and
  // later recreated on history navigations. Disable back/forward cache to
  // ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // 1. Start on a page that injects a nested iframe into an injected
  // about:blank iframe.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/inject_subframe_into_blank_iframe.html"));
  GURL blank_url(url::kAboutBlankURL);
  GURL inner_url(
      embedded_test_server()->GetURL("/navigation_controller/form.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Verify that the inner iframe was able to load.
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(1U, root->child_at(0)->child_count());
  ASSERT_EQ(0U, root->child_at(0)->child_at(0)->child_count());
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(blank_url, root->child_at(0)->current_url());
  EXPECT_EQ(inner_url, root->child_at(0)->child_at(0)->current_url());

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();

  // The entry should have FrameNavigationEntries for the subframes.
  ASSERT_EQ(1U, entry->root_node()->children.size());
  scoped_refptr<FrameNavigationEntry> child_frame_entry =
      entry->root_node()->children[0]->frame_entry.get();
  EXPECT_EQ(blank_url, entry->root_node()->children[0]->frame_entry->url());
  EXPECT_EQ(inner_url,
            entry->root_node()->children[0]->children[0]->frame_entry->url());

  // Set a value in the form which will be stored in the PageState.
  EXPECT_TRUE(ExecJs(root->child_at(0)->child_at(0),
                     "document.getElementById('itext').value = 'modified';"));

  // 2. Navigate the main frame same-site, destroying the subframes.
  GURL main_url_2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url_2));
  ASSERT_EQ(0U, root->child_count());
  EXPECT_EQ(main_url_2, root->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // 3. Go back, recreating the subframes.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    controller.GoBack();
    back_load_observer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(blank_url, root->child_at(0)->current_url());

  // Verify that the inner iframe went to the correct URL.
  EXPECT_EQ(inner_url, root->child_at(0)->child_at(0)->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());

  // There is only 1 child frame in the frame tree and only 1 FNE, because when
  // the child frame is dynamically created or recreated from javascript, it's
  // FNE will be removed when the frame is removed.
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(1U, entry->root_node()->children.size());

  // The entry should have FrameNavigationEntries for the subframes.
  EXPECT_NE(child_frame_entry,
            entry->root_node()->children[0]->frame_entry.get());
  EXPECT_EQ(blank_url, entry->root_node()->children[0]->frame_entry->url());
  EXPECT_EQ(inner_url,
            entry->root_node()->children[0]->children[0]->frame_entry->url());

  // With injected about:blank iframes, we never restore form values from
  // PageState.
  EXPECT_EQ("", EvalJs(root->child_at(0)->child_at(0),
                       "document.getElementById('itext').value"));
}

// Verify that we correctly load a nested iframe created by an injected iframe
// srcdoc if we go back and recreate the frames.
//
// This test is similar to
// NavigationControllerBrowserTest.
//     FrameNavigationEntry_RecreatedInjectedBlankSubframe
// and RenderFrameHostManagerTest.RestoreSubframeFileAccessForHistoryNavigation.
//
// This test worked before and after the fix for https://crbug.com/657896, but
// it failed with a preliminary version of the fix (see also
// https://crbug.com/657896#c9).
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_RecreatedInjectedSrcdocSubframe) {
  // The test assumes the previous iframe gets deleted after navigation and
  // later recreated on history navigations. Disable back/forward cache to
  // ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  // 1. Start on a page that injects a nested iframe srcdoc which contains a
  // nested iframe.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/inject_iframe_srcdoc_with_nested_frame.html"));
  GURL inner_url(
      embedded_test_server()->GetURL("/navigation_controller/form.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Verify that the inner iframe was able to load.
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(1U, root->child_at(0)->child_count());
  ASSERT_EQ(0U, root->child_at(0)->child_at(0)->child_count());
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_TRUE(root->child_at(0)->current_url().IsAboutSrcdoc());
  EXPECT_EQ(inner_url, root->child_at(0)->child_at(0)->current_url());

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();

  // The entry should have FrameNavigationEntries for the subframes.
  ASSERT_EQ(1U, entry->root_node()->children.size());
  scoped_refptr<FrameNavigationEntry> child_frame_entry =
      entry->root_node()->children[0]->frame_entry.get();
  EXPECT_TRUE(
      entry->root_node()->children[0]->frame_entry->url().IsAboutSrcdoc());
  EXPECT_EQ(inner_url,
            entry->root_node()->children[0]->children[0]->frame_entry->url());

  // Set a value in the form which will be stored in the PageState.
  EXPECT_TRUE(ExecJs(root->child_at(0)->child_at(0),
                     "document.getElementById('itext').value = 'modified';"));

  // 2. Navigate the main frame same-site, destroying the subframes.
  GURL main_url_2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url_2));
  ASSERT_EQ(0U, root->child_count());
  EXPECT_EQ(main_url_2, root->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // 3. Go back, recreating the subframes.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    controller.GoBack();
    back_load_observer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(1U, root->child_at(0)->child_count());
  ASSERT_EQ(0U, root->child_at(0)->child_at(0)->child_count());
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_TRUE(root->child_at(0)->current_url().IsAboutSrcdoc());

  // Verify that the inner iframe went to the correct URL.
  EXPECT_EQ(inner_url, root->child_at(0)->child_at(0)->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());

  // There is only 1 child frame in the frame tree and only 1 FNE, because when
  // the child frame is dynamically created or recreated from javascript, it's
  // FNE will be removed when the frame is removed.
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(1U, entry->root_node()->children.size());

  // The entry should have FrameNavigationEntries for the subframes.
  EXPECT_NE(child_frame_entry,
            entry->root_node()->children[0]->frame_entry.get());
  EXPECT_TRUE(
      entry->root_node()->children[0]->frame_entry->url().IsAboutSrcdoc());
  EXPECT_EQ(inner_url,
            entry->root_node()->children[0]->children[0]->frame_entry->url());

  // With *injected* iframe srcdoc pages, we don't restore form values from
  // PageState (because iframes injected by javascript always get a fresh,
  // random unique name each time they are created or recreated - see
  // https://crbug.com/500260).
  //
  // Note that restoring form values in srcdoc frames created via static html is
  // expected to work and is tested by
  // RenderFrameHostManagerTest.RestoreSubframeFileAccessForHistoryNavigation.
  EXPECT_EQ("", EvalJs(root->child_at(0)->child_at(0),
                       "document.getElementById('itext').value"));
}

// Verify that we can load about:blank in an iframe when going back to a page,
// if that iframe did not originally have about:blank in it.  See
// https://crbug.com/657896.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_RecreatedSubframeToBlank) {
  // 1. Start on a page with a data iframe.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  GURL data_url("data:text/html,Subframe");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  // 2. Navigate the subframe to about:blank.
  GURL blank_url(url::kAboutBlankURL);
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), blank_url));
  EXPECT_EQ(blank_url, root->child_at(0)->current_url());
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();

  // The entry should have a FrameNavigationEntry for the blank subframe.
  ASSERT_EQ(1U, entry->root_node()->children.size());
  scoped_refptr<FrameNavigationEntry> blank_entry =
      entry->root_node()->children[0]->frame_entry.get();
  EXPECT_EQ(blank_url, blank_entry->url());

  // 3. Navigate the main frame, destroying the frames.
  GURL main_url_2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url_2));
  ASSERT_EQ(0U, root->child_count());
  EXPECT_EQ(main_url_2, root->current_url());

  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  // 3. Go back, recreating the iframe.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    controller.GoBack();
    back_load_observer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(blank_url, root->child_at(0)->current_url());

  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());

  // The entry should have a FrameNavigationEntry for the blank subframe.
  ASSERT_EQ(1U, entry->root_node()->children.size());
  EXPECT_EQ(blank_entry, entry->root_node()->children[0]->frame_entry.get());
  EXPECT_EQ(blank_url, entry->root_node()->children[0]->frame_entry->url());
}

// Ensure we don't crash if an onload handler removes an about:blank frame after
// recreating it on a back/forward.  See https://crbug.com/638166.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_RemoveRecreatedBlankSubframe) {
  // 1. Start on a page that removes its about:blank iframe during onload.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/remove_blank_iframe_on_load.html"));
  GURL blank_url(url::kAboutBlankURL);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(main_url, root->current_url());

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();

  // The entry should have a FrameNavigationEntry for the blank subframe, even
  // though it is being removed from the page.
  ASSERT_EQ(1U, entry->root_node()->children.size());
  EXPECT_EQ(blank_url, entry->root_node()->children[0]->frame_entry->url());

  // 2. Navigate the main frame, destroying the frames.
  GURL main_url_2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url_2));
  ASSERT_EQ(0U, root->child_count());
  EXPECT_EQ(main_url_2, root->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // 3. Go back, recreating the iframe (and removing it again).
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    controller.GoBack();
    back_load_observer.Wait();
  }
  EXPECT_EQ(main_url, root->current_url());

  // Check that the renderer is still alive.
  EXPECT_TRUE(ExecJs(shell(), "console.log('Success');"));

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());

  // The entry should have a FrameNavigationEntry for the blank subframe.
  ASSERT_EQ(1U, entry->root_node()->children.size());
  EXPECT_EQ(blank_url, entry->root_node()->children[0]->frame_entry->url());
}

// Verifies that we clear the children FrameNavigationEntries if a history
// navigation redirects, so that we don't try to load previous history items in
// frames of the new page.  This should only clear the children of the frame
// that is redirecting.  See https://crbug.com/585194.
//
// Specifically, this test covers the following interesting cases:
// - Subframe redirect when going back from a different main frame (step 4).
// - Subframe redirect without changing the main frame (step 6).
// - Main frame redirect, clearing the children (step 8).
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_BackWithRedirect) {
  // The test assumes the previous iframe gets deleted after navigation and
  // later recreated on history navigations. Disable back/forward cache to
  // ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // 1. Start on a page with two frames.
  GURL initial_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(initial_url, root->current_url());
  EXPECT_EQ(2U, root->child_count());
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();
  EXPECT_EQ(2U, entry1->root_node()->children.size());

  // 2. Navigate both iframes to a page with a nested iframe.
  GURL frame_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/page_with_data_iframe.html"));
  GURL data_url("data:text/html,Subframe");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), frame_url));
  EXPECT_EQ(initial_url, root->current_url());
  EXPECT_EQ(frame_url, root->child_at(0)->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->child_at(0)->current_url());
  EXPECT_EQ(frame_url, root->child_at(1)->current_url());
  EXPECT_EQ(data_url, root->child_at(1)->child_at(0)->current_url());

  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // Verify subframe entries.
  NavigationEntryImpl::TreeNode* root_node = entry2->root_node();
  ASSERT_EQ(2U, root_node->children.size());
  EXPECT_EQ(frame_url, root_node->children[0]->frame_entry->url());
  EXPECT_EQ(data_url, root_node->children[0]->children[0]->frame_entry->url());
  EXPECT_EQ(frame_url, root_node->children[1]->frame_entry->url());
  EXPECT_EQ(data_url, root_node->children[1]->children[0]->frame_entry->url());

  // Cause the first iframe to redirect when we come back later.  It will go
  // cross-site to a page with an about:blank iframe.
  GURL frame_redirect_dest_url(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/page_with_iframe.html"));
  GURL blank_url(url::kAboutBlankURL);
  {
    TestNavigationObserver observer(shell()->web_contents());
    std::string script = "history.replaceState({}, '', '/server-redirect?" +
                         frame_redirect_dest_url.spec() + "')";
    EXPECT_TRUE(ExecJs(root->child_at(0), script));
    observer.Wait();
  }

  // We should not have lost subframe entries for the nested frame.
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  scoped_refptr<FrameNavigationEntry> nested_entry =
      entry2->GetFrameEntry(root->child_at(0)->child_at(0));
  EXPECT_TRUE(nested_entry);
  EXPECT_EQ(data_url, nested_entry->url());

  // 3. Navigate the main frame to a different page.  When we come back, we'll
  // commit the main frame first and have no pending entry when navigating the
  // subframes.
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());

  // 4. Go back. The first iframe should redirect to a cross-site page with a
  // different nested iframe.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  EXPECT_EQ(initial_url, root->current_url());
  EXPECT_EQ(frame_redirect_dest_url, root->child_at(0)->current_url());
  EXPECT_EQ(blank_url, root->child_at(0)->child_at(0)->current_url());
  EXPECT_EQ(frame_url, root->child_at(1)->current_url());
  EXPECT_EQ(data_url, root->child_at(1)->child_at(0)->current_url());

  // Check the FrameNavigationEntries as well.
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(frame_redirect_dest_url,
            entry2->GetFrameEntry(root->child_at(0))->url());
  EXPECT_EQ(blank_url,
            entry2->GetFrameEntry(root->child_at(0)->child_at(0))->url());
  EXPECT_EQ(frame_url, entry2->GetFrameEntry(root->child_at(1))->url());
  EXPECT_EQ(data_url,
            entry2->GetFrameEntry(root->child_at(1)->child_at(0))->url());

  // Now cause the second iframe to redirect when we come back to it.
  {
    TestNavigationObserver observer(shell()->web_contents());
    std::string script = "history.replaceState({}, '', '/server-redirect?" +
                         frame_redirect_dest_url.spec() + "')";
    EXPECT_TRUE(ExecJs(root->child_at(1), script));
    observer.Wait();
  }

  // 5. Navigate the other iframe elsewhere, so that going back does not
  // require a navigation in the main frame.  This means there will be a
  // pending entry when the subframe commits, exercising a different path than
  // step 4.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(1));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), url2));
    capturer.Wait();
  }
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());

  // 6. As in step 4, go back but redirect, resetting the children.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  EXPECT_EQ(initial_url, root->current_url());
  EXPECT_EQ(frame_redirect_dest_url, root->child_at(0)->current_url());
  EXPECT_EQ(blank_url, root->child_at(0)->child_at(0)->current_url());
  EXPECT_EQ(frame_redirect_dest_url, root->child_at(1)->current_url());
  EXPECT_EQ(blank_url, root->child_at(1)->child_at(0)->current_url());

  // Check the FrameNavigationEntries as well.
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(frame_redirect_dest_url,
            entry2->GetFrameEntry(root->child_at(0))->url());
  EXPECT_EQ(blank_url,
            entry2->GetFrameEntry(root->child_at(0)->child_at(0))->url());
  EXPECT_EQ(frame_redirect_dest_url,
            entry2->GetFrameEntry(root->child_at(1))->url());
  EXPECT_EQ(blank_url,
            entry2->GetFrameEntry(root->child_at(1)->child_at(0))->url());

  // Now cause the main frame to redirect to a page with no frames when we come
  // back to it.
  GURL redirect_dest_url(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/simple_page_2.html"));
  {
    TestNavigationObserver observer(shell()->web_contents());
    std::string script = "history.replaceState({}, '', '/server-redirect?" +
                         redirect_dest_url.spec() + "')";
    EXPECT_TRUE(ExecJs(root, script));
    observer.Wait();
  }

  // 7. Navigate the main frame to a different page.
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());

  // 8. Go back, causing the main frame to redirect to a page with no frames.
  // All child items should be gone, and |entry2| is deleted and replaced with a
  // new entry.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  EXPECT_EQ(redirect_dest_url, root->current_url());
  EXPECT_EQ(0U, root->child_count());
  EXPECT_EQ(0U,
            controller.GetLastCommittedEntry()->root_node()->children.size());
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
}

// Similar to FrameNavigationEntry_BackWithRedirect but with same-origin frames.
// (This wasn't working initially).
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_SameOriginBackWithRedirect) {
  // The test assumes the previous iframe gets deleted after navigation and
  // later recreated on history navigations. Disable back/forward cache to
  // ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // 1. Start on a page with an iframe.
  GURL initial_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(initial_url, root->current_url());
  EXPECT_EQ(1U, root->child_count());
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();
  EXPECT_EQ(1U, entry1->root_node()->children.size());

  // 2. Navigate the iframe to a page with a nested iframe.
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  GURL data_url("data:text/html,Subframe");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
  EXPECT_EQ(initial_url, root->current_url());
  EXPECT_EQ(frame_url, root->child_at(0)->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->child_at(0)->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // Verify subframe entries.
  NavigationEntryImpl::TreeNode* root_node = entry2->root_node();
  ASSERT_EQ(1U, root_node->children.size());
  EXPECT_EQ(frame_url, root_node->children[0]->frame_entry->url());
  EXPECT_EQ(data_url, root_node->children[0]->children[0]->frame_entry->url());

  // Cause the iframe to redirect when we come back later.  It will go
  // same-origin to a page with an about:blank iframe.
  GURL frame_redirect_dest_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  {
    TestNavigationObserver observer(shell()->web_contents());
    std::string script = "history.replaceState({}, '', '/server-redirect?" +
                         frame_redirect_dest_url.spec() + "')";
    EXPECT_TRUE(ExecJs(root->child_at(0), script));
    observer.Wait();
  }

  // We should not have lost subframe entries for the nested frame.
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  scoped_refptr<FrameNavigationEntry> nested_entry =
      entry2->GetFrameEntry(root->child_at(0)->child_at(0));
  EXPECT_TRUE(nested_entry);
  EXPECT_EQ(data_url, nested_entry->url());

  // 3. Navigate the main frame to a different page.  When we come back, we'll
  // commit the main frame first and have no pending entry when navigating the
  // subframes.
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  // 4. Go back. The first iframe should redirect to a same-origin page with a
  // different nested iframe.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    controller.GoBack();
    back_load_observer.Wait();
  }
  GURL blank_url(url::kAboutBlankURL);
  EXPECT_EQ(initial_url, root->current_url());
  EXPECT_EQ(frame_redirect_dest_url, root->child_at(0)->current_url());
  EXPECT_EQ(blank_url, root->child_at(0)->child_at(0)->current_url());

  // Check the FrameNavigationEntries as well.
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(frame_redirect_dest_url,
            entry2->GetFrameEntry(root->child_at(0))->url());
  EXPECT_EQ(blank_url,
            entry2->GetFrameEntry(root->child_at(0)->child_at(0))->url());

  // Now cause the main frame to redirect to a page with no frames when we come
  // back to it.
  GURL redirect_dest_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    TestNavigationObserver observer(shell()->web_contents());
    std::string script = "history.replaceState({}, '', '/server-redirect?" +
                         redirect_dest_url.spec() + "')";
    EXPECT_TRUE(ExecJs(root, script));
    observer.Wait();
  }

  // 5. Navigate the main frame to a different page.
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  // 6. Go back, causing the main frame to redirect to a page with no frames.
  // All child items should be gone.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    controller.GoBack();
    back_load_observer.Wait();
  }
  EXPECT_EQ(redirect_dest_url, root->current_url());
  EXPECT_EQ(0U, root->child_count());
  EXPECT_EQ(0U, entry2->root_node()->children.size());
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
}

// Verify that subframes can be restored in a new NavigationController using the
// PageState of an existing NavigationEntry.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_RestoreViaPageState) {
  // 1. Start on a page with a data URL iframe.
  GURL main_url_a(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/page_with_data_iframe.html"));
  GURL data_url("data:text/html,Subframe");
  EXPECT_TRUE(NavigateToURL(shell(), main_url_a));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();

  // The entry should have a FrameNavigationEntry for the data subframe.
  ASSERT_EQ(1U, entry1->root_node()->children.size());
  EXPECT_EQ(data_url, entry1->root_node()->children[0]->frame_entry->url());

  // 2. Navigate the iframe cross-site.
  GURL frame_url_b(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/simple_page_1.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url_b));
    capturer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // The entry should have a FrameNavigationEntry for the b.com subframe.
  ASSERT_EQ(1U, entry2->root_node()->children.size());
  EXPECT_EQ(frame_url_b, entry2->root_node()->children[0]->frame_entry->url());

  // 3. Navigate main frame cross-site, destroying the frames.
  GURL main_url_c(embedded_test_server()->GetURL(
      "c.com", "/navigation_controller/simple_page_2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url_c));
  ASSERT_EQ(0U, root->child_count());
  EXPECT_EQ(main_url_c, root->current_url());

  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry3 = controller.GetLastCommittedEntry();
  EXPECT_EQ(0U, entry3->root_node()->children.size());

  // 4. Create a NavigationEntry with the same PageState as |entry2| and verify
  // it has the same FrameNavigationEntry structure.
  std::unique_ptr<NavigationEntryImpl> restored_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              main_url_a, Referrer(), /* initiator_origin= */ absl::nullopt,
              /* initiator_base_url= */ absl::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  EXPECT_EQ(0U, restored_entry->root_node()->children.size());
  std::unique_ptr<NavigationEntryRestoreContextImpl> context =
      std::make_unique<NavigationEntryRestoreContextImpl>();
  restored_entry->SetPageState(entry2->GetPageState(), context.get());

  // The entry should have a FrameNavigationEntry for the b.com subframe.
  EXPECT_EQ(main_url_a, restored_entry->root_node()->frame_entry->url());
  ASSERT_EQ(1U, restored_entry->root_node()->children.size());
  EXPECT_EQ(frame_url_b,
            restored_entry->root_node()->children[0]->frame_entry->url());

  // 5. Restore the new entry in a new tab and verify the correct URLs load.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  Shell* new_shell = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(
          new_shell->web_contents()->GetController());
  new_controller.Restore(entries.size() - 1, RestoreType::kRestored, &entries);
  ASSERT_EQ(0u, entries.size());
  {
    TestNavigationObserver restore_observer(new_shell->web_contents());
    new_controller.LoadIfNecessary();
    restore_observer.Wait();
  }
  ASSERT_EQ(1U, new_root->child_count());
  EXPECT_EQ(main_url_a, new_root->current_url());
  EXPECT_EQ(frame_url_b, new_root->child_at(0)->current_url());

  EXPECT_EQ(1, new_controller.GetEntryCount());
  EXPECT_EQ(0, new_controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* new_entry = new_controller.GetLastCommittedEntry();

  // The entry should have a FrameNavigationEntry for the b.com subframe.
  EXPECT_EQ(main_url_a, new_entry->root_node()->frame_entry->url());
  ASSERT_EQ(1U, new_entry->root_node()->children.size());
  EXPECT_EQ(frame_url_b,
            new_entry->root_node()->children[0]->frame_entry->url());
}

// Verify that we can finish loading a page on restore if the PageState is
// missing subframes.  See https://crbug.com/638088.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_RestoreViaPartialPageState) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/inject_into_blank_iframe.html"));
  GURL blank_url(url::kAboutBlankURL);
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Create a NavigationEntry to restore, as if it had been loaded before.  The
  // page has an about:blank iframe and injects content into it, but the
  // PageState lacks any subframe history items.  This may happen during a
  // restore of a bad session or if the page has changed since the last visit.
  // Chrome should be robust to this and should be able to load the frame from
  // its default URL.
  std::unique_ptr<NavigationEntryImpl> restored_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              main_url, Referrer(), absl::nullopt /* initiator_origin= */,
              /* initiator_base_url= */ absl::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  std::unique_ptr<NavigationEntryRestoreContextImpl> context =
      std::make_unique<NavigationEntryRestoreContextImpl>();
  restored_entry->SetPageState(blink::PageState::CreateFromURL(main_url),
                               context.get());
  EXPECT_EQ(0U, restored_entry->root_node()->children.size());

  // Restore the new entry in a new tab and verify the iframe loads and has
  // content injected into it.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  controller.Restore(entries.size() - 1, RestoreType::kRestored, &entries);
  ASSERT_EQ(0u, entries.size());
  {
    TestNavigationObserver restore_observer(shell()->web_contents());
    controller.LoadIfNecessary();
    restore_observer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(blank_url, root->child_at(0)->current_url());

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* new_entry = controller.GetLastCommittedEntry();

  // The entry should have a FrameNavigationEntry for the blank subframe.
  EXPECT_EQ(main_url, new_entry->root_node()->frame_entry->url());
  ASSERT_EQ(1U, new_entry->root_node()->children.size());
  EXPECT_EQ(blank_url, new_entry->root_node()->children[0]->frame_entry->url());

  // Verify that the parent was able to script the iframe.
  EXPECT_EQ("Injected text",
            EvalJs(root->child_at(0), "document.body.innerHTML"));
}

// Verify that calling Restore() and LoadIfNecessary() on a NavigationController
// that already has existing entries & non-null pending entry won't crash and
// won't clear existing non-initial entries. This can happen because WebView's
// restoreState() API, which triggers the session restore code, can be called at
// any point in time, which used to cause crashes.
// Regression test for https://crbug.com/1287624.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       RestoreAndLoadOnUsedNavigationController) {
  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_3(embedded_test_server()->GetURL("a.com", "/title3.html"));

  // 1. Start in a tab with 1 NavigationEntry.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(url_1, root->current_url());

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();

  // 2. Create a NavigationEntry with the same PageState as |entry1|.
  std::unique_ptr<NavigationEntryImpl> restored_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              url_1, Referrer(), absl::nullopt /* initiator_origin= */,
              /* initiator_base_url= */ absl::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  EXPECT_EQ(0U, restored_entry->root_node()->children.size());
  std::unique_ptr<NavigationEntryRestoreContextImpl> context =
      std::make_unique<NavigationEntryRestoreContextImpl>();
  restored_entry->SetPageState(entry1->GetPageState(), context.get());

  // 3. Create a new tab with a new NavigationController, and navigate it twice.
  Shell* new_shell = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(
          new_shell->web_contents()->GetController());
  EXPECT_TRUE(NavigateToURL(new_shell, url_2));
  EXPECT_EQ(1, new_controller.GetEntryCount());
  EXPECT_EQ(0, new_controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = new_controller.GetLastCommittedEntry();
  EXPECT_EQ(url_2, entry2->GetURL());

  EXPECT_TRUE(NavigateToURL(new_shell, url_3));
  EXPECT_EQ(2, new_controller.GetEntryCount());
  EXPECT_EQ(1, new_controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_3, new_controller.GetLastCommittedEntry()->GetURL());

  // 4. Start and pause a back navigation in the new NavigationController, which
  // will set the pending entry to `entry2`. Disable BFCache so that we can
  // pause the navigation.
  DisableBackForwardCacheForTesting(new_shell->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  TestNavigationManager back_navigation_manager(new_shell->web_contents(),
                                                url_2);
  new_controller.GoBack();
  EXPECT_TRUE(back_navigation_manager.WaitForRequestStart());
  EXPECT_EQ(entry2, new_controller.GetPendingEntry());
  EXPECT_TRUE(new_root->navigation_request());

  // 5. Restore `restored_entry` in the new tab.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  new_controller.Restore(entries.size() - 1, RestoreType::kRestored, &entries);
  // The previously existing entries were kept, in addition to the restored
  // entries. Note that the last committed entry index is updated to 0, which
  // is supposed to point to `url_1`'s entry, but actually points to `url_2`'s
  // entry because the pre-existing entries are not cleared.
  // TODO(https://crbug.com/1287624): Fix this.
  EXPECT_EQ(3, new_controller.GetEntryCount());
  EXPECT_EQ(0, new_controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_2, new_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url_3, new_controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url_1, new_controller.GetEntryAtIndex(2)->GetURL());
  // The pending entry is not cleared and the back navigation is still pending.
  EXPECT_EQ(entry2, new_controller.GetPendingEntry());
  EXPECT_TRUE(new_root->navigation_request());

  // 6. Call LoadIfNecessary() in the new tab.
  ASSERT_EQ(0u, entries.size());
  {
    TestNavigationObserver restore_observer(new_shell->web_contents());
    new_controller.LoadIfNecessary();
    restore_observer.Wait();
  }
  // Because the pending entry still pointed to the back navigation's entry,
  // `url_2` is loaded instead of the entry restored from the other tab.
  // TODO(https://crbug.com/1287624): This should load the restored entry
  // instead.
  EXPECT_EQ(3, new_controller.GetEntryCount());
  EXPECT_EQ(0, new_controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* current_entry = new_controller.GetLastCommittedEntry();
  EXPECT_EQ(entry2, current_entry);
  EXPECT_EQ(url_2, current_entry->GetURL());
  EXPECT_EQ(url_2, new_root->current_url());
}

// Verify that calling Restore with an empty set of entries does not crash,
// since this may be possible via Android WebView's restoreState() API.
// See https://crbug.com/1287624.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       RestoreEmptyAndLoadOnUsedNavigationController) {
  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // 1. Create a new tab with a new NavigationController, and navigate it twice.
  Shell* new_shell =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             GURL::EmptyGURL(), nullptr, gfx::Size());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(
          new_shell->web_contents()->GetController());
  EXPECT_TRUE(NavigateToURL(new_shell, url_1));
  EXPECT_EQ(1, new_controller.GetEntryCount());
  EXPECT_EQ(0, new_controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry1 = new_controller.GetLastCommittedEntry();
  EXPECT_EQ(url_1, entry1->GetURL());

  EXPECT_TRUE(NavigateToURL(new_shell, url_2));
  EXPECT_EQ(2, new_controller.GetEntryCount());
  EXPECT_EQ(1, new_controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_2, new_controller.GetLastCommittedEntry()->GetURL());

  // 2. Start and pause a back navigation in the new NavigationController, which
  // will set the pending entry to `entry1`. Disable BFCache so that we can
  // pause the navigation.
  DisableBackForwardCacheForTesting(new_shell->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  TestNavigationManager back_navigation_manager(new_shell->web_contents(),
                                                url_1);
  new_controller.GoBack();
  EXPECT_TRUE(back_navigation_manager.WaitForRequestStart());
  EXPECT_EQ(entry1, new_controller.GetPendingEntry());
  EXPECT_TRUE(new_root->navigation_request());

  // 3. Restore an empty set of entries in the new tab, which is possible via
  // Android WebView's restoreState API.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  new_controller.Restore(-1, RestoreType::kRestored, &entries);
  // The previously existing entries were kept without crashing, and no changes
  // to the entries were made except the last committed index.
  // TODO(https://crbug.com/1287624): A selected index of -1 should not be
  // allowed.
  EXPECT_EQ(2, new_controller.GetEntryCount());

  // There is always a valid index.
  EXPECT_EQ(0, new_controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_1, new_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url_2, new_controller.GetEntryAtIndex(1)->GetURL());

  // The pending entry is not cleared and the back navigation is still pending.
  EXPECT_EQ(entry1, new_controller.GetPendingEntry());
  EXPECT_TRUE(new_root->navigation_request());

  // 4. Call LoadIfNecessary() in the new tab.
  ASSERT_EQ(0u, entries.size());
  {
    TestNavigationObserver restore_observer(new_shell->web_contents());
    new_controller.LoadIfNecessary();
    restore_observer.Wait();
  }
  // Because the pending entry still pointed to the back navigation's entry,
  // `url_1` is loaded.
  // TODO(https://crbug.com/1287624): A selected index of -1 should not be
  // allowed.
  EXPECT_EQ(2, new_controller.GetEntryCount());
  EXPECT_EQ(0, new_controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* current_entry = new_controller.GetLastCommittedEntry();
  EXPECT_EQ(url_1, current_entry->GetURL());
  EXPECT_EQ(url_1, new_root->current_url());
}

// Verify that calling Restore with an entry that has an empty URL on a
// fresh/unused NavigationController does not crash.
// See also https://crbug.com/1240138.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       RestoreEmptyURLEntryAndLoadOnFreshNavigationController) {
  // 1. Start in a tab and navigate it to about:blank.
  GURL about_blank_url(url::kAboutBlankURL);
  EXPECT_TRUE(NavigateToURL(shell(), about_blank_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(about_blank_url, root->current_url());

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();

  // 2. Create a NavigationEntry with the same PageState as |entry1| but with
  // an empty URL.
  std::unique_ptr<NavigationEntryImpl> restored_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              GURL(), Referrer(), absl::nullopt /* initiator_origin= */,
              /* initiator_base_url= */ absl::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  EXPECT_EQ(0U, restored_entry->root_node()->children.size());
  std::unique_ptr<NavigationEntryRestoreContextImpl> context =
      std::make_unique<NavigationEntryRestoreContextImpl>();
  entry1->SetURL(GURL());
  restored_entry->SetPageState(entry1->GetPageState(), context.get());

  // 3. Restore the empty URL entry in a new tab.
  Shell* new_shell = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(
          new_shell->web_contents()->GetController());
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  new_controller.Restore(0, RestoreType::kRestored, &entries);
  EXPECT_EQ(0u, entries.size());

  // The restored entry has its URL set to about:blank instead of an empty URL.
  EXPECT_EQ(1, new_controller.GetEntryCount());
  EXPECT_EQ(0, new_controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(about_blank_url, new_controller.GetLastCommittedEntry()->GetURL());

  // 4. Call LoadIfNecessary() in the new tab.
  {
    TestNavigationObserver restore_observer(new_shell->web_contents());
    new_controller.LoadIfNecessary();
    restore_observer.Wait();
  }

  // After loading, the restored entry's URL stays as "about:blank".
  EXPECT_EQ(1, new_controller.GetEntryCount());
  EXPECT_EQ(0, new_controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(about_blank_url, new_controller.GetLastCommittedEntry()->GetURL());
}

// Verify that the restore state of a pending entry doesn't change if an
// unrelated subframe commits while the navigation is in progress.
// Regression test for https://crbug.com/923423.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       RestoreAndAddSubframeDuringPendingBack) {
  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // 1. Start in a tab with 2 NavigationEntries.
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_2, root->current_url());

  // 2. Create NavigationEntries with the same PageState as the current entries.
  std::unique_ptr<NavigationEntryRestoreContextImpl> context =
      std::make_unique<NavigationEntryRestoreContextImpl>();
  std::unique_ptr<NavigationEntryImpl> restored_entry1 =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              url_1, Referrer(), absl::nullopt /* initiator_origin= */,
              /* initiator_base_url= */ absl::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  restored_entry1->SetPageState(entry1->GetPageState(), context.get());
  std::unique_ptr<NavigationEntryImpl> restored_entry2 =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              url_2, Referrer(), absl::nullopt /* initiator_origin= */,
              /* initiator_base_url= */ absl::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  restored_entry2->SetPageState(entry2->GetPageState(), context.get());

  // 3. Create a new tab and restore the entries.
  Shell* new_shell = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  WebContentsImpl* new_web_contents =
      static_cast<WebContentsImpl*>(new_shell->web_contents());
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(new_web_contents->GetController());
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry1));
  entries.push_back(std::move(restored_entry2));
  new_controller.Restore(entries.size() - 1, RestoreType::kRestored, &entries);
  {
    TestNavigationObserver restore_observer(new_shell->web_contents());
    new_controller.LoadIfNecessary();
    restore_observer.Wait();
  }
  NavigationEntryImpl* new_entry1 = new_controller.GetEntryAtIndex(0);
  NavigationEntryImpl* new_entry2 = new_controller.GetEntryAtIndex(1);
  EXPECT_EQ(new_entry2, new_controller.GetLastCommittedEntry());

  // 4. Start and pause a back navigation in the new NavigationController, which
  // will set the pending entry to `new_entry1`.
  TestNavigationManager back_navigation_manager(new_shell->web_contents(),
                                                url_1);
  new_controller.GoBack();
  EXPECT_TRUE(back_navigation_manager.WaitForRequestStart());
  EXPECT_EQ(new_entry1, new_controller.GetPendingEntry());
  EXPECT_TRUE(new_entry1->IsRestored());

  // While waiting for the back navigation, add a new subframe and wait for it
  // to commit. This should not change the restore status of new_entry1, which
  // used to fail in https://crbug.com/923423 and caused a DCHECK crash. Note
  // that passing true for `wait_for_navigation` does not work because it waits
  // for load stop, which will not happen while the back navigation is pending.
  TestNavigationObserver subframe_observer(new_web_contents);
  CreateSubframe(new_web_contents, "child", url_2,
                 false /* wait_for_navigation */);
  subframe_observer.WaitForNavigationFinished();
  EXPECT_EQ(new_entry2, new_controller.GetLastCommittedEntry());
  EXPECT_EQ(new_entry1, new_controller.GetPendingEntry());
  EXPECT_TRUE(new_entry1->IsRestored());

  // Allow the back navigation to complete, clearing the restore status.
  ASSERT_TRUE(back_navigation_manager.WaitForNavigationFinished());
  EXPECT_FALSE(new_entry1->IsRestored());
  EXPECT_EQ(new_entry1, new_controller.GetLastCommittedEntry());
  EXPECT_EQ(2, new_controller.GetEntryCount());
  EXPECT_EQ(0, new_controller.GetLastCommittedEntryIndex());
}

// Verifies that the |frame_unique_name| is set to the correct frame, so that we
// can match subframe FrameNavigationEntries to newly created frames after
// back/forward and restore.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_FrameUniqueName) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // 1. Navigate the main frame.
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  SiteInstanceImpl* main_site_instance =
      root->current_frame_host()->GetSiteInstance();

  // The main frame defaults to an empty name.
  scoped_refptr<FrameNavigationEntry> frame_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(root);
  EXPECT_EQ("", frame_entry->frame_unique_name());

  // 2. Add an unnamed subframe, which does an AUTO_SUBFRAME navigation.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // The root FrameNavigationEntry hasn't changed.
  EXPECT_EQ(frame_entry,
            controller.GetLastCommittedEntry()->GetFrameEntry(root));

  // The subframe should have a generated name.
  FrameTreeNode* subframe = root->child_at(0);
  EXPECT_EQ(main_site_instance,
            subframe->current_frame_host()->GetSiteInstance());
  scoped_refptr<FrameNavigationEntry> subframe_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(subframe);
  EXPECT_THAT(subframe_entry->frame_unique_name(),
              testing::HasSubstr("dynamicFrame"));

  // 3. Add a named subframe.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = JsReplace(
        "var iframe = document.createElement('iframe');"
        "iframe.src = $1;"
        "iframe.name = 'foo';"
        "document.body.appendChild(iframe);",
        url);
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // The new subframe should have the specified name.
  EXPECT_EQ(frame_entry,
            controller.GetLastCommittedEntry()->GetFrameEntry(root));
  FrameTreeNode* foo_subframe = root->child_at(1);
  EXPECT_EQ(main_site_instance,
            foo_subframe->current_frame_host()->GetSiteInstance());
  scoped_refptr<FrameNavigationEntry> foo_subframe_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(foo_subframe);
  EXPECT_THAT(foo_subframe_entry->frame_unique_name(),
              testing::HasSubstr("dynamicFrame"));

  // 4. Navigating in the subframes cross-process shouldn't change their names.
  // TODO(creis): Fix the unnamed case in https://crbug.com/502317.
  GURL bar_url(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(foo_subframe, bar_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  if (AreStrictSiteInstancesEnabled()) {
    EXPECT_NE(main_site_instance,
              foo_subframe->current_frame_host()->GetSiteInstance());
  } else {
    // When strict SiteInstances are not being used, the subframe should be
    // the same as its parent because both sites get routed to the default
    // SiteInstance.
    EXPECT_TRUE(main_site_instance->IsDefaultSiteInstance());
    EXPECT_EQ(main_site_instance,
              foo_subframe->current_frame_host()->GetSiteInstance());
  }

  foo_subframe_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(foo_subframe);
  EXPECT_THAT(foo_subframe_entry->frame_unique_name(),
              testing::HasSubstr("dynamicFrame"));
}

// Verify that navigations caused by client-side redirects populates the entry's
// replaced data.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ReplacedNavigationEntryData_ClientSideRedirect) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/client_redirect.html"));
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));

  {
    TestNavigationManager navigation_manager_1(shell()->web_contents(), url1);
    TestNavigationManager navigation_manager_2(shell()->web_contents(), url2);

    shell()->LoadURL(url1);

    ASSERT_TRUE(navigation_manager_1
                    .WaitForNavigationFinished());  // Initial navigation.
    ASSERT_TRUE(navigation_manager_2
                    .WaitForNavigationFinished());  // Client-side redirect.

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry1 = controller.GetEntryAtIndex(0);
    ASSERT_EQ(url2, entry1->GetURL());
    ASSERT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry1->GetTransitionType(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));

    ASSERT_TRUE(entry1->GetReplacedEntryData().has_value());
    EXPECT_EQ(url1, entry1->GetReplacedEntryData()->first_committed_url);
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry1->GetReplacedEntryData()->first_transition_type,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)))
        << base::StringPrintf(
               "%X", entry1->GetReplacedEntryData()->first_transition_type);
  }
}

// Verify that navigations caused by location.replace() populates the entry's
// replaced data.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ReplacedNavigationEntryData_LocationReplace) {
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));

  // Test fixture: start with typing a URL.
  {
    ASSERT_TRUE(NavigateToURL(shell(), url1));
    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry1 = controller.GetEntryAtIndex(0);
    ASSERT_EQ(url1, entry1->GetURL());
    ASSERT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry1->GetTransitionType(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_FALSE(entry1->GetReplacedEntryData().has_value());
  }

  const base::Time time1 = controller.GetEntryAtIndex(0)->GetTimestamp();

  {
    // location.replace().
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "location.replace('" + url2.spec() + "')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry1 = controller.GetEntryAtIndex(0);
    ASSERT_EQ(url2, entry1->GetURL());
    ASSERT_NE(time1, entry1->GetTimestamp());

    ASSERT_TRUE(entry1->GetReplacedEntryData().has_value());
    EXPECT_EQ(url1, entry1->GetReplacedEntryData()->first_committed_url);
    EXPECT_EQ(time1, entry1->GetReplacedEntryData()->first_timestamp);
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry1->GetReplacedEntryData()->first_transition_type,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
  }
}

// Verify that history.replaceState() populates the navigation entry's replaced
// entry data.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ReplacedNavigationEntryData_ReplaceState) {
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  GURL url3(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_3.html"));

  // Test fixture: start with typing a URL.
  {
    ASSERT_TRUE(NavigateToURL(shell(), url1));
    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry1 = controller.GetEntryAtIndex(0);
    ASSERT_EQ(url1, entry1->GetURL());
    ASSERT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry1->GetTransitionType(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_FALSE(entry1->GetReplacedEntryData().has_value());
  }

  const base::Time time1 = controller.GetEntryAtIndex(0)->GetTimestamp();

  {
    // history.replaceState().
    FrameNavigateParamsCapturer capturer(root);
    std::string script =
        "history.replaceState({}, 'page 2', 'simple_page_2.html')";
    ASSERT_TRUE(ExecJs(root, script));
    capturer.Wait();

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry1 = controller.GetEntryAtIndex(0);
    ASSERT_EQ(url2, entry1->GetURL());
    ASSERT_NE(time1, entry1->GetTimestamp());

    ASSERT_TRUE(entry1->GetReplacedEntryData().has_value());
    EXPECT_EQ(url1, entry1->GetReplacedEntryData()->first_committed_url);
    EXPECT_EQ(time1, entry1->GetReplacedEntryData()->first_timestamp);
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry1->GetReplacedEntryData()->first_transition_type,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
  }

  {
    // Reload from the renderer side and make sure the replaced entry data
    // doesn't change.
    FrameNavigateParamsCapturer capturer(root);
    ASSERT_TRUE(ExecJs(root, "location.reload()"));
    capturer.Wait();

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry1 = controller.GetEntryAtIndex(0);
    ASSERT_EQ(url2, entry1->GetURL());
    ASSERT_NE(time1, entry1->GetTimestamp());

    ASSERT_TRUE(entry1->GetReplacedEntryData().has_value());
    EXPECT_EQ(url1, entry1->GetReplacedEntryData()->first_committed_url);
    EXPECT_EQ(time1, entry1->GetReplacedEntryData()->first_timestamp);
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry1->GetReplacedEntryData()->first_transition_type,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
  }

  {
    // history.replaceState().
    FrameNavigateParamsCapturer capturer(root);
    std::string script =
        "history.replaceState({}, 'page 3', 'simple_page_3.html')";
    ASSERT_TRUE(ExecJs(root, script));
    capturer.Wait();

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry1 = controller.GetEntryAtIndex(0);
    ASSERT_EQ(url3, entry1->GetURL());
    ASSERT_NE(time1, entry1->GetTimestamp());

    ASSERT_TRUE(entry1->GetReplacedEntryData().has_value());
    EXPECT_EQ(url1, entry1->GetReplacedEntryData()->first_committed_url);
    EXPECT_EQ(time1, entry1->GetReplacedEntryData()->first_timestamp);
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry1->GetReplacedEntryData()->first_transition_type,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
  }
}

// Verify that history.pushState() does not populate the navigation entry's
// replaced entry data.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ReplacedNavigationEntryData_PushState) {
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));

  // Test fixture: start with typing a URL.
  {
    ASSERT_TRUE(NavigateToURL(shell(), url1));
    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry1 = controller.GetEntryAtIndex(0);
    ASSERT_EQ(url1, entry1->GetURL());
    ASSERT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry1->GetTransitionType(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_FALSE(entry1->GetReplacedEntryData().has_value());
  }

  {
    // history.pushState().
    FrameNavigateParamsCapturer capturer(root);
    std::string script =
        "history.pushState({}, 'page 2', 'simple_page_2.html')";
    ASSERT_TRUE(ExecJs(root, script));
    capturer.Wait();

    ASSERT_EQ(2, controller.GetEntryCount());
    ASSERT_EQ(url1, controller.GetEntryAtIndex(0)->GetURL());
    ASSERT_EQ(url2, controller.GetEntryAtIndex(1)->GetURL());

    EXPECT_FALSE(
        controller.GetEntryAtIndex(0)->GetReplacedEntryData().has_value());
    EXPECT_FALSE(
        controller.GetEntryAtIndex(1)->GetReplacedEntryData().has_value());
  }
}

// Verify that location.reload() does not populate the navigation entry's
// replaced entry data.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ReplacedNavigationEntryData_LocationReload) {
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));

  // Test fixture: start with typing a URL.
  {
    ASSERT_TRUE(NavigateToURL(shell(), url1));
    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry1 = controller.GetEntryAtIndex(0);
    ASSERT_EQ(url1, entry1->GetURL());
    ASSERT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry1->GetTransitionType(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_FALSE(entry1->GetReplacedEntryData().has_value());
  }

  const base::Time time1 = controller.GetEntryAtIndex(0)->GetTimestamp();

  {
    // Reload from the renderer side and make sure replaced entry data is not
    // stored.
    FrameNavigateParamsCapturer capturer(root);
    ASSERT_TRUE(ExecJs(root, "location.reload()"));
    capturer.Wait();

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry1 = controller.GetEntryAtIndex(0);
    ASSERT_EQ(url1, entry1->GetURL());
    ASSERT_NE(time1, entry1->GetTimestamp());

    // At least the timestamp has changed, so we need to keep a copy of the
    // replaced data.
    ASSERT_TRUE(entry1->GetReplacedEntryData().has_value());
    EXPECT_EQ(url1, entry1->GetReplacedEntryData()->first_committed_url);
    EXPECT_EQ(time1, entry1->GetReplacedEntryData()->first_timestamp);
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry1->GetReplacedEntryData()->first_transition_type,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
  }
}

// Verify the scenario where the user goes back to a navigatin entry that had
// previously replaced it's URL (via history.replaceState()), for a URL that
// (if fetched) causes a server-side redirect. In this scenario, the fact of
// going back should not influence the replaced data, and hence the first URL
// prior to history.replaceState() should remain set.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    ReplacedNavigationEntryData_BackAfterReplaceStateWithRedirect) {
  // The test assumes the previous iframe gets deleted after navigation and
  // later recreated on history navigations. Disable back/forward cache to
  // ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  GURL redirecting_url_to_url2(
      embedded_test_server()->GetURL("/server-redirect?" + url2.spec()));
  GURL url3(embedded_test_server()->GetURL("/simple_page.html"));

  // Start with typing a URL.
  {
    ASSERT_TRUE(NavigateToURL(shell(), url1));
    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry1 = controller.GetEntryAtIndex(0);
    ASSERT_EQ(url1, entry1->GetURL());
    ASSERT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry1->GetTransitionType(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_FALSE(entry1->GetReplacedEntryData().has_value());
  }

  {
    // history.replaceState(), pointing to a URL that would redirect to |url2|.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "history.replaceState({}, 'page 2', '" +
                         redirecting_url_to_url2.spec() + "')";

    ASSERT_TRUE(ExecJs(root, script));
    capturer.Wait();

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry1 = controller.GetEntryAtIndex(0);
    ASSERT_EQ(redirecting_url_to_url2, entry1->GetURL());
    ASSERT_TRUE(entry1->GetReplacedEntryData().has_value());
    ASSERT_EQ(url1, entry1->GetReplacedEntryData()->first_committed_url);
  }

  // Type another URL, |url3|.
  {
    ASSERT_TRUE(NavigateToURL(shell(), url3));
    ASSERT_EQ(2, controller.GetEntryCount());
  }

  // Back, which should redirect to |url2|.
  {
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();

    NavigationEntry* entry1 = controller.GetEntryAtIndex(0);
    ASSERT_EQ(url2, entry1->GetURL());

    // We still expect |url1| in the replaced data.
    ASSERT_TRUE(entry1->GetReplacedEntryData().has_value());
    EXPECT_EQ(url1, entry1->GetReplacedEntryData()->first_committed_url);
  }
}

// Verify that navigating back in history does not populate the navigation
// entry's replaced entry data.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ReplacedNavigationEntryData_Back) {
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));

  // Test fixture: start with typing two URLs.
  {
    ASSERT_TRUE(NavigateToURL(shell(), url1));
    ASSERT_TRUE(NavigateToURL(shell(), url2));
    ASSERT_EQ(2, controller.GetEntryCount());
    NavigationEntry* entry1 = controller.GetEntryAtIndex(0);
    NavigationEntry* entry2 = controller.GetEntryAtIndex(1);
    ASSERT_EQ(url1, entry1->GetURL());
    ASSERT_EQ(url2, entry2->GetURL());
    ASSERT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry1->GetTransitionType(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    ASSERT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry2->GetTransitionType(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_FALSE(entry1->GetReplacedEntryData().has_value());
    EXPECT_FALSE(entry2->GetReplacedEntryData().has_value());
  }

  const base::Time time1 = controller.GetEntryAtIndex(0)->GetTimestamp();

  {
    // Back.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();

    // Assertions below document the current behavior.
    NavigationEntry* entry1 = controller.GetEntryAtIndex(0);
    NavigationEntry* entry2 = controller.GetEntryAtIndex(1);
    ASSERT_EQ(url1, entry1->GetURL());
    ASSERT_NE(time1, entry1->GetTimestamp());
    ASSERT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry1->GetTransitionType(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR |
                                  ui::PAGE_TRANSITION_FORWARD_BACK)));
    ASSERT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        entry2->GetTransitionType(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));

    // It is questionable whether a copy of the replaced data should be made
    // here too, because of the modified timestamp as well as the new qualifier,
    // ui::PAGE_TRANSITION_FORWARD_BACK. However, we've decided against since
    // there is no actual replacement happening.
    EXPECT_FALSE(entry1->GetReplacedEntryData().has_value());
    EXPECT_FALSE(entry2->GetReplacedEntryData().has_value());
  }
}

// Ensure we don't crash when cloning a named window.  This happened in
// https://crbug.com/603245 because neither the FrameTreeNode ID nor the name of
// the cloned window matched the root FrameNavigationEntry.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, CloneNamedWindow) {
  // Start on an initial page.
  GURL url_1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  // Name the window.
  EXPECT_TRUE(ExecJs(shell(), "window.name = 'foo';"));

  // Navigate it.
  GURL url_2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // Clone the tab and load the page.
  std::unique_ptr<WebContents> new_tab = shell()->web_contents()->Clone();
  WebContentsImpl* new_tab_impl = static_cast<WebContentsImpl*>(new_tab.get());
  NavigationController& new_controller = new_tab_impl->GetController();
  EXPECT_TRUE(new_controller.IsInitialNavigation());
  EXPECT_TRUE(new_controller.NeedsReload());
  {
    TestNavigationObserver clone_observer(new_tab.get());
    new_controller.LoadIfNecessary();
    clone_observer.Wait();
  }
}

// Ensure we don't crash when going back in a cloned named window.  This
// happened in https://crbug.com/603245 because neither the FrameTreeNode ID nor
// the name of the cloned window matched the root FrameNavigationEntry.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       CloneAndGoBackWithNamedWindow) {
  // Start on an initial page.
  GURL url_1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  // Name the window.
  EXPECT_TRUE(ExecJs(shell(), "window.name = 'foo';"));

  // Navigate it.
  GURL url_2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // Clear the name.
  EXPECT_TRUE(ExecJs(shell(), "window.name = '';"));

  // Navigate it again.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  // Clone the tab and load the page.
  std::unique_ptr<WebContents> new_tab = shell()->web_contents()->Clone();
  WebContentsImpl* new_tab_impl = static_cast<WebContentsImpl*>(new_tab.get());
  NavigationController& new_controller = new_tab_impl->GetController();
  EXPECT_TRUE(new_controller.IsInitialNavigation());
  EXPECT_TRUE(new_controller.NeedsReload());
  {
    TestNavigationObserver clone_observer(new_tab.get());
    new_controller.LoadIfNecessary();
    clone_observer.Wait();
  }

  // Go back.
  {
    TestNavigationObserver back_load_observer(new_tab.get());
    new_controller.GoBack();
    back_load_observer.Wait();
  }
}

// Ensure that going back/forward to an apparently same document
// NavigationEntry works when the renderer process hasn't committed anything
// yet.  This can happen when using Ctrl+Back or after a crash.  See
// https://crbug.com/635403.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       BackSameDocumentInNewWindow) {
  // Start on an initial page.
  GURL url_1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  // Perform same document navigation.
  GURL url_2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html#foo"));
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // Clone the tab but don't load last committed page.
  std::unique_ptr<WebContents> new_tab = shell()->web_contents()->Clone();
  WebContentsImpl* new_tab_impl = static_cast<WebContentsImpl*>(new_tab.get());
  NavigationController& new_controller = new_tab_impl->GetController();
  EXPECT_TRUE(new_controller.IsInitialNavigation());
  EXPECT_TRUE(new_controller.NeedsReload());

  // Go back in the new tab.
  {
    TestNavigationObserver back_load_observer(new_tab.get());
    new_controller.GoBack();
    back_load_observer.Wait();
  }

  // Make sure the new tab isn't still loading.
  EXPECT_EQ(url_1, new_controller.GetLastCommittedEntry()->GetURL());
  EXPECT_FALSE(new_tab_impl->IsLoading());

  // Also check going back in the original tab after a renderer crash.
  NavigationController& controller = shell()->web_contents()->GetController();
  RenderProcessHost* process =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    controller.GoBack();
    back_load_observer.Wait();
  }

  // Make sure the original tab isn't still loading.
  EXPECT_EQ(url_1, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_FALSE(shell()->web_contents()->IsLoading());
}

// Ensures that FrameNavigationEntries for dynamically added iframes can be
// found correctly when cloning them during a transfer.  If we don't look for
// them based on unique name in AddOrUpdateFrameEntry, the FrameTreeNode ID
// mismatch will cause us to create a second FrameNavigationEntry during the
// transfer.  Later, we'll find the wrong FrameNavigationEntry (the earlier one
// from the clone which still has a PageState), and this will cause the renderer
// to crash in NavigateInternal because the PageState is present but the page_id
// is -1 (similar to https://crbug.com/568703).  See https://crbug.com/568768.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_RepeatCreatedFrame) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // 1. Navigate the main frame.
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  SiteInstance* main_site_instance =
      root->current_frame_host()->GetSiteInstance();

  // 2. Add a cross-site subframe.
  GURL frame_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  std::string script = JsReplace(kAddFrameWithSrcScript, frame_url);
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  FrameTreeNode* subframe = root->child_at(0);
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(main_site_instance,
              subframe->current_frame_host()->GetSiteInstance());
  }
  scoped_refptr<FrameNavigationEntry> subframe_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(subframe);
  EXPECT_EQ(frame_url, subframe_entry->url());

  // 3. Reload the main frame.
  {
    FrameNavigateParamsCapturer capturer(root);
    controller.Reload(ReloadType::NORMAL, false);
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_RELOAD));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());
  }

  // 4. Add the iframe again.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(main_site_instance,
              root->child_at(0)->current_frame_host()->GetSiteInstance());
  }
}

// Verifies that item sequence numbers and document sequence numbers update
// properly for main frames and subframes.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_SequenceNumbers) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // 1. Navigate the main frame.
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  scoped_refptr<FrameNavigationEntry> frame_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(root);
  int64_t isn_1 = frame_entry->item_sequence_number();
  int64_t dsn_1 = frame_entry->document_sequence_number();
  EXPECT_NE(-1, isn_1);
  EXPECT_NE(-1, dsn_1);

  // 2. Do a same document fragment navigation.
  std::string script = "document.getElementById('fraglink').click()";
  EXPECT_TRUE(ExecJs(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  frame_entry = controller.GetLastCommittedEntry()->GetFrameEntry(root);
  int64_t isn_2 = frame_entry->item_sequence_number();
  int64_t dsn_2 = frame_entry->document_sequence_number();
  EXPECT_NE(-1, isn_2);
  EXPECT_NE(isn_1, isn_2);
  EXPECT_EQ(dsn_1, dsn_2);

  // 3. Add a subframe, which does an AUTO_SUBFRAME navigation.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, url)));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // The root FrameNavigationEntry hasn't changed.
  EXPECT_EQ(frame_entry,
            controller.GetLastCommittedEntry()->GetFrameEntry(root));

  // We should have a unique ISN and DSN for the subframe entry.
  FrameTreeNode* subframe = root->child_at(0);
  scoped_refptr<FrameNavigationEntry> subframe_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(subframe);
  int64_t isn_3 = subframe_entry->item_sequence_number();
  int64_t dsn_3 = subframe_entry->document_sequence_number();
  EXPECT_NE(-1, isn_2);
  EXPECT_NE(isn_2, isn_3);
  EXPECT_NE(dsn_2, dsn_3);

  // 4. Do a same document fragment navigation in the subframe.
  EXPECT_TRUE(ExecJs(subframe, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  subframe_entry = controller.GetLastCommittedEntry()->GetFrameEntry(subframe);
  int64_t isn_4 = subframe_entry->item_sequence_number();
  int64_t dsn_4 = subframe_entry->document_sequence_number();
  EXPECT_NE(-1, isn_4);
  EXPECT_NE(isn_3, isn_4);
  EXPECT_EQ(dsn_3, dsn_4);
}

// Verifies that the FrameNavigationEntry's redirect chain is created for the
// main frame.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_MainFrameRedirectChain) {
  NavigationControllerImpl& controller = contents()->GetController();

  // Navigate the main frame to a redirecting URL (server-side)
  GURL final_url(embedded_test_server()->GetURL("/simple_page.html"));
  GURL redirecting_url(
      embedded_test_server()->GetURL("/server-redirect?/simple_page.html"));
  NavigateToURLBlockUntilNavigationsComplete(shell(), redirecting_url, 1);
  EXPECT_TRUE(IsLastCommittedEntryOfPageType(contents(), PAGE_TYPE_NORMAL));
  EXPECT_TRUE(contents()->GetLastCommittedURL() == final_url);

  // The last committed NavigationEntry's redirect chain will contain the
  // server-side redirecting URL, then the final URL.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntry* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(entry->GetRedirectChain().size(), 2u);
  EXPECT_EQ(entry->GetRedirectChain()[0], redirecting_url);
  EXPECT_EQ(entry->GetRedirectChain()[1], final_url);

  // No replaced entry because it's not a client-side redirect.
  EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

  // The original request URL will be the first entry of redirect chain, which
  // is the URL that initiated the server redirect.
  EXPECT_EQ(entry->GetOriginalRequestURL(), redirecting_url);

  // No referrer since this is a browser-initiated navigation.
  ExpectReferrerWithDefaultPolicy(entry, GURL());
}

// Verifies that FrameNavigationEntry's redirect chain is created and stored on
// the right subframe (AUTO_SUBFRAME navigation).
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_AutoSubFrameRedirectChain) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe_redirect.html"));
  GURL iframe_redirect_url(
      embedded_test_server()->GetURL("/server-redirect?/simple_page.html"));
  GURL iframe_final_url(embedded_test_server()->GetURL("/simple_page.html"));

  // Navigate to a page with an redirecting iframe.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Check that the main frame redirect chain contains only one url.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* main_entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_entry->GetRedirectChain().size(), 1u);
  EXPECT_EQ(main_entry->GetRedirectChain()[0], main_url);

  // Check that the FrameNavigationEntry's redirect chain contains 2 urls.
  ASSERT_EQ(1U, main_entry->root_node()->children.size());
  scoped_refptr<FrameNavigationEntry> frame_entry =
      main_entry->root_node()->children[0]->frame_entry.get();
  EXPECT_EQ(frame_entry->redirect_chain().size(), 2u);
  EXPECT_EQ(frame_entry->redirect_chain()[0], iframe_redirect_url);
  EXPECT_EQ(frame_entry->redirect_chain()[1], iframe_final_url);

  // No referrer for the main frame since its last navigation is a
  // browser-initiated navigation.
  ExpectReferrerWithDefaultPolicy(main_entry, GURL());

  // The referrer for the iframe is the main frame's URL since the iframe is
  // created by the main frame at load time.
  ExpectReferrerWithDefaultPolicy(frame_entry.get(), main_url);
}

// Verifies that FrameNavigationEntry's redirect chain is created and stored on
// the right subframe (NEW_SUBFRAME navigation).
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FrameNavigationEntry_NewSubFrameRedirectChain) {
  NavigationControllerImpl& controller = contents()->GetController();
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();

  // 1. Navigate to a page with an iframe.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(1, controller.GetEntryCount());

  // 2. Navigate in the subframe (browser-initiated) with a redirection.
  GURL frame_final_url(embedded_test_server()->GetURL("/simple_page.html"));
  GURL frame_redirect_url(
      embedded_test_server()->GetURL("/server-redirect?/simple_page.html"));
  NavigateFrameToURL(root->child_at(0), frame_redirect_url);

  // Check that the main frame redirect chain contains only the main_url.
  EXPECT_EQ(2, controller.GetEntryCount());
  NavigationEntryImpl* main_entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_entry->GetRedirectChain().size(), 1u);
  EXPECT_EQ(main_entry->GetRedirectChain()[0], main_url);

  // Check that the FrameNavigationEntry's redirect chain contains 2 urls.
  ASSERT_EQ(1U, main_entry->root_node()->children.size());
  scoped_refptr<FrameNavigationEntry> frame_entry =
      main_entry->root_node()->children[0]->frame_entry.get();
  EXPECT_EQ(frame_entry->redirect_chain().size(), 2u);
  EXPECT_EQ(frame_entry->redirect_chain()[0], frame_redirect_url);
  EXPECT_EQ(frame_entry->redirect_chain()[1], frame_final_url);

  // No referrer for the main frame since its last navigation is a
  // browser-initiated navigation.
  ExpectReferrerWithDefaultPolicy(main_entry, GURL());

  // No referrer for the iframe since its last navigation is a
  // browser-initiated navigation.
  ExpectReferrerWithDefaultPolicy(frame_entry.get(), GURL());
}

// Checks the contents of the redirect chain after same-document navigations.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_MainFrameRedirectChain_NormalThenSameDocNavigations) {
  NavigationControllerImpl& controller = contents()->GetController();

  // Navigate the main frame to a normal URL that won't cause any redirects.
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));

  {
    EXPECT_TRUE(NavigateToURL(shell(), start_url));

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    ASSERT_EQ(start_url, entry->GetURL());
    // The redirect chain contains only the URL we navigated to.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], start_url);

    // No replaced entry because it's not a client-side redirect.
    EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain, which
    // is also the final URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

    // No referrer since the last navigation is a browser-initiated navigation.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }

  GURL fragment_url(embedded_test_server()->GetURL("/title1.html#foo"));
  {
    // Renderer-initiated fragment navigation with location.replace, which will
    // be treated as a client-side redirect.
    TestNavigationManager navigation_manager(contents(), fragment_url);
    EXPECT_TRUE(ExecJs(contents(), "location.replace('#foo')"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(fragment_url, entry->GetURL());

    EXPECT_EQ(entry->GetRedirectChain().size(), 2u);
    // On script-initiated fragment navigations, the redirect chain contains
    // the previous URL, then the fragment URL, because script-initiated
    // navigations are classified as client redirects when it does replacement.
    // So, they start with the previous page's URL in the redirect chain.
    EXPECT_EQ(entry->GetRedirectChain()[0], start_url);
    EXPECT_EQ(entry->GetRedirectChain()[1], fragment_url);

    EXPECT_TRUE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain,
    // which is the URL that initiated the client redirect.
    EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

    // No referrer (same as the referrer used for loading the document) since
    // the refererrer doesn't change on same-document navigations.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }

  {
    // History API same-document navigation through history.replaceState, which
    // will be treated as a client-side redirect.
    GURL replace_state_url(embedded_test_server()->GetURL("/title1.html#bar"));
    TestNavigationManager navigation_manager(contents(), replace_state_url);
    EXPECT_TRUE(
        ExecJs(shell(), "history.replaceState({}, '', '/title1.html#bar')"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(replace_state_url, entry->GetURL());

    EXPECT_EQ(entry->GetRedirectChain().size(), 2u);
    // On History API navigations with history.replaceState, the redirect chain
    // contains the previous URL, then the fragment URL, because
    // script-initiated navigations are classified as client redirects when it
    // does replacement. So, they start with the previous page's URL in the
    // redirect chain.
    EXPECT_EQ(entry->GetRedirectChain()[0], fragment_url);
    EXPECT_EQ(entry->GetRedirectChain()[1], replace_state_url);

    EXPECT_TRUE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain,
    // which is the URL that initiated the client redirect.
    // It should be client_redirect_target_url since that's what triggered
    // the replacement. But, because this is a same-document replacement,
    // it goes through the EXISTING_ENTRY path which reuses the old entry
    // and doesn't set the original request URL with the new navigation's
    // original request URL.
    // TODO(https://crbug.com/1226489): Fix this.
    EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

    // No referrer (same as the referrer used for loading the document) since
    // the refererrer doesn't change on same-document navigations.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }

  {
    // Browser-initiated fragment navigation.
    GURL fragment_url_2(embedded_test_server()->GetURL("/title1.html#baz"));
    EXPECT_TRUE(NavigateToURL(shell(), fragment_url_2));

    EXPECT_EQ(2, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(fragment_url_2, entry->GetURL());

    // On browser-initiated fragment navigations, the redirect chain contains
    // only the fragment URL, because  browser-initiated navigations aren't
    // classified as client redirects. So, they always start with an empty
    // redirect chain.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], fragment_url_2);

    EXPECT_TRUE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain, which
    // is also the final URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), fragment_url_2);

    // No referrer (same as the referrer used for loading the document) since
    // the refererrer doesn't change on same-document navigations.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }

  {
    // Renderer-initiated fragment navigation with a link click, which won't be
    // treated as a client-side redirect.
    GURL fragment_url_3(embedded_test_server()->GetURL("/title1.html#click"));
    TestNavigationManager navigation_manager(contents(), fragment_url_3);
    EXPECT_TRUE(ExecJs(contents(),
                       "let a = document.createElement('a');"
                       "a.href = 'title1.html#click';"
                       "document.body.appendChild(a);"
                       "a.click();"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(3, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(fragment_url_3, entry->GetURL());

    // The redirect chain contains only the fragment URL, because a link click
    // isn't classified as a client redirect.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], fragment_url_3);

    EXPECT_TRUE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain, which
    // is also the final URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), fragment_url_3);

    // No referrer (same as the referrer used for loading the document) since
    // the refererrer doesn't change on same-document navigations.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }
}

// Checks the contents of the redirect chain and referrer after the main frame
// initiated navigation on its subframe.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_SubframeRedirectChain_MainFrameNavigatingSubframe) {
  NavigationControllerImpl& controller = contents()->GetController();

  // Navigate the main frame to a page with an iframe.
  GURL start_url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* iframe = nullptr;
  GURL iframe_url;

  {
    EXPECT_TRUE(NavigateToURL(shell(), start_url));

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    ASSERT_EQ(start_url, entry->GetURL());
    // The redirect chain contains only the URL we navigated to.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], start_url);

    // No replaced entry because it's not a client-side redirect.
    EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain, which
    // is also the final URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

    iframe = root->child_at(0);
    iframe_url = iframe->current_url();

    scoped_refptr<FrameNavigationEntry> frame_entry =
        controller.GetLastCommittedEntry()->GetFrameEntry(iframe);
    EXPECT_EQ(frame_entry->redirect_chain().size(), 1u);
    EXPECT_EQ(frame_entry->redirect_chain()[0], iframe_url);

    // The referrer for the iframe is the main frame's URL since the iframe is
    // created by the main frame at load time.
    ExpectReferrerWithDefaultPolicy(frame_entry.get(), start_url);
  }

  GURL fragment_url(embedded_test_server()->GetURL("/title1.html#foo"));
  {
    // Renderer-initiated fragment navigation on the iframe, triggered by the
    // main frame setting the src attribute.
    TestNavigationManager navigation_manager(contents(), fragment_url);
    EXPECT_TRUE(ExecJs(
        contents(), JsReplace("document.getElementById('test_iframe').src = $1",
                              fragment_url)));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(2, controller.GetEntryCount());
    scoped_refptr<FrameNavigationEntry> frame_entry =
        controller.GetLastCommittedEntry()->GetFrameEntry(iframe);
    EXPECT_EQ(fragment_url, frame_entry->url());

    EXPECT_EQ(frame_entry->redirect_chain().size(), 1u);
    // Script-initiated fragment navigations are considered as client redirect
    // only when only when it does replacement.
    // This script-initiated fragment navigations is not a client redirect
    // since it doesn't do replacement. So it only contains the
    // fragment URL in its redirect chain.
    EXPECT_EQ(frame_entry->redirect_chain()[0], fragment_url);

    // The referrer is the main frame's URL (same as the referrer used for
    // loading the document) since the refererrer doesn't change on
    // same-document navigations.
    ExpectReferrerWithDefaultPolicy(frame_entry.get(), start_url);
  }

  GURL fragment_url_2(embedded_test_server()->GetURL("/title1.html#bar"));
  {
    // Renderer-initiated fragment navigation on the iframe, triggered by a link
    // click in the main frame.
    TestNavigationManager navigation_manager(contents(), fragment_url_2);
    EXPECT_TRUE(ExecJs(contents(),
                       "let a = document.createElement('a');"
                       "a.href = 'title1.html#bar';"
                       "a.target = 'test_iframe';"
                       "document.body.appendChild(a);"
                       "a.click();"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(3, controller.GetEntryCount());
    scoped_refptr<FrameNavigationEntry> frame_entry =
        controller.GetLastCommittedEntry()->GetFrameEntry(iframe);
    EXPECT_EQ(fragment_url_2, frame_entry->url());

    // The redirect chain only contains the final fragment URL because we don't
    // consider the navigation as a client side redirect.
    EXPECT_EQ(frame_entry->redirect_chain().size(), 1u);
    EXPECT_EQ(frame_entry->redirect_chain()[0], fragment_url_2);

    // The referrer is the main frame's URL (same as the referrer used for
    // loading the document) since the refererrer doesn't change on
    // same-document navigations.
    ExpectReferrerWithDefaultPolicy(frame_entry.get(), start_url);
  }

  GURL cross_document_url(embedded_test_server()->GetURL("/title2.html"));
  {
    // Renderer-initiated cross-document navigation on the iframe, triggered by
    // a link click in the main frame.
    TestNavigationManager navigation_manager(contents(), cross_document_url);
    EXPECT_TRUE(ExecJs(contents(),
                       "let a = document.createElement('a');"
                       "a.href = 'title2.html';"
                       "a.target = 'test_iframe';"
                       "document.body.appendChild(a);"
                       "a.click();"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(4, controller.GetEntryCount());
    scoped_refptr<FrameNavigationEntry> frame_entry =
        controller.GetLastCommittedEntry()->GetFrameEntry(iframe);
    EXPECT_EQ(cross_document_url, frame_entry->url());

    // The redirect chain only contains the final URL because we don't consider
    // the navigation as a client side redirect.
    EXPECT_EQ(frame_entry->redirect_chain().size(), 1u);
    EXPECT_EQ(frame_entry->redirect_chain()[0], cross_document_url);

    // The referrer is the main frame's URL since the cross-document navigation
    // is started by a link click in the main frame.
    ExpectReferrerWithDefaultPolicy(frame_entry.get(), start_url);
  }
}

// Checks the contents of the redirect chain after same-site navigations.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_MainFrameRedirectChain_NormalThenSameSiteNavigations) {
  NavigationControllerImpl& controller = contents()->GetController();

  // Navigate the main frame to a normal URL that won't cause any redirects.
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));

  {
    EXPECT_TRUE(NavigateToURL(shell(), start_url));

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    ASSERT_EQ(start_url, entry->GetURL());
    // The redirect chain contains only the URL we navigated to.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], start_url);

    // No replaced entry because it's not a client-side redirect.
    EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain, which
    // is also the final URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

    // No referrer since the last navigation is a browser-initiated navigation.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }

  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  {
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();

    // Renderer-initiated same-site navigation with location.replace,
    // causing it to be treated as a client redirect.
    TestNavigationObserver navigation_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace("location.replace($1)", url_2)));
    navigation_observer.WaitForNavigationFinished();

    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(url_2, entry->GetURL());

    // On script-initiated same-site navigations with no server redirects, the
    // redirect chain contains the previous page's URL, because script-initiated
    // navigations are classified as client redirects when it does replacement.
    // So, they start with the previous page's URL in the redirect chain.
    EXPECT_EQ(entry->GetRedirectChain().size(), 2u);
    EXPECT_EQ(entry->GetRedirectChain()[0], start_url);
    EXPECT_EQ(entry->GetRedirectChain()[1], url_2);

    EXPECT_TRUE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain,
    // which is the URL that initiated the client redirect.
    EXPECT_EQ(entry->GetOriginalRequestURL(), entry->GetRedirectChain()[0]);

    // The referrer is |start_url| (the previous URL) since the same-site
    // navigation is considered a client-side redirect.
    ExpectReferrerWithDefaultPolicy(entry, start_url);
  }

  {
    // Browser-initiated same-site navigation.
    GURL url_3(embedded_test_server()->GetURL("/title3.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url_3));

    EXPECT_EQ(2, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(url_3, entry->GetURL());

    // On browser-initiated same-site navigations with no redirects, the
    // redirect chain contains only the navigating URL, because
    // browser-initiated navigations aren't classified as client redirects. So,
    // they always start with an empty redirect chain.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], url_3);

    // No replaced entry because it's not a client-side redirect.
    EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain, which
    // is also the final URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), url_3);

    // No referrer since the last navigation is a browser-initiated navigation.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }
}

// Checks the contents of the redirect chain after cross-site navigations.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_MainFrameRedirectChain_NormalThenCrossSiteNavigations) {
  NavigationControllerImpl& controller = contents()->GetController();

  // Navigate the main frame to a normal URL that won't cause any redirects.
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));

  {
    EXPECT_TRUE(NavigateToURL(shell(), start_url));

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    ASSERT_EQ(start_url, entry->GetURL());
    // The redirect chain contains only the URL we navigated to.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], start_url);

    // No replaced entry because it's not a client-side redirect.
    EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain, which
    // is also the final URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

    // No referrer since the last navigation is a browser-initiated navigation.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }

  GURL url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  {
    // Do a renderer-initiated cross-site navigation using location.replace
    // that's considered a client-side redirect.
    TestNavigationObserver nav_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(shell(), JsReplace("location.replace($1)", url_2)));
    nav_observer.Wait();

    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(url_2, entry->GetURL());

    // On script-initiated cross-site navigations with no server redirects, the
    // redirect chain contains the previous page's URL, because script-initiated
    // navigations are classified as client redirects when they do replacement.
    // So, they start with the previous page's URL in the redirect chain.
    EXPECT_EQ(entry->GetRedirectChain().size(), 2u);
    EXPECT_EQ(entry->GetRedirectChain()[0], start_url);
    EXPECT_EQ(entry->GetRedirectChain()[1], url_2);

    EXPECT_TRUE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain,
    // which is the URL that initiated the client redirect.
    EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

    // The referrer is |start_url| since the cross-site navigation is
    // renderer-initiated (and considered a client-side redirect)
    // TODO(https://crbug.com/1218786): This uses the full URL instead of only
    // the origin even though it went cross-origin, because of the special
    // handling of client-side redirects in the referrer calculation for the
    // FrameNavigationEntry. This might not be the same as the referrer value
    // used by the navigation request (which is correctly sanitized to only
    // contain the origin). Maybe fix this?
    ExpectReferrerWithDefaultPolicy(entry, start_url);
  }

  GURL url_3(embedded_test_server()->GetURL("c.com", "/title3.html"));
  {
    // Do a renderer-initiated cross-site navigation that's not considered a
    // client-side redirect.
    TestNavigationManager navigation_manager(contents(), url_3);
    EXPECT_TRUE(
        ExecJs(contents(), JsReplace("let a = document.createElement('a');"
                                     "a.href = $1;"
                                     "document.body.appendChild(a);"
                                     "a.click();",
                                     url_3)));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(2, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(url_3, entry->GetURL());

    // Since the navigation is not a client-side redirect and did not encounter
    // server-side redirects, the redirect chain only contains one URL: |url_3|.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], url_3);

    // The original request URL will be the first entry of redirect chain,
    // which is the URL that initiated the client redirect.
    EXPECT_EQ(entry->GetOriginalRequestURL(), url_3);

    // The referrer is |url_2| since it's the URL that started the navigation.
    // Since the navigation went through a cross-origin redirect and the default
    // referrer policy strips referrers to their origins on cross-origin
    // requests, only the origin is saved.
    ExpectReferrerWithDefaultPolicy(entry, url_2.DeprecatedGetOriginAsURL());
  }

  {
    // Browser-initiated cross-site navigation.
    GURL url_4(embedded_test_server()->GetURL("d.com", "/empty.html"));
    EXPECT_TRUE(NavigateToURL(shell(), url_4));

    EXPECT_EQ(3, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(url_4, entry->GetURL());

    // On browser-initiated cross-site navigations with no redirects, the
    // redirect chain contains only the navigating URL, because
    // browser-initiated navigations aren't classified as client redirects. So,
    // they always start with an empty redirect chain.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], url_4);

    // No replaced entry because it's not a client-side redirect.
    EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain, which
    // is also the final URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), url_4);

    // No referrer since the last navigation is a browser-initiated navigation.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }
}

// Verifies that the FrameNavigationEntry's redirect chain and referrer is
// correct for the main frame after a cross-origin navigation triggered by
// setting location.replace, which is considered a client redirect.
// location.replace is used instead of location.href as frame navigation is
// considered to be a client redirect only if it does replacement.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_MainFrameRedirectChain_CrossOriginClientRedirect_LocationReplace) {
  NavigationControllerImpl& controller = contents()->GetController();

  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(1, controller.GetEntryCount());

  // Navigate the main frame to a redirecting URL (server-side) by setting
  // location.replace, which should count as a client side redirect.

  GURL target_url(embedded_test_server()->GetURL("b.com", "/simple_page.html"));
  TestNavigationManager navigation_manager(contents(), target_url);
  EXPECT_TRUE(
      ExecJs(contents(), JsReplace("location.replace($1);", target_url)));
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  // The last committed NavigationEntry's redirect chain will contain the
  // client-side redirector URL, then the target URL.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntry* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(entry->GetRedirectChain().size(), 2u);
  EXPECT_EQ(entry->GetRedirectChain()[0], start_url);
  EXPECT_EQ(entry->GetRedirectChain()[1], target_url);

  // The original request URL will be the first entry of redirect chain, which
  // is the URL that initiated the server redirect.
  EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

  // The referrer is |start_url| since the navigation is considered a
  // client-side redirect so it uses the previous document's URL.
  // TODO(https://crbug.com/1218786): This uses the full URL instead of only
  // the origin even though it went cross-origin, because of the special
  // handling of client-side redirects in the referrer calculation for the
  // FrameNavigationEntry. This might not be the same as the referrer value
  // used by the navigation request (which is correctly sanitized to only
  // contain the origin). Maybe fix this?
  ExpectReferrerWithDefaultPolicy(entry, start_url);
}

// Verifies that the FrameNavigationEntry's redirect chain and referrer is
// correct for the main frame after an initially same-origin navigation that
// ended up as cross origin, triggered by setting location.replace.
// location.replace is used instead of location.href as frame navigation is
// considered to be a client redirect only if it does replacement.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_MainFrameRedirectChain_SameOriginThenCrossOriginRedirect_LocationReplace) {
  NavigationControllerImpl& controller = contents()->GetController();

  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(1, controller.GetEntryCount());

  // Navigate the main frame to a redirecting URL (server-side) by setting
  // location.replace, which should count as a client side redirect.
  GURL final_url(embedded_test_server()->GetURL("b.com", "/simple_page.html"));
  GURL redirecting_url(
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec()));
  TestNavigationManager navigation_manager(contents(), redirecting_url);
  EXPECT_TRUE(
      ExecJs(contents(), JsReplace("location.replace($1);", redirecting_url)));
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  // The last committed NavigationEntry's redirect chain will contain the
  // client-side redirector URL, server-side redirecting URL, then the final
  // URL.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntry* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(entry->GetRedirectChain().size(), 3u);
  EXPECT_EQ(entry->GetRedirectChain()[0], start_url);
  EXPECT_EQ(entry->GetRedirectChain()[1], redirecting_url);
  EXPECT_EQ(entry->GetRedirectChain()[2], final_url);

  // The original request URL will be the first entry of redirect chain, which
  // is the URL that initiated the server redirect.
  EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

  // The referrer is |start_url| since the navigation is considered a
  // client-side redirect so it uses the previous document's URL.
  // TODO(https://crbug.com/1218786): This uses the full URL instead of only
  // the origin even though it went cross-origin, because of the special
  // handling of client-side redirects in the referrer calculation for the
  // FrameNavigationEntry. This might not be the same as the referrer value
  // used by the navigation request (which is correctly sanitized to only
  // contain the origin). Maybe fix this?
  ExpectReferrerWithDefaultPolicy(entry, start_url);
}

// Verifies that the FrameNavigationEntry's redirect chain and referrer is
// correct for the main frame after an initially same-origin navigation that
// ended up as cross origin, triggered by a link click.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_MainFrameRedirectChain_SameOriginThenCrossOriginRedirect_LinkClick) {
  NavigationControllerImpl& controller = contents()->GetController();

  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(1, controller.GetEntryCount());

  // Navigate the main frame to a redirecting URL (server-side) by clicking a
  // link, which should not count as a client side redirect.
  GURL final_url(embedded_test_server()->GetURL("b.com", "/simple_page.html"));
  GURL redirecting_url(
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec()));

  TestNavigationManager navigation_manager(contents(), redirecting_url);
  EXPECT_TRUE(
      ExecJs(contents(), JsReplace("let a = document.createElement('a');"
                                   "a.href = $1;"
                                   "document.body.appendChild(a);"
                                   "a.click();",
                                   redirecting_url)));
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  // The last committed NavigationEntry's redirect chain will contain the
  // server-side redirecting URL and the final URL.
  EXPECT_EQ(2, controller.GetEntryCount());
  NavigationEntry* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(entry->GetRedirectChain().size(), 2u);
  EXPECT_EQ(entry->GetRedirectChain()[0], redirecting_url);
  EXPECT_EQ(entry->GetRedirectChain()[1], final_url);

  // The original request URL will be the first entry of redirect chain, which
  // is the URL that initiated the server redirect.
  EXPECT_EQ(entry->GetOriginalRequestURL(), redirecting_url);

  // The referrer is |start_url| since it's the URL that started the navigation.
  // Since the navigation went through a cross-origin redirect and the default
  // referrer policy strips referrers to their origins on cross-origin
  // requests, only the origin is saved.
  ExpectReferrerWithDefaultPolicy(entry, start_url.DeprecatedGetOriginAsURL());
}

// Verifies that the FrameNavigationEntry's redirect chain and referrer is
// correct for the main frame after a navigation triggered by a link click to a
// URL that started out as cross origin but ended up being same origin to the
// original page.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_MainFrameRedirectChain_CrossOriginThenSameOriginRedirect_LinkClick) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(1, controller.GetEntryCount());

  // Navigate the main frame to a redirecting URL (server-side) by clicking a
  // link, which should not count as a client side redirect. The link started
  // out cross-origin but ended up being in the same origin as the current
  // document.
  GURL final_url(embedded_test_server()->GetURL("/simple_page.html"));
  GURL redirecting_url(embedded_test_server()->GetURL(
      "b.com", "/server-redirect?" + final_url.spec()));

  TestNavigationManager navigation_manager(shell()->web_contents(),
                                           redirecting_url);
  EXPECT_TRUE(
      ExecJs(contents(), JsReplace("let a = document.createElement('a');"
                                   "a.href = $1;"
                                   "document.body.appendChild(a);"
                                   "a.click();",
                                   redirecting_url)));
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  // The last committed NavigationEntry's redirect chain will contain the
  // server-side redirecting URL and the final URL.
  EXPECT_EQ(2, controller.GetEntryCount());
  content::NavigationEntry* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(entry->GetRedirectChain().size(), 2u);
  EXPECT_EQ(entry->GetRedirectChain()[0], redirecting_url);
  EXPECT_EQ(entry->GetRedirectChain()[1], final_url);

  // The original request URL will be the first entry of redirect chain, which
  // is the URL that initiated the server redirect.
  EXPECT_EQ(entry->GetOriginalRequestURL(), redirecting_url);

  // The referrer is |start_url| since it's the URL that started the navigation.
  // Since the navigation went through a cross-origin URL (even though it ended
  // up on a same-origin URL after redirects), only the origin is saved.
  ExpectReferrerWithDefaultPolicy(entry, start_url.DeprecatedGetOriginAsURL());
}

// Checks the contents of the redirect chain after reloads.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_MainFrameRedirectChain_NormalThenReloads) {
  NavigationControllerImpl& controller = contents()->GetController();

  // Navigate the main frame to a normal URL that won't cause any redirects.
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));

  {
    EXPECT_TRUE(NavigateToURL(shell(), start_url));

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    ASSERT_EQ(start_url, entry->GetURL());
    // The redirect chain contains only the URL we navigated to.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], start_url);

    // No replaced entry because it's not a client-side redirect.
    EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain, which
    // is also the final URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

    // No referrer since the last navigation is a browser-initiated navigation.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }

  {
    // Browser-initiated tab reload.
    TestNavigationManager navigation_manager(contents(), start_url);
    shell()->Reload();
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(start_url, entry->GetURL());

    // On browser-initiated tab reloads with no redirects, the redirect
    // chain only contains the original URL once, because browser-initiated
    // navigations aren't classified as client redirects. So, they always start
    // with an empty redirect chain.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], start_url);

    // There is no "replaced entry".
    EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain, which
    // is also the final URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

    // A browser-initiated reload reuses the most recent entry's referrer; since
    // the most recent navigation before the reload was browser-initiated, the
    // referrer URL is empty.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }

  {
    // Renderer-initiated reload.
    TestNavigationManager navigation_manager(contents(), start_url);
    EXPECT_TRUE(ExecJs(contents(), "location.reload();"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(start_url, entry->GetURL());

    // On main frame script-initiated reloads with no redirects, the redirect
    // chain contains the reloaded page's URL twice, because script-initiated
    // navigations are always classified as client redirects. So, they start
    // with the reloaded page's URL in the redirect chain.
    EXPECT_EQ(entry->GetRedirectChain().size(), 2u);
    EXPECT_EQ(entry->GetRedirectChain()[0], start_url);
    EXPECT_EQ(entry->GetRedirectChain()[1], start_url);

    // The URL is saved as the "replaced entry" because it's a reload.
    ASSERT_TRUE(entry->GetReplacedEntryData().has_value());
    EXPECT_EQ(start_url, entry->GetReplacedEntryData()->first_committed_url);

    // The original request URL will be the first entry of redirect chain,
    // which is the URL that initiated the client redirect.
    EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

    // The referrer is |start_url| since this is a renderer-initiated reload.
    ExpectReferrerWithDefaultPolicy(entry, start_url);
  }

  {
    // Browser-initiated tab reload after a renderer-initiated reload.
    TestNavigationManager navigation_manager(contents(), start_url);
    shell()->Reload();
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(start_url, entry->GetURL());

    // On browser-initiated tab reloads with no redirects, the redirect
    // chain only contains the original URL once, because browser-initiated
    // navigations aren't classified as client redirects. So, they always start
    // with an empty redirect chain.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], start_url);

    // The URL is saved as the "replaced entry" because we reuse the entry's
    // replaced entry on reloads.
    ASSERT_TRUE(entry->GetReplacedEntryData().has_value());
    EXPECT_EQ(start_url, entry->GetReplacedEntryData()->first_committed_url);

    // The original request URL will be the first entry of redirect chain, which
    // is also the final URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

    // The referrer is |start_url| since we reuse the entry's referrer in
    // browser-initiated reloads.
    ExpectReferrerWithDefaultPolicy(entry, start_url);
  }

  {
    // Browser-initiated frame reload. Note that this is different than tab
    // reload, as this case goes through NavigationControllerImpl::ReloadFrame()
    // instead of NavigationControllerImpl::Reload().
    TestNavigationManager navigation_manager(contents(), start_url);
    contents()->GetPrimaryMainFrame()->Reload();
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(start_url, entry->GetURL());

    // On main-frame browser-initiated frame reloads with no redirects, the
    // redirect chain only contains the original URL once.
    // This should've contained two copies of `start_url`, because the browser
    // actually sent the previous FNE's redirect chain at commit, containing the
    // one entry of `start_url` from before. The renderer should've added one
    // more entry of `start_url` (as the current document URL) and sent that
    // back to the browser, but the renderer actually thought that the redirect
    // chain is empty (because it checked for the redirect_response array,
    // instead of the redirects array). So we end up with a redirect chain of
    // size 1.
    // TODO(https://crbug.com/1171225): Fix this.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], start_url);

    // The URL is saved as the "replaced entry" because we reuse the entry's
    // replaced entry on reloads.
    ASSERT_TRUE(entry->GetReplacedEntryData().has_value());
    EXPECT_EQ(start_url, entry->GetReplacedEntryData()->first_committed_url);

    // The original request URL will be the first entry of redirect chain, which
    // is also the final URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

    // The referrer is |start_url| since we reuse the entry's referrer in
    // browser-initiated reloads.
    ExpectReferrerWithDefaultPolicy(entry, start_url);
  }
}

// Checks the contents of the redirect chain after reloads on a subframe.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_MainFrameRedirectChain_NormalThenReloads_Subframe) {
  NavigationControllerImpl& controller = contents()->GetController();

  // Navigate the main frame to a normal URL that won't cause any redirects and
  // has an iframe.
  GURL start_url(embedded_test_server()->GetURL("/page_with_iframe.html"));

  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* iframe = nullptr;
  GURL iframe_url;

  {
    EXPECT_TRUE(NavigateToURL(shell(), start_url));

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    ASSERT_EQ(start_url, entry->GetURL());
    // The redirect chain contains only the URL we navigated to.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], start_url);

    // No replaced entry because it's not a client-side redirect.
    EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain, which
    // is also the final URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

    iframe = root->child_at(0);
    iframe_url = iframe->current_url();

    scoped_refptr<FrameNavigationEntry> frame_entry =
        controller.GetLastCommittedEntry()->GetFrameEntry(iframe);
    EXPECT_EQ(frame_entry->redirect_chain().size(), 1u);
    EXPECT_EQ(frame_entry->redirect_chain()[0], iframe_url);

    // The referrer for the iframe is the main frame's URL since the iframe is
    // created by the main frame at load time.
    ExpectReferrerWithDefaultPolicy(frame_entry.get(), start_url);
  }

  {
    // Browser-initiated reload.
    TestNavigationManager navigation_manager(contents(), iframe_url);
    root->child_at(0)->current_frame_host()->Reload();
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(1, controller.GetEntryCount());
    scoped_refptr<FrameNavigationEntry> frame_entry =
        controller.GetLastCommittedEntry()->GetFrameEntry(iframe);
    EXPECT_EQ(iframe_url, frame_entry->url());

    // On subframe browser-initiated reloads, the redirect chain contains two
    // copies of `iframe_url`, because we reused the previous FNE's redirect
    // chain at commit, containing the two entries seen above, and we added one
    // more entry of `iframe_url` (as the current document URL).
    EXPECT_EQ(frame_entry->redirect_chain().size(), 2u);
    EXPECT_EQ(frame_entry->redirect_chain()[0], iframe_url);
    EXPECT_EQ(frame_entry->redirect_chain()[1], iframe_url);

    // The referrer is |start_url| since we reuse the entry's referrer in
    // browser-initiated reloads.
    ExpectReferrerWithDefaultPolicy(frame_entry.get(), start_url);
  }

  {
    // Renderer-initiated reload on the iframe.
    TestNavigationManager navigation_manager(contents(), iframe_url);
    EXPECT_TRUE(ExecJs(iframe, "location.reload();"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(1, controller.GetEntryCount());
    scoped_refptr<FrameNavigationEntry> frame_entry =
        controller.GetLastCommittedEntry()->GetFrameEntry(iframe);
    EXPECT_EQ(iframe_url, frame_entry->url());

    // On renderer-initiated reloads with no redirects, the redirect chain
    // contains the reloaded page's URL twice.
    EXPECT_EQ(frame_entry->redirect_chain().size(), 2u);
    EXPECT_EQ(frame_entry->redirect_chain()[0], iframe_url);
    EXPECT_EQ(frame_entry->redirect_chain()[1], iframe_url);

    // The referrer is |iframe_url| since this is a renderer-initiated reload.
    ExpectReferrerWithDefaultPolicy(frame_entry.get(), iframe_url);
  }

  {
    // Browser-initiated reload after a renderer-initiated reload.
    TestNavigationManager navigation_manager(contents(), iframe_url);
    root->child_at(0)->current_frame_host()->Reload();
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(1, controller.GetEntryCount());
    scoped_refptr<FrameNavigationEntry> frame_entry =
        controller.GetLastCommittedEntry()->GetFrameEntry(iframe);
    EXPECT_EQ(iframe_url, frame_entry->url());

    // On subframe browser-initiated reloads, the redirect chain contains three
    // copies of `iframe_url`, because we reused the previous FNE's redirect
    // chain at commit, containing the two entries seen above, and we added one
    // more entry of `iframe_url` (as the current document URL).
    EXPECT_EQ(frame_entry->redirect_chain().size(), 3u);
    EXPECT_EQ(frame_entry->redirect_chain()[0], iframe_url);
    EXPECT_EQ(frame_entry->redirect_chain()[1], iframe_url);
    EXPECT_EQ(frame_entry->redirect_chain()[2], iframe_url);

    // The referrer is the same as the previous navigation since this is a
    // browser-initiated reload and we reuse the entry's referrer.
    ExpectReferrerWithDefaultPolicy(frame_entry.get(), iframe_url);
  }
}

// Checks the contents of the redirect chain after navigation to an error page.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_MainFrameRedirectChain_NormalThenErrorPage) {
  NavigationControllerImpl& controller = contents()->GetController();

  // Navigate the main frame to a normal URL that won't cause any redirects.
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));

  {
    EXPECT_TRUE(NavigateToURL(shell(), start_url));

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    ASSERT_EQ(start_url, entry->GetURL());
    // The redirect chain contains only the URL we navigated to.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], start_url);

    // No replaced entry because it's not a client-side redirect.
    EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain, which
    // is also the final URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

    // No referrer since the last navigation is a browser-initiated navigation.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }

  GURL url_2(embedded_test_server()->GetURL("b.com", "/empty404.html"));
  {
    // Renderer-initiated cross-site navigation to an error page.
    EXPECT_FALSE(NavigateToURLFromRenderer(shell(), url_2));

    EXPECT_EQ(2, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(url_2, entry->GetURL());

    // On renderer-initiated cross-site navigations with no redirects that end
    // up in error pages, the redirect chain contains the final URL.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], url_2);

    // No replaced entry because it's not a client-side redirect.
    EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

    // The original request URL on navigations that end up in an error page will
    // be the URL of the page that failed to load.
    EXPECT_EQ(entry->GetOriginalRequestURL(), url_2);

    // The referrer is the previous page's URL but only the origin since it went
    // cross-origin, and even though it is using a renderer-initiated method
    // that is typically classified as a client side redirect, it is not
    // considered as a client-side redirect since we end up in an error page.
    // (Non-error client-side redirects will always return the full URL).
    ExpectReferrerWithDefaultPolicy(entry,
                                    start_url.DeprecatedGetOriginAsURL());
  }

  {
    // Browser-initiated cross-site navigation to an error page.
    GURL url_3(embedded_test_server()->GetURL("c.com", "/empty404.html"));
    EXPECT_FALSE(NavigateToURL(shell(), url_3));

    EXPECT_EQ(3, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(url_3, entry->GetURL());

    // On browser-initiated cross-site navigations with no redirects that end
    // up in error pages, the redirect chain contains the final URL.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], url_3);

    // No replaced entry because it's not a client-side redirect.
    EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

    // The original request URL on navigations that end up in an error page will
    // be the URL of the page that failed to load.
    EXPECT_EQ(entry->GetOriginalRequestURL(), url_3);

    // No referrer since the last navigation is a browser-initiated navigation.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }
}

// Checks the contents of the redirect chain after a browser-initiated
// navigation that server-redirects to an error page.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_ServerRedirectToErrorPage_BrowserInitiated) {
  NavigationControllerImpl& controller = contents()->GetController();

  GURL server_redirecting_url(
      embedded_test_server()->GetURL("/server-redirect?/empty404.html"));

  // Browser-initiated same-site navigation that server-redirects to an empty
  // 404 page, which would result in an error page.
  EXPECT_FALSE(NavigateToURL(shell(), server_redirecting_url));

  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntry* entry = controller.GetLastCommittedEntry();
  GURL fail_url(embedded_test_server()->GetURL("/empty404.html"));
  EXPECT_EQ(fail_url, entry->GetURL());

  // On navigations that end up in error pages, the redirect chain only
  // contains the final URL, even if the navigation went through server
  // redirects.
  EXPECT_THAT(entry->GetRedirectChain(),
              testing::ElementsAre(server_redirecting_url, fail_url));

  // No replaced entry because it's not a client-side redirect.
  EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

  // The original request URL on navigations that end up in an error page will
  // still be the iniital URL.
  EXPECT_EQ(entry->GetOriginalRequestURL(), server_redirecting_url);

  // No referrer since the last navigation is a browser-initiated navigation.
  ExpectReferrerWithDefaultPolicy(entry, GURL());
}

// Checks the contents of the redirect chain after a renderer-initiated
// navigation that server-redirects to an error page.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_ServerRedirectToErrorPage_RendererInitiated) {
  NavigationControllerImpl& controller = contents()->GetController();

  // Navigate the main frame to a normal URL that won't cause any redirects, so
  // that we can do a renderer-initiated navigation after this.
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));

  {
    EXPECT_TRUE(NavigateToURL(shell(), start_url));

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    ASSERT_EQ(start_url, entry->GetURL());
    // The redirect chain contains only the URL we navigated to.
    EXPECT_EQ(entry->GetRedirectChain().size(), 1u);
    EXPECT_EQ(entry->GetRedirectChain()[0], start_url);

    // No replaced entry because it's not a client-side redirect.
    EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

    // The original request URL will be the first entry of redirect chain, which
    // is also the final URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), start_url);

    // No referrer since the last navigation is a browser-initiated navigation.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }

  {
    // Renderer-initiated same-site navigation that server-redirects to an
    // empty 404 page, which would result in an error page. Note that since this
    // is a script-initiated navigation, it will be marked as a client redirect
    // too.
    GURL server_redirecting_url(
        embedded_test_server()->GetURL("/server-redirect?/empty404.html"));
    GURL fail_url(embedded_test_server()->GetURL("/empty404.html"));
    EXPECT_FALSE(NavigateToURLFromRenderer(shell(), server_redirecting_url));

    EXPECT_EQ(2, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(fail_url, entry->GetURL());

    // On navigations that end up in error pages, the redirect chain should
    // still contain the original and all the intermediate URLs.
    EXPECT_THAT(entry->GetRedirectChain(),
                testing::ElementsAre(server_redirecting_url, fail_url));

    // No replaced entry because it's not a client-side redirect.
    EXPECT_FALSE(entry->GetReplacedEntryData().has_value());

    // The original request URL on navigations that end up in an error page will
    // still be the iniital URL.
    EXPECT_EQ(entry->GetOriginalRequestURL(), server_redirecting_url);

    // The referrer is the previous page's URL since this is renderer-initiated,
    // but is not considered as a client-side redirect since we end up in an
    // error page.
    ExpectReferrerWithDefaultPolicy(entry, start_url);
  }
}

// Tests navigating from an error page that server-redirects to the current URL
// (which results in an error page again).
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ServerRedirectToSameURLErrorPage) {
  NavigationControllerImpl& controller = contents()->GetController();

  GURL fail_url(embedded_test_server()->GetURL("/empty404.html"));
  GURL server_redirecting_url(
      embedded_test_server()->GetURL("/server-redirect?/empty404.html"));

  // Navigate to a URL that will result in an error page.
  EXPECT_FALSE(NavigateToURL(shell(), fail_url));
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntry* last_entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(fail_url, last_entry->GetURL());

  // Navigate to a URL that redirects to the same URL we're currently on, which
  // will commit an error page again.
  EXPECT_FALSE(NavigateToURL(shell(), server_redirecting_url, fail_url));

  // The navigation will do a replacement.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_NE(last_entry, controller.GetLastCommittedEntry());
  last_entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(fail_url, last_entry->GetURL());

  // We replaced the previous entry.
  EXPECT_TRUE(last_entry->GetReplacedEntryData().has_value());
  EXPECT_EQ(fail_url, last_entry->GetReplacedEntryData()->first_committed_url);

  // No referrer since the last navigation is a browser-initiated navigation.
  ExpectReferrerWithDefaultPolicy(last_entry, GURL());
}

// Checks the contents of the redirect chain after client-side redirect to a
// different document than the original URL.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_MainFrameRedirectChain_ClientRedirectThenFragment) {
  NavigationControllerImpl& controller = contents()->GetController();

  // Navigate the main frame to a redirecting URL (client-side) that will
  // redirect to a different document than the original URL's document.
  GURL client_redirecting_url(embedded_test_server()->GetURL(
      "/navigation_controller/client_redirect.html"));
  GURL client_redirect_target_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));

  {
    // Client-side redirects will result in a new navigation, so wait for two
    // navigations to finish.
    TestNavigationManager navigation_manager_1(contents(),
                                               client_redirecting_url);
    TestNavigationManager navigation_manager_2(contents(),
                                               client_redirect_target_url);
    shell()->LoadURL(client_redirecting_url);
    ASSERT_TRUE(navigation_manager_1
                    .WaitForNavigationFinished());  // Initial navigation.
    ASSERT_TRUE(navigation_manager_2
                    .WaitForNavigationFinished());  // Client-side redirect.

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetEntryAtIndex(0);
    ASSERT_EQ(client_redirect_target_url, entry->GetURL());
    EXPECT_EQ(entry->GetRedirectChain().size(), 2u);
    // When a client-side redirect happens after a navigation, the redirect
    // chain contains the URL that triggers the client redirect first, then the
    // URL that we got redirected to.
    EXPECT_EQ(entry->GetRedirectChain()[0], client_redirecting_url);
    EXPECT_EQ(entry->GetRedirectChain()[1], client_redirect_target_url);

    // The client-redirecting URL is saved as the "replaced entry".
    ASSERT_TRUE(entry->GetReplacedEntryData().has_value());
    EXPECT_EQ(client_redirecting_url,
              entry->GetReplacedEntryData()->first_committed_url);

    // The original request URL will be the first entry of redirect chain,
    // which is the URL that initiated the client redirect.
    EXPECT_EQ(entry->GetOriginalRequestURL(), client_redirecting_url);

    // The referrer is |client_redirecting_url| since the navigation is a
    // client-side redirect.
    ExpectReferrerWithDefaultPolicy(entry, client_redirecting_url);
  }

  {
    // Renderer-initiated fragment navigation that does replacement.
    GURL fragment_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html#foo"));
    TestNavigationManager navigation_manager(contents(), fragment_url);
    EXPECT_TRUE(ExecJs(contents(), "location.replace('#foo');"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(fragment_url, entry->GetURL());

    EXPECT_EQ(entry->GetRedirectChain().size(), 2u);
    // When a renderer-initiated fragment navigation that does replacement
    // happens after a client-side redirect navigation, the redirect chain
    // will contain the previous URL
    // (the URL we ended up in after client redirect), then the fragment URL.
    EXPECT_EQ(entry->GetRedirectChain()[0], client_redirect_target_url);
    EXPECT_EQ(entry->GetRedirectChain()[1], fragment_url);

    // The client-redirecting URL is still saved as the "replaced entry".
    EXPECT_TRUE(entry->GetReplacedEntryData().has_value());
    EXPECT_EQ(client_redirecting_url,
              entry->GetReplacedEntryData()->first_committed_url);

    // The original request URL will be the first entry of redirect chain,
    // which is the URL that initiated the client redirect.
    // It should be client_redirect_target_url since that's what triggered
    // the replacement. But, because this is a same-document replacement,
    // it goes through the EXISTING_ENTRY path which reuses the old entry
    // and doesn't set the original request URL with the new navigation's
    // original request URL.
    // TODO(https://crbug.com/1226489): Fix this.
    EXPECT_EQ(entry->GetOriginalRequestURL(), client_redirecting_url);

    // The referrer is |client_redirecting_url| (same as the referrer used for
    // loading the document) since the referrer doesn't change on same-document
    // navigations.
    ExpectReferrerWithDefaultPolicy(entry, client_redirecting_url);
  }
}

// Checks the contents of the redirect chain after client-side redirect within
// the same document.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_MainFrameRedirectChain_ClientRedirectSameDocThenFragment) {
  NavigationControllerImpl& controller = contents()->GetController();

  // Navigate the main frame to a redirecting URL (client-side) that will
  // redirect to the same document as the original URL.
  GURL client_redirecting_url(embedded_test_server()->GetURL(
      "/navigation_controller/client_redirect_fragment.html"));
  GURL client_redirect_target_url(embedded_test_server()->GetURL(
      "/navigation_controller/client_redirect_fragment.html#foo"));

  {
    // Client-side redirects will result in a new navigation, so wait for two
    // navigations to finish.
    TestNavigationManager navigation_manager_1(contents(),
                                               client_redirecting_url);
    TestNavigationManager navigation_manager_2(contents(),
                                               client_redirect_target_url);
    shell()->LoadURL(client_redirecting_url);
    ASSERT_TRUE(navigation_manager_1
                    .WaitForNavigationFinished());  // Initial navigation.
    ASSERT_TRUE(navigation_manager_2
                    .WaitForNavigationFinished());  // Client-side redirect.

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetEntryAtIndex(0);
    ASSERT_EQ(client_redirect_target_url, entry->GetURL());

    // When a client-side redirect happens after a navigation, the redirect
    // chain contains the URL that triggers the client redirect first, then the
    // URL that we got redirected to.
    EXPECT_EQ(entry->GetRedirectChain().size(), 2u);
    EXPECT_EQ(entry->GetRedirectChain()[0], client_redirecting_url);
    EXPECT_EQ(entry->GetRedirectChain()[1], client_redirect_target_url);

    // The client-redirecting URL is saved as the "replaced entry".
    ASSERT_TRUE(entry->GetReplacedEntryData().has_value());
    EXPECT_EQ(client_redirecting_url,
              entry->GetReplacedEntryData()->first_committed_url);

    // The original request URL will be the first entry of redirect chain,
    // which is the URL that initiated the client redirect.
    EXPECT_EQ(entry->GetOriginalRequestURL(), client_redirecting_url);

    // No referrer since the navigation started as a browser-initiated
    // navigation, and the client redirect that happened afterwards is a
    // same-document navigation, which won't change the referrer.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }

  {
    // Renderer-initiated fragment navigation that does replacement.
    GURL fragment_url(embedded_test_server()->GetURL(
        "/navigation_controller/client_redirect_fragment.html#bar"));
    TestNavigationManager navigation_manager(contents(), fragment_url);
    EXPECT_TRUE(ExecJs(contents(), "location.replace('#bar');"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(fragment_url, entry->GetURL());

    // When a renderer-initiated fragment navigation that does replacement
    // happens after a client-side redirect navigation,
    // the redirect chain will contain the previous URL
    // (the URL we ended up in after client redirect), then the fragment URL.
    EXPECT_EQ(entry->GetRedirectChain().size(), 2u);
    EXPECT_EQ(entry->GetRedirectChain()[0], client_redirect_target_url);
    EXPECT_EQ(entry->GetRedirectChain()[1], fragment_url);

    EXPECT_TRUE(entry->GetReplacedEntryData().has_value());
    EXPECT_EQ(client_redirecting_url,
              entry->GetReplacedEntryData()->first_committed_url);

    // The original request URL will be the first entry of redirect chain,
    // which is the URL that initiated the client redirect.
    // It should be client_redirect_target_url since that's what triggered
    // the replacement. But, because this is a same-document replacement,
    // it goes through the EXISTING_ENTRY path which reuses the old entry
    // and doesn't set the original request URL with the new navigation's
    // original request URL.
    // TODO(https://crbug.com/1226489): Fix this.
    EXPECT_EQ(entry->GetOriginalRequestURL(), client_redirecting_url);

    // No referrer (same as the referrer used for loading the document) since
    // the refererrer doesn't change on same-document navigations.
    ExpectReferrerWithDefaultPolicy(entry, GURL());
  }
}

// Checks the contents of the redirect chain after a client-side redirect that
// happens after a server-side redirect.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_MainFrameRedirectChain_ServerThenClientRedirect) {
  NavigationControllerImpl& controller = contents()->GetController();

  // Navigate the main frame to a redirecting URL (server-side then client-side)
  GURL server_redirecting_url(embedded_test_server()->GetURL(
      "/server-redirect?/navigation_controller/client_redirect.html"));
  GURL client_redirecting_url(embedded_test_server()->GetURL(
      "/navigation_controller/client_redirect.html"));
  GURL final_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  {
    TestNavigationManager navigation_manager_1(contents(),
                                               server_redirecting_url);
    TestNavigationManager navigation_manager_2(contents(), final_url);

    shell()->LoadURL(server_redirecting_url);

    ASSERT_TRUE(navigation_manager_1
                    .WaitForNavigationFinished());  // Initial navigation.
    ASSERT_TRUE(navigation_manager_2
                    .WaitForNavigationFinished());  // Client-side redirect.

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetEntryAtIndex(0);
    ASSERT_EQ(final_url, entry->GetURL());

    // The redirect chain only contains the URL that triggers the client
    // redirect and the URL that we got redirected to. It does not contain the
    // server-redirecting URL.
    EXPECT_EQ(entry->GetRedirectChain().size(), 2u);
    EXPECT_EQ(entry->GetRedirectChain()[0], client_redirecting_url);
    EXPECT_EQ(entry->GetRedirectChain()[1], final_url);

    // The client-redirecting URL is saved as the "replaced entry".
    ASSERT_TRUE(entry->GetReplacedEntryData().has_value());
    EXPECT_EQ(client_redirecting_url,
              entry->GetReplacedEntryData()->first_committed_url);

    // The original request URL will be the first entry of redirect chain,
    // which is the URL that initiated the client redirect.
    EXPECT_EQ(entry->GetOriginalRequestURL(), client_redirecting_url);

    // The referrer is |client_redirecting_url| since the final navigation is a
    // client-side redirect.
    ExpectReferrerWithDefaultPolicy(entry, client_redirecting_url);
  }
}

// Checks the contents of the redirect chain after a server-side redirect that
// happens after a client-side redirect.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    FrameNavigationEntry_MainFrameRedirectChain_ClientThenServerRedirect) {
  NavigationControllerImpl& controller = contents()->GetController();

  // Navigate the main frame to a redirecting URL (client-side then server-side)
  GURL client_redirecting_url(embedded_test_server()->GetURL(
      "/navigation_controller/client_redirect_server.html"));
  GURL server_redirecting_url(embedded_test_server()->GetURL(
      "/server-redirect?/navigation_controller/simple_page_1.html"));
  GURL final_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  {
    TestNavigationManager navigation_manager_1(contents(),
                                               client_redirecting_url);
    TestNavigationManager navigation_manager_2(contents(),
                                               server_redirecting_url);

    shell()->LoadURL(client_redirecting_url);

    ASSERT_TRUE(navigation_manager_1
                    .WaitForNavigationFinished());  // Initial navigation +
                                                    // client-side redirect.
    ASSERT_TRUE(navigation_manager_2
                    .WaitForNavigationFinished());  // Server-side redirect.

    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntry* entry = controller.GetEntryAtIndex(0);
    ASSERT_EQ(final_url, entry->GetURL());

    // The redirect chain will contain the URL that triggers the client
    // redirect, then the server-redirecting URL (the target of the
    // client-redirector URL) and finally the final URL (the target of the
    // server-redirector URL).
    EXPECT_EQ(entry->GetRedirectChain().size(), 3u);
    EXPECT_EQ(entry->GetRedirectChain()[0], client_redirecting_url);
    EXPECT_EQ(entry->GetRedirectChain()[1], server_redirecting_url);
    EXPECT_EQ(entry->GetRedirectChain()[2], final_url);

    // The client-redirecting URL is saved as the "replaced entry".
    ASSERT_TRUE(entry->GetReplacedEntryData().has_value());
    EXPECT_EQ(client_redirecting_url,
              entry->GetReplacedEntryData()->first_committed_url);

    // The original request URL will be the first entry of redirect chain,
    // which is the URL that initiated the client redirect.
    EXPECT_EQ(entry->GetOriginalRequestURL(), client_redirecting_url);

    // The referrer is |client_redirecting_url| since the navigation started
    // with a client-side redirect, and a same-origin server redirect does not
    // affect a request's referrer.
    ExpectReferrerWithDefaultPolicy(entry, client_redirecting_url);
  }
}

// Verify that restoring a NavigationEntry with cross-site subframes does not
// create out-of-process iframes unless the current SiteIsolationPolicy says to.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       RestoreWithoutExtraOopifs) {
  // 1. Start on a page with a data URL iframe.
  GURL main_url_a(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/page_with_data_iframe.html"));
  GURL data_url("data:text/html,Subframe");
  EXPECT_TRUE(NavigateToURL(shell(), main_url_a));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  // 2. Navigate the iframe cross-site.
  GURL frame_url_b(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url_b));
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // 3. Create a NavigationEntry with the same PageState as |entry2|.
  std::unique_ptr<NavigationEntryImpl> restored_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              main_url_a, Referrer(), absl::nullopt /* initiator_origin= */,
              /* initiator_base_url= */ absl::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  EXPECT_EQ(0U, restored_entry->root_node()->children.size());
  std::unique_ptr<NavigationEntryRestoreContextImpl> context =
      std::make_unique<NavigationEntryRestoreContextImpl>();
  restored_entry->SetPageState(entry2->GetPageState(), context.get());

  // The entry should have no SiteInstance in the FrameNavigationEntry for the
  // b.com subframe.
  EXPECT_FALSE(
      restored_entry->root_node()->children[0]->frame_entry->site_instance());

  // 4. Restore the new entry in a new tab and verify the correct URLs load.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  Shell* new_shell = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(
          new_shell->web_contents()->GetController());
  new_controller.Restore(entries.size() - 1, RestoreType::kRestored, &entries);
  ASSERT_EQ(0u, entries.size());
  {
    TestNavigationObserver restore_observer(new_shell->web_contents());
    new_controller.LoadIfNecessary();
    restore_observer.Wait();
  }
  ASSERT_EQ(1U, new_root->child_count());
  EXPECT_EQ(main_url_a, new_root->current_url());
  EXPECT_EQ(frame_url_b, new_root->child_at(0)->current_url());

  if (AreStrictSiteInstancesEnabled()) {
    EXPECT_NE(new_root->current_frame_host()->GetSiteInstance(),
              new_root->child_at(0)->current_frame_host()->GetSiteInstance());
  } else {
    // When strict SiteInstances are not enabled, the subframe should be in the
    // same SiteInstance as the parent because both sites get mapped to the
    // default SiteInstance.
    EXPECT_TRUE(new_root->current_frame_host()
                    ->GetSiteInstance()
                    ->IsDefaultSiteInstance());
    EXPECT_EQ(new_root->current_frame_host()->GetSiteInstance(),
              new_root->child_at(0)->current_frame_host()->GetSiteInstance());
  }
}

namespace {

// Loads |start_url|, then loads |stalled_url| which stalls. While the page is
// stalled, a same document navigation happens. Make sure that all the
// navigations are properly classified.
void DoReplaceStateWhilePending(Shell* shell,
                                const GURL& start_url,
                                const GURL& stalled_url,
                                const std::string& replace_state_filename) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell->web_contents()->GetController());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Start with one page.
  EXPECT_TRUE(NavigateToURL(shell, start_url));

  // Have the user decide to go to a different page which is very slow.
  TestNavigationManager stalled_navigation(shell->web_contents(), stalled_url);
  controller.LoadURL(stalled_url, Referrer(), ui::PAGE_TRANSITION_LINK,
                     std::string());
  EXPECT_TRUE(stalled_navigation.WaitForRequestStart());

  // That should be the pending entry.
  NavigationEntryImpl* entry = controller.GetPendingEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(stalled_url, entry->GetURL());

  {
    // Now the existing page uses history.replaceState().
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    std::string script =
        "history.replaceState({}, '', '" + replace_state_filename + "')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();

    // The fact that there was a pending entry shouldn't interfere with the
    // classification.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
  }
}

}  // namespace

IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    NavigationTypeClassification_On1SameDocumentToXWhile2Pending) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  DoReplaceStateWhilePending(shell(), url1, url2, "x");
}

IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    NavigationTypeClassification_On1SameDocumentTo2While2Pending) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  DoReplaceStateWhilePending(shell(), url1, url2, "simple_page_2.html");
}

IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    NavigationTypeClassification_On1SameDocumentToXWhile1Pending) {
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  DoReplaceStateWhilePending(shell(), url, url, "x");
}

IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    NavigationTypeClassification_On1SameDocumentTo1While1Pending) {
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  DoReplaceStateWhilePending(shell(), url, url, "simple_page_1.html");
}

// Ensure that a pending NavigationEntry for a different navigation doesn't
// cause a commit to be incorrectly treated as a replacement.
// See https://crbug.com/593153.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       OtherCommitDuringPendingEntryWithReplacement) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Load an initial page.
  GURL start_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  int entry_count = controller.GetEntryCount();
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(start_url, controller.GetLastCommittedEntry()->GetURL());

  // Start a cross-process navigation with replacement, which never completes.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/page_with_links.html"));
  TestNavigationManager stalled_navigation(shell()->web_contents(), foo_url);
  NavigationController::LoadURLParams params(foo_url);
  params.should_replace_current_entry = true;
  controller.LoadURLWithParams(params);
  EXPECT_TRUE(stalled_navigation.WaitForRequestStart());

  // That should be the pending entry.
  NavigationEntryImpl* entry = controller.GetPendingEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(foo_url, entry->GetURL());
  EXPECT_EQ(entry_count, controller.GetEntryCount());

  {
    // Now the existing page uses history.pushState() while the pending entry
    // for the other navigation still exists.
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    std::string script = "history.pushState({}, '', 'pushed')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());
  }

  // The same document navigation should not have replaced the previous entry.
  GURL push_state_url(
      embedded_test_server()->GetURL("/navigation_controller/pushed"));
  EXPECT_EQ(entry_count + 1, controller.GetEntryCount());
  EXPECT_EQ(push_state_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(start_url, controller.GetEntryAtIndex(0)->GetURL());
}

// This test ensures that if we go back from a page that has a replaceState()
// call in the window.beforeunload function, we commit to the proper navigation
// entry. https://crbug.com/597239
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       BackFromPageWithReplaceStateInBeforeUnload) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  // With BackForwardCache, old frame doesn't fire beforeunload handlers as the
  // page is stored in BackForwardCache on navigation.
  controller.GetBackForwardCache().DisableForTesting(
      content::BackForwardCache::TEST_USES_UNLOAD_EVENT);

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Load an initial page.
  GURL start_url(embedded_test_server()->GetURL(
      "/navigation_controller/beforeunload_replacestate_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(start_url, controller.GetLastCommittedEntry()->GetURL());

  // Go to the second page.
  std::string script = "document.getElementById('thelink').click()";
  EXPECT_TRUE(ExecJs(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Go back to the first page, which never completes. The attempt to unload the
  // second page, though, causes it to do a replaceState().
  TestNavigationManager manager(shell()->web_contents(), start_url);
  controller.GoBack();
  EXPECT_TRUE(manager.WaitForRequestStart());

  // The navigation that just happened was the replaceState(), which should not
  // have changed the position into the navigation entry list. Make sure that
  // the pending navigation didn't confuse anything.
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
}

// Ensure the renderer process does not get confused about the current entry
// due to subframes and replaced entries.  See https://crbug.com/480201.
// TODO(creis): Re-enable for Site Isolation FYI bots: https://crbug.com/502317.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       PreventSpoofFromSubframeAndReplace) {
  // Start at an initial URL.
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Now go to a page with a real iframe.
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  {
    // Navigate in the iframe.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
  }

  {
    // Go back in the iframe.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }

  {
    // Go forward in the iframe.
    TestNavigationObserver forward_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    forward_load_observer.Wait();
  }

  GURL url3(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  {
    // location.replace() to cause an inert commit.
    TestNavigationObserver replace_load_observer(shell()->web_contents());
    std::string script = "location.replace('" + url3.spec() + "')";
    EXPECT_TRUE(ExecJs(root, script));
    replace_load_observer.Wait();
  }

  {
    // Go back to url2.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();

    // Make sure the URL is correct for both the entry and the main frame, and
    // that the process hasn't been killed for showing a spoof.
    EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
    EXPECT_EQ(url2, shell()->web_contents()->GetLastCommittedURL());
    EXPECT_EQ(url2, root->current_url());
  }

  {
    // Go back to reset main frame entirely.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
    EXPECT_EQ(url1, shell()->web_contents()->GetLastCommittedURL());
    EXPECT_EQ(url1, root->current_url());
  }

  {
    // Go forward.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    back_load_observer.Wait();
    EXPECT_EQ(url2, shell()->web_contents()->GetLastCommittedURL());
    EXPECT_EQ(url2, root->current_url());
  }

  {
    // Go forward to the replaced URL.
    TestNavigationObserver forward_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    forward_load_observer.Wait();

    // Make sure the URL is correct for both the entry and the main frame, and
    // that the process hasn't been killed for showing a spoof.
    EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
    EXPECT_EQ(url3, shell()->web_contents()->GetLastCommittedURL());
    EXPECT_EQ(url3, root->current_url());
  }
}

// Ensure the renderer process does not get killed if the main frame URL's path
// changes when going back in a subframe, since this was possible after
// a replaceState in the main frame (thanks to https://crbug.com/373041).
// See https:///crbug.com/486916.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       SubframeBackFromReplaceState) {
  // Start at a page with a real iframe.
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  {
    // Navigate in the iframe.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
  }

  {
    // history.replaceState().
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "history.replaceState({}, 'replaced', 'replaced')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
  }

  GURL replaced_url = shell()->web_contents()->GetLastCommittedURL();

  {
    // Go back in the iframe.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }

  // The replaceState() should not be reverted by the iframe back.
  EXPECT_EQ(replaced_url, shell()->web_contents()->GetLastCommittedURL());

  // Make sure the renderer process has not been killed.
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
}

// Ensure that a main frame replaceState remains in effect after a subframe back
// navigation, even after cloning the tab. Also ensure that
// FrameNavigationEntries are not shared across cloned tabs.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       SubframeBackFromReplaceStateInClonedTab) {
  // Start at a page with a real iframe.
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  NavigationControllerImpl& original_controller =
      static_cast<NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  {
    // Navigate in the iframe.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
  }

  // Clone the tab without navigating it.
  std::unique_ptr<WebContents> new_tab = shell()->web_contents()->Clone();
  WebContentsImpl* cloned_tab_impl =
      static_cast<WebContentsImpl*>(new_tab.get());
  NavigationControllerImpl& cloned_controller =
      static_cast<NavigationControllerImpl&>(cloned_tab_impl->GetController());
  EXPECT_TRUE(cloned_controller.IsInitialNavigation());
  EXPECT_TRUE(cloned_controller.NeedsReload());

  // The FrameNavigationEntries should not be shared across tabs, but they
  // should be shared among entries in each tab.
  ASSERT_EQ(2, original_controller.GetEntryCount());
  ASSERT_EQ(2, cloned_controller.GetEntryCount());
  NavigationEntryImpl* original_previous_entry =
      original_controller.GetEntryAtIndex(0);
  NavigationEntryImpl* original_current_entry =
      original_controller.GetEntryAtIndex(1);
  NavigationEntryImpl* cloned_previous_entry =
      cloned_controller.GetEntryAtIndex(0);
  NavigationEntryImpl* cloned_current_entry =
      cloned_controller.GetEntryAtIndex(1);
  EXPECT_NE(original_current_entry->root_node()->frame_entry.get(),
            cloned_current_entry->root_node()->frame_entry.get());
  EXPECT_NE(original_current_entry->root_node()->children[0]->frame_entry.get(),
            cloned_current_entry->root_node()->children[0]->frame_entry.get());
  EXPECT_EQ(original_previous_entry->root_node()->frame_entry.get(),
            original_current_entry->root_node()->frame_entry.get());
  EXPECT_EQ(cloned_previous_entry->root_node()->frame_entry.get(),
            cloned_current_entry->root_node()->frame_entry.get());

  {
    // history.replaceState() in the original tab.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "history.replaceState({}, 'replaced', 'replaced2')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
  }

  {
    // Go back in the iframe in the cloned tab.
    TestNavigationObserver back_load_observer(new_tab.get());
    new_tab->GetController().GoBack();
    back_load_observer.Wait();
  }

  // Make sure the renderer process has not been killed.
  FrameTreeNode* cloned_root = cloned_tab_impl->GetPrimaryFrameTree().root();
  EXPECT_TRUE(cloned_root->current_frame_host()->IsRenderFrameLive());

  // Only the cloned tab's NavigationEntry should have changed.
  EXPECT_EQ(original_current_entry,
            original_controller.GetLastCommittedEntry());
  EXPECT_EQ(cloned_previous_entry, cloned_controller.GetLastCommittedEntry());

  // The url should have changed in the original tab but not in the clone.
  EXPECT_EQ(cloned_previous_entry, cloned_controller.GetLastCommittedEntry());
  EXPECT_NE(original_previous_entry->root_node()->frame_entry->url(),
            cloned_previous_entry->root_node()->frame_entry->url());
}

// Test that shared FrameNavigationEntries with opaque initiator origins can be
// restored in future sessions, even if the internal nonce is not preserved.
// See https://crbug.com/1354634.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       RestoreSharedFrameEntryWithOpaqueInitiator) {
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // 1. Start at a data: URL with an iframe, giving the iframe an opaque
  // initiator origin.
  GURL data_url("data:text/html,<iframe></iframe>");
  EXPECT_TRUE(NavigateToURL(shell(), data_url));
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();
  FrameTreeNode* child = root->child_at(0);
  ASSERT_TRUE(entry1->GetFrameEntry(child)->initiator_origin()->opaque());

  // 2. Navigate to a fragment, staying in the same document. This shares the
  // iframe's FrameNavigationEntry from entry1.
  GURL data_url_fragment("data:text/html,<iframe></iframe>#foo");
  EXPECT_TRUE(NavigateToURL(shell(), data_url_fragment));
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();
  ASSERT_EQ(entry1->GetFrameEntry(child), entry2->GetFrameEntry(child));

  // 3. Simulate session restore by cloning the NavigationEntries but calling
  // SetPageState with saved PageStates from the entries, using a shared
  // RestoreContext. The second SetPageState call used to fail a DCHECK that the
  // initiator origin of the de-duplicated FrameNavigationEntries should match,
  // because the internal nonce of the opaque origin isn't preserved. See
  // https://crbug.com/1354634.
  std::unique_ptr<NavigationEntryRestoreContextImpl> context =
      std::make_unique<NavigationEntryRestoreContextImpl>();
  std::unique_ptr<NavigationEntryImpl> restored_entry1 = entry1->Clone();
  restored_entry1->SetPageState(entry1->GetPageState(), context.get());
  std::unique_ptr<NavigationEntryImpl> restored_entry2 = entry2->Clone();
  restored_entry2->SetPageState(entry2->GetPageState(), context.get());
  std::vector<std::unique_ptr<NavigationEntry>> restored_entries;
  restored_entries.push_back(std::move(restored_entry1));
  restored_entries.push_back(std::move(restored_entry2));

  // 4. Create a new tab and restore the entries, confirming that it works.
  Shell* new_shell = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  WebContentsImpl* new_web_contents =
      static_cast<WebContentsImpl*>(new_shell->web_contents());
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(new_web_contents->GetController());
  new_controller.Restore(restored_entries.size() - 1, RestoreType::kRestored,
                         &restored_entries);
  {
    TestNavigationObserver restore_observer(new_shell->web_contents());
    new_controller.LoadIfNecessary();
    restore_observer.Wait();
  }
  NavigationEntryImpl* new_entry2 = new_controller.GetEntryAtIndex(1);
  EXPECT_EQ(new_entry2, new_controller.GetLastCommittedEntry());
}

// Test that duplicate FrameNavigationEntries from pre-M93 can diverge in URL
// and other values (e.g., initiator origin), and will only be de-duplicated if
// their URLs, item sequence numbers, and targets match. See
// https://crbug.com/1275257. Also tests that we do not fail a DCHECK if
// remaining values like initiator origin still differ when de-duplicating them,
// per https://crbug.com/1354634.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       RestoreNonSharedFrameEntryWithCrossOriginInitiator) {
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // 1. Visit an a.com URL.
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();

  // 2. Do a renderer-initiated navigation to a cross-origin URL, so that the
  // initiator origin is cross-origin.
  GURL url2(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/page_with_iframe_simple.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url2));
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();
  url::Origin initiator_origin2 =
      entry2->GetFrameEntry(root)->initiator_origin().value();
  ASSERT_EQ(initiator_origin2, url::Origin::Create(url1));

  // Serialize the state of `entry1` and `entry2` now, before `entry2` starts
  // sharing its main frame FNE in the next step. This simulates a pre-M93 saved
  // session where FNEs were not shared.  (See https://crbug.com/1211683).
  blink::PageState entry1_state = entry1->GetPageState();
  blink::PageState entry2_state = entry2->GetPageState();

  // 3. Navigate the subframe to create a shared FrameNavigationEntry in the
  // main frame.
  GURL frame_url(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/simple_page_2.html"));
  FrameTreeNode* child = root->child_at(0);
  {
    FrameNavigateParamsCapturer capturer(child);
    ASSERT_TRUE(NavigateFrameToURL(child, frame_url));
    capturer.Wait();
  }
  NavigationEntryImpl* entry3 = controller.GetLastCommittedEntry();

  // 4. Do a replaceState in the main frame, changing the initiator origin to
  // the current origin.
  GURL url2_fragment(url2.spec() + "#foo");
  {
    FrameNavigateParamsCapturer capturer(root);
    std::string script =
        "history.replaceState({}, 'replaced', '" + url2_fragment.spec() + "')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
  }
  url::Origin initiator_origin3 =
      entry3->GetFrameEntry(root)->initiator_origin().value();
  ASSERT_EQ(initiator_origin3, url::Origin::Create(url2));
  EXPECT_NE(initiator_origin3, initiator_origin2);

  // Serialize the state of `entry3` now, to simulate a diverging main-frame FNE
  // from pre-M93.
  blink::PageState entry3_state = entry3->GetPageState();

  // Simulate session restore in a post-M93 version by cloning the
  // NavigationEntries but calling SetPageState with the previously saved
  // PageStates, using a shared RestoreContext. This attempts to de-duplicate
  // FNEs if they match by item sequence number, URL, and target.
  std::unique_ptr<NavigationEntryRestoreContextImpl> context1 =
      std::make_unique<NavigationEntryRestoreContextImpl>();
  std::unique_ptr<NavigationEntryImpl> restored_entry1 = entry1->Clone();
  restored_entry1->SetPageState(entry1_state, context1.get());
  std::unique_ptr<NavigationEntryImpl> restored_entry2 = entry2->Clone();
  restored_entry2->SetPageState(entry2_state, context1.get());
  std::unique_ptr<NavigationEntryImpl> restored_entry3 = entry3->Clone();
  restored_entry3->SetPageState(entry3_state, context1.get());

  // The main frame FNE will not be de-duplicated, because the URL of the two
  // PageStates differs.  See https://crbug.com/1275257.
  EXPECT_NE(restored_entry2->GetFrameEntry(root),
            restored_entry3->GetFrameEntry(root));
  EXPECT_NE(restored_entry2->GetFrameEntry(root)->initiator_origin().value(),
            restored_entry3->GetFrameEntry(root)->initiator_origin().value());

  // 5. Actually restore the entries in a new tab.
  std::vector<std::unique_ptr<NavigationEntry>> restored_entries;
  restored_entries.push_back(std::move(restored_entry1));
  restored_entries.push_back(std::move(restored_entry2));
  restored_entries.push_back(std::move(restored_entry3));
  Shell* shell2 = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  WebContentsImpl* web_contents2 =
      static_cast<WebContentsImpl*>(shell2->web_contents());
  FrameTreeNode* root2 = static_cast<WebContentsImpl*>(shell2->web_contents())
                             ->GetPrimaryFrameTree()
                             .root();
  NavigationControllerImpl& controller2 =
      static_cast<NavigationControllerImpl&>(web_contents2->GetController());
  controller2.Restore(restored_entries.size() - 1, RestoreType::kRestored,
                      &restored_entries);
  {
    TestNavigationObserver restore_observer(shell2->web_contents());
    controller2.LoadIfNecessary();
    restore_observer.Wait();
  }
  NavigationEntryImpl* new_entry1 = controller2.GetEntryAtIndex(0);
  NavigationEntryImpl* new_entry2 = controller2.GetEntryAtIndex(1);
  NavigationEntryImpl* new_entry3 = controller2.GetEntryAtIndex(2);
  EXPECT_EQ(new_entry3, controller2.GetLastCommittedEntry());

  // 6. Now do a replaceState in the main frame back to the original URL, such
  // that the FNEs can be de-duplicated on a future restore.
  {
    FrameNavigateParamsCapturer capturer(root2);
    std::string script = "history.replaceState({}, '', '" + url2.spec() + "')";
    EXPECT_TRUE(ExecJs(root2, script));
    capturer.Wait();
  }

  // The duplicate main frame FNEs should now have matching URLs and ISNs but
  // mismatched initiator origins.
  EXPECT_NE(new_entry2->GetFrameEntry(root2), new_entry3->GetFrameEntry(root2));
  EXPECT_EQ(new_entry2->GetFrameEntry(root2)->url(),
            new_entry3->GetFrameEntry(root2)->url());
  EXPECT_EQ(new_entry2->GetFrameEntry(root2)->item_sequence_number(),
            new_entry3->GetFrameEntry(root2)->item_sequence_number());
  EXPECT_NE(new_entry2->GetFrameEntry(root2)->initiator_origin().value(),
            new_entry3->GetFrameEntry(root2)->initiator_origin().value());

  // Simulate another session restore across GetPageState and SetPageState.
  // This time the main frame FNEs will be de-duplicated, despite not being
  // fully identical. This used to fail a DCHECK that the initiator origin of
  // de-duplicated FrameNavigationEntries should match. See
  // https://crbug.com/1354634.
  std::unique_ptr<NavigationEntryRestoreContextImpl> context2 =
      std::make_unique<NavigationEntryRestoreContextImpl>();
  std::unique_ptr<NavigationEntryImpl> restored2_entry1 = new_entry1->Clone();
  restored2_entry1->SetPageState(new_entry1->GetPageState(), context2.get());
  std::unique_ptr<NavigationEntryImpl> restored2_entry2 = new_entry2->Clone();
  restored2_entry2->SetPageState(new_entry2->GetPageState(), context2.get());
  std::unique_ptr<NavigationEntryImpl> restored2_entry3 = new_entry3->Clone();
  restored2_entry3->SetPageState(new_entry3->GetPageState(), context2.get());

  // The main frame FrameNavigationEntries should be shared in the new session.
  EXPECT_EQ(restored2_entry2->GetFrameEntry(root2),
            restored2_entry3->GetFrameEntry(root2));

  // 7. Actually restore the entries in a new tab.
  Shell* shell3 = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  WebContentsImpl* web_contents3 =
      static_cast<WebContentsImpl*>(shell3->web_contents());
  FrameTreeNode* root3 = static_cast<WebContentsImpl*>(shell3->web_contents())
                             ->GetPrimaryFrameTree()
                             .root();
  NavigationControllerImpl& controller3 =
      static_cast<NavigationControllerImpl&>(web_contents3->GetController());
  std::vector<std::unique_ptr<NavigationEntry>> restored_entries2;
  restored_entries2.push_back(std::move(restored2_entry1));
  restored_entries2.push_back(std::move(restored2_entry2));
  restored_entries2.push_back(std::move(restored2_entry3));
  controller3.Restore(restored_entries2.size() - 1, RestoreType::kRestored,
                      &restored_entries2);
  {
    TestNavigationObserver restore_observer(shell3->web_contents());
    controller3.LoadIfNecessary();
    restore_observer.Wait();
  }

  // Verify that the main frame FNE is now shared.
  FrameNavigationEntry* root3_fne =
      controller3.GetEntryAtIndex(2)->GetFrameEntry(root3);
  EXPECT_EQ(root3_fne, controller3.GetEntryAtIndex(1)->GetFrameEntry(root3));
}

// Test that restoring a corrupt PageState can still successfully load the
// original URL without crashing, even if the rest of the PageState is lost.
// See https://crbug.com/1412401 and https://crbug.com/1196330, as well as
// SessionRestoreTest.RestoreInvalidPageState.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       RestoreSessionWithInvalidPageState) {
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // 1. Navigate to a page with an iframe.
  GURL url1(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/page_with_iframe_simple.html"));
  GURL original_frame_url(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();
  FrameTreeNode* child = root->child_at(0);
  ASSERT_EQ(original_frame_url, entry1->GetFrameEntry(child)->url());

  // 2. Navigate the subframe.
  GURL frame_url(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/simple_page_2.html"));
  {
    FrameNavigateParamsCapturer capturer(child);
    ASSERT_TRUE(NavigateFrameToURL(child, frame_url));
    capturer.Wait();
  }
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // Simulate a session restore using a corrupted PageState.  The other values
  // from the entry are preserved (simulated by cloning the entry).
  blink::PageState garbage = blink::PageState::CreateFromEncodedData("garbage");
  blink::ExplodedPageState exploded_state;
  ASSERT_FALSE(
      blink::DecodePageState(garbage.ToEncodedData(), &exploded_state));
  std::unique_ptr<NavigationEntryRestoreContextImpl> context1 =
      std::make_unique<NavigationEntryRestoreContextImpl>();
  std::unique_ptr<NavigationEntryImpl> restored_entry1 = entry1->Clone();
  restored_entry1->SetPageState(garbage, context1.get());
  std::unique_ptr<NavigationEntryImpl> restored_entry2 = entry2->Clone();
  restored_entry2->SetPageState(garbage, context1.get());

  // 3. Actually restore the entries in a new tab.
  std::vector<std::unique_ptr<NavigationEntry>> restored_entries;
  restored_entries.push_back(std::move(restored_entry1));
  restored_entries.push_back(std::move(restored_entry2));
  Shell* shell2 = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  WebContentsImpl* web_contents2 =
      static_cast<WebContentsImpl*>(shell2->web_contents());
  FrameTreeNode* root2 = static_cast<WebContentsImpl*>(shell2->web_contents())
                             ->GetPrimaryFrameTree()
                             .root();
  NavigationControllerImpl& controller2 =
      static_cast<NavigationControllerImpl&>(web_contents2->GetController());
  controller2.Restore(restored_entries.size() - 1, RestoreType::kRestored,
                      &restored_entries);
  {
    TestNavigationObserver restore_observer(shell2->web_contents());
    controller2.LoadIfNecessary();
    restore_observer.Wait();
  }
  NavigationEntryImpl* new_entry1 = controller2.GetEntryAtIndex(0);
  NavigationEntryImpl* new_entry2 = controller2.GetEntryAtIndex(1);
  EXPECT_EQ(new_entry2, controller2.GetLastCommittedEntry());

  // Verify that the subframe loads its original URL, because frame_url was
  // lost in the corrupted PageState.
  FrameTreeNode* child2 = root2->child_at(0);
  FrameNavigationEntry* child2_fne = new_entry2->GetFrameEntry(child2);
  EXPECT_EQ(original_frame_url, child2_fne->url());

  // Ensure that the main frame entries do not share a FrameNavigationEntry,
  // despite ending up with identical item sequence numbers (i.e., 0) in the
  // corrupted PageState. This works because ISNs of 0 are exempt from the
  // de-deduplication logic.
  EXPECT_NE(new_entry1->GetFrameEntry(root2), new_entry2->GetFrameEntry(root2));
}

// Tests the value of history.state after same-document replacement, in all
// affected entries.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       StateAfterSameDocumentReplacement) {
  // Start at a page with an iframe.
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe_simple.html"));
  GURL url1_foo(url1.spec() + "#foo");
  GURL url1_bar(url1.spec() + "#bar");
  GURL frame_url2(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  EXPECT_EQ(1, controller.GetEntryCount());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  NavigationEntryImpl* previous_entry = controller.GetLastCommittedEntry();
  scoped_refptr<FrameNavigationEntry> previous_root_entry =
      previous_entry->root_node()->frame_entry.get();

  {
    // pushState "foo" in the main frame.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.pushState('foo', '', '#foo')"));
    capturer.Wait();
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_TRUE(capturer.is_same_document());
    // Current history state:
    //  [url1(frame_url1), *url1_foo(frame_url1)]
    EXPECT_EQ(url1_foo, contents()->GetLastCommittedURL());
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ(1, controller.GetCurrentEntryIndex());

    // The main frame's history.state is now "foo".
    EXPECT_EQ("foo", EvalJs(root, "history.state"));

    // The root FrameNavigationEntry and NavigationEntry are not reused.
    EXPECT_NE(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());

    previous_entry = controller.GetLastCommittedEntry();
    previous_root_entry = previous_entry->root_node()->frame_entry.get();
  }

  {
    // Navigate in the iframe. The main frame should stay the same.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url2));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());

    // Current history state:
    //  [url1(frame_url1), url1_foo(frame_url1), *url1_foo(frame_url2)]
    EXPECT_EQ(url1_foo, contents()->GetLastCommittedURL());
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_EQ(2, controller.GetCurrentEntryIndex());

    // The main frame's history.state stays the same.
    EXPECT_EQ("foo", EvalJs(root, "history.state"));

    // The root FrameNavigationEntry is reused in the new NavigationEntry.
    EXPECT_EQ(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());

    previous_entry = controller.GetLastCommittedEntry();
    previous_root_entry = previous_entry->root_node()->frame_entry.get();
  }

  {
    // Do a location.replace() to a same-document URL in the main frame.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "location.replace('#bar');"));
    capturer.Wait();
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_TRUE(capturer.is_same_document());
    // This gets classified as EXISTING_ENTRY since it's a same-document
    // replacement. Note that a cross-document location.replace() would've been
    // classified as NEW_ENTRY instead.
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());

    EXPECT_EQ(url1_bar, contents()->GetLastCommittedURL());
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_EQ(2, controller.GetCurrentEntryIndex());
    // Current history state:
    //  [url1(frame_url1), url1_bar(frame_url1), *url1_bar(frame_url2)]
    // Notice the middle entry is updated to url1_bar too because the FNE is
    // shared with the replaced/updated entry.
    // TODO(https://crbug.com/1226489): Don't share the FNE so that we won't
    // update the middle entry.
    NavigationEntryImpl* current_entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(url1_bar, current_entry->GetURL());
    EXPECT_EQ(url1_bar, controller.GetEntryAtIndex(1)->GetURL());
    EXPECT_NE(current_entry, controller.GetEntryAtIndex(1));
    EXPECT_EQ(current_entry->GetFrameEntry(root),
              controller.GetEntryAtIndex(1)->GetFrameEntry(root));

    // The main frame's history.state is reset
    EXPECT_EQ(nullptr, EvalJs(root, "history.state"));

    // The root FrameNavigationEntry and NavigationEntry are both reused. This
    // means the previous pushState FrameNavigationEntry is shared with the
    // current FrameNavigationEntry.
    EXPECT_EQ(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());

    previous_entry = controller.GetLastCommittedEntry();
    previous_root_entry = previous_entry->root_node()->frame_entry.get();
  }

  {
    // Go back. This will navigate the subframe.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    controller.GoBack();
    capturer.Wait();
    // Current history state:
    //  [url1(frame_url1), *url1_bar(frame_url1), url1_bar(frame_url2)]
    // The main frame stays at the same URL, url1_bar (?!)
    EXPECT_EQ(url1_bar, contents()->GetLastCommittedURL());
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_EQ(1, controller.GetCurrentEntryIndex());

    // The main frame's history.state stays as null instead of being restored
    // to "foo", since the location.replace() call also affected other entries
    // that share the FrameNavigatonEntry.
    // TODO(https://crbug.com/1226489): We should probably restore "foo" here,
    // as location.replace() shouldn't affect entries other than the one it
    // replaced.
    EXPECT_EQ(nullptr, EvalJs(root, "history.state"));

    // The root FrameNavigationEntry is reused since it is shared with the
    // traversed NavigationEntry.
    EXPECT_EQ(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());

    previous_entry = controller.GetLastCommittedEntry();
    previous_root_entry = previous_entry->root_node()->frame_entry.get();
  }

  {
    // Go back. This will navigate the main frame.
    FrameNavigateParamsCapturer capturer(root);
    controller.GoBack();
    capturer.Wait();
    EXPECT_TRUE(capturer.is_same_document());

    // Current history state:
    //  [*url1(frame_url1), url1_bar(frame_url1), url1_bar(frame_url2)]
    // The main frame is back to url1.
    EXPECT_EQ(url1, contents()->GetLastCommittedURL());
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_TRUE(capturer.is_same_document());

    // The main frame's history.state stays as null.
    EXPECT_EQ(nullptr, EvalJs(root, "history.state"));

    // The root FrameNavigationEntry and NavigationEntry are not reused.
    EXPECT_NE(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
  }
}

// Tests the value of history.state after cross-document replacement, in all
// affected entries.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       StateAfterCrossDocumentReplacement) {
  // Start at a page with an iframe.
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  GURL url1_foo(url1.spec() + "#foo");
  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  EXPECT_EQ(1, controller.GetEntryCount());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  NavigationEntryImpl* previous_entry = controller.GetLastCommittedEntry();
  scoped_refptr<FrameNavigationEntry> previous_root_entry =
      previous_entry->root_node()->frame_entry.get();

  {
    // pushState "foo" in the main frame.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.pushState('foo', '', '#foo')"));
    capturer.Wait();
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_TRUE(capturer.is_same_document());

    // Current history state:
    //  [url1(frame_url1), *url1_foo(frame_url1)]
    EXPECT_EQ(url1_foo, contents()->GetLastCommittedURL());
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ(1, controller.GetCurrentEntryIndex());

    // The main frame's history.state is now "foo".
    EXPECT_EQ("foo", EvalJs(root, "history.state"));

    // The root FrameNavigationEntry and NavigationEntry are not reused.
    EXPECT_NE(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());

    previous_entry = controller.GetLastCommittedEntry();
    previous_root_entry = previous_entry->root_node()->frame_entry.get();
  }

  {
    // Navigate in the iframe.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url2(embedded_test_server()->GetURL("/title1.html"));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url2));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());

    // Current history state:
    //  [url1(frame_url1), url1_foo(frame_url1), *url1_foo(frame_url2)]
    EXPECT_EQ(url1_foo, contents()->GetLastCommittedURL());
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_EQ(2, controller.GetCurrentEntryIndex());

    // The main frame's history.state stays the same.
    EXPECT_EQ("foo", EvalJs(root, "history.state"));

    // The root FrameNavigationEntry is reused in the new NavigationEntry.
    EXPECT_EQ(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());

    previous_entry = controller.GetLastCommittedEntry();
    previous_root_entry = previous_entry->root_node()->frame_entry.get();
  }

  {
    // Do a location.replace() to a cross-document URL in the main frame.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, JsReplace("location.replace($1);", url2)));
    capturer.Wait();
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_FALSE(capturer.is_same_document());

    // Current history state:
    //  [url1(frame_url1), url1_foo(frame_url1), *url2]
    EXPECT_EQ(url2, contents()->GetLastCommittedURL());
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_EQ(2, controller.GetCurrentEntryIndex());

    // The main frame's history.state is reset
    EXPECT_EQ(nullptr, EvalJs(root, "history.state"));

    // The root FrameNavigationEntry and NavigationEntry are not reused.
    EXPECT_NE(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());

    previous_entry = controller.GetLastCommittedEntry();
    previous_root_entry = previous_entry->root_node()->frame_entry.get();
  }

  {
    // Go back. This will navigate the main frame and recreate the subframe.
    FrameNavigateParamsCapturer capturer(root);
    controller.GoBack();
    capturer.Wait();
    EXPECT_FALSE(capturer.is_same_document());

    // Current history state:
    //  [url1(frame_url1), *url1_foo(frame_url1), url2]
    EXPECT_EQ(url1_foo, contents()->GetLastCommittedURL());
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_EQ(1, controller.GetCurrentEntryIndex());

    // The main frame's history.state is restored to "foo".
    EXPECT_EQ("foo", EvalJs(root, "history.state"));

    // The root FrameNavigationEntry and NavigationEntry are not reused.
    EXPECT_NE(
        previous_root_entry,
        controller.GetLastCommittedEntry()->root_node()->frame_entry.get());
    EXPECT_NE(previous_entry, controller.GetLastCommittedEntry());
  }
}

namespace {

class FailureWatcher : public WebContentsObserver {
 public:
  // Observes failure for the specified |node|.
  explicit FailureWatcher(FrameTreeNode* node)
      : WebContentsObserver(
            WebContents::FromRenderFrameHost(node->current_frame_host())),
        frame_tree_node_id_(node->frame_tree_node_id()) {}

  void Wait() { loop_.Run(); }

 private:
  void DidFailLoad(RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override {
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(render_frame_host);
    if (rfh->frame_tree_node()->frame_tree_node_id() != frame_tree_node_id_)
      return;

    loop_.Quit();
  }

  void DidFinishNavigation(NavigationHandle* handle) override {
    if (handle->HasCommitted() ||
        handle->GetFrameTreeNodeId() != frame_tree_node_id_) {
      return;
    }

    loop_.Quit();
  }

  // The id of the FrameTreeNode whose navigations to observe.
  int frame_tree_node_id_;

  base::RunLoop loop_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       StopCausesFailureDespiteJavaScriptURL) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Start with a normal page.
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Have the user decide to go to a different page which will not commit.
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  TestNavigationManager stalled_navigation(shell()->web_contents(), url2);
  controller.LoadURL(url2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(stalled_navigation.WaitForResponse());

  // That should be the pending entry.
  NavigationEntryImpl* entry = controller.GetPendingEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(url2, entry->GetURL());

  // Loading a JavaScript URL shouldn't affect the ability to stop.
  {
    FailureWatcher watcher(root);
    GURL js("javascript:(function(){})()");
    controller.LoadURL(js, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
    EXPECT_EQ(entry, controller.GetPendingEntry());
    EXPECT_TRUE(shell()->web_contents()->IsLoading());
    shell()->web_contents()->Stop();
    watcher.Wait();
    EXPECT_FALSE(shell()->web_contents()->IsLoading());
  }
}

namespace {
class RenderProcessKilledObserver : public WebContentsObserver {
 public:
  explicit RenderProcessKilledObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~RenderProcessKilledObserver() override {}

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    CHECK_NE(status,
             base::TerminationStatus::TERMINATION_STATUS_PROCESS_WAS_KILLED);
  }
};
}  // namespace

// Ensure that loading the original request URL results in a simple reload if
// there was no redirect, to avoid clearing state (e.g., navigation API state).
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       LoadOriginalRequestURLSameURL) {
  // Load a page on origin A.
  GURL original_url(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), original_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Update navigation API state.
  {
    std::string script = "navigation.updateCurrentEntry({ state: {x: 'y'} });";
    EXPECT_TRUE(ExecJs(shell(), script));
  }

  // Navigate to another page and then come back, creating a forward entry.
  GURL forward_url(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/simple_page_2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), forward_url));
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    controller.GoBack();
    observer.Wait();
    ASSERT_TRUE(controller.CanGoForward());
  }

  // Load the original request URL, which is the same URL.
  TestNavigationObserver nav_observer(shell()->web_contents());
  controller.LoadOriginalRequestURL();
  nav_observer.Wait();
  ASSERT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());

  // The navigation API state should still be present, because the request was
  // treated as a reload.
  EXPECT_EQ(true,
            EvalJs(shell(), "navigation.currentEntry.getState().x == 'y'"));

  // The forward entry should still exist.
  EXPECT_TRUE(controller.CanGoForward());
}

// Ensure that loading the original request URL after a redirect uses a clean
// slate and doesn't crash (e.g., due to using the wrong process for the
// initiator origin of an about:blank URL, as in https://crbug.com/1427288).
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       LoadOriginalRequestURLCrossSite) {
  // Load a page on site A.
  GURL original_url(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), original_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Navigate to about:blank, inheriting A's origin.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), GURL(url::kAboutBlankURL)));

  // Redirect to a page on site B.
  GURL redirect_url(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/simple_page_1.html"));
  {
    std::string script = "location.replace('" + redirect_url.spec() + "');";
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(shell(), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_TRUE(capturer.did_replace_entry());
  }

  // There should be a Referrer at this point.
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            controller.GetLastCommittedEntry()->GetReferrer().url);

  // Load the original request URL. This should not crash, which used to happen
  // when loading about:blank with A's origin into B's process.
  TestNavigationObserver nav_observer(shell()->web_contents());
  controller.LoadOriginalRequestURL();
  nav_observer.Wait();
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            shell()->web_contents()->GetLastCommittedURL());

  // There should be no Referrer after loading the original URL.
  EXPECT_EQ(GURL::EmptyGURL(),
            controller.GetLastCommittedEntry()->GetReferrer().url);
}

// Ensure that loading the original request URL does not reuse state from the
// current URL (e.g., in a cross-origin but same-process navigation).
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       LoadOriginalRequestURLCrossOriginSameSite) {
  // Load a page on origin A.
  GURL original_url(embedded_test_server()->GetURL(
      "a.foo.com", "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), original_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Redirect to a page on origin B, within the same site.
  GURL redirect_url(embedded_test_server()->GetURL(
      "b.foo.com", "/navigation_controller/simple_page_1.html"));
  {
    std::string script = "location.replace('" + redirect_url.spec() + "');";
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(shell(), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_TRUE(capturer.did_replace_entry());
  }

  // Update navigation API state for the new origin.
  {
    std::string script = "navigation.updateCurrentEntry({ state: {x: 'y'} });";
    EXPECT_TRUE(ExecJs(shell(), script));
  }

  // Navigate to another page and then come back, creating a forward entry.
  GURL forward_url(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/simple_page_2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), forward_url));
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    controller.GoBack();
    observer.Wait();
    ASSERT_TRUE(controller.CanGoForward());
  }

  // Load the original request URL.
  TestNavigationObserver nav_observer(shell()->web_contents());
  controller.LoadOriginalRequestURL();
  nav_observer.Wait();
  ASSERT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());

  // The navigation API state for origin B should not be visible to origin A.
  EXPECT_EQ(true,
            EvalJs(shell(), "navigation.currentEntry.getState() == undefined"));

  // The forward entry should still exist.
  EXPECT_TRUE(controller.CanGoForward());
}

// This tests a race in LoadOriginalRequestURL, where a cross-origin reload was
// causing an in-flight replaceState to look like a cross-origin navigation,
// even though it's same document.  (The reload should not modify the underlying
// last committed entry.)  Not crashing means that the test is successful.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       LoadOriginalRequestURLRace) {
  // TODO(lukasza): https://crbug.com/1159466: Get tests working for all
  // process model modes.
  if (AreStrictSiteInstancesEnabled() ||
      CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    return;
  }

  GURL original_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), original_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderProcessKilledObserver kill_observer(shell()->web_contents());

  // Redirect so that we can use LoadOriginalRequestURL.
  GURL redirect_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    std::string script = "location.replace('" + redirect_url.spec() + "');";
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(shell(), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
  }

  // Modify an entry in the session history and reload the original request.
  {
    // We first send a replaceState() to the renderer, which will cause the
    // renderer to send back a DidCommitProvisionalLoad. Immediately after,
    // we use LoadOriginalRequestURL (which in this case is a different origin)
    // and will also cause the renderer to commit the frame. In the end we
    // verify that both navigations committed and that the URLs are correct.
    std::string script = "history.replaceState({}, '', 'foo');";
    root->render_manager()
        ->current_frame_host()
        ->ExecuteJavaScriptWithUserGestureForTests(base::UTF8ToUTF16(script),
                                                   base::NullCallback());
    EXPECT_FALSE(shell()->web_contents()->IsLoading());
    shell()->web_contents()->GetController().LoadOriginalRequestURL();
    EXPECT_TRUE(shell()->web_contents()->IsLoading());
    EXPECT_EQ(redirect_url, shell()->web_contents()->GetLastCommittedURL());

    // Wait until there's no more navigations.
    GURL modified_url(embedded_test_server()->GetURL(
        "foo.com", "/navigation_controller/foo"));
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    capturer.set_navigations_remaining(2);
    capturer.Wait();
    EXPECT_EQ(2U, capturer.urls().size());
    EXPECT_EQ(modified_url, capturer.urls()[0]);
    EXPECT_EQ(original_url, capturer.urls()[1]);
    EXPECT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());
  }

  // Make sure the renderer is still alive.
  EXPECT_TRUE(ExecJs(shell(), "console.log('Success');"));
}

// This test shows that the initial "about:blank" URL in a subframe is replaced
// in all navigation entries when the subframe is navigated, even if a different
// frame has created a new navigation entry since then.
//
// It also prevents regression for an same document navigation renderer kill
// when going back after an in-page navigation in the main frame is followed by
// an auto subframe navigation, due to a bug in
// WebHistoryEntry::CloneAndReplace. See https://crbug.com/612713.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       InitialAboutBlankInIframeIsReplaced) {
  GURL original_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), original_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, EvalJs(shell(), "history.length"));

  // Add an iframe with no 'src'.

  std::string script =
      "var iframe = document.createElement('iframe');"
      "iframe.id = 'frame';"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(ExecJs(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, EvalJs(shell(), "history.length"));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* frame = root->child_at(0);
  ASSERT_NE(nullptr, frame);

  GURL blank_url(url::kAboutBlankURL);
  EXPECT_EQ(blank_url, frame->current_url());

  // Now create a new navigation entry. The old navigation entry and the new
  // navigation entry will share the same FrameNavigationEntry for the iframe,
  // which will still be at about blank.

  script = "history.pushState({}, '', 'notarealurl.html')";
  EXPECT_TRUE(ExecJs(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, EvalJs(shell(), "history.length"));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Load the iframe; the initial "about:blank" URL should be elided and thus we
  // shouldn't get a new navigation entry. Because the FrameNavigationEntry for
  // the iframe is shared, this will update the url for the iframe in both
  // navigation entries.

  GURL frame_url = embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(frame, frame_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, EvalJs(shell(), "history.length"));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  EXPECT_EQ(frame_url, frame->current_url());

  // Go back.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    ASSERT_TRUE(controller.CanGoBack());
    controller.GoBack();
    observer.Wait();
  }

  // The iframe's url should not have changed.
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, EvalJs(shell(), "history.length"));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(frame_url, frame->current_url());

  // Now test for https://crbug.com/612713 to prevent an NC_IN_PAGE_NAVIGATION
  // renderer kill.

  // Do a same document navigation in the subframe.
  std::string fragment_script = "location.href = \"#foo\";";
  EXPECT_TRUE(ExecJs(frame->current_frame_host(), fragment_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, EvalJs(shell(), "history.length"));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  GURL frame_url_after_fragment_nav = embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html#foo");
  EXPECT_EQ(frame_url_after_fragment_nav, frame->current_url());

  // Go back.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    controller.GoBack();
    observer.Wait();
  }

  // Verify the process is still alive by running script.  We can't just call
  // IsRenderFrameLive after the navigation since it might not have disconnected
  // yet.
  EXPECT_TRUE(ExecJs(root->current_frame_host(), "true;"));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());

  // The back navigation should still return to the first non-empty url the
  // subframe had, not about:blank
  EXPECT_EQ(frame_url, frame->current_url());
}

// This test is similar to "BackToAboutBlankIframe" above, except that a
// fragment navigation is used rather than pushState (both create a same
// document navigation, so we need to test both), and an initial 'src' is given
// to the iframe to test proper restoration in that case.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       BackToIframeWithContent) {
  GURL links_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  EXPECT_TRUE(NavigateToURL(shell(), links_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, EvalJs(shell(), "history.length"));

  // Add an iframe with a 'src'.

  GURL frame_url_1 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html");
  std::string script = JsReplace(
      "var iframe = document.createElement('iframe');"
      "iframe.src = $1;"
      "iframe.id = 'frame';"
      "document.body.appendChild(iframe);",
      frame_url_1);
  EXPECT_TRUE(ExecJs(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, EvalJs(shell(), "history.length"));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* frame = root->child_at(0);
  ASSERT_NE(nullptr, frame);

  EXPECT_EQ(frame_url_1, frame->current_url());

  // Do a fragment navigation, creating a new navigation entry. Note that the
  // old navigation entry has frame_url_1 as the URL in the iframe.

  script = "document.getElementById('fraglink').click()";
  EXPECT_TRUE(ExecJs(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, EvalJs(shell(), "history.length"));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  EXPECT_EQ(frame_url_1, frame->current_url());

  // Navigate the iframe; unlike the test "BackToAboutBlankIframe" above, this
  // _will_ create a new navigation entry.

  GURL frame_url_2 = embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(frame, frame_url_2));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(3, EvalJs(shell(), "history.length"));
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  EXPECT_EQ(frame_url_2, frame->current_url());

  // Go back two entries.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    ASSERT_TRUE(controller.CanGoToOffset(-2));
    controller.GoToOffset(-2);
    observer.Wait();
  }

  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(3, EvalJs(shell(), "history.length"));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(frame_url_1, frame->current_url());

  // Now test for https://crbug.com/612713 to prevent an NC_IN_PAGE_NAVIGATION
  // renderer kill.

  // Do a same document navigation in the subframe.
  std::string fragment_script = "location.href = \"#foo\";";
  EXPECT_TRUE(ExecJs(frame->current_frame_host(), fragment_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, EvalJs(shell(), "history.length"));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Go back.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    controller.GoBack();
    observer.Wait();
  }

  // Verify the process is still alive by running script.  We can't just call
  // IsRenderFrameLive after the navigation since it might not have disconnected
  // yet.
  EXPECT_TRUE(ExecJs(root->current_frame_host(), "true;"));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());

  // TODO(creis): It's a bit surprising to go to frame_url_1 here instead of
  // frame_url_2.  Perhaps we should be going back to frame_url_1 when going
  // back two entries above, since it's different than the initial blank case.
  EXPECT_EQ(frame_url_1, frame->current_url());
}

// Test for same document navigation kills due to using the wrong history item
// in HistoryController::RecursiveGoToEntry and
// NavigationControllerImpl::FindFramesToNavigate.
// See https://crbug.com/612713.
//
// TODO(creis): Enable this test when https://crbug.com/618100 is fixed.
// Disabled for now while we switch to the new navigation path, since this kill
// is exceptionally rare in practice.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       DISABLED_BackTwiceToIframeWithContent) {
  GURL links_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  EXPECT_TRUE(NavigateToURL(shell(), links_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, EvalJs(shell(), "history.length"));

  // Add an iframe with a 'src'.

  GURL frame_url_1 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html");
  std::string script = JsReplace(
      "var iframe = document.createElement('iframe');"
      "iframe.src = $1;"
      "iframe.id = 'frame';"
      "document.body.appendChild(iframe);",
      frame_url_1);
  EXPECT_TRUE(ExecJs(root->current_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, EvalJs(shell(), "history.length"));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* frame = root->child_at(0);
  ASSERT_NE(nullptr, frame);

  EXPECT_EQ(frame_url_1, frame->current_url());

  // Do a same document navigation in the subframe.
  GURL frame_url_2 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html#foo");
  std::string fragment_script = "location.href = \"#foo\";";
  EXPECT_TRUE(ExecJs(frame->current_frame_host(), fragment_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, EvalJs(shell(), "history.length"));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(frame_url_2, frame->current_url());

  // Do a fragment navigation at the top level.
  std::string link_script = "document.getElementById('fraglink').click()";
  EXPECT_TRUE(ExecJs(root->current_frame_host(), link_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(3, EvalJs(shell(), "history.length"));
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(frame_url_2, frame->current_url());

  // Go cross-site in the iframe.
  GURL frame_url_3 = embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(frame, frame_url_3));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(4, EvalJs(shell(), "history.length"));
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(frame_url_3, frame->current_url());

  // Go back two entries.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    ASSERT_TRUE(controller.CanGoToOffset(-2));
    controller.GoToOffset(-2);
    observer.Wait();
  }
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(4, EvalJs(shell(), "history.length"));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(links_url, root->current_url());
  EXPECT_EQ(frame_url_2, frame->current_url());

  // Now test for https://crbug.com/612713 to prevent an NC_IN_PAGE_NAVIGATION
  // renderer kill.

  // Go back.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    controller.GoBack();
    observer.Wait();
  }

  // Verify the process is still alive by running script.  We can't just call
  // IsRenderFrameLive after the navigation since it might not have disconnected
  // yet.
  EXPECT_TRUE(ExecJs(root->current_frame_host(), "true;"));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());

  // TODO(creis): It's a bit surprising to go to frame_url_1 here instead of
  // frame_url_2.  Perhaps we should be going back to frame_url_1 when going
  // back two entries above, since it's different than the initial blank case.
  EXPECT_EQ(frame_url_1, frame->current_url());
}

// Test for same document navigation kills when going back to about:blank after
// a document.write.  See https://crbug.com/446959.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       BackAfterIframeDocumentWrite) {
  GURL links_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  EXPECT_TRUE(NavigateToURL(shell(), links_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, EvalJs(shell(), "history.length"));

  // Add an iframe with no 'src'.
  GURL blank_url(url::kAboutBlankURL);
  std::string script =
      "var iframe = document.createElement('iframe');"
      "iframe.id = 'frame';"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(ExecJs(root->current_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, EvalJs(shell(), "history.length"));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* frame = root->child_at(0);
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(blank_url, frame->current_url());

  // Do a document.write in the subframe to create a link to click.
  std::string document_write_script =
      "var iframe = document.getElementById('frame');"
      "iframe.contentWindow.document.write("
      "    \"<a id='fraglink' href='#frag'>fragment link</a>\");"
      "iframe.contentWindow.document.close();";
  EXPECT_TRUE(ExecJs(root->current_frame_host(), document_write_script));

  // Click the link to do a same document navigation.  Due to the
  // document.write, the new URL matches the parent frame's URL.
  GURL frame_url_2(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html#frag"));
  std::string link_script = "document.getElementById('fraglink').click()";
  EXPECT_TRUE(ExecJs(frame->current_frame_host(), link_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, EvalJs(shell(), "history.length"));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(frame_url_2, frame->current_url());

  // Go back.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    controller.GoBack();
    observer.Wait();
  }

  // Verify the process is still alive by running script.  We can't just call
  // IsRenderFrameLive after the navigation since it might not have disconnected
  // yet.
  EXPECT_TRUE(ExecJs(root->current_frame_host(), "true;"));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());

  EXPECT_EQ(blank_url, frame->current_url());
}

// Test for same document navigation kills when going back to about:blank in an
// iframe of a data URL, after a document.write.  This differs from
// BackAfterIframeDocumentWrite because both about:blank and the data URL are
// considered unique origins.  See https://crbug.com/446959.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       BackAfterIframeDocumentWriteInDataURL) {
  GURL data_url("data:text/html,Top level page");
  EXPECT_TRUE(NavigateToURL(shell(), data_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, EvalJs(shell(), "history.length"));
  const url::Origin opaque_origin = root->current_origin();
  EXPECT_TRUE(opaque_origin.opaque());
  EXPECT_EQ(url::SchemeHostPort(),
            opaque_origin.GetTupleOrPrecursorTupleIfOpaque());

  // Add an iframe with no 'src'.
  GURL blank_url(url::kAboutBlankURL);
  std::string script =
      "var iframe = document.createElement('iframe');"
      "iframe.id = 'frame';"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(ExecJs(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, EvalJs(root, "history.length"));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* frame = root->child_at(0);
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(blank_url, frame->current_url());
  EXPECT_EQ(opaque_origin, root->current_origin());
  EXPECT_EQ(opaque_origin, frame->current_origin());

  // Do a document.write in the subframe to create a link to click.
  std::string html = "<a id='fraglink' href='#frag'>fragment link</a>";
  std::string document_write_script = JsReplace(
      "var iframe = document.getElementById('frame');"
      "iframe.contentWindow.document.write($1);"
      "iframe.contentWindow.document.close();",
      html);
  EXPECT_TRUE(ExecJs(root, document_write_script));
  EXPECT_EQ(opaque_origin, root->current_origin());
  EXPECT_EQ(opaque_origin, frame->current_origin());

  // Click the link to do a same document navigation.  Due to the
  // document.write, the new URL matches the parent frame's URL, but the
  // opaque origin is preserved.
  GURL frame_url_2("data:text/html,Top level page#frag");
  std::string link_script = "document.getElementById('fraglink').click()";
  EXPECT_TRUE(ExecJs(frame, link_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(opaque_origin, root->current_origin());
  EXPECT_EQ(opaque_origin, frame->current_origin());
  EXPECT_EQ(ListValueOf("Top level page", "fragment link"),
            EvalJs(frame,
                   "[window.parent.document.body.textContent,"
                   " document.body.textContent]"))
      << "Frames should be same-origin and able to script each other.";
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, EvalJs(root, "history.length"));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(frame_url_2, frame->current_url());

  // Go back.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    controller.GoBack();
    observer.Wait();
  }

  // Verify the process is still alive by running script.  We can't just call
  // IsRenderFrameLive after the navigation since it might not have disconnected
  // yet.
  EXPECT_EQ("ping", EvalJs(root, "'ping'"));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());

  EXPECT_EQ(blank_url, frame->current_url());
  EXPECT_EQ(opaque_origin, frame->current_origin());
}

// Ensure that we do not corrupt a NavigationEntry's PageState if a subframe
// forward navigation commits after we've already started another forward
// navigation in the main frame.  See https://crbug.com/597322.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ForwardInSubframeWithPendingForward) {
  // The test expects history navigations to be not instant, so that it can
  // start another forward navigation while another forward navigation has
  // already started.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  // Navigate to a page with an iframe.
  GURL url_a(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  GURL frame_url_a1("data:text/html,Subframe");
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(url_a, root->current_url());
  FrameTreeNode* frame = root->child_at(0);
  EXPECT_EQ(frame_url_a1, frame->current_url());

  // Navigate the iframe to a second page.
  GURL frame_url_a2 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(frame, frame_url_a2));

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_a, root->current_url());
  EXPECT_EQ(frame_url_a2, frame->current_url());

  // Navigate the top-level frame to another page with an iframe.
  GURL url_b(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  GURL frame_url_2(url::kAboutBlankURL);
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(url_b, root->current_url());
  EXPECT_EQ(frame_url_2, root->child_at(0)->current_url());

  // Go back two entries. The original frame URL should be back.
  ASSERT_TRUE(controller.CanGoToOffset(-2));
  controller.GoToOffset(-2);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_a, root->current_url());
  EXPECT_EQ(frame_url_a1, root->child_at(0)->current_url());

  // Go forward two times in a row, being careful that the subframe commits
  // after the second forward navigation begins but before the main frame
  // commits.
  FrameTestNavigationManager subframe_delayer(
      root->child_at(0)->frame_tree_node_id(), shell()->web_contents(),
      frame_url_a2);
  TestNavigationManager mainframe_delayer(shell()->web_contents(), url_b);
  controller.GoForward();
  EXPECT_TRUE(subframe_delayer.WaitForRequestStart());
  controller.GoForward();
  EXPECT_TRUE(mainframe_delayer.WaitForRequestStart());
  EXPECT_EQ(2, controller.GetPendingEntryIndex());

  // Let the subframe commit.
  ASSERT_TRUE(subframe_delayer.WaitForNavigationFinished());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_a, root->current_url());
  EXPECT_EQ(frame_url_a2, root->child_at(0)->current_url());

  // Let the main frame commit.
  ASSERT_TRUE(mainframe_delayer.WaitForNavigationFinished());
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_b, root->current_url());
  EXPECT_EQ(frame_url_2, root->child_at(0)->current_url());

  // Check the PageState of the previous entry to ensure it isn't corrupted.
  NavigationEntry* entry = controller.GetEntryAtIndex(1);
  EXPECT_EQ(url_a, entry->GetURL());
  blink::ExplodedPageState exploded_state;
  EXPECT_TRUE(blink::DecodePageState(entry->GetPageState().ToEncodedData(),
                                     &exploded_state));
  EXPECT_EQ(url_a,
            GURL(exploded_state.top.url_string.value_or(std::u16string())));
  EXPECT_EQ(frame_url_a2,
            GURL(exploded_state.top.children.at(0).url_string.value_or(
                std::u16string())));
}

// Start a provisional navigation, but abort it by going back before it commits.
// In crbug.com/631617 there was an issue which cleared the
// pending_navigation_params_ in RenderFrameImpl. This caused the interrupting
// navigation to lose important navigation data like its nav_entry_id, which
// could cause it to commit in-place instead of in the correct location in the
// browsing history.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       AbortProvisionalLoadRetainsNavigationParams) {
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  TestNavigationManager delayer(shell()->web_contents(),
                                embedded_test_server()->GetURL("/title3.html"));
  shell()->LoadURL(embedded_test_server()->GetURL("/title3.html"));
  EXPECT_TRUE(delayer.WaitForRequestStart());

  NavigationController& controller = shell()->web_contents()->GetController();
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_TRUE(controller.CanGoForward());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
}

// Make sure that a 304 response to a navigation aborts the navigation.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, NavigateTo304) {
  // URL that just returns a blank page.
  GURL initial_url = embedded_test_server()->GetURL("/set-header");
  // URL that returns a response with a 304 status code.
  GURL not_modified_url = embedded_test_server()->GetURL("/echo?status=304");

  EXPECT_TRUE(NavigateToURL(shell(), initial_url));
  EXPECT_EQ(initial_url, shell()->web_contents()->GetVisibleURL());

  // The navigation should be aborted.
  EXPECT_FALSE(NavigateToURL(shell(), not_modified_url));
  EXPECT_EQ(initial_url, shell()->web_contents()->GetVisibleURL());
}

// Ensure that we do not corrupt a NavigationEntry's PageState if two forward
// navigations compete in different frames.  See https://crbug.com/623319.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       PageStateAfterForwardInCompetingFrames) {
  // Navigate to a page with an iframe.
  GURL url_a(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  GURL frame_url_a1("data:text/html,Subframe");
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(url_a, root->current_url());
  EXPECT_EQ(frame_url_a1, root->child_at(0)->current_url());

  // Navigate the iframe to a second page.
  GURL frame_url_a2 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url_a2));

  // Navigate the iframe to about:blank.
  GURL blank_url(url::kAboutBlankURL);
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), blank_url));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_a, root->current_url());
  EXPECT_EQ(blank_url, root->child_at(0)->current_url());

  // Go back to the middle entry.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Replace the entry with a cross-site top-level page.  By doing a
  // replacement, the main frame pages before and after have the same item
  // sequence number, and thus going between them only requires loads in the
  // subframe.
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/simple_page_2.html"));
  std::string replace_script = "location.replace('" + url_b.spec() + "')";
  TestNavigationObserver replace_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell()->web_contents(), replace_script));
  replace_observer.Wait();
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_b, root->current_url());

  // Go back to the original page.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // This test might trigger the "undo commit" path in the renderer, that does
  // not work with RenderDocument, causing flakiness.
  // TODO(https://crbug.com/1220337): Fix this.
  if (root->current_frame_host()
          ->ShouldChangeRenderFrameHostOnSameSiteNavigation()) {
    return;
  }

  // Navigate forward twice using script.  In https://crbug.com/623319, this
  // caused a mismatch between the NavigationEntry's URL and PageState.
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(), "history.forward(); history.forward();"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_b, root->current_url());
  NavigationEntry* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(url_b, entry->GetURL());
  blink::ExplodedPageState exploded_state;
  EXPECT_TRUE(blink::DecodePageState(entry->GetPageState().ToEncodedData(),
                                     &exploded_state));
  EXPECT_EQ(url_b,
            GURL(exploded_state.top.url_string.value_or(std::u16string())));
  EXPECT_EQ(0U, exploded_state.top.children.size());

  // Go back and then forward to see if the PageState loads correctly.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  controller.GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We should be on url_b, and the renderer process shouldn't be killed.
  ASSERT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_b, shell()->web_contents()->GetVisibleURL());
  EXPECT_EQ(url_b, root->current_url());
  EXPECT_EQ(0U, root->child_count());
}

// Ensure that we do not corrupt a NavigationEntry's PageState if two forward
// navigations compete in different frames, and the main frame entry contains an
// iframe of its own.  See https://crbug.com/623319.
// TODO(https://crbug.com/1101292): flaky.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    DISABLED_PageStateWithIframeAfterForwardInCompetingFrames) {
  // Navigate to a page with an iframe.
  GURL url_a(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  GURL data_url("data:text/html,Subframe");
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(url_a, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  // Navigate the iframe to a first real page.
  GURL frame_url_a1 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url_a1));

  // Navigate the iframe to a second real page.
  GURL frame_url_a2 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url_a2));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_a, root->current_url());
  EXPECT_EQ(frame_url_a2, root->child_at(0)->current_url());

  // Go back to the middle entry.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Replace the entry with a cross-site top-level page with an iframe.  By
  // doing a replacement, the main frame pages before and after have the same
  // item sequence number, and thus going between them only requires loads in
  // the subframe.
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/page_with_data_iframe.html"));
  std::string replace_script = "location.replace('" + url_b.spec() + "')";
  TestNavigationObserver replace_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell()->web_contents(), replace_script));
  replace_observer.Wait();
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_b, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  // Go back to the original page.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Navigate forward twice using script.  This will race, but in either outcome
  // we want to ensure that the subframes target entry index 1 and not 2.  In
  // https://crbug.com/623319, the subframes targeted the wrong entry, leading
  // to a URL spoof and renderer kill.
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(), "history.forward(); history.forward();"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_EQ(url_b, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());
  NavigationEntry* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(url_b, entry->GetURL());
  blink::ExplodedPageState exploded_state;
  EXPECT_TRUE(blink::DecodePageState(entry->GetPageState().ToEncodedData(),
                                     &exploded_state));
  EXPECT_EQ(url_b,
            GURL(exploded_state.top.url_string.value_or(std::u16string())));

  // Go back and then forward to see if the PageState loads correctly.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  controller.GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We should be on url_b, and the renderer process shouldn't be killed.
  ASSERT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_b, shell()->web_contents()->GetVisibleURL());
  EXPECT_EQ(url_b, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());
}

// Ensure that forward navigations in cloned tabs can commit if they redirect to
// a different site than before.  This causes the navigation's item sequence
// number to change, meaning that we can't use it for determining whether the
// commit matches the history item.  See https://crbug.com/600238.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ForwardRedirectWithNoCommittedEntry) {
  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Put 2 pages in history.
  GURL url_1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  GURL url_2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  EXPECT_EQ(url_2, root->current_url());
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Do a replaceState to a URL that will redirect when we come back to it via
  // session history.
  GURL url_3(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/page_with_links.html"));
  {
    TestNavigationObserver observer(shell()->web_contents());
    std::string script =
        "history.replaceState({}, '', '/server-redirect?" + url_3.spec() + "')";
    EXPECT_TRUE(ExecJs(root, script));
    observer.Wait();
  }

  // Go back.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_1, root->current_url());

  // Clone the tab without navigating it.
  std::unique_ptr<WebContents> new_tab = shell()->web_contents()->Clone();
  WebContentsImpl* new_tab_impl = static_cast<WebContentsImpl*>(new_tab.get());
  NavigationController& new_controller = new_tab_impl->GetController();
  FrameTreeNode* new_root = new_tab_impl->GetPrimaryFrameTree().root();
  EXPECT_TRUE(new_controller.IsInitialNavigation());
  EXPECT_TRUE(new_controller.NeedsReload());

  // Go forward in the new tab.
  {
    TestNavigationObserver observer(new_tab.get());
    new_controller.GoForward();
    observer.Wait();
  }
  EXPECT_TRUE(new_root->current_frame_host()->IsRenderFrameLive());
  EXPECT_EQ(url_3, new_root->current_url());
}

// Ensure that we can support cross-process navigations in subframes due to
// redirects.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       SubframeForwardRedirect) {
  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  GURL url_1(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/page_with_data_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  GURL frame_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));

  EXPECT_EQ(url_1, root->current_url());
  EXPECT_EQ(frame_url, root->child_at(0)->current_url());
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Do a replaceState to a URL that will redirect cross-site when we come back
  // to it via session history.
  GURL frame_url2(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/simple_page_2.html"));
  {
    TestNavigationObserver observer(shell()->web_contents());
    std::string script = "history.replaceState({}, '', '/server-redirect?" +
                         frame_url2.spec() + "')";
    EXPECT_TRUE(ExecJs(root->child_at(0), script));
    observer.Wait();
  }

  // Go back.
  {
    TestNavigationObserver observer(shell()->web_contents());
    controller.GoBack();
    observer.Wait();
  }
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_1, root->current_url());

  // Go forward.
  {
    TestNavigationObserver observer(shell()->web_contents());
    controller.GoForward();
    observer.Wait();
  }
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());
  EXPECT_EQ(url_1, root->current_url());
  EXPECT_EQ(frame_url2, root->child_at(0)->current_url());
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ(GURL("http://bar.com"), root->child_at(0)
                                          ->current_frame_host()
                                          ->GetSiteInstance()
                                          ->GetSiteURL());
  }
}

// Tests that when using FrameNavigationEntries, knowledge of POST navigations
// is recorded on a subframe level.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, PostInSubframe) {
  GURL page_with_form_url = embedded_test_server()->GetURL(
      "/navigation_controller/subframe_form.html");
  EXPECT_TRUE(NavigateToURL(shell(), page_with_form_url));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  FrameTreeNode* frame = root->child_at(0);
  EXPECT_EQ(1, controller.GetEntryCount());

  {
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    scoped_refptr<FrameNavigationEntry> root_entry = entry->GetFrameEntry(root);
    scoped_refptr<FrameNavigationEntry> frame_entry =
        entry->GetFrameEntry(frame);
    EXPECT_NE(nullptr, root_entry);
    EXPECT_NE(nullptr, frame_entry);
    EXPECT_EQ("GET", root_entry->method());
    EXPECT_EQ(-1, root_entry->post_id());
    EXPECT_EQ("GET", frame_entry->method());
    EXPECT_EQ(-1, frame_entry->post_id());
    EXPECT_FALSE(entry->GetHasPostData());
    EXPECT_EQ(-1, entry->GetPostID());
  }

  // Submit the form.
  TestNavigationObserver observer(shell()->web_contents(), 1);
  ExecuteScriptAsync(shell(), "submitForm('isubmit')");
  observer.Wait();

  EXPECT_EQ(2, controller.GetEntryCount());
  {
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    scoped_refptr<FrameNavigationEntry> root_entry = entry->GetFrameEntry(root);
    scoped_refptr<FrameNavigationEntry> frame_entry =
        entry->GetFrameEntry(frame);
    EXPECT_NE(nullptr, root_entry);
    EXPECT_NE(nullptr, frame_entry);
    EXPECT_EQ("GET", root_entry->method());
    EXPECT_EQ(-1, root_entry->post_id());
    EXPECT_EQ("POST", frame_entry->method());
    EXPECT_NE(-1, frame_entry->post_id());
    EXPECT_FALSE(entry->GetHasPostData());
    EXPECT_EQ(-1, entry->GetPostID());
  }
}

// Tests that POST body is not lost when decidePolicyForNavigation tells the
// renderer to route the request via OpenURL mojo method sent to the browser.
// See also https://crbug.com/344348.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, PostViaOpenUrlMsg) {
  GURL main_url(
      embedded_test_server()->GetURL("/form_that_posts_to_echoall.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Ask the renderer to go through OpenURL Mojo method. Without this, the test
  // wouldn't repro https://crbug.com/344348.
  shell()
      ->web_contents()
      ->GetMutableRendererPrefs()
      ->browser_handles_all_top_level_requests = true;
  shell()->web_contents()->SyncRendererPrefs();

  // Submit the form.
  TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "document.getElementById('form').submit();"));
  form_post_observer.Wait();

  // Verify that we arrived at the expected location.
  GURL target_url(embedded_test_server()->GetURL("/echoall"));
  EXPECT_EQ(target_url, shell()->web_contents()->GetLastCommittedURL());

  // Verify that POST body was correctly passed to the server and ended up in
  // the body of the page.
  EXPECT_EQ(
      "text=value\n",
      EvalJs(shell(), "document.getElementsByTagName('pre')[0].innerText"));
}

// This test verifies that reloading a POST request that is uncacheable won't
// incorrectly result in a GET request.  This is a regression test for
// https://crbug.com/860807.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, UncacheablePost) {
  GURL main_url(embedded_test_server()->GetURL(
      "initial-page.example.com", "/form_that_posts_to_echoall_nocache.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContents* web_contents = shell()->web_contents();
  EXPECT_EQ(0, web_contents->GetController().GetLastCommittedEntryIndex());

  // Tweak the test page, so that it POSTs directly to the right cross-site URL
  // (without going through the /cross-site-307/host.com handler, because it
  // seems that such redirects do not preserve the Origin header).
  GURL target_url(
      embedded_test_server()->GetURL("another-site.com", "/echoall/nocache"));
  ASSERT_TRUE(ExecJs(
      web_contents,
      JsReplace("document.getElementById('form').action = $1", target_url)));

  // Submit the form.
  TestNavigationObserver form_post_observer(web_contents, 1);
  EXPECT_TRUE(
      ExecJs(web_contents, "document.getElementById('form').submit();"));
  form_post_observer.Wait();

  // Verify that we arrived at the expected location.
  EXPECT_EQ(target_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(1, web_contents->GetController().GetLastCommittedEntryIndex());

  // Verify that this was a POST request.
  std::string request_headers =
      EvalJs(web_contents, "document.getElementsByTagName('pre')[1].innerText;")
          .ExtractString();
  EXPECT_THAT(request_headers, ::testing::HasSubstr("POST /echoall/nocache"));

  // Verify that POST body was correctly passed to the server and ended up in
  // the body of the page.
  EXPECT_EQ("text=value\n",
            EvalJs(web_contents,
                   "document.getElementsByTagName('pre')[0].innerText;"));

  // Extract the response nonce.
  std::string old_response_nonce =
      EvalJs(web_contents,
             "document.getElementById('response-nonce').innerText")
          .ExtractString();

  // Verify that the Origin header correctly reflects the initial initiator.
  EXPECT_THAT(EvalJs(web_contents,
                     "document.getElementById('request-headers').innerText")
                  .ExtractString(),
              ::testing::HasSubstr("Origin: http://initial-page.example.com"));

  // Go back.
  {
    TestNavigationObserver observer(web_contents);
    web_contents->GetController().GoBack();
    observer.Wait();
  }
  EXPECT_EQ(main_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(0, web_contents->GetController().GetLastCommittedEntryIndex());

  // Go forward.
  {
    TestNavigationObserver navigation_observer(web_contents);
    NavigationHandleObserver handle_observer(web_contents, target_url);
    web_contents->GetController().GoForward();
    navigation_observer.Wait();

    // Verify that the previous response response really was treated as
    // uncacheable.
    EXPECT_TRUE(handle_observer.is_error());
    EXPECT_EQ(net::ERR_CACHE_MISS, handle_observer.net_error_code());
  }
  EXPECT_EQ(target_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(1, web_contents->GetController().GetLastCommittedEntryIndex());

  // Reload
  {
    TestNavigationObserver observer(web_contents);
    web_contents->GetController().Reload(content::ReloadType::NORMAL,
                                         false);  // check_for_repost
    observer.Wait();
  }
  EXPECT_EQ(target_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(1, web_contents->GetController().GetLastCommittedEntryIndex());

  // MAIN VERIFICATION for https://crbug.com/860807: Verify that the reload was
  // a POST request.
  request_headers =
      EvalJs(web_contents, "document.getElementsByTagName('pre')[1].innerText;")
          .ExtractString();
  EXPECT_THAT(request_headers, ::testing::HasSubstr("POST /echoall/nocache"));

  // Verify that POST body was correctly passed to the server and ended up in
  // the body of the page.  This is supplementary verification against
  // https://crbug.com/860807.
  EXPECT_EQ("text=value\n",
            EvalJs(web_contents,
                   "document.getElementsByTagName('pre')[0].innerText;"));

  // Extract the new response nonce and verify that it did change (e.g. that the
  // reload did load fresh content).
  EXPECT_NE(old_response_nonce,
            EvalJs(web_contents,
                   "document.getElementById('response-nonce').innerText"));

  // Verify that the Origin header correctly reflects the initial initiator.
  // This is a regression test for https://crbug.com/915538.
  EXPECT_THAT(EvalJs(web_contents,
                     "document.getElementById('request-headers').innerText")
                  .ExtractString(),
              ::testing::HasSubstr("Origin: http://initial-page.example.com"));
}

// This test verifies that it is possible to reload a POST request that
// initially failed (e.g. because the network was offline or the host was
// unreachable during the initial navigation).  This is a regression test for
// https://crbug.com/869117.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ReloadOfInitiallyFailedPost) {
  GURL main_url(embedded_test_server()->GetURL(
      "/form_that_posts_to_echoall_nocache.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContents* web_contents = shell()->web_contents();
  EXPECT_EQ(0, web_contents->GetController().GetLastCommittedEntryIndex());

  // Submit the form while simulating "network down" conditions.
  GURL target_url(embedded_test_server()->GetURL("/echoall/nocache"));
  {
    std::unique_ptr<URLLoaderInterceptor> interceptor =
        URLLoaderInterceptor::SetupRequestFailForURL(
            target_url, net::ERR_INTERNET_DISCONNECTED);
    TestNavigationObserver form_post_observer(web_contents, 1);
    EXPECT_TRUE(
        ExecJs(web_contents, "document.getElementById('form').submit();"));
    form_post_observer.Wait();
  }
  EXPECT_EQ(target_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(1, web_contents->GetController().GetLastCommittedEntryIndex());

  // Reload
  {
    TestNavigationObserver observer(web_contents);
    web_contents->GetController().Reload(content::ReloadType::NORMAL,
                                         false);  // check_for_repost
    observer.Wait();
  }
  EXPECT_EQ(target_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(1, web_contents->GetController().GetLastCommittedEntryIndex());

  // Verify that the reload was a POST request.
  std::string request_headers =
      EvalJs(web_contents, "document.getElementsByTagName('pre')[1].innerText;")
          .ExtractString();
  EXPECT_THAT(request_headers, ::testing::HasSubstr("POST /echoall/nocache"));

  // Verify that POST body was correctly passed to the server and ended up in
  // the body of the page.
  EXPECT_EQ("text=value\n",
            EvalJs(web_contents,
                   "document.getElementsByTagName('pre')[0].innerText;"));
}

// Tests that inserting a named subframe into the FrameTree clears any
// previously existing FrameNavigationEntry objects for the same name.
// See https://crbug.com/628677.
// Crashes/fails inconsistently on windows and ChromeOS:
// https://crbug.com/783806.
// Flaky on every platforms:
// https://crbug.com/765107#c15
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       DISABLED_EnsureFrameNavigationEntriesClearedOnMismatch) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  NavigationControllerImpl& controller = web_contents->GetController();
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();

  // Start by navigating to a page with complex frame hierarchy.
  GURL start_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(3U, root->child_count());
  EXPECT_EQ(2U, root->child_at(0)->child_count());

  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();

  // Verify only the parts of the NavigationEntry affected by this test.
  {
    // * Main frame has 3 subframes.
    scoped_refptr<FrameNavigationEntry> root_entry = entry->GetFrameEntry(root);
    EXPECT_NE(nullptr, root_entry);
    EXPECT_EQ("", root_entry->frame_unique_name());
    EXPECT_EQ(3U, entry->root_node()->children.size());
    EXPECT_EQ(2U, entry->root_node()->children[0]->children.size());

    // * The first child of the main frame is named and has two more children.
    FrameTreeNode* frame = root->child_at(0)->child_at(0);
    NavigationEntryImpl::TreeNode* tree_node = entry->GetTreeNode(frame);
    scoped_refptr<FrameNavigationEntry> frame_entry =
        entry->GetFrameEntry(frame);
    EXPECT_NE(nullptr, tree_node);
    EXPECT_NE(nullptr, frame_entry);
    EXPECT_EQ("1-1: 2-1: name", frame_entry->frame_unique_name());
    EXPECT_EQ(frame_entry, tree_node->frame_entry);
    EXPECT_EQ(0U, tree_node->children.size());
  }

  // Removing the first child of the main frame should remove the corresponding
  // FrameTreeNode.
  EXPECT_TRUE(ExecJs(root, kRemoveFrameScript));
  EXPECT_EQ(2U, root->child_count());

  // However, the FrameNavigationEntry objects for the frame that was removed
  // should still be around.
  {
    scoped_refptr<FrameNavigationEntry> root_entry = entry->GetFrameEntry(root);
    EXPECT_NE(nullptr, root_entry);
    EXPECT_EQ(3U, entry->root_node()->children.size());
    EXPECT_EQ(2U, entry->root_node()->children[0]->children.size());

    // Since child count is known only to the FrameNavigationEntry::TreeNode,
    // traverse the root entry to find the correct one matching the
    // frame_unique_name. The ordering of entries in the FrameNavigationEntry
    // tree is not guaranteed to be the same as the order in the FrameTreeNode
    // tree. The latter depends on the order of frames committing navigations,
    // which is undefined and depends on responses from the network.
    // Traverse the FrameNavigationEntry tree, since the FrameTreeNode has
    // been deleted and cannot be used for looking up the TreeNode.
    NavigationEntryImpl::TreeNode* tree_node = nullptr;
    for (auto& node : entry->root_node()->children[0]->children) {
      if (node->frame_entry->frame_unique_name() == "1-1: 2-1: name") {
        tree_node = node.get();
        break;
      }
    }
    EXPECT_TRUE(tree_node);
    EXPECT_EQ(0U, tree_node->children.size());
  }

  // Now, insert a frame with the same name as the previously removed one
  // at a different layer of the frame tree.
  FrameTreeNode* subframe = root->child_at(1)->child_at(1)->child_at(0);
  EXPECT_EQ(2U, root->child_at(1)->child_count());
  EXPECT_EQ(0U, subframe->child_count());
  std::string add_matching_name_frame_script =
      "var f = document.createElement('iframe');"
      "f.name = '1-1-name';"
      "f.src = '1-1.html';"
      "document.body.appendChild(f);";
  TestNavigationObserver observer(web_contents, 1);
  EXPECT_TRUE(ExecJs(subframe, add_matching_name_frame_script));
  EXPECT_EQ(1U, subframe->child_count());
  observer.Wait();

  // Verify that the FrameNavigationEntry for the original frame is now gone.
  {
    scoped_refptr<FrameNavigationEntry> root_entry = entry->GetFrameEntry(root);
    EXPECT_NE(nullptr, root_entry);
    EXPECT_EQ(3U, entry->root_node()->children.size());

    // Both children of |entry->root_node()->children[0]| should be removed by
    // NavigationEntryImpl::RemoveEntryForFrame, because both will have
    // colliding unique names (the removed parent and the newly added frame both
    // load '1-1.html' - which has 2 named framse).
    EXPECT_EQ(0U, entry->root_node()->children[0]->children.size());
  }
}

// This test ensures that the comparison of tree position between a
// FrameTreeNode and FrameNavigationEntry works correctly for matching
// first-level frames.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       EnsureFirstLevelFrameNavigationEntriesMatch) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();

  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  NavigationEntryImpl* nav_entry =
      web_contents->GetController().GetLastCommittedEntry();

  // Add, then remove a named frame. It will create a FrameNavigationEntry
  // for the name and remove it (since this is a frame created by script).
  EXPECT_TRUE(ExecJs(root, kAddNamedFrameScript));
  EXPECT_EQ(1U, root->child_count());
  EXPECT_EQ(1U, nav_entry->root_node()->children.size());
  scoped_refptr<FrameNavigationEntry> old_fne =
      nav_entry->root_node()->children[0]->frame_entry;

  EXPECT_TRUE(ExecJs(root, kRemoveFrameScript));
  EXPECT_EQ(0U, root->child_count());
  EXPECT_EQ(0U, nav_entry->root_node()->children.size());

  // Add another frame with the same name as before. The matching logic should
  // NOT consider them the same and should NOT result in the
  // FrameNavigationEntry being reused (because the frame injected by javascript
  // will get a fresh, random unique name each time it is created or recreated).
  EXPECT_TRUE(ExecJs(root, kAddNamedFrameScript));
  EXPECT_EQ(1U, root->child_count());
  EXPECT_EQ(1U, nav_entry->root_node()->children.size());
  scoped_refptr<FrameNavigationEntry> new_fne =
      nav_entry->root_node()->children[0]->frame_entry;
  EXPECT_TRUE(old_fne->HasOneRef());  // Only the test keeps the old FNE alive.
  EXPECT_NE(old_fne.get(), new_fne.get());

  EXPECT_TRUE(ExecJs(root, kRemoveFrameScript));
  EXPECT_EQ(0U, root->child_count());
}

// Test that converted reload navigations classified as EXISTING_ENTRY properly
// update all the members of FrameNavigationEntry if they redirect. If not, it
// is possible to get a mismatch between the origin and URL of a document as
// seen in https://crbug.com/630103.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       EnsureSameURLNavigationUpdatesFrameNavigationEntry) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();

  // Navigate to a simple page and then perform a fragment change navigation.
  GURL start_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL fragment_change_url(
      embedded_test_server()->GetURL("a.com", "/title1.html#foo"));
  EXPECT_TRUE(NavigateToURL(shell(), fragment_change_url));
  EXPECT_EQ(2, web_contents->GetController().GetEntryCount());

  // Replace the URL of the current NavigationEntry with one that will cause
  // a server redirect when loaded.
  {
    GURL redirect_dest_url(
        embedded_test_server()->GetURL("sub.a.com", "/simple_page.html"));
    TestNavigationObserver observer(web_contents);
    std::string script = "history.replaceState({}, '', '/server-redirect?" +
                         redirect_dest_url.spec() + "')";
    EXPECT_TRUE(ExecJs(root, script));
    observer.Wait();
  }

  // Simulate the user hitting Enter in the omnibox without changing the URL.
  {
    TestNavigationObserver observer(web_contents);
    web_contents->GetController().LoadURL(web_contents->GetLastCommittedURL(),
                                          Referrer(), ui::PAGE_TRANSITION_LINK,
                                          std::string());
    observer.Wait();
  }

  // Prior to fixing the issue, the above omnibox navigation (which is now
  // classified as EXISTING_ENTRY) was leaving the FrameNavigationEntry with the
  // same document sequence number as the previous entry but updates the URL.
  // Doing a back session history navigation now will cause the browser to
  // consider it as same document because of this matching document sequence
  // number and lead to a mismatch of origin and URL in the renderer process.
  {
    TestNavigationObserver observer(web_contents);
    web_contents->GetController().GoBack();
    observer.Wait();
  }

  // Verify the expected origin through JavaScript. It also has the additional
  // verification of the process also being still alive.
  EXPECT_EQ(url::Origin::Create(start_url).Serialize(),
            EvalJs(web_contents, "self.origin"));
}

// Helper to trigger a history-back navigation in the WebContents after the
// renderer has committed a same-process and cross-origin navigation to the
// given |url|, but before the browser side has had a chance to process the
// DidCommitProvisionalLoad message.
class HistoryNavigationBeforeCommitInjector
    : public DidCommitNavigationInterceptor {
 public:
  HistoryNavigationBeforeCommitInjector(WebContentsImpl* web_contents,
                                        const GURL& url)
      : DidCommitNavigationInterceptor(web_contents),
        did_trigger_history_navigation_(false),
        url_(url) {}

  HistoryNavigationBeforeCommitInjector(
      const HistoryNavigationBeforeCommitInjector&) = delete;
  HistoryNavigationBeforeCommitInjector& operator=(
      const HistoryNavigationBeforeCommitInjector&) = delete;

  ~HistoryNavigationBeforeCommitInjector() override {}

  bool did_trigger_history_navigation() const {
    return did_trigger_history_navigation_;
  }

 private:
  // DidCommitNavigationInterceptor:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    if (!render_frame_host->GetParent() && (**params).url == url_) {
      did_trigger_history_navigation_ = true;
      web_contents()->GetController().GoBack();
    }
    return true;
  }

  bool did_trigger_history_navigation_;
  GURL url_;
};

// Test which simulates a race condition between a cross-origin, same-process
// navigation and a same document session history navigation. When such a race
// occurs, the renderer will commit the cross-origin navigation, updating its
// version of the current document sequence number, and will send an IPC to the
// browser process. The session history navigation comes after the commit for
// the cross-origin navigation and updates the URL, but not the origin of the
// document. This results in mismatch between the two and causes the renderer
// process to be killed. See https://crbug.com/630103.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    RaceCrossOriginNavigationAndSameDocumentHistoryNavigation) {
  if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // When RenderDocument is enabled, the cross-document navigation uses a
    // new RenderFrameHost to commit the navigation, causing the previous
    // RenderFrameHost to be deleted and the same-document navigation to be
    // cancelled, so just return early.
    // TODO(https://crbug.com/936696): When
    // ShouldAvoidRedundantNavigationCancellations() returns true, we won't
    // actually cancel the same-document navigation as it still lives in the
    // FrameTreeNode (instead of owned by the swapped out RenderFrameHost), but
    // apparently there is a bug with same-document history navigations that
    // happen while there are pending cross-document commits, where we will
    // still try to trigger beforeunload, causing the same-document history
    // navigation to get stalled forever. Also, due to the way the history
    // navigation injection works, it might trigger the deletion of the pending
    // commit RenderFrameHost while in the middle of processing the
    // DidCommitNavigation call because the history navigation will try to
    // create a new speculative RenderFrameHost, unless when navigation queueing
    // is enabled. After fixing the beforeunload bug, we should be able to
    // continue running the test when RenderDocument is enabled
    // and ShouldQueueNavigationsWhenPendingCommitRFHExists() is true.
    return;
  }
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();

  // Navigate to a simple page and then perform a same document navigation.
  GURL start_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL same_document_url(
      embedded_test_server()->GetURL("a.com", "/title1.html#foo"));
  EXPECT_TRUE(NavigateToURL(shell(), same_document_url));
  EXPECT_EQ(2, web_contents->GetController().GetEntryCount());

  // Create a HistoryNavigationBeforeCommitInjector, which will perform a
  // GoBack() just before a cross-origin, same process navigation commits.
  GURL cross_origin_url(
      embedded_test_server()->GetURL("suborigin.a.com", "/title2.html"));
  HistoryNavigationBeforeCommitInjector trigger(web_contents, cross_origin_url);

  // Navigate cross-origin, waiting for the commit to occur.
  UrlCommitObserver cross_origin_commit_observer(root, cross_origin_url);
  UrlCommitObserver history_commit_observer(root, start_url);
  shell()->LoadURL(cross_origin_url);
  cross_origin_commit_observer.Wait();
  EXPECT_EQ(cross_origin_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(2, web_contents->GetController().GetLastCommittedEntryIndex());
  EXPECT_TRUE(trigger.did_trigger_history_navigation());

  // Wait for the back navigation to commit as well.
  history_commit_observer.Wait();
  EXPECT_EQ(start_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(0, web_contents->GetController().GetLastCommittedEntryIndex());
  EXPECT_EQ(3, web_contents->GetController().GetEntryCount());

  // Verify the expected origin through JavaScript. It also has the additional
  // verification of the process also being still alive.
  EXPECT_EQ(url::Origin::Create(start_url).Serialize(),
            EvalJs(web_contents, "self.origin"));
}

// This test simulates what happens when OnCommitTimeout is triggered after
// ResetForCrossDocumentRestart. See https://crbug.com/1006677.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       OnCommitTimeoutAfterResetForCrossDocumentRestart) {
  if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // When RenderDocument is enabled, the cross-document navigation uses a
    // new RenderFrameHost to commit the navigation, causing the previous
    // RenderFrameHost to be deleted and the same-document navigation to be
    // cancelled, so just return early.
    // TODO(https://crbug.com/936696): When
    // ShouldAvoidRedundantNavigationCancellations() returns true, we won't
    // actually cancel the same-document navigation as it still lives in the
    // FrameTreeNode (instead of owned by the swapped out RenderFrameHost), but
    // apparently there is a bug with same-document history navigations that
    // happen while there are pending cross-document commits, where we will
    // still try to trigger beforeunload, causing the same-document history
    // navigation to get stalled forever. Also, due to the way the history
    // navigation injection works, it might trigger the deletion of the pending
    // commit RenderFrameHost while in the middle of processing the
    // DidCommitNavigation call because the history navigation will try to
    // create a new speculative RenderFrameHost, unless when navigation queueing
    // is enabled. After fixing the beforeunload bug, we should be able to
    // continue running the test when RenderDocument is enabled
    // and ShouldQueueNavigationsWhenPendingCommitRFHExists() is true.
    return;
  }

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();

  // Navigate to a simple page and then perform a same document navigation.
  GURL start_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL same_document_url(
      embedded_test_server()->GetURL("a.com", "/title1.html#foo"));
  EXPECT_TRUE(NavigateToURL(shell(), same_document_url));
  EXPECT_EQ(2, web_contents->GetController().GetEntryCount());

  // Create a HistoryNavigationBeforeCommitInjector, which will perform a
  // GoBack() just before a cross-origin, same process navigation commits.
  GURL cross_origin_url(
      embedded_test_server()->GetURL("suborigin.a.com", "/title2.html"));
  HistoryNavigationBeforeCommitInjector trigger(web_contents, cross_origin_url);

  // Trigger OnCommitTimeout by setting commit timeout to 1 microsecond.
  NavigationRequest::SetCommitTimeoutForTesting(base::Microseconds(1));

  // Navigate cross-origin, waiting for the commit to occur.
  UrlCommitObserver cross_origin_commit_observer(root, cross_origin_url);
  UrlCommitObserver history_commit_observer(root, start_url);
  shell()->LoadURL(cross_origin_url);
  cross_origin_commit_observer.Wait();
  EXPECT_EQ(cross_origin_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(2, web_contents->GetController().GetLastCommittedEntryIndex());

  // Wait for the history navigation to commit.
  history_commit_observer.Wait();
  EXPECT_EQ(start_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(0, web_contents->GetController().GetLastCommittedEntryIndex());

  // Reset the timeout.
  NavigationRequest::SetCommitTimeoutForTesting(base::TimeDelta());
}

// This test simulates a same-document navigation racing with a cross-document
// one. Historically this would have been started as a same-document navigation
// then restarted by the renderer as a cross-document navigation (see
// https://crbug.com/936962).
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTestNoServer,
                       SameDocumentNavigationRaceWithCrossDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();

  // 1. Navigate to a simple page that cannot be BFCached.
  GURL start_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  {
    EXPECT_TRUE(NavigateToURL(shell(), start_url));
    DisableBackForwardCacheForTesting(
        web_contents, BackForwardCache::TEST_REQUIRES_NO_CACHING);
    EXPECT_EQ(0, web_contents->GetController().GetLastCommittedEntryIndex());
  }

  // 2. Perform a same-document navigation forward.
  {
    GURL same_document_url(
        embedded_test_server()->GetURL("a.com", "/title1.html#foo"));
    EXPECT_TRUE(NavigateToURL(shell(), same_document_url));
    EXPECT_EQ(1, web_contents->GetController().GetLastCommittedEntryIndex());
  }

  // 3. Create a HistoryNavigationBeforeCommitInjector, which will perform a
  // same-document back navigation just before a cross-document navigation
  // commits. This triggers a race condition and forces the
  // same-document navigation to become a cross-document navigation.
  {
    NavigationHandleCommitObserver back_navigation(web_contents, start_url);

    GURL cross_document_url(
        embedded_test_server()->GetURL("a.com", "/title2.html"));
    HistoryNavigationBeforeCommitInjector trigger(web_contents,
                                                  cross_document_url);

    // Navigate cross-document, waiting for the commit to occur.
    UrlCommitObserver cross_doc_commit_observer(root, cross_document_url);
    shell()->LoadURL(cross_document_url);
    EXPECT_TRUE(web_contents->GetController().GetPendingEntry());
    cross_doc_commit_observer.Wait();

    // The cross-document navigation is done, and we're at history entry 2 (the
    // third document in the list).
    EXPECT_EQ(cross_document_url, web_contents->GetLastCommittedURL());
    EXPECT_EQ(2, web_contents->GetController().GetLastCommittedEntryIndex());
    // Verify the same-document history navigation was started before this
    // committed.
    EXPECT_TRUE(trigger.did_trigger_history_navigation());

    if (ShouldCreateNewHostForAllFrames()) {
      // When RenderDocument is enabled, the cross-document navigation used a
      // new RenderFrameHost to commit the navigation, causing the previous
      // RenderFrameHost to be deleted and the same-document navigation to be
      // cancelled. Since there are no navigations left, just return early.
      // TODO(https://crbug.com/936696): When
      // ShouldAvoidRedundantNavigationCancellations() returns true, we won't
      // actually cancel the same-document navigation as it still lives in the
      // FrameTreeNode (instead of owned by the swapped out RenderFrameHost),
      // but apparently there is a bug with same-document history navigations
      // that happen while there are pending cross-document commits, where we
      // will still try to trigger beforeunload, causing the same-document
      // history navigation to get stalled forever. After fixing that bug, we
      // should be able to continue running the test when RenderDocument is
      // enabled and ShouldAvoidRedundantNavigationCancellations() is true.
      EXPECT_EQ(ShouldAvoidRedundantNavigationCancellations(),
                !!root->navigation_request());
      return;
    }

    // Otherwise, the same-document back navigation had to be converted to a
    // cross-document navigation because it was racing with, and will complete
    // after, the cross-document navigation. It is still waiting to complete.
    EXPECT_TRUE(root->navigation_request());
    // This is the history navigation.
    EXPECT_EQ(root->navigation_request()->common_params().url.spec(),
              start_url.spec());
    // It was not same-document because of the race.
    EXPECT_FALSE(root->navigation_request()->IsSameDocument());

    UrlCommitObserver back_history_commit_observer(root, start_url);
    back_history_commit_observer.Wait();
    // The back navigation completes afterward. There is no more requests to
    // run, and no pending commits left.
    EXPECT_FALSE(root->navigation_request());
    EXPECT_FALSE(root->current_frame_host()->HasPendingCommitNavigation());
    // The back navigation was not same-document due to the race with a
    // cross-document navigation committing first.
    EXPECT_TRUE(back_navigation.has_committed());
    EXPECT_FALSE(back_navigation.was_same_document());
    // The back navigation took us back to the expected history entry.
    EXPECT_EQ(0, web_contents->GetController().GetLastCommittedEntryIndex());
  }
}

// Test that verifies that Referer and Origin http headers are correctly sent
// to the final destination of a cross-site POST with a few redirects thrown in.
// This test is somewhat related to https://crbug.com/635400.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       RefererAndOriginHeadersAfterRedirects) {
  // Navigate to the page with form that posts via 307 redirection to
  // |redirect_target_url| (cross-site from |form_url|).  Using 307 (rather than
  // 302) redirection is important to preserve the HTTP method and POST body.
  GURL form_url(embedded_test_server()->GetURL(
      "a.com", "/form_that_posts_cross_site.html"));
  GURL redirect_target_url(embedded_test_server()->GetURL("x.com", "/echoall"));
  EXPECT_TRUE(NavigateToURL(shell(), form_url));

  // Submit the form.  The page submitting the form is at 0, and will
  // go through 307 redirects from 1 -> 2 and 2 -> 3:
  // 0. http://a.com:.../form_that_posts_cross_site.html
  // 1. http://a.com:.../cross-site-307/i.com/cross-site-307/x.com/echoall
  // 2. http://i.com:.../cross-site-307/x.com/echoall
  // 3. http://x.com:.../echoall/
  TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(
      ExecJs(shell(), "document.getElementById('text-form').submit();"));
  form_post_observer.Wait();

  // Verify that we arrived at the expected, redirected location.
  EXPECT_EQ(redirect_target_url,
            shell()->web_contents()->GetLastCommittedURL());

  // Get the http request headers.
  std::string headers =
      EvalJs(shell(), "document.getElementsByTagName('pre')[1].innerText")
          .ExtractString();

  // Verify the Origin and Referer headers.
  EXPECT_THAT(headers, ::testing::HasSubstr("Origin: null"));
  EXPECT_THAT(headers, ::testing::ContainsRegex("Referer: http://a.com:.*/"));
}

// Test that verifies that Content-Type http header is correctly sent
// to the final destination of a cross-site POST with a few redirects thrown in.
// Test for https://crbug.com/860546.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ContentTypeHeaderAfterRedirectAndRefresh) {
  // Navigate to the page with form that posts via 307 redirection to
  // |redirect_target_url| (cross-site from |form_url|).  Using 307 (rather than
  // 302) redirection is important to preserve the HTTP method and POST body.
  GURL form_url(embedded_test_server()->GetURL(
      "a.com", "/form_that_posts_cross_site.html"));
  GURL redirect_target_url(embedded_test_server()->GetURL("x.com", "/echoall"));
  EXPECT_TRUE(NavigateToURL(shell(), form_url));

  // Submit the form.  The page submitting the form is at 0, and will
  // go through 307 redirects from 1 -> 2 and 2 -> 3:
  // 0. http://a.com:.../form_that_posts_cross_site.html
  // 1. http://a.com:.../cross-site-307/i.com/cross-site-307/x.com/echoall
  // 2. http://i.com:.../cross-site-307/x.com/echoall
  // 3. http://x.com:.../echoall/
  TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(
      ExecJs(shell(), "document.getElementById('text-form').submit();"));
  form_post_observer.Wait();

  // Verify that we arrived at the expected, redirected location.
  EXPECT_EQ(redirect_target_url,
            shell()->web_contents()->GetLastCommittedURL());

  // Get the http request headers.
  std::string headers =
      EvalJs(shell(), "document.getElementsByTagName('pre')[1].innerText")
          .ExtractString();

  // Verify the Content-Type header.
  EXPECT_THAT(headers, ::testing::HasSubstr(
                           "Content-Type: application/x-www-form-urlencoded"));

  // Reload the page.
  TestNavigationObserver reload_observer(shell()->web_contents(), 1);
  ASSERT_TRUE(ExecJs(shell(), "location.reload()"));
  reload_observer.Wait();

  // Re-verify the Content-Type header.
  headers = EvalJs(shell(), "document.getElementsByTagName('pre')[1].innerText")
                .ExtractString();
  EXPECT_THAT(headers, ::testing::HasSubstr(
                           "Content-Type: application/x-www-form-urlencoded"));
}

// Check that the favicon is not cleared for same document navigations.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       SameDocumentNavigationDoesNotClearFavicon) {
  // Load a page and fake a favicon for it.
  NavigationController& controller = shell()->web_contents()->GetController();
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/simple_page.html")));
  NavigationEntry* entry = controller.GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  content::FaviconStatus& favicon_status = entry->GetFavicon();
  favicon_status.valid = true;

  ASSERT_TRUE(RendererLocationReplace(
      shell(), embedded_test_server()->GetURL(
                   "/simple_page.html#same-document-navigation")));
  entry = controller.GetLastCommittedEntry();
  content::FaviconStatus& favicon_status2 = entry->GetFavicon();
  EXPECT_TRUE(favicon_status2.valid);

  ASSERT_TRUE(RendererLocationReplace(
      shell(),
      embedded_test_server()->GetURL("/simple_page.html?new-navigation")));
  entry = controller.GetLastCommittedEntry();
  content::FaviconStatus& favicon_status3 = entry->GetFavicon();
  EXPECT_FALSE(favicon_status3.valid);
}

namespace {

class AllowDialogInterceptor
    : public blink::mojom::LocalFrameHostInterceptorForTesting {
 public:
  AllowDialogInterceptor() = default;
  ~AllowDialogInterceptor() override = default;

  void Init(RenderFrameHostImpl* render_frame_host) {
    render_frame_host_ = render_frame_host;
    std::ignore = render_frame_host_->local_frame_host_receiver_for_testing()
                      .SwapImplForTesting(this);
  }

  blink::mojom::LocalFrameHost* GetForwardingInterface() override {
    return render_frame_host_;
  }

  void RunModalAlertDialog(const std::u16string& alert_message,
                           bool disable_third_party_subframe_suppresion,
                           RunModalAlertDialogCallback callback) override {
    alert_callback_ = std::move(callback);
    alert_message_ = alert_message;
  }

  void ResumeProcessingModalAlertDialogHandling() {
    has_called_callback_ = true;
    render_frame_host_->RunModalAlertDialog(alert_message_, false,
                                            std::move(alert_callback_));
  }

  bool HasCalledAlertCallback() const { return has_called_callback_; }

 private:
  raw_ptr<RenderFrameHostImpl> render_frame_host_;
  std::u16string alert_message_;
  RunModalAlertDialogCallback alert_callback_;
  bool has_called_callback_ = false;
};

class NavigationControllerAlertDialogBrowserTest
    : public NavigationControllerBrowserTest,
      public WebContentsObserver,
      public WebContentsDelegate {
 public:
  void BindWebContents(WebContents* web_contents) {
    alert_interceptor_.Init(
        static_cast<RenderFrameHostImpl*>(web_contents->GetPrimaryMainFrame()));
    Observe(web_contents);
    web_contents->SetDelegate(this);
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!navigation_handle->HasCommitted())
      return;

    // Continue handling the rest of the alert dialog handling.
    alert_interceptor_.ResumeProcessingModalAlertDialogHandling();
  }

  // WebContentsDelegate:
  JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source) override {
    CHECK(false);
    return nullptr;  // agh compiler
  }

  bool HasCalledAlertCallback() const {
    return alert_interceptor_.HasCalledAlertCallback();
  }

 private:
  AllowDialogInterceptor alert_interceptor_;
};

}  // namespace

// Check that swapped out frames cannot spawn JavaScript dialogs.
// TODO(crbug.com/1112336): Flaky
IN_PROC_BROWSER_TEST_P(NavigationControllerAlertDialogBrowserTest,
                       DISABLED_NoDialogsFromSwappedOutFrames) {
  // Start on a normal page.
  GURL url1 = embedded_test_server()->GetURL(
      "/navigation_controller/beforeunload_dialog.html");
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Bind the WebContents observer to start watching for the finished
  // navigation callback. When the navigation is called we resume the
  // suspended alert dialog handling.
  WebContents* web_contents = shell()->web_contents();
  BindWebContents(web_contents);

  // Use a chrome:// url to force the second page to be in a different process.
  GURL url2(std::string(kChromeUIScheme) + url::kStandardSchemeSeparator +
            kChromeUIGpuHost);
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  // What happens now is that attempting to unload the first page will trigger a
  // JavaScript alert but allow navigation. The alert mojo message will be
  // suspended by the subclassed RenderFrameHostImplForAllowDialogInterceptor.
  // The commit of the second page will cause the alert dialog message handling
  // to resume. If the dialog mojo message is allowed to spawn a dialog, the
  // call by the WebContents to its delegate to get the JavaScriptDialogManager
  // will cause a CHECK and the test will fail.
  EXPECT_TRUE(HasCalledAlertCallback());
}

// Check that the referrer is stored inside FrameNavigationEntry for subframes.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       RefererStoredForSubFrame) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL url_simple(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe_simple.html"));
  GURL url_redirect(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe_redirect.html"));

  // Run this test twice: with and without a redirection.
  for (const GURL& url : {url_simple, url_redirect}) {
    // Navigate to a page with an iframe.
    EXPECT_TRUE(NavigateToURL(shell(), url));

    // Check the FrameNavigationEntry's referrer.
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    ASSERT_EQ(1U, entry->root_node()->children.size());
    scoped_refptr<FrameNavigationEntry> frame_entry =
        entry->root_node()->children[0]->frame_entry.get();
    EXPECT_EQ(frame_entry->referrer().url, url);
  }
}

namespace {

class RequestMonitoringNavigationBrowserTest
    : public NavigationControllerBrowserTest {
 public:
  RequestMonitoringNavigationBrowserTest() = default;

  const net::test_server::HttpRequest* FindAccumulatedRequest(
      const GURL& url_to_find) {
    DCHECK(url_to_find.SchemeIsHTTPOrHTTPS());

    auto it = base::ranges::find(accumulated_requests_, url_to_find,
                                 &net::test_server::HttpRequest::GetURL);
    if (it == accumulated_requests_.end())
      return nullptr;
    return &*it;
  }

 protected:
  void SetUpOnMainThread() override {
    // Accumulate all http requests made to |embedded_test_server| into
    // |accumulated_requests_| container.
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &RequestMonitoringNavigationBrowserTest::MonitorRequestOnIoThread,
        weak_factory_.GetWeakPtr(),
        base::SequencedTaskRunner::GetCurrentDefault()));

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDown() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

 private:
  static void MonitorRequestOnIoThread(
      const base::WeakPtr<RequestMonitoringNavigationBrowserTest>& weak_this,
      const scoped_refptr<base::SequencedTaskRunner>& postback_task_runner,
      const net::test_server::HttpRequest& request) {
    postback_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RequestMonitoringNavigationBrowserTest::MonitorRequestOnMainThread,
            weak_this, request));
  }

  void MonitorRequestOnMainThread(
      const net::test_server::HttpRequest& request) {
    accumulated_requests_.push_back(request);
  }

  std::vector<net::test_server::HttpRequest> accumulated_requests_;
  // Must be last member.
  base::WeakPtrFactory<RequestMonitoringNavigationBrowserTest> weak_factory_{
      this};
};

// Helper for waiting until the main frame of |web_contents| has loaded
// |expected_url| (and all subresources have finished loading).
class WebContentsLoadFinishedWaiter : public WebContentsObserver {
 public:
  WebContentsLoadFinishedWaiter(WebContents* web_contents,
                                const GURL& expected_url)
      : WebContentsObserver(web_contents), expected_url_(expected_url) {
    EXPECT_TRUE(web_contents != nullptr);
  }

  void Wait() { loop_.Run(); }

 private:
  void DidFinishLoad(RenderFrameHost* render_frame_host,
                     const GURL& url) override {
    bool is_main_frame = !render_frame_host->GetParent();
    if (url == expected_url_ && is_main_frame) {
      loop_.Quit();
    }
  }

  GURL expected_url_;
  base::RunLoop loop_;
};

}  // namespace

// Check that NavigationController::LoadURLParams::extra_headers are not copied
// to subresource requests.
IN_PROC_BROWSER_TEST_P(RequestMonitoringNavigationBrowserTest,
                       ExtraHeadersVsSubresources) {
  GURL page_url = embedded_test_server()->GetURL("/page_with_image.html");
  GURL image_url = embedded_test_server()->GetURL("/blank.jpg");

  // Navigate via LoadURLWithParams (setting |extra_headers| field).
  WebContentsLoadFinishedWaiter waiter(shell()->web_contents(), page_url);
  NavigationController::LoadURLParams load_url_params(page_url);
  load_url_params.extra_headers =
      "X-ExtraHeadersVsSubresources: 1\n"
      "X-2ExtraHeadersVsSubresources: 2";
  shell()->web_contents()->GetController().LoadURLWithParams(load_url_params);
  waiter.Wait();
  EXPECT_EQ(page_url, shell()->web_contents()->GetLastCommittedURL());

  // Verify that the extra header was present for the page.
  const net::test_server::HttpRequest* page_request =
      FindAccumulatedRequest(page_url);
  ASSERT_TRUE(page_request);
  EXPECT_THAT(page_request->headers,
              testing::Contains(testing::Key("X-ExtraHeadersVsSubresources")));
  EXPECT_THAT(page_request->headers,
              testing::Contains(testing::Key("X-2ExtraHeadersVsSubresources")));

  // Verify that the extra header was NOT present for the subresource.
  const net::test_server::HttpRequest* image_request =
      FindAccumulatedRequest(image_url);
  ASSERT_TRUE(image_request);
  EXPECT_THAT(image_request->headers,
              testing::Not(testing::Contains(
                  testing::Key("X-ExtraHeadersVsSubresources"))));
  EXPECT_THAT(image_request->headers,
              testing::Not(testing::Contains(
                  testing::Key("X-2ExtraHeadersVsSubresources"))));
}

// Test that a same document navigation does not lead to the deletion of the
// NavigationHandle for an ongoing different document navigation.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       SameDocumentNavigationDoesntDeleteNavigationHandle) {
  const GURL kURL1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kPushStateURL =
      embedded_test_server()->GetURL("/title1.html#fragment");
  const GURL kURL2 = embedded_test_server()->GetURL("/title2.html");

  // Navigate to the initial page.
  EXPECT_TRUE(NavigateToURL(shell(), kURL1));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_FALSE(root->navigation_request());

  // Start navigating to the second page.
  TestNavigationManager manager(shell()->web_contents(), kURL2);
  NavigationHandleCommitObserver navigation_observer(shell()->web_contents(),
                                                     kURL2);
  shell()->web_contents()->GetController().LoadURL(
      kURL2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(manager.WaitForRequestStart());

  // This should create a NavigationHandle.
  NavigationRequest* request = root->navigation_request();
  EXPECT_TRUE(request);

  // The current page does a PushState.
  NavigationHandleCommitObserver push_state_observer(shell()->web_contents(),
                                                     kPushStateURL);
  std::string push_state =
      JsReplace("history.pushState({}, 'title 1', $1);", kPushStateURL);
  EXPECT_TRUE(ExecJs(shell()->web_contents(), push_state));
  NavigationEntry* last_committed =
      shell()->web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(last_committed);
  EXPECT_EQ(kPushStateURL, last_committed->GetURL());

  EXPECT_TRUE(push_state_observer.has_committed());
  EXPECT_TRUE(push_state_observer.was_same_document());
  EXPECT_TRUE(push_state_observer.was_renderer_initiated());

  // This shouldn't affect the ongoing navigation.
  EXPECT_TRUE(root->navigation_request());
  EXPECT_EQ(request, root->navigation_request());

  // Let the navigation finish. It should commit successfully.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  last_committed =
      shell()->web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(last_committed);
  EXPECT_EQ(kURL2, last_committed->GetURL());

  EXPECT_TRUE(navigation_observer.has_committed());
  EXPECT_FALSE(navigation_observer.was_same_document());
  EXPECT_FALSE(navigation_observer.was_renderer_initiated());
}

// Tests that a same document browser-initiated navigation is properly reported
// by the NavigationHandle.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       SameDocumentBrowserInitiated) {
  const GURL kURL = embedded_test_server()->GetURL("/title1.html");
  const GURL kFragmentURL =
      embedded_test_server()->GetURL("/title1.html#fragment");

  // Navigate to the initial page.
  EXPECT_TRUE(NavigateToURL(shell(), kURL));

  // Do a browser-initiated fragment navigation.
  NavigationHandleCommitObserver handle_observer(shell()->web_contents(),
                                                 kFragmentURL);
  EXPECT_TRUE(NavigateToURL(shell(), kFragmentURL));

  EXPECT_TRUE(handle_observer.has_committed());
  EXPECT_TRUE(handle_observer.was_same_document());
  EXPECT_FALSE(handle_observer.was_renderer_initiated());
}

// Tests that a 204 response to a browser-initiated navigation does not result
// in a new NavigationEntry being committed.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, 204Navigation) {
  const GURL kURL = embedded_test_server()->GetURL("/title1.html");
  const GURL kURL204 = embedded_test_server()->GetURL("/page204.html");

  // Navigate to the initial page.
  EXPECT_TRUE(NavigateToURL(shell(), kURL));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(kURL, entry->GetURL());
  EXPECT_EQ(1, controller.GetEntryCount());

  // Do a 204 navigation.
  EXPECT_FALSE(NavigateToURL(shell(), kURL204));

  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(kURL, entry->GetURL());
  EXPECT_EQ(1, controller.GetEntryCount());
}

// Tests that navigating to an empty URL or a URL that never commits will
// have appropriate origins. Empty URL navigations are not currently known to
// be possible, but it used to be possible and caused crashes because the
// initiator origin wasn't inherited correctly with empty URLs. See also
// https://crbug.com/1240138.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, NavigateToEmptyURL) {
  GURL url_1 = embedded_test_server()->GetURL("/title1.html");
  GURL url_2 = embedded_test_server()->GetURL("/title2.html");
  GURL url_204 = embedded_test_server()->GetURL("/page204.html");

  // Navigate to the initial page.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(url_1, entry->GetURL());
  EXPECT_EQ(1, controller.GetEntryCount());

  // Navigate (browser-initiated) to an empty URL, which will get rewritten to
  // about:blank and commit an opaque origin.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(), GURL("about:blank")));

  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(GURL(url::kAboutBlankURL), entry->GetURL());
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_TRUE(
      contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin().opaque());
  EXPECT_FALSE(entry->root_node()->frame_entry->initiator_origin().has_value());

  // Trying to navigate to an empty URL through setting the location would fail
  // because it's not a valid URL and it's stopped in the renderer.
  //
  // Note: this is only true for things like about:blank and data URLs.
  EXPECT_THAT(EvalJs(shell(), "try { location = ''; } catch (e) { e.message; }")
                  .ExtractString(),
              HasSubstr("'' is not a valid URL."));
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());
  EXPECT_EQ(2, controller.GetEntryCount());

  // Navigate another URL.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  const url::Origin& opener_origin =
      contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  {
    // Pop open a new window which won't commit a navigation, so it will stay at
    // the initial NavigationEntry.
    Shell* new_shell = OpenWindow(contents(), url_204);
    WebContentsImpl* new_contents =
        static_cast<WebContentsImpl*>(new_shell->web_contents());

    // The initial NavigationEntry will have an empty URL, and its initiator
    // origin is set to the origin of the document that opened the window.
    NavigationControllerImpl& new_controller = new_contents->GetController();
    EXPECT_EQ(1, new_controller.GetEntryCount());
    entry = new_controller.GetLastCommittedEntry();
    EXPECT_TRUE(entry->IsInitialEntry());
    EXPECT_EQ(GURL(), entry->GetURL());

    scoped_refptr<FrameNavigationEntry> frame_entry =
        entry->root_node()->frame_entry.get();
    ASSERT_TRUE(frame_entry->initiator_origin().has_value());
    EXPECT_EQ(opener_origin, frame_entry->initiator_origin().value());
    EXPECT_EQ(opener_origin, new_shell->web_contents()
                                 ->GetPrimaryMainFrame()
                                 ->GetLastCommittedOrigin());
  }

  {
    // Pop open a new window which won't commit a navigation, but this time use
    // the 'noopener' option, which will cause the navigation to be triggered
    // by the browser.
    ShellAddedObserver new_shell_observer;
    EXPECT_TRUE(
        ExecJs(shell(), "window.open('/page204.html', '_blank', 'noopener');"));
    Shell* new_shell = new_shell_observer.GetShell();

    // The initial NavigationEntry will have an empty URL, and its initiator
    // origin is set to an opaque origin (not the opener's origin).
    NavigationControllerImpl& new_controller =
        static_cast<WebContentsImpl*>(new_shell->web_contents())
            ->GetController();
    EXPECT_EQ(1, new_controller.GetEntryCount());
    entry = new_controller.GetLastCommittedEntry();
    EXPECT_TRUE(entry->IsInitialEntry());
    EXPECT_EQ(GURL(), entry->GetURL());

    scoped_refptr<FrameNavigationEntry> frame_entry =
        entry->root_node()->frame_entry.get();
    ASSERT_TRUE(frame_entry->initiator_origin().has_value());
    EXPECT_NE(opener_origin, frame_entry->initiator_origin().value());
    EXPECT_TRUE(frame_entry->initiator_origin()->opaque());
    EXPECT_EQ(frame_entry->initiator_origin().value(),
              new_shell->web_contents()
                  ->GetPrimaryMainFrame()
                  ->GetLastCommittedOrigin());
  }

  {
    // Pop open another window, this time to an empty URL and with the
    // 'noopener' option. This navigation will go through the browser, but the
    // initiator origin is still set to the opener's origin.
    ShellAddedObserver new_shell_observer;
    EXPECT_TRUE(ExecJs(shell(), "window.open('', '_blank', 'noopener');"));
    Shell* new_shell = new_shell_observer.GetShell();
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

    NavigationControllerImpl& new_controller =
        static_cast<WebContentsImpl*>(new_shell->web_contents())
            ->GetController();
    EXPECT_EQ(1, new_controller.GetEntryCount());
    entry = new_controller.GetLastCommittedEntry();
    EXPECT_FALSE(entry->IsInitialEntry());
    EXPECT_EQ(GURL(url::kAboutBlankURL), entry->GetURL());

    scoped_refptr<FrameNavigationEntry> frame_entry =
        entry->root_node()->frame_entry.get();
    ASSERT_TRUE(frame_entry->initiator_origin().has_value());
    EXPECT_EQ(opener_origin, frame_entry->initiator_origin().value());
    EXPECT_EQ(opener_origin, new_shell->web_contents()
                                 ->GetPrimaryMainFrame()
                                 ->GetLastCommittedOrigin());
  }
}

// Tests that stopping a load clears the pending navigation entry.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, StopDuringLoad) {
  // Load an initial page since the behavior differs for the first entry.
  GURL start_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
  GURL slow_url = embedded_test_server()->GetURL("/slow?60");
  shell()->LoadURL(slow_url);
  shell()->web_contents()->Stop();

  NavigationController& controller = shell()->web_contents()->GetController();
  ASSERT_EQ(controller.GetPendingEntry(), nullptr);
}

// Tests that reloading a page that has no title doesn't inherit the title from
// the previous version of the page.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, ReloadDoesntKeepTitle) {
  NavigationController& controller = shell()->web_contents()->GetController();
  GURL start_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  GURL intermediate_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  std::u16string title = u"title";

  // Reload from the browser side.
  {
    EXPECT_TRUE(NavigateToURL(shell(), start_url));

    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(entry->GetTitle().empty());
    entry->SetTitle(title);

    controller.Reload(ReloadType::NORMAL, false);
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

    EXPECT_TRUE(entry->GetTitle().empty());
  }

  // Load an unrelated page; this disconnects these two tests.
  EXPECT_TRUE(NavigateToURL(shell(), intermediate_url));

  // Reload from the renderer side.
  {
    EXPECT_TRUE(NavigateToURL(shell(), start_url));

    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(entry->GetTitle().empty());
    entry->SetTitle(title);

    TestNavigationObserver reload_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(shell(), "location.reload()"));
    reload_observer.Wait();

    EXPECT_TRUE(entry->GetTitle().empty());
  }

  // Load an unrelated page; this disconnects these two tests.
  EXPECT_TRUE(NavigateToURL(shell(), intermediate_url));

  // "Reload" by loading the same page again.
  {
    EXPECT_TRUE(NavigateToURL(shell(), start_url));

    NavigationEntry* entry1 = controller.GetLastCommittedEntry();
    EXPECT_TRUE(entry1->GetTitle().empty());
    entry1->SetTitle(title);

    EXPECT_TRUE(NavigateToURL(shell(), start_url));
    NavigationEntry* entry2 = controller.GetLastCommittedEntry();

    EXPECT_EQ(entry1, entry2);
    EXPECT_TRUE(entry1->GetTitle().empty());
  }
}

// Verify that session history navigations (back/forward) correctly hit the
// cache instead of going to the server. The test loads a page with no-cache
// header, stops the server, and goes back expecting successful navigation.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       HistoryNavigationUsesCache) {
  GURL no_cache_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_no_cache_header.html"));
  GURL regular_url(embedded_test_server()->GetURL("/title2.html"));

  NavigationController& controller = shell()->web_contents()->GetController();

  EXPECT_TRUE(NavigateToURL(shell(), no_cache_url));
  EXPECT_TRUE(NavigateToURL(shell(), regular_url));
  EXPECT_EQ(2, controller.GetEntryCount());

  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  TestNavigationObserver back_observer(shell()->web_contents());
  controller.GoBack();
  back_observer.Wait();

  EXPECT_TRUE(back_observer.last_navigation_succeeded());
}

// Test to verify that navigating to a blocked URL does not result in a
// NavigationEntry that allows the navigation to succeed when using a history
// navigation. See https://crbug.com/723796.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       VerifyBlockedErrorPageURL_SessionHistory) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  // Navigate to a URL that is blocked, which results in an error page.
  GURL blocked_url(embedded_test_server()->GetURL("/blocked.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(blocked_url,
                                                   net::ERR_BLOCKED_BY_CLIENT);
  EXPECT_FALSE(NavigateToURL(shell(), blocked_url));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(PAGE_TYPE_ERROR, controller.GetLastCommittedEntry()->GetPageType());

  // Navigate to a new document, then go back in history trying to load the
  // blocked URL.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  TestNavigationObserver back_load_observer(shell()->web_contents());
  controller.GoBack();
  back_load_observer.Wait();

  // The expectation is that the blocked URL is present in the NavigationEntry,
  // and shows up in both GetURL and GetVirtualURL.
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(blocked_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(blocked_url, controller.GetLastCommittedEntry()->GetVirtualURL());
}

// Verifies that unsafe redirects to javascript: URLs are canceled and don't
// make a spoof possible. Ideally they would create an error page, but some
// extensions rely on them being silently blocked. See https://crbug.com/935175
// and https://crbug.com/941653.
// This test is flaky on Linux : http://crbug.com/1223051
#if BUILDFLAG(IS_LINUX)
#define MAYBE_JavascriptRedirectSilentlyCanceled \
  DISABLED_JavascriptRedirectSilentlyCanceled
#else
#define MAYBE_JavascriptRedirectSilentlyCanceled \
  JavascriptRedirectSilentlyCanceled
#endif
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       MAYBE_JavascriptRedirectSilentlyCanceled) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  // Navigating to a URL that redirects to a javascript: URL doesn't create an
  // error page; the navigation is simply ignored. Check the pending URL is not
  // left in the address bar.
  GURL redirect_to_unsafe_url(
      embedded_test_server()->GetURL("/server-redirect?javascript:Hello!"));
  EXPECT_FALSE(NavigateToURL(shell(), redirect_to_unsafe_url));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(PAGE_TYPE_NORMAL,
            controller.GetLastCommittedEntry()->GetPageType());
  EXPECT_EQ(controller.GetVisibleEntry(), controller.GetLastCommittedEntry());
  EXPECT_EQ(start_url, controller.GetVisibleEntry()->GetURL());
}

// Verifies that redirecting to a blocked URL and going back does not allow a
// URL spoof.  See https://crbug.com/777419.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       PreventSpoofFromBlockedRedirect) {
  GURL url1 = embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/simple_page_1.html");
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Pop open a new window.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(root, "var w = window.open()"));
  Shell* new_shell = new_shell_observer.GetShell();
  ASSERT_NE(new_shell->web_contents(), shell()->web_contents());

  // Navigate it to a cross-site URL that redirects to a data: URL.  Since it is
  // an unsafe redirect, it will result in a blocked navigation and error page.
  GURL redirect_to_data_url(
      embedded_test_server()->GetURL("/server-redirect?data:text/html,Hello!"));
  TestNavigationObserver nav_observer(new_shell->web_contents(), 1);
  EXPECT_TRUE(
      ExecJs(root, "w.location.href = '" + redirect_to_data_url.spec() + "';"));
  nav_observer.WaitForNavigationFinished();
  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      new_shell->web_contents()->GetController());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(redirect_to_data_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(PAGE_TYPE_ERROR, controller.GetLastCommittedEntry()->GetPageType());

  // Navigate to a new document, then go back in history trying to load the
  // blocked URL.
  EXPECT_TRUE(NavigateToURL(new_shell, url1));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url1, controller.GetLastCommittedEntry()->GetURL());
  TestNavigationObserver back_load_observer(new_shell->web_contents());
  controller.GoBack();
  back_load_observer.Wait();
  EXPECT_EQ(redirect_to_data_url, controller.GetLastCommittedEntry()->GetURL());

  // The opener should not be able to script the page, which should be another
  // error message and not a blank page.
  std::string result = EvalJs(shell(),
                              "try {\n"
                              "  w.document.body.innerHTML;\n"
                              "} catch (e) {\n"
                              "  e.toString();\n"
                              "}")
                           .ExtractString();
  DLOG(INFO) << "Result: " << result;
  EXPECT_THAT(result,
              ::testing::MatchesRegex("SecurityError: Failed to read a named "
                                      "property 'document' from 'Window': "
                                      "Blocked a frame with origin "
                                      "\"http://a.com:\\d+\" from accessing a "
                                      "cross-origin frame."));
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       SameDocumentNavAfterJavaScriptURL) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Navigate to |start_url|.
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
  EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  const int64_t start_dsn = controller.GetLastCommittedEntry()
                                ->GetFrameEntry(root)
                                ->document_sequence_number();

  // Do a javascript: URL "navigation", which will create a new document but
  // won't send anything to the browser.
  EXPECT_TRUE(ExecJs(root, R"(window.location = 'javascript:"foo"';)"));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(start_url, root->current_url());
  EXPECT_EQ("foo", EvalJs(shell(), "document.body.innerHTML"));
  EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
  EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
  EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                           ->GetFrameEntry(root)
                           ->document_sequence_number());

  // Do a same-document renderer-initiated fragment navigation, which should
  // retain the HTTP method and status code.
  GURL fragment_url = embedded_test_server()->GetURL("/title1.html#bar");
  {
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "location.href='#bar';"));
    capturer.Wait();
    EXPECT_NE(start_url, root->current_url());
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
    EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
    EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                             ->GetFrameEntry(root)
                             ->document_sequence_number());
  }

  // Go back. This should be a same-document navigation and retain the HTTP
  // method and status code.
  {
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.go(-1);"));
    capturer.Wait();
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ(start_url, root->current_url());
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
    EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
    EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                             ->GetFrameEntry(root)
                             ->document_sequence_number());
  }
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       BrowserInitiatedSameDocumentNavAfterJavaScriptURL) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Navigate to |start_url|.
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
  EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  const int64_t start_dsn = controller.GetLastCommittedEntry()
                                ->GetFrameEntry(root)
                                ->document_sequence_number();

  // Do a javascript: URL "navigation", which will create a new document but
  // won't send anything to the browser.
  EXPECT_TRUE(ExecJs(root, R"(window.location = 'javascript:"foo"';)"));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(start_url, root->current_url());
  EXPECT_EQ("foo", EvalJs(shell(), "document.body.innerHTML"));
  EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
  EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
  EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                           ->GetFrameEntry(root)
                           ->document_sequence_number());

  // Do a same-document browser-initiated fragment navigation, which should
  // retain the HTTP method and status code.
  GURL fragment_url = embedded_test_server()->GetURL("/title1.html#bar");
  {
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), fragment_url));
    capturer.Wait();
    EXPECT_EQ(fragment_url, root->current_url());
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
    EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
    EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                             ->GetFrameEntry(root)
                             ->document_sequence_number());
  }

  // Go back. This should be a same-document navigation and retain the HTTP
  // method and status code.
  {
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.go(-1);"));
    capturer.Wait();
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ(start_url, root->current_url());
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
    EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
    EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                             ->GetFrameEntry(root)
                             ->document_sequence_number());
  }
}

// Does a same-document navigation on an iframe after its document gets
// replaced.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       SameDocumentNavAfterDocumentReplaceChild) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Navigate to a page with an iframe.
  GURL main_url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
  EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  FrameTreeNode* iframe = root->child_at(0);
  const int64_t start_dsn = controller.GetLastCommittedEntry()
                                ->GetFrameEntry(iframe)
                                ->document_sequence_number();
  GURL iframe_url = iframe->current_url();

  // Create a new document with document.implementation.createHTMLDocument that
  // will replace the iframe's document, but won't notify the browser.
  EXPECT_TRUE(ExecJs(root, R"(
    let newDoc = document.implementation.createHTMLDocument();
    newDoc.body.innerHTML = "foo";
    let frame = document.getElementById("test_iframe");
    let destDocument = frame.contentDocument;
    let newNode = destDocument.importNode(newDoc.documentElement, true);
    destDocument.replaceChild(newNode, destDocument.documentElement);
  )"));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(iframe_url, iframe->current_url());
  EXPECT_EQ("foo", EvalJs(iframe, "document.body.innerHTML"));
  EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
  EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
  EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                           ->GetFrameEntry(iframe)
                           ->document_sequence_number());

  // Do a same-document renderer-initiated fragment navigation, which should
  // retain the HTTP method and status code.
  GURL fragment_url = embedded_test_server()->GetURL("/title1.html#bar");
  {
    FrameNavigateParamsCapturer capturer(iframe);
    EXPECT_TRUE(ExecJs(iframe, "location.href='#bar';"));
    capturer.Wait();
    EXPECT_EQ(iframe_url.spec() + "#bar", iframe->current_url().spec());
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
    EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
    EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                             ->GetFrameEntry(iframe)
                             ->document_sequence_number());
  }

  // Go back. This should be a same-document navigation and retain the HTTP
  // method and status code.
  {
    FrameNavigateParamsCapturer capturer(iframe);
    EXPECT_TRUE(ExecJs(iframe, "history.go(-1);"));
    capturer.Wait();
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ(iframe_url, iframe->current_url());
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
    EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
    EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                             ->GetFrameEntry(iframe)
                             ->document_sequence_number());
  }
}

// Does a same-document navigation on an iframe after its document gets
// replaced.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       SameDocumentNavAfterXSL) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Navigate to an XML page.
  GURL start_url(embedded_test_server()->GetURL("/permissions-policy.xml"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
  EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  const int64_t start_dsn = controller.GetLastCommittedEntry()
                                ->GetFrameEntry(root)
                                ->document_sequence_number();
  // Wait until the page has fully loaded, which will trigger an XSLT document
  // change in the renderer.
  EXPECT_TRUE(WaitForLoadStop(contents()));

  // Do a same-document browser-initiated fragment navigation, which should
  // retain the HTTP method and status code.
  GURL fragment_url =
      embedded_test_server()->GetURL("/permissions-policy.xml#bar");
  {
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), fragment_url));
    capturer.Wait();
    EXPECT_EQ(fragment_url, root->current_url());
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
    EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
    EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                             ->GetFrameEntry(root)
                             ->document_sequence_number());
  }

  // Go back. This should be a same-document navigation and retain the HTTP
  // method and status code.
  {
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.go(-1);"));
    capturer.Wait();
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ(start_url, root->current_url());
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
    EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
    EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                             ->GetFrameEntry(root)
                             ->document_sequence_number());
  }
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       SameDocumentNavAfterJavaScriptURLOn404Page) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Navigate to |start_url|.
  GURL start_url(embedded_test_server()->GetURL("/page404.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
  EXPECT_EQ(404, contents()->GetPrimaryMainFrame()->last_http_status_code());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  const int64_t start_dsn = controller.GetLastCommittedEntry()
                                ->GetFrameEntry(root)
                                ->document_sequence_number();

  // Do a javascript: URL "navigation", which will create a new document but
  // won't send anything to the browser.
  EXPECT_TRUE(ExecJs(root, R"(window.location = 'javascript:"foo"';)"));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(start_url, root->current_url());
  EXPECT_EQ("foo", EvalJs(shell(), "document.body.innerHTML"));
  EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
  EXPECT_EQ(404, contents()->GetPrimaryMainFrame()->last_http_status_code());
  EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                           ->GetFrameEntry(root)
                           ->document_sequence_number());

  // Do a same-document renderer-initiated fragment navigation, which should
  // retain the HTTP method and status code.
  GURL fragment_url = embedded_test_server()->GetURL("/page404.html#bar");
  {
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "location.href='#bar';"));
    capturer.Wait();
    EXPECT_NE(start_url, root->current_url());
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
    EXPECT_EQ(404, contents()->GetPrimaryMainFrame()->last_http_status_code());
    EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                             ->GetFrameEntry(root)
                             ->document_sequence_number());
  }

  // Go back. This should be a same-document navigation and retain the HTTP
  // method and status code.
  {
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.go(-1);"));
    capturer.Wait();
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ(start_url, root->current_url());
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
    EXPECT_EQ(404, contents()->GetPrimaryMainFrame()->last_http_status_code());
    EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                             ->GetFrameEntry(root)
                             ->document_sequence_number());
  }
}

// Same-document navigations can sometimes succeed but then later be blocked by
// policy (e.g., X-Frame-Options) after a page is restored or reloaded.  Ensure
// that navigating back from a newly blocked URL in a subframe is not treated as
// same-document, even if it had been same-document originally.
// See https://crbug.com/765291.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       BackSameDocumentAfterBlockedSubframe) {
  // The test assumes the previous iframe gets deleted after navigation and
  // later recreated on history navigations. Disable back/forward cache to
  // ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // 1) Navigate to a page with an iframe.
  GURL start_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe_simple.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  GURL original_child_url = root->child_at(0)->current_url();

  // 2) pushState to a URL that will be blocked by XFO if loaded from scratch.
  GURL x_frame_options_deny_url =
      embedded_test_server()->GetURL("/x-frame-options-deny.html");
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    std::string pushStateToXfo =
        "history.pushState({}, '', '/x-frame-options-deny.html')";
    EXPECT_TRUE(ExecJs(root->child_at(0), pushStateToXfo));
    capturer.Wait();
    EXPECT_EQ(x_frame_options_deny_url, root->child_at(0)->current_url());
    EXPECT_TRUE(capturer.is_same_document());
  }

  // 3) Navigate the main frame to another page.
  GURL new_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), new_url));

  // 4) Go back, causing the subframe to be blocked by XFO.
  {
    TestNavigationObserver observer(shell()->web_contents());
    controller.GoBack();
    observer.Wait();
    EXPECT_EQ(start_url, root->current_url());
    EXPECT_EQ(x_frame_options_deny_url, root->child_at(0)->current_url());
    EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE, observer.last_net_error_code());
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  // 5) Go back again.  This would have been same-document if the prior
  // navigation had succeeded, but we did a cross-document navigation instead
  // because the previous page is an error page.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    TestNavigationObserver observer(shell()->web_contents());
    controller.GoBack();
    capturer.Wait();
    observer.Wait();
    EXPECT_FALSE(capturer.is_same_document());
    EXPECT_EQ(start_url, root->current_url());
    EXPECT_EQ(original_child_url, root->child_at(0)->current_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(net::OK, observer.last_net_error_code());
  }

  // 6) Go forward two steps. This would load the page from step 3.
  {
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.go(2)"));
    capturer.Wait();
    EXPECT_EQ(new_url, root->current_url());
    EXPECT_FALSE(capturer.is_same_document());
  }

  // 7) Go back two steps. This would load the page from step 1, with the iframe
  // loaded to the original URL from step 1.
  {
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.go(-2)"));
    capturer.Wait();
    EXPECT_EQ(start_url, root->current_url());
    EXPECT_EQ(original_child_url, root->child_at(0)->current_url());
    EXPECT_FALSE(capturer.is_same_document());
  }

  // 8) Go forward one step. This would do a same document navigation, with the
  // iframe still loaded to the original URL from step 1, but the URL is updated
  // to the XFO URL.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    TestNavigationObserver observer(shell()->web_contents());
    controller.GoForward();
    capturer.Wait();
    observer.Wait();
    EXPECT_EQ(start_url, root->current_url());
    EXPECT_EQ(x_frame_options_deny_url, root->child_at(0)->current_url());
    if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(
            /*in_main_frame=*/false)) {
      EXPECT_TRUE(capturer.is_same_document());
      EXPECT_TRUE(observer.last_navigation_succeeded());
      EXPECT_EQ(net::OK, observer.last_net_error_code());
    } else {
      EXPECT_FALSE(capturer.is_same_document());
      EXPECT_FALSE(observer.last_navigation_succeeded());
      EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE, observer.last_net_error_code());
    }
  }

  // Check that the renderer is still alive.
  EXPECT_TRUE(ExecJs(root->child_at(0), "console.log('Success');"));
}

// Similar to BackSameDocumentAfterBlockedSubframe but does the navigation on a
// main frame instead (and does a 404 instead of XFO error).
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       BackSameDocumentAfter404MainFrame) {
  // This test expects going back to trigger a new page load and fetch a URL
  // (which would fail with a 404 error). Disable back/forward cache to ensure
  // that it doesn't happen.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  // 1) Navigate to |start_url|.
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 2) pushState to a URL that will 404 & result in Chrome's error page if
  // loaded from scratch.
  GURL error_url = embedded_test_server()->GetURL("/empty404.html");
  {
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.pushState({}, '', '/empty404.html')"));
    capturer.Wait();
    EXPECT_EQ(error_url, root->current_url());
    EXPECT_TRUE(capturer.is_same_document());
  }

  // 3) Navigate the main frame to another page.
  GURL new_url(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), new_url));

  // 4) Go back. This will 404 so we will show an error page.
  {
    TestNavigationObserver observer(shell()->web_contents());
    controller.GoBack();
    observer.Wait();
    EXPECT_EQ(error_url, root->current_url());
    EXPECT_EQ(net::ERR_HTTP_RESPONSE_CODE_FAILURE,
              observer.last_net_error_code());
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  // 5) Go back again.  This would have been same-document if the prior
  // navigation had succeeded, but we did a cross-document navigation instead
  // because the previous page is an error page.
  {
    FrameNavigateParamsCapturer capturer(root);
    TestNavigationObserver observer(shell()->web_contents());
    controller.GoBack();
    capturer.Wait();
    observer.Wait();
    EXPECT_EQ(start_url, root->current_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(net::OK, observer.last_net_error_code());
    EXPECT_FALSE(capturer.is_same_document());
  }

  // Check that the renderer is still alive.
  EXPECT_TRUE(ExecJs(root, "console.log('Success');"));
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       HistoryAPIHistoryNavigation) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1) Navigate to |start_url|.
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  const int64_t start_dsn = controller.GetLastCommittedEntry()
                                ->GetFrameEntry(root)
                                ->document_sequence_number();

  // 2) pushState a different-document URL. This will be classified as a
  // same-document navigation and will keep the previous Document Sequence
  // Number.
  GURL push_state_url(embedded_test_server()->GetURL("/title2.html"));
  {
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.pushState({}, '', '/title2.html')"));
    capturer.Wait();
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                             ->GetFrameEntry(root)
                             ->document_sequence_number());
  }

  // 3) Navigate to another page. This will be classified as a cross-document
  // navigation and will change the Document Sequence Number.
  GURL end_url(embedded_test_server()->GetURL("/title3.html"));
  EXPECT_TRUE(NavigateToURL(shell(), end_url));
  EXPECT_NE(start_dsn, controller.GetLastCommittedEntry()
                           ->GetFrameEntry(root)
                           ->document_sequence_number());

  // 4) Go back. This will load the URL from pushState. This will be classified
  // as a cross-document history navigation, and will use the Document Sequence
  // Number from the FrameNavigationEntry.
  {
    FrameNavigateParamsCapturer capturer(root);
    TestNavigationObserver observer(shell()->web_contents());
    controller.GoBack();
    capturer.Wait();
    observer.Wait();
    EXPECT_FALSE(capturer.is_same_document());
    EXPECT_EQ(push_state_url, root->current_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                             ->GetFrameEntry(root)
                             ->document_sequence_number());
    // Set a variable in this document.
    EXPECT_TRUE(ExecJs(shell(), "var foo = 42;"));
  }

  // 5) Go back. This will load the starting URL but it's still on the same
  // document as the one loaded in the last navigation, due to the
  // FrameNavigationEntry having the same Document Sequence Number as the
  // previous FrameNavigationEntry.
  {
    FrameNavigateParamsCapturer capturer(root);
    TestNavigationObserver observer(shell()->web_contents());
    controller.GoBack();
    capturer.Wait();
    observer.Wait();
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_EQ(start_url, root->current_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(start_dsn, controller.GetLastCommittedEntry()
                             ->GetFrameEntry(root)
                             ->document_sequence_number());
    // The variable set in the document at step 4 can be accessed because we did
    // a same-document navigation.
    EXPECT_EQ(42, EvalJs(shell(), "foo"));
  }
}

// If the main frame does a load, it should not be reported as a subframe
// navigation. This used to occur in the following case:
// 1. You're on a site with frames.
// 2. You do a subframe navigation. This was stored with transition type
//    MANUAL_SUBFRAME.
// 3. You navigate to some non-frame site.
// 4. You navigate back to the page from step 2. Since it was initially
//    MANUAL_SUBFRAME, it will be that same transition type here.
// We don't want that, because any navigation that changes the toplevel frame
// should be tracked as a toplevel navigation (this allows us to update the URL
// bar, etc).
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       GoBackToManualSubFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(1, controller.GetEntryCount());

  {
    // Iframe initial load.
    LoadCommittedCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    NavigateFrameToURL(root->child_at(0), frame_url);
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    EXPECT_EQ(1, controller.GetEntryCount());
  }

  {
    // Iframe manual navigation.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    NavigateFrameToURL(root->child_at(0), frame_url);
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.navigation_type());
    EXPECT_EQ(2, controller.GetEntryCount());
  }

  {
    // Main frame navigation.
    FrameNavigateParamsCapturer capturer(root);
    GURL main_url_2(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    NavigateFrameToURL(root, main_url_2);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(), ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(3, controller.GetEntryCount());
  }

  {
    // Check the history before going back.
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        controller.GetEntryAtIndex(0)->GetTransitionType(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    // TODO(creis, arthursonzogni): The correct PageTransition is still an open
    // question. Maybe PAGE_TRANSITION_MANUAL_SUBFRAME is more appropriate.
    // Please see https://crbug.com/740461.
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        controller.GetEntryAtIndex(1)->GetTransitionType(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        controller.GetEntryAtIndex(2)->GetTransitionType(),
        ui::PAGE_TRANSITION_LINK));
  }

  {
    // Back.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
  }

  {
    // Check the history again.
    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        controller.GetEntryAtIndex(0)->GetTransitionType(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        controller.GetEntryAtIndex(1)->GetTransitionType(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        controller.GetEntryAtIndex(2)->GetTransitionType(),
        ui::PAGE_TRANSITION_LINK));
  }
}

// Regression test for https://crbug.com/845923.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       GoBackFromCrossSiteSubFrame) {
  // Navigate to a page with a cross-site frame.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  GURL initial_subframe_url =
      root->child_at(0)->current_frame_host()->GetLastCommittedURL();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());

  // Navigate the subframe to another cross-site location
  // (this prepares for executing history.back() in a later step).
  GURL final_subframe_url =
      embedded_test_server()->GetURL("b.com", "/title1.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), final_subframe_url));
  EXPECT_EQ(final_subframe_url,
            root->child_at(0)->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());

  // Execute |history.back()| in the subframe.
  TestNavigationObserver nav_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(root->child_at(0), "history.back()"));
  nav_observer.Wait();
  EXPECT_EQ(initial_subframe_url,
            root->child_at(0)->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       HashNavigationVsBeforeUnloadEvent) {
  GURL main_url(embedded_test_server()->GetURL("/title1.html"));
  GURL hash_url(embedded_test_server()->GetURL("/title1.html#hash"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_TRUE(ExecJs(shell(),
                     R"( window.addEventListener("beforeunload", function(e) {
              domAutomationController.send("beforeunload");
          });
          window.addEventListener("unload", function(e) {
              domAutomationController.send("unload");
          });
      )"));

  DOMMessageQueue message_queue(shell()->web_contents());
  std::vector<std::string> messages;
  std::string message;
  EXPECT_TRUE(NavigateToURL(shell(), hash_url));
  while (message_queue.PopMessage(&message))
    messages.push_back(message);

  // Verify that none of "beforeunload", "unload" events fired.
  EXPECT_THAT(messages, testing::IsEmpty());
}

// This test helps verify that the browser does not retain history entries
// for removed frames *if* the removed frame was created by a script.
// Such frames get a fresh, random, unique name every time they are created
// or recreated and therefore in such case will never match previous history
// entries.  See also https://crbug.com/784356.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       PruningOfEntriesForDynamicFrames_ChildRemoved) {
  GURL main_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Repeatedly create and remove a frame from a script.
  std::string script = R"(
        (async () => {
           for (let i = 0; i < 5; i++) {
             // Create and remove an iframe.
             let iframe = document.createElement('iframe');
             document.body.appendChild(iframe);
             document.body.removeChild(iframe);
             // Let the message loop run (this works in an async function).
             await new Promise(resolve => setTimeout(resolve, 0));
           }
           return 'done-with-test';
        })(); )";
  EXPECT_EQ("done-with-test", EvalJs(shell(), script));

  // Grab the last committed entry.
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());

  // Verify that the number of FrameNavigationEntries stayed low (i.e. that we
  // do not retain history entries for the 5 frames removed by the test).
  EXPECT_EQ(0U, entry->root_node()->children.size());

  // Sanity check - there are no children in the frame tree.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(0U, root->child_count());
}

// This test helps verify that the browser does not retain history entries
// for removed frames *if* the removed frame was created by a script.
// Such frames get a fresh, random, unique name every time they are created
// or recreated and therefore in such case will never match previous history
// entries.  See also https://crbug.com/784356.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       PruningOfEntriesForDynamicFrames_ParentNavigatedAway) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/page_with_iframe_simple.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Add 5 dynamic subframes to |frame|.
  RenderFrameHost* frame = ChildFrameAt(current_main_frame_host(), 0);
  std::string script = R"(
        for (var i = 0; i < 5; i++) {
          var iframe = document.createElement("iframe");
          document.body.appendChild(iframe);
        }; )";
  EXPECT_TRUE(ExecJs(frame, script));

  // Verify that now there are 5 FNEs for the dynamic frames.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  EXPECT_EQ(1U, entry->root_node()->children.size());
  EXPECT_EQ(5U, entry->root_node()->children[0]->children.size());

  // Navigate |frame| (the parent of the dynamic frames) away.
  // This will destroy the 5 dynamic children of |frame|.
  GURL next_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "frame", next_url));

  // Verify that there are now 0 FNEs for the dynamic frames.
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(main_url, entry->GetURL());
  EXPECT_EQ(1U, entry->root_node()->children.size());
  EXPECT_EQ(0U, entry->root_node()->children[0]->children.size());
}

// This test helps verify that the browser does not retain history entries
// for removed frames *if* the removed frame was created by a script.
// Such frames get a fresh, random, unique name every time they are created
// or recreated and therefore in such case will never match previous history
// entries.  See also https://crbug.com/784356.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    PruningOfEntriesForDynamicFrames_MainFrameNavigatedAway) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/page_with_iframe_simple.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Add 5 dynamic subframes to |frame|.
  RenderFrameHost* frame = ChildFrameAt(current_main_frame_host(), 0);
  std::string script = R"(
        for (var i = 0; i < 5; i++) {
          var iframe = document.createElement("iframe");
          document.body.appendChild(iframe);
        }; )";
  EXPECT_TRUE(ExecJs(frame, script));

  // Verify that now there are 5 FNEs for the dynamic frames.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  EXPECT_EQ(1U, entry->root_node()->children.size());
  EXPECT_EQ(5U, entry->root_node()->children[0]->children.size());

  // Navigate the main frame (the grandparent of the dynamic frames) away.
  // This will destroy the 5 dynamic children of |frame|.
  GURL next_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), next_url));

  // Verify that there are now 0 FNEs for the dynamic frames.
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(main_url, entry->GetURL());
  EXPECT_EQ(1U, entry->root_node()->children.size());
  EXPECT_EQ(0U, entry->root_node()->children[0]->children.size());
}

// This test supplements SpareRenderProcessHostUnitTest to verify that the spare
// RenderProcessHost is actually used in cross-process navigations.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       UtilizationOfSpareRenderProcessHost) {
  GURL first_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL second_url = embedded_test_server()->GetURL("b.com", "/title2.html");
  RenderProcessHost* prev_spare = nullptr;
  RenderProcessHost* curr_spare = nullptr;
  RenderProcessHost* prev_host = nullptr;
  RenderProcessHost* curr_host = nullptr;

  // In the current implementation the spare is not warmed-up until the first
  // real navigation.  It might be okay to change that in the future.
  curr_spare = RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  curr_host = shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  EXPECT_FALSE(curr_spare);

  // Navigate to the first URL.
  prev_host = curr_host;
  prev_spare = curr_spare;
  EXPECT_TRUE(NavigateToURL(shell(), first_url));
  curr_spare = RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  curr_host = shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  EXPECT_NE(curr_spare, curr_host);
  // No process swap when navigating away from the initial blank page.
  EXPECT_EQ(prev_host, curr_host);
  // We should always keep a spare RenderProcessHost around in site-per-process
  // mode.  We don't assert what should happen in other scenarios (to give
  // flexibility to platform-specific decisions - e.g. on the desktop there
  // might be no spare outside of site-per-process, but on Android the spare
  // might still be opportunistically warmed up).
  if (AreAllSitesIsolatedForTesting())
    EXPECT_TRUE(curr_spare);

  // Perform a cross-site omnibox navigation.
  prev_host = curr_host;
  prev_spare = curr_spare;

  // With BackForwardCache the old process won't get deleted on navigation as it
  // is still in use by the bfcached document, disable back/forward cache to
  // ensure that the process gets deleted.
  DisableBackForwardCacheForTesting(contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  RenderProcessHostWatcher prev_host_watcher(
      prev_host, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  EXPECT_TRUE(NavigateToURL(shell(), second_url));
  // Wait until the |prev_host| goes away - this ensures that the spare will be
  // picked up by subsequent back navigation below.
  prev_host_watcher.Wait();
  curr_spare = RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  curr_host = shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  // The cross-site omnibox navigation should swap processes.
  EXPECT_NE(prev_host, curr_host);
  // If present, the spare RenderProcessHost should have been be used.
  if (prev_spare)
    EXPECT_EQ(prev_spare, curr_host);
  // A new spare should be warmed-up in site-per-process mode.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_TRUE(curr_spare);
    EXPECT_NE(prev_spare, curr_spare);
  }

  // Perform a back navigation.
  prev_host = curr_host;
  prev_spare = curr_spare;
  TestNavigationObserver back_load_observer(shell()->web_contents());
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  controller.GoBack();
  back_load_observer.Wait();
  curr_spare = RenderProcessHostImpl::GetSpareRenderProcessHostForTesting();
  curr_host = shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  // The cross-site back navigation should swap processes.
  EXPECT_NE(prev_host, curr_host);
  // If present, the spare RenderProcessHost should have been used.
  if (prev_spare)
    EXPECT_EQ(prev_spare, curr_host);
  // A new spare should be warmed-up in site-per-process mode.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_TRUE(curr_spare);
    EXPECT_NE(prev_spare, curr_spare);
  }
}

// Data URLs can have a reference fragment like any other URLs. In this test,
// there are two navigations with the same data URL, but with a different
// reference. The second navigation must be classified as "same-document".
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       DataURLSameDocumentNavigation) {
  GURL url_first("data:text/html,body#foo");
  GURL url_second("data:text/html,body#bar");
  EXPECT_TRUE(url_first.EqualsIgnoringRef(url_second));

  EXPECT_TRUE(NavigateToURL(shell(), url_first));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  FrameNavigateParamsCapturer capturer(root);
  shell()->LoadURL(url_second);
  capturer.Wait();
  EXPECT_TRUE(capturer.is_same_document());
}

// Verify that navigating to a page with status 404 and an empty body will
// result in an error page.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       EmptyBody404CommitsErrorPage) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  GURL url(embedded_test_server()->GetURL("/empty404.html"));

  {
    // Go to a non-existent page resulting in a 404 with an empty body.
    TestNavigationObserverInternal observer(contents());
    shell()->LoadURL(url);
    observer.Wait();

    // The navigation fails and commits a 404 error page.
    EXPECT_FALSE(observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_HTTP_RESPONSE_CODE_FAILURE,
              observer.last_net_error_code());
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY,
              observer.last_navigation_type());
    EXPECT_EQ(PAGE_TYPE_ERROR,
              controller.GetLastCommittedEntry()->GetPageType());

    // Check that the error page contains the error code.
    EXPECT_EQ(true,
              EvalJs(contents(), "document.body.innerText.includes('404')"));
  }

  {
    // Reloads will still result in an error page.
    TestNavigationObserverInternal reload_observer(contents());
    shell()->Reload();
    reload_observer.Wait();
    EXPECT_FALSE(reload_observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_HTTP_RESPONSE_CODE_FAILURE,
              reload_observer.last_net_error_code());
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              reload_observer.last_navigation_type());
    EXPECT_EQ(PAGE_TYPE_ERROR,
              controller.GetLastCommittedEntry()->GetPageType());
  }

  {
    // Same-URL navigation will still result in an error page.
    TestNavigationObserverInternal same_url_observer(contents());
    controller.LoadURL(url, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string() /* extra_headers */);
    same_url_observer.Wait();
    EXPECT_FALSE(same_url_observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_HTTP_RESPONSE_CODE_FAILURE,
              same_url_observer.last_net_error_code());
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY,
              same_url_observer.last_navigation_type());
    EXPECT_EQ(PAGE_TYPE_ERROR,
              controller.GetLastCommittedEntry()->GetPageType());
  }
}

// Verify that navigating to a page with status 500 and an empty body will
// result in an error page.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTestNoServer,
                       EmptyBody500CommitsErrorPage) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/title1.html");
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());

  // Go to a page that has a HTTP 500 status code with an empty body.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  TestNavigationObserver observer(contents());
  shell()->LoadURL(url);
  response.WaitForRequest();
  response.Send(net::HTTP_INTERNAL_SERVER_ERROR);
  response.Done();
  observer.Wait();

  // The navigation fails and commits a 500 error page.
  EXPECT_FALSE(observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_HTTP_RESPONSE_CODE_FAILURE,
            observer.last_net_error_code());
  EXPECT_EQ(PAGE_TYPE_ERROR, controller.GetLastCommittedEntry()->GetPageType());

  // Check that the error page contains the error code.
  EXPECT_EQ(true,
            EvalJs(contents(), "document.body.innerText.includes('500')"));
}

// Verify that navigating to a page with status 404 but a non-empty body won't
// result in an error page.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       NonEmpty404BodyDoesNotCommitErrorPage) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());

  // Go to a non-existent page with a non-empty body.
  GURL url(embedded_test_server()->GetURL("/page404.html"));
  TestNavigationObserver observer(contents());
  EXPECT_TRUE(NavigateToURL(shell(), url));
  observer.WaitForNavigationFinished();

  // The navigation succeeds and commits the response body.
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(net::OK, observer.last_net_error_code());
  EXPECT_EQ(PAGE_TYPE_NORMAL,
            controller.GetLastCommittedEntry()->GetPageType());
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ErrorPageNavigationWithoutNavigationRequestGetsKilled) {
  // Navigate normally to a page.
  GURL good_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), good_url));

  // Try to fake an error page navigation by doing a DidCommitProvisionalLoad
  // call. The browser doesn't know about the navigation at all previously.
  GURL bad_url(embedded_test_server()->GetURL("/title2.html"));
  auto params = mojom::DidCommitProvisionalLoadParams::New();
  params->did_create_new_entry = true;
  params->url = bad_url;
  params->referrer = blink::mojom::Referrer::New();
  params->transition = ui::PAGE_TRANSITION_LINK;
  params->page_state = blink::PageState::CreateFromURL(bad_url);
  params->method = "POST";
  params->post_id = 2;
  params->url_is_unreachable = true;
  params->embedding_token = base::UnguessableToken::Create();
  RenderFrameHostImpl* rfh = contents()->GetPrimaryMainFrame();
  RenderProcessHostBadIpcMessageWaiter kill_waiter(rfh->GetProcess());
  static_cast<mojom::FrameHost*>(rfh)->DidCommitProvisionalLoad(
      std::move(params),
      mojom::DidCommitProvisionalLoadInterfaceParams::New(
          mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>()
              .InitWithNewPipeAndPassReceiver()));

  // Verify that the malicious renderer got killed.
  EXPECT_EQ(bad_message::RFH_NO_MATCHING_NAVIGATION_REQUEST_ON_COMMIT,
            kill_waiter.Wait());
}

// Load the same page twice, once as a GET and once as a POST.
// We should update the post state on the NavigationEntry.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       LoadURL_SamePage_DifferentMethod) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // Create a form in the page then submit it to create a POST request.
  GURL form_submit_url(embedded_test_server()->GetURL("/title2.html"));
  CreateAndSubmitForm(form_submit_url);

  // Navigate to the page with a "GET" request. This will reload the page with a
  // different method, and the last committed entry should have the POST-related
  // data cleared.
  EXPECT_TRUE(NavigateToURL(shell(), form_submit_url));
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(form_submit_url, entry->GetURL());
  EXPECT_FALSE(entry->GetHasPostData());
  EXPECT_EQ(entry->GetPostID(), -1);
  EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
}

// Similar to LoadURL_SamePage_DifferentMethod but does a renderer-initiated
// navigation instead.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       LoadURL_SamePage_DifferentMethod_RendererInitiated) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // Create a form in the page then submit it to create a POST request.
  GURL form_submit_url(embedded_test_server()->GetURL("/title2.html"));
  CreateAndSubmitForm(form_submit_url);

  // Navigate to the page with a "GET" request. This will reload the page with a
  // different method, and the last committed entry should have the POST-related
  // data cleared.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), form_submit_url));
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(form_submit_url, entry->GetURL());
  EXPECT_FALSE(entry->GetHasPostData());
  EXPECT_EQ(entry->GetPostID(), -1);
  EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
}

// Tests that doing a form submission that opens a new about:blank tab won't
// crash.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FormSubmissionToNewTab) {
  GURL url_start(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_start));

  // Create and submit a form that will create a new about:blank tab.
  WebContentsAddedObserver web_contents_added_observer;
  TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();
  ASSERT_TRUE(ExecJs(contents(),
                     R"(let form = document.createElement('form');
                                 form.method = 'POST';
                                 form.target = '_blank';
                                 form.action = 'about:blank';
                                 document.body.appendChild(form);
                                 form.submit();)"));
  WebContentsImpl* popup_contents = static_cast<WebContentsImpl*>(
      web_contents_added_observer.GetWebContents());
  navigation_observer.WaitForNavigationFinished();
  EXPECT_EQ(GURL(url::kAboutBlankURL), popup_contents->GetLastCommittedURL());

  // Ensure that the new tab committed the form submission to about:blank
  // correctly.
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(popup_contents->GetController());
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(GURL(url::kAboutBlankURL), entry->GetURL());
  EXPECT_TRUE(entry->GetHasPostData());
  EXPECT_NE(entry->GetPostID(), -1);
  EXPECT_EQ("POST", popup_contents->GetPrimaryMainFrame()->last_http_method());
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FormSubmitServerRedirect) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  GURL redirect_to_url2_url(
      embedded_test_server()->GetURL("/server-redirect?" + url2.spec()));

  // Create a form in the page then submit it to create a POST request, but the
  // request got server-redirected and lost the POST data.
  TestNavigationObserver form_nav_observer(contents());
  EXPECT_TRUE(
      ExecJs(contents(), JsReplace("var form = document.createElement('form');"
                                   "form.method = 'POST';"
                                   "form.action = $1;"
                                   "document.body.appendChild(form);"
                                   "form.submit();",
                                   redirect_to_url2_url)));
  form_nav_observer.Wait();
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(url2, entry->GetURL());
  EXPECT_FALSE(entry->GetHasPostData());
  EXPECT_EQ(-1, entry->GetPostID());
  EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       FormSubmitClientRedirect) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  GURL redirect_to_url2_url(embedded_test_server()->GetURL(
      "/navigation_controller/client_redirect.html"));

  // Create a form in the page then submit it to create a POST request, but we
  // lost the POST data after client-redirect.
  TestNavigationObserver form_nav_observer(contents());
  EXPECT_TRUE(
      ExecJs(contents(), JsReplace("var form = document.createElement('form');"
                                   "form.method = 'POST';"
                                   "form.action = $1;"
                                   "document.body.appendChild(form);"
                                   "form.submit();",
                                   redirect_to_url2_url)));
  form_nav_observer.Wait();
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(url2, entry->GetURL());
  EXPECT_FALSE(entry->GetHasPostData());
  EXPECT_EQ(-1, entry->GetPostID());
  EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
}

// Counts the occurrences of form repost warning dialogs.
class CountRepostFormWarningWebContentsDelegate : public WebContentsDelegate {
 public:
  CountRepostFormWarningWebContentsDelegate() = default;

  int repost_form_warning_count() { return repost_form_warning_count_; }

  void ShowRepostFormWarningDialog(WebContents* source) override {
    repost_form_warning_count_++;
  }

 private:
  // The number of times ShowRepostFormWarningDialog() was called.
  int repost_form_warning_count_ = 0;
};

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ResetPendingLoadTypeWhenCancelPendingReload) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  auto delegate = std::make_unique<CountRepostFormWarningWebContentsDelegate>();
  contents()->SetDelegate(delegate.get());
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // Create a form in the page then submit it to create a POST request.
  GURL form_submit_url(embedded_test_server()->GetURL("/title2.html"));
  const int64_t form_post_id = CreateAndSubmitForm(form_submit_url);
  EXPECT_EQ(0, delegate->repost_form_warning_count());

  // Reload. We should show a repost warning dialog.
  {
    NavigationControllerImpl::ScopedShowRepostDialogForTesting show_repost;
    controller.Reload(ReloadType::NORMAL, true /* check_for_repost */);
    EXPECT_TRUE(WaitForLoadStop(contents()));
    EXPECT_EQ(form_submit_url, contents()->GetLastCommittedURL());
    EXPECT_TRUE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(form_post_id, controller.GetLastCommittedEntry()->GetPostID());
  }
  EXPECT_EQ(ReloadType::NORMAL, controller.pending_reload_);
  EXPECT_EQ(1, delegate->repost_form_warning_count());

  // Trigger a new navigation to cancel pending reload.
  const GURL url1(embedded_test_server()->GetURL("/title3.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_EQ(url1, contents()->GetLastCommittedURL());
  EXPECT_EQ(ReloadType::NONE, controller.pending_reload_);

  // |pending_reload_| has been cleared and no new pending entry will be
  // created.
  controller.ContinuePendingReload();
  EXPECT_FALSE(controller.GetPendingEntry());
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, PostThenReload) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  auto delegate = std::make_unique<CountRepostFormWarningWebContentsDelegate>();
  contents()->SetDelegate(delegate.get());
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // Create a form in the page then submit it to create a POST request.
  GURL form_submit_url(embedded_test_server()->GetURL("/title2.html"));
  const int64_t form_post_id = CreateAndSubmitForm(form_submit_url);
  EXPECT_EQ(0, delegate->repost_form_warning_count());

  // Reload. We should show a repost warning dialog.
  {
    NavigationControllerImpl::ScopedShowRepostDialogForTesting show_repost;
    controller.Reload(ReloadType::NORMAL, true /* check_for_repost */);
    EXPECT_TRUE(WaitForLoadStop(contents()));
    EXPECT_EQ(form_submit_url, contents()->GetLastCommittedURL());
    EXPECT_TRUE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(form_post_id, controller.GetLastCommittedEntry()->GetPostID());
  }
  EXPECT_EQ(1, delegate->repost_form_warning_count());
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       PostThenReplaceStateThenReload) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  auto delegate = std::make_unique<CountRepostFormWarningWebContentsDelegate>();
  contents()->SetDelegate(delegate.get());
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // Create a form in the page then submit it to create a POST request.
  GURL form_submit_url(embedded_test_server()->GetURL("/title2.html"));
  CreateAndSubmitForm(form_submit_url);
  const int kExpectedRepostFormWarningCount = 0;
  EXPECT_EQ(kExpectedRepostFormWarningCount,
            delegate->repost_form_warning_count());

  // history.replaceState() is called, which clears the POST data.
  GURL replace_url(embedded_test_server()->GetURL("/title2.html#foo"));
  {
    EXPECT_TRUE(
        ExecJs(contents(), "history.replaceState({}, '', 'title2.html#foo')"));
    EXPECT_TRUE(WaitForLoadStop(contents()));
    EXPECT_EQ(replace_url, contents()->GetLastCommittedURL());
    EXPECT_FALSE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(-1, controller.GetLastCommittedEntry()->GetPostID());
  }
  EXPECT_EQ(kExpectedRepostFormWarningCount,
            delegate->repost_form_warning_count());

  // Now reload. replaceState overrides the POST, so we should not show a repost
  // warning dialog.
  {
    NavigationControllerImpl::ScopedShowRepostDialogForTesting show_repost;
    controller.Reload(ReloadType::NORMAL, true /* check_for_repost */);
    EXPECT_TRUE(WaitForLoadStop(contents()));
    EXPECT_EQ(replace_url, contents()->GetLastCommittedURL());
    EXPECT_FALSE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(-1, controller.GetLastCommittedEntry()->GetPostID());
  }
  EXPECT_EQ(kExpectedRepostFormWarningCount,
            delegate->repost_form_warning_count());
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       PostThenPushStateThenReloadThenHistory) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  auto delegate = std::make_unique<CountRepostFormWarningWebContentsDelegate>();
  contents()->SetDelegate(delegate.get());
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // Create a form in the page then submit it to create a POST request.
  GURL form_submit_url(embedded_test_server()->GetURL("/title2.html"));
  CreateAndSubmitForm(form_submit_url);
  const int kExpectedRepostFormWarningCount = 0;
  EXPECT_EQ(kExpectedRepostFormWarningCount,
            delegate->repost_form_warning_count());
  EXPECT_EQ("POST", contents()->GetPrimaryMainFrame()->last_http_method());

  // history.pushState() is called, which clears the POST data.
  GURL push_url(embedded_test_server()->GetURL("/title2.html#foo"));
  {
    EXPECT_TRUE(
        ExecJs(contents(), "history.pushState({}, '', 'title2.html#foo')"));
    EXPECT_TRUE(WaitForLoadStop(contents()));
    EXPECT_EQ(push_url, contents()->GetLastCommittedURL());
    EXPECT_FALSE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(-1, controller.GetLastCommittedEntry()->GetPostID());
    EXPECT_EQ(kExpectedRepostFormWarningCount,
              delegate->repost_form_warning_count());
    EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
  }

  // Now reload. pushState overrides the POST, so we should not show a
  // repost warning dialog.
  {
    NavigationControllerImpl::ScopedShowRepostDialogForTesting show_repost;
    controller.Reload(ReloadType::NORMAL, true /* check_for_repost */);
    EXPECT_TRUE(WaitForLoadStop(contents()));
    EXPECT_EQ(push_url, contents()->GetLastCommittedURL());
    EXPECT_FALSE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(-1, controller.GetLastCommittedEntry()->GetPostID());
    EXPECT_EQ(kExpectedRepostFormWarningCount,
              delegate->repost_form_warning_count());
    EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
  }

  // Go back to the first URL. This will be a same document navigation. Even
  // though the original navigation is a "POST" navigation, the POST data is
  // already cleared on the renderer side so it will end as a "GET" navigation.
  {
    NavigationControllerImpl::ScopedShowRepostDialogForTesting show_repost;
    TestNavigationObserver load_observer(shell()->web_contents());
    controller.GoBack();
    load_observer.Wait();
    EXPECT_EQ(form_submit_url, contents()->GetLastCommittedURL());
    EXPECT_FALSE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(-1, controller.GetLastCommittedEntry()->GetPostID());
    EXPECT_EQ(kExpectedRepostFormWarningCount,
              delegate->repost_form_warning_count());
    EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
  }

  // Go forward to the pushState URL. This will be a GET navigation again.
  {
    NavigationControllerImpl::ScopedShowRepostDialogForTesting show_repost;
    TestNavigationObserver load_observer(shell()->web_contents());
    controller.GoForward();
    load_observer.Wait();
    EXPECT_EQ(push_url, contents()->GetLastCommittedURL());
    EXPECT_FALSE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(-1, controller.GetLastCommittedEntry()->GetPostID());
    EXPECT_EQ(kExpectedRepostFormWarningCount,
              delegate->repost_form_warning_count());
    EXPECT_EQ("GET", contents()->GetPrimaryMainFrame()->last_http_method());
  }
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       PostThenFragmentNavigationThenReloadThenHistory) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  auto delegate = std::make_unique<CountRepostFormWarningWebContentsDelegate>();
  contents()->SetDelegate(delegate.get());
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // Create a form in the page then submit it to create a POST request.
  GURL form_submit_url(embedded_test_server()->GetURL("/title2.html"));
  const int64_t form_post_id = CreateAndSubmitForm(form_submit_url);
  EXPECT_EQ(0, delegate->repost_form_warning_count());

  // Do a renderer-initiated fragment navigation. This should preserve the POST
  // data.
  GURL fragment_url(embedded_test_server()->GetURL("/title2.html#foo"));
  {
    EXPECT_TRUE(ExecJs(contents(), "location.href = '#foo'"));
    EXPECT_TRUE(WaitForLoadStop(contents()));
    EXPECT_EQ(fragment_url, contents()->GetLastCommittedURL());
    EXPECT_TRUE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(form_post_id, controller.GetLastCommittedEntry()->GetPostID());
    EXPECT_EQ(0, delegate->repost_form_warning_count());
    EXPECT_EQ("POST", contents()->GetPrimaryMainFrame()->last_http_method());
  }

  // Now reload. Fragment navigation keeps the previous POST data, so we should
  // show a repost warning dialog.
  {
    NavigationControllerImpl::ScopedShowRepostDialogForTesting show_repost;
    controller.Reload(ReloadType::NORMAL, true /* check_for_repost */);
    EXPECT_TRUE(WaitForLoadStop(contents()));
    EXPECT_EQ(fragment_url, contents()->GetLastCommittedURL());
    EXPECT_TRUE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(form_post_id, controller.GetLastCommittedEntry()->GetPostID());
    EXPECT_EQ(1, delegate->repost_form_warning_count());
    EXPECT_EQ("POST", contents()->GetPrimaryMainFrame()->last_http_method());
  }

  // Go back. This will be a same document navigation. We won't show a repost
  // warning dialog, but will keep the "POST" method.
  {
    NavigationControllerImpl::ScopedShowRepostDialogForTesting show_repost;
    TestNavigationObserver load_observer(shell()->web_contents());
    controller.GoBack();
    load_observer.Wait();
    EXPECT_EQ(form_submit_url, contents()->GetLastCommittedURL());
    EXPECT_TRUE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(form_post_id, controller.GetLastCommittedEntry()->GetPostID());
    EXPECT_EQ(1, delegate->repost_form_warning_count());
    EXPECT_EQ("POST", contents()->GetPrimaryMainFrame()->last_http_method());
  }

  // Go forward. This will be a same document navigation. We won't show a repost
  // warning dialog, but will keep the "POST" method.
  {
    NavigationControllerImpl::ScopedShowRepostDialogForTesting show_repost;
    TestNavigationObserver load_observer(shell()->web_contents());
    controller.GoForward();
    load_observer.Wait();
    EXPECT_EQ(fragment_url, contents()->GetLastCommittedURL());
    EXPECT_TRUE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(form_post_id, controller.GetLastCommittedEntry()->GetPostID());
    EXPECT_EQ(1, delegate->repost_form_warning_count());
    EXPECT_EQ("POST", contents()->GetPrimaryMainFrame()->last_http_method());
  }
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       PostThenBrowserInitiatedFragmentNavigationThenReload) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  auto delegate = std::make_unique<CountRepostFormWarningWebContentsDelegate>();
  contents()->SetDelegate(delegate.get());
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // Create a form in the page then submit it to create a POST request.
  GURL form_submit_url(embedded_test_server()->GetURL("/title2.html"));
  const int64_t form_post_id = CreateAndSubmitForm(form_submit_url);
  EXPECT_EQ(0, delegate->repost_form_warning_count());

  // Do a browser-initiated fragment navigation. This should preserve the POST
  // data.
  GURL fragment_url(embedded_test_server()->GetURL("/title2.html#foo"));
  {
    EXPECT_TRUE(NavigateToURL(shell(), fragment_url));
    EXPECT_EQ(fragment_url, contents()->GetLastCommittedURL());
    EXPECT_TRUE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(form_post_id, controller.GetLastCommittedEntry()->GetPostID());
    EXPECT_EQ(0, delegate->repost_form_warning_count());
    EXPECT_EQ("POST", contents()->GetPrimaryMainFrame()->last_http_method());
  }

  // Now reload. Fragment navigation keeps the previous POST data, so we should
  // show a repost warning dialog.
  {
    NavigationControllerImpl::ScopedShowRepostDialogForTesting show_repost;
    controller.Reload(ReloadType::NORMAL, true /* check_for_repost */);
    EXPECT_TRUE(WaitForLoadStop(contents()));
    EXPECT_EQ(fragment_url, contents()->GetLastCommittedURL());
    EXPECT_TRUE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(form_post_id, controller.GetLastCommittedEntry()->GetPostID());
    EXPECT_EQ(1, delegate->repost_form_warning_count());
    EXPECT_EQ("POST", contents()->GetPrimaryMainFrame()->last_http_method());
  }
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       PostThenJavaScriptURLThenBrowserInitiatedFragment) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  const int64_t start_dsn = controller.GetLastCommittedEntry()
                                ->GetFrameEntry(root)
                                ->document_sequence_number();

  // Create a form in the page then submit it to create a POST request.
  GURL form_submit_url(embedded_test_server()->GetURL("/title2.html"));
  const int64_t form_post_id = CreateAndSubmitForm(form_submit_url);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ("POST", contents()->GetPrimaryMainFrame()->last_http_method());
  EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
  const int64_t form_dsn = controller.GetLastCommittedEntry()
                               ->GetFrameEntry(root)
                               ->document_sequence_number();
  DCHECK_NE(start_dsn, form_dsn);

  // Do a javascript: URL "navigation", which will create a new document but
  // won't send anything to the browser.
  {
    EXPECT_TRUE(ExecJs(root, R"(window.location = 'javascript:"foo"';)"));
    EXPECT_TRUE(WaitForLoadStop(contents()));

    EXPECT_EQ(form_submit_url, root->current_url());
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ("foo", EvalJs(shell(), "document.body.innerHTML"));
    EXPECT_TRUE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(form_post_id, controller.GetLastCommittedEntry()->GetPostID());
    EXPECT_EQ("POST", contents()->GetPrimaryMainFrame()->last_http_method());
    EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
    DCHECK_EQ(form_dsn, controller.GetLastCommittedEntry()
                            ->GetFrameEntry(root)
                            ->document_sequence_number());
  }

  // Do a browser-initiated fragment navigation. This should preserve the POST
  // data.
  GURL fragment_url(embedded_test_server()->GetURL("/title2.html#foo"));
  {
    EXPECT_TRUE(NavigateToURL(shell(), fragment_url));
    EXPECT_EQ(fragment_url, contents()->GetLastCommittedURL());

    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_TRUE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(form_post_id, controller.GetLastCommittedEntry()->GetPostID());
    EXPECT_EQ("POST", contents()->GetPrimaryMainFrame()->last_http_method());
    EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());

    DCHECK_EQ(form_dsn, controller.GetLastCommittedEntry()
                            ->GetFrameEntry(root)
                            ->document_sequence_number());
  }
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       PostThenJavaScriptURLThenRendererInitiatedFragment) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  const int64_t start_dsn = controller.GetLastCommittedEntry()
                                ->GetFrameEntry(root)
                                ->document_sequence_number();

  // Create a form in the page then submit it to create a POST request.
  GURL form_submit_url(embedded_test_server()->GetURL("/title2.html"));
  const int64_t form_post_id = CreateAndSubmitForm(form_submit_url);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ("POST", contents()->GetPrimaryMainFrame()->last_http_method());
  EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
  const int64_t form_dsn = controller.GetLastCommittedEntry()
                               ->GetFrameEntry(root)
                               ->document_sequence_number();
  DCHECK_NE(start_dsn, form_dsn);

  // Do a javascript: URL "navigation", which will create a new document but
  // won't send anything to the browser.
  {
    EXPECT_TRUE(ExecJs(root, R"(window.location = 'javascript:"foo"';)"));
    EXPECT_TRUE(WaitForLoadStop(contents()));

    EXPECT_EQ(form_submit_url, root->current_url());
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ("foo", EvalJs(shell(), "document.body.innerHTML"));
    EXPECT_EQ("POST", contents()->GetPrimaryMainFrame()->last_http_method());
    EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
    DCHECK_EQ(form_dsn, controller.GetLastCommittedEntry()
                            ->GetFrameEntry(root)
                            ->document_sequence_number());
  }

  // Do a renderer-initiated fragment navigation. This should preserve the POST
  // data.
  GURL fragment_url(embedded_test_server()->GetURL("/title2.html#foo"));
  {
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "location.href='#foo';"));
    capturer.Wait();
    EXPECT_NE(start_url, root->current_url());
    EXPECT_TRUE(capturer.is_same_document());

    EXPECT_EQ(3, controller.GetEntryCount());
    EXPECT_TRUE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(form_post_id, controller.GetLastCommittedEntry()->GetPostID());
    EXPECT_EQ("POST", contents()->GetPrimaryMainFrame()->last_http_method());
    EXPECT_EQ(200, contents()->GetPrimaryMainFrame()->last_http_status_code());
    DCHECK_EQ(form_dsn, controller.GetLastCommittedEntry()
                            ->GetFrameEntry(root)
                            ->document_sequence_number());
  }
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, PostSubframe) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  auto delegate = std::make_unique<CountRepostFormWarningWebContentsDelegate>();
  contents()->SetDelegate(delegate.get());
  // 1) Load a page with an iframe.
  GURL start_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe_simple.html"));
  GURL iframe_start_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  FrameTreeNode* iframe = root->child_at(0);
  EXPECT_EQ(iframe_start_url,
            iframe->current_frame_host()->GetLastCommittedURL());

  // 2) Create a form in the main frame which submits to the iframe to the same
  // URL with an anchor link, but it will be a cross-document navigation due to
  // the POST data.
  GURL form_submit_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html#foo"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    EXPECT_TRUE(
        ExecJs(root, JsReplace(R"(var form = document.createElement('form');
                               form.method = 'POST';
                               form.action = $1;
                               form.target = 'simple_iframe';
                               document.body.appendChild(form);
                               form.submit();)",
                               form_submit_url)));
    capturer.Wait();
    EXPECT_EQ(form_submit_url,
              iframe->current_frame_host()->GetLastCommittedURL());
    EXPECT_EQ(0, delegate->repost_form_warning_count());
    EXPECT_EQ("POST", iframe->current_frame_host()->last_http_method());

    // The last committed entry refers to the main frame, so no POST data there.
    EXPECT_EQ("GET", root->current_frame_host()->last_http_method());
    EXPECT_FALSE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(-1, controller.GetLastCommittedEntry()->GetPostID());
  }
  const int64_t form_post_id = iframe->current_frame_host()->last_post_id();

  // 3) Do a same-document navigation in the iframe. POST ID on the iframe
  // should be retained.
  GURL fragment_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html#bar"));
  {
    NavigationControllerImpl::ScopedShowRepostDialogForTesting show_repost;
    EXPECT_TRUE(ExecJs(iframe, "location.href = '#bar'"));
    EXPECT_TRUE(WaitForLoadStop(contents()));

    EXPECT_EQ(fragment_url,
              iframe->current_frame_host()->GetLastCommittedURL());
    EXPECT_EQ(0, delegate->repost_form_warning_count());
    EXPECT_EQ("POST", iframe->current_frame_host()->last_http_method());
    EXPECT_EQ(form_post_id, iframe->current_frame_host()->last_post_id());

    // The last committed entry refers to the main frame, so no POST data there.
    EXPECT_EQ("GET", root->current_frame_host()->last_http_method());
    EXPECT_FALSE(controller.GetLastCommittedEntry()->GetHasPostData());
    EXPECT_EQ(-1, controller.GetLastCommittedEntry()->GetPostID());
  }

  // 4) Reload the iframe. The iframe will keep the POST ID.
  {
    NavigationControllerImpl::ScopedShowRepostDialogForTesting show_repost;
    FrameNavigateParamsCapturer capturer(iframe);
    EXPECT_TRUE(ExecJs(iframe, "location.reload();"));
    capturer.Wait();
    EXPECT_EQ(fragment_url,
              iframe->current_frame_host()->GetLastCommittedURL());
    EXPECT_EQ(0, delegate->repost_form_warning_count());
    EXPECT_EQ("POST", iframe->current_frame_host()->last_http_method());
    EXPECT_EQ(form_post_id, iframe->current_frame_host()->last_post_id());
  }

  // 5) Reload the tab. The iframe will reload the original page it loaded, and
  // the POST ID on the iframe will be removed.
  {
    NavigationControllerImpl::ScopedShowRepostDialogForTesting show_repost;
    controller.Reload(ReloadType::NORMAL, true /* check_for_repost */);
    EXPECT_TRUE(WaitForLoadStop(contents()));
    iframe = root->child_at(0);
    EXPECT_EQ(iframe_start_url,
              iframe->current_frame_host()->GetLastCommittedURL());
    EXPECT_EQ(0, delegate->repost_form_warning_count());
    EXPECT_EQ("GET", iframe->current_frame_host()->last_http_method());
    EXPECT_EQ(-1, iframe->current_frame_host()->last_post_id());
  }
}

// Verify that a session history navigation which results in a different
// SiteInstance from the original commit is correctly handled - classified
// as new navigation with replacement, resulting in no new navigation
// entries.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       SiteInstanceChangeOnHistoryNavigation) {
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url3(embedded_test_server()->GetURL("c.com", "/title3.html"));
  GURL redirecting_url(embedded_test_server()->GetURL(
      "a.com", "/server-redirect?" + url3.spec()));

  // Start with an initial URL.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url1, controller.GetEntryAtIndex(0)->GetURL());
  scoped_refptr<SiteInstance> initial_site_instance =
      root->current_frame_host()->GetSiteInstance();

  {
    // history.replaceState(), pointing to a URL that would redirect to |url3|.
    FrameNavigateParamsCapturer capturer(root);
    std::string script =
        "history.replaceState({}, '', '" + redirecting_url.spec() + "')";
    EXPECT_TRUE(ExecJs(root, script));
    capturer.Wait();
  }
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(redirecting_url, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(initial_site_instance,
            root->current_frame_host()->GetSiteInstance());

  // Navigate to a new URL to get new session history entry.
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_NE(initial_site_instance,
            root->current_frame_host()->GetSiteInstance());

  // This test expects using different SiteInstance/URL when navigating back.
  // This won't happen with BackForwardCache as document is restored directly
  // instead of redirecting, disable back/forward cache to ensure that redirect
  // happens on history navigation.
  DisableBackForwardCacheForTesting(contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // Back, which should redirect to |url3|.
  FrameNavigateParamsCapturer capturer(root);
  shell()->web_contents()->GetController().GoBack();
  capturer.Wait();
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  NavigationEntry* entry = controller.GetEntryAtIndex(0);
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());
  EXPECT_EQ(url3, entry->GetURL());
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(initial_site_instance,
              root->current_frame_host()->GetSiteInstance());
    EXPECT_EQ(
        SiteInfo::CreateForTesting(
            IsolationContext(shell()->web_contents()->GetBrowserContext()),
            url3),
        root->current_frame_host()->GetSiteInstance()->GetSiteInfo());
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
  } else {
    EXPECT_EQ(initial_site_instance,
              root->current_frame_host()->GetSiteInstance());
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
  }
}

// Tests that user activation/gesture is not "inherited" by a new document
// on cross-site browser-initiated navigation.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    NoGestureInheritanceAfterCrossSiteNavigation_BrowserInitiated) {
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url3(embedded_test_server()->GetURL("c.com", "/title3.html"));
  // Initial navigation.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  {
    // Renderer-initiated navigation with user gesture.
    FrameNavigateParamsCapturer url2_capturer(root);
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url2));
    url2_capturer.Wait();
    EXPECT_TRUE(url2_capturer.has_user_gesture());
    EXPECT_TRUE(root->current_frame_host()
                    ->last_committed_common_params_has_user_gesture());
  }

  {
    // Cross-site browser-initiated navigation without user gesture.
    FrameNavigateParamsCapturer url3_capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url3));
    url3_capturer.Wait();
    EXPECT_FALSE(url3_capturer.has_user_gesture());
    EXPECT_FALSE(root->current_frame_host()
                     ->last_committed_common_params_has_user_gesture());
  }
}

// Tests that user activation/gesture is not "inherited" by a new document
// on cross-site renderer-initiated navigation.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    NoGestureInheritanceAfterCrossSiteNavigation_RendererInitiated) {
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url3(embedded_test_server()->GetURL("c.com", "/title3.html"));
  // Initial navigation.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  {
    // Renderer-initiated navigation with user gesture.
    FrameNavigateParamsCapturer url2_capturer(root);
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url2));
    url2_capturer.Wait();
    EXPECT_TRUE(url2_capturer.has_user_gesture());
    EXPECT_TRUE(root->current_frame_host()
                    ->last_committed_common_params_has_user_gesture());
  }

  {
    // Cross-site renderer-initiated navigation without user gesture.
    FrameNavigateParamsCapturer url3_capturer(root);
    EXPECT_TRUE(NavigateToURLFromRendererWithoutUserGesture(shell(), url3));
    url3_capturer.Wait();
    EXPECT_FALSE(url3_capturer.has_user_gesture());
    EXPECT_FALSE(root->current_frame_host()
                     ->last_committed_common_params_has_user_gesture());
  }
}

// Tests that user activation/gesture is not "inherited" by a new document
// on same-site browser-initiated navigation.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    NoGestureInheritanceAfterSameSiteNavigation_BrowserInitiated) {
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url3(embedded_test_server()->GetURL("a.com", "/title3.html"));
  // Initial navigation.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  {
    // Renderer-initiated navigation with user gesture.
    FrameNavigateParamsCapturer url2_capturer(root);
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url2));
    url2_capturer.Wait();
    EXPECT_TRUE(url2_capturer.has_user_gesture());
    EXPECT_TRUE(root->current_frame_host()
                    ->last_committed_common_params_has_user_gesture());
  }

  {
    // Same-site browser-initiated navigation without user gesture.
    FrameNavigateParamsCapturer url3_capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url3));
    url3_capturer.Wait();
    EXPECT_FALSE(url3_capturer.has_user_gesture());
    EXPECT_FALSE(root->current_frame_host()
                     ->last_committed_common_params_has_user_gesture());
  }
}

// Tests that user activation/gesture is not "inherited" by a new document
// on same-site renderer-initiated navigation.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    NoGestureInheritanceAfterSameSiteNavigation_RendererInitiated) {
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url3(embedded_test_server()->GetURL("a.com", "/title3.html"));
  // Initial navigation.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  {
    // Renderer-initiated navigation with user gesture.
    FrameNavigateParamsCapturer url2_capturer(root);
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url2));
    url2_capturer.Wait();
    EXPECT_TRUE(url2_capturer.has_user_gesture());
    EXPECT_TRUE(root->current_frame_host()
                    ->last_committed_common_params_has_user_gesture());
  }

  {
    // Same-site renderer-initiated navigation without user gesture.
    FrameNavigateParamsCapturer url3_capturer(root);
    EXPECT_TRUE(NavigateToURLFromRendererWithoutUserGesture(shell(), url3));
    url3_capturer.Wait();
    EXPECT_FALSE(url3_capturer.has_user_gesture());
    EXPECT_FALSE(root->current_frame_host()
                     ->last_committed_common_params_has_user_gesture());
  }
}

// Tests that user activation/gesture returned by DidCommitProvisionalLoadParams
// reflects the latest navigation's gesture on a document when the initial
// document load was loaded with no user activation/gesture.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    GestureChangesAfterSameDocumentNavOnDocumentLoadedWithoutGesture) {
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("a.com", "/title1.html#a"));
  GURL url3(embedded_test_server()->GetURL("a.com", "/title1.html#b"));
  GURL url4(embedded_test_server()->GetURL("a.com", "/title1.html#c"));
  GURL url5(embedded_test_server()->GetURL("a.com", "/title1.html#d"));
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();

  {
    // Initial navigation without user gesture.
    FrameNavigateParamsCapturer url1_capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url1));
    url1_capturer.Wait();
    EXPECT_FALSE(url1_capturer.has_user_gesture());
    EXPECT_FALSE(root->current_frame_host()
                     ->last_committed_common_params_has_user_gesture());
  }

  {
    // Renderer-initiated same-document navigation without user gesture.
    FrameNavigateParamsCapturer url2_capturer(root);
    EXPECT_TRUE(NavigateToURLFromRendererWithoutUserGesture(shell(), url2));
    url2_capturer.Wait();
    EXPECT_FALSE(url2_capturer.has_user_gesture());
    EXPECT_FALSE(root->current_frame_host()
                     ->last_committed_common_params_has_user_gesture());
  }

  {
    // Renderer-initiated same-document navigation with user gesture.
    FrameNavigateParamsCapturer url3_capturer(root);
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url3));
    url3_capturer.Wait();
    EXPECT_TRUE(url3_capturer.has_user_gesture());
    EXPECT_TRUE(root->current_frame_host()
                    ->last_committed_common_params_has_user_gesture());
  }

  {
    // Browser-initiated same-document navigation without user gesture.
    FrameNavigateParamsCapturer url4_capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url4));
    url4_capturer.Wait();
    EXPECT_FALSE(url4_capturer.has_user_gesture());
    EXPECT_FALSE(root->current_frame_host()
                     ->last_committed_common_params_has_user_gesture());
  }

  {
    // Browser-initiated same-document navigation with user gesture.
    FrameNavigateParamsCapturer url5_capturer(root);
    TestNavigationObserver navigation_observer(contents);
    NavigationController::LoadURLParams params(url5);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    params.has_user_gesture = true;
    contents->GetController().LoadURLWithParams(params);
    navigation_observer.Wait();
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
    url5_capturer.Wait();
    EXPECT_TRUE(url5_capturer.has_user_gesture());
    EXPECT_TRUE(root->current_frame_host()
                    ->last_committed_common_params_has_user_gesture());
  }
}

// Tests that user activation/gesture returned by DidCommitProvisionalLoadParams
// reflects the latest navigation's gesture when the initial document load was
// loaded with user activation/gesture.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    GestureChangesAfterSameDocumentNavOnDocumentLoadedWithGesture) {
  // Initial navigation to allow doing navigation from the renderer after this.
  GURL url0(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url0));

  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  GURL url1(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url2(embedded_test_server()->GetURL("a.com", "/title2.html#foo"));
  GURL url3(embedded_test_server()->GetURL("a.com", "/title2.html#bar"));
  GURL url4(embedded_test_server()->GetURL("a.com", "/title2.html#baz"));

  {
    // Navigation with user gesture.
    FrameNavigateParamsCapturer url1_capturer(root);
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url1));
    url1_capturer.Wait();
    EXPECT_TRUE(url1_capturer.has_user_gesture());
    EXPECT_TRUE(root->current_frame_host()
                    ->last_committed_common_params_has_user_gesture());
  }

  {
    // Renderer-initiated same-document navigation without user gesture.
    FrameNavigateParamsCapturer url2_capturer(root);
    EXPECT_TRUE(NavigateToURLFromRendererWithoutUserGesture(shell(), url2));
    url2_capturer.Wait();
    EXPECT_FALSE(url2_capturer.has_user_gesture());
    EXPECT_FALSE(root->current_frame_host()
                     ->last_committed_common_params_has_user_gesture());
  }

  {
    // Browser-initiated same-document navigation without user gesture.
    FrameNavigateParamsCapturer url3_capturer(root);
    EXPECT_TRUE(NavigateToURL(shell(), url3));
    url3_capturer.Wait();
    EXPECT_FALSE(url3_capturer.has_user_gesture());
    EXPECT_FALSE(root->current_frame_host()
                     ->last_committed_common_params_has_user_gesture());
  }

  {
    // Browser-initiated same-document navigation with user gesture.
    FrameNavigateParamsCapturer url4_capturer(root);
    TestNavigationObserver navigation_observer(contents);
    NavigationController::LoadURLParams params(url4);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    params.has_user_gesture = true;
    contents->GetController().LoadURLWithParams(params);
    navigation_observer.Wait();
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
    url4_capturer.Wait();
    EXPECT_TRUE(url4_capturer.has_user_gesture());
    EXPECT_TRUE(root->current_frame_host()
                    ->last_committed_common_params_has_user_gesture());
  }
}

// history.back() called twice in the renderer process should not make the user
// navigate back twice.
// Regression test for https://crbug.com/869710
// TODO(crbug.com/1328912): flaky.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    DISABLED_HistoryBackTwiceFromRendererWithoutUserGesture) {
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url3(embedded_test_server()->GetURL("c.com", "/title3.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  EXPECT_TRUE(NavigateToURL(shell(), url3));

  EXPECT_TRUE(ExecJs(shell(), "history.back(); history.back();",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(url2, shell()->web_contents()->GetLastCommittedURL());
}

// history.back() called twice in the renderer process should not make the user
// navigate back twice. Even with a user gesture.
// Regression test for https://crbug.com/869710
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       HistoryBackTwiceFromRendererWithUserGesture) {
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url3(embedded_test_server()->GetURL("c.com", "/title3.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  EXPECT_TRUE(NavigateToURL(shell(), url2));
  EXPECT_TRUE(NavigateToURL(shell(), url3));

  EXPECT_TRUE(ExecJs(shell(), "history.back(); history.back();"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // TODO(https://crbug.com/869710): This should be url2.
  EXPECT_EQ(url1, shell()->web_contents()->GetLastCommittedURL());
}

// Test to verify that LoadPostCommitErrorPage loads an error page even with a
// valid URL.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       BrowserInitiatedLoadPostCommitErrorPage) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHost* root = shell()->web_contents()->GetPrimaryMainFrame();
  scoped_refptr<SiteInstance> success_site_instance = root->GetSiteInstance();

  std::string error_html = "Error page";
  TestNavigationObserver error_observer(shell()->web_contents());
  controller.LoadPostCommitErrorPage(root, url, error_html);
  error_observer.Wait();

  scoped_refptr<SiteInstance> error_site_instance =
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();

  EXPECT_FALSE(error_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, error_observer.last_net_error_code());
  EXPECT_EQ(PAGE_TYPE_ERROR, controller.GetLastCommittedEntry()->GetPageType());
  EXPECT_EQ(error_html, EvalJs(shell(), "document.body.innerHTML"));

  if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(true))
    return;

  // Verify the error page committed to the error page process.
  EXPECT_NE(success_site_instance, error_site_instance);
  EXPECT_TRUE(
      success_site_instance->IsRelatedSiteInstance(error_site_instance.get()));
  EXPECT_NE(success_site_instance->GetProcess()->GetID(),
            error_site_instance->GetProcess()->GetID());
  EXPECT_EQ(GURL(kUnreachableWebDataURL), error_site_instance->GetSiteURL());

  EXPECT_TRUE(
      error_site_instance->GetProcess()->GetProcessLock().is_error_page());
}

// Test to verify that LoadPostCommitErrorPage loads an error page in a subframe
// correctly.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       BrowserInitiatedLoadPostCommitErrorPageForSubframe) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHost* child =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  scoped_refptr<SiteInstance> success_site_instance = child->GetSiteInstance();

  std::string error_html = "Error page";
  TestNavigationObserver error_observer(shell()->web_contents());
  controller.LoadPostCommitErrorPage(child, url, error_html);
  error_observer.Wait();

  // If error page isolation is enabled the `child` pointer will be invalid
  // and should be retrieved again. It is safe to do so even when disabled.
  child = ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);

  EXPECT_FALSE(error_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, error_observer.last_net_error_code());
  EXPECT_EQ(child->GetLastCommittedURL(), url);
  EXPECT_EQ(error_html, EvalJs(child, "document.body.innerHTML"));

  // Verify that the subframe error page committed in the correct SiteInstance.
  EXPECT_TRUE(IsExpectedSubframeErrorTransition(success_site_instance.get(),
                                                child->GetSiteInstance()));
}

// Checks that a browser initiated error page navigation in a frame pending
// deletion is ignored and does not result in a crash. See
// https://crbug.com/1019180.
// TODO(crbug.com/1291862): Flaky failures
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_BrowserInitiatedLoadPostCommitErrorPageIgnoredForFramePendingDeletion \
  DISABLED_BrowserInitiatedLoadPostCommitErrorPageIgnoredForFramePendingDeletion
#else
#define MAYBE_BrowserInitiatedLoadPostCommitErrorPageIgnoredForFramePendingDeletion \
  BrowserInitiatedLoadPostCommitErrorPageIgnoredForFramePendingDeletion
#endif
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    MAYBE_BrowserInitiatedLoadPostCommitErrorPageIgnoredForFramePendingDeletion) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL url(embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHost* frame = shell()->web_contents()->GetPrimaryMainFrame();

  // Create an unload handler and force the browser process to wait before
  // deleting |frame|.
  EXPECT_TRUE(ExecJs(frame, R"(
             window.onunload=function(e){
               window.domAutomationController.send('done');
             };)"));

  // With BackForwardCache, old RenderFrameHost won't enter pending deletion
  // on navigation as it is stored in bfcache, disable back/forward cache to
  // ensure that the RFH will enter pending deletion state.
  DisableBackForwardCacheForTesting(contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // Navigate the main frame cross-process and wait for the unload event to
  // fire.
  DOMMessageQueue dom_message_queue(frame);
  GURL cross_process_url(
      embedded_test_server()->GetURL("b.com", "/page_with_iframe.html"));
  shell()->LoadURL(cross_process_url);

  std::string message;
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"done\"", message);

  // |frame| is now pending deletion.
  EXPECT_TRUE(static_cast<RenderFrameHostImpl*>(frame)->IsPendingDeletion());

  std::string error_html = "Error page";
  DidStartNavigationObserver did_start_navigation_observer(
      shell()->web_contents());
  controller.LoadPostCommitErrorPage(frame, url, error_html);

  // The error page navigation was ignored.
  EXPECT_FALSE(did_start_navigation_observer.observed());
}

// Test to verify that LoadPostCommitErrorPage works correctly when supplied
// with an about:blank url for the error page.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    BrowserInitiatedLoadPostCommitErrorPageWithAboutBlankUrl) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHost* child =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  scoped_refptr<SiteInstance> success_site_instance = child->GetSiteInstance();

  std::string error_html = "Error page";
  GURL error_url("about:blank#error");
  TestNavigationObserver error_observer(shell()->web_contents());
  controller.LoadPostCommitErrorPage(child, error_url, error_html);
  error_observer.Wait();

  // If error page isolation is enabled the `child` pointer will be invalid
  // and should be retrieved again. It is safe to do so even when disabled.
  child = ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);

  EXPECT_FALSE(error_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, error_observer.last_net_error_code());
  EXPECT_EQ(child->GetLastCommittedURL(), error_url);
  EXPECT_EQ(error_html, EvalJs(child, "document.body.innerHTML"));

  // Verify that the subframe error page committed in the correct SiteInstance.
  // process.
  EXPECT_TRUE(IsExpectedSubframeErrorTransition(success_site_instance.get(),
                                                child->GetSiteInstance()));
}

// Test to verify that LoadPostCommitErrorPage works correctly when done on a
// popup's main frame without any committed entry.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       LoadPostCommitErrorPageFromPopupWithoutCommittedEntry) {
  // Navigate to a page.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Open a popup that never finishes loading and never commits any entry.
  GURL hung_url(embedded_test_server()->GetURL("/hung"));
  Shell* popup;
  {
    ShellAddedObserver new_shell_observer;
    EXPECT_TRUE(ExecJs(shell(), "var window = window.open('/hung');"));
    popup = new_shell_observer.GetShell();
  }

  WebContentsImpl* popup_contents =
      static_cast<WebContentsImpl*>(popup->web_contents());
  NavigationControllerImpl& controller = popup_contents->GetController();
  RenderFrameHostImpl* popup_main_frame = popup_contents->GetPrimaryMainFrame();
  scoped_refptr<SiteInstance> original_site_instance =
      popup_main_frame->GetSiteInstance();

  // The last committed URL is the empty URL, as it never committed.
  EXPECT_EQ(GURL(), popup_main_frame->GetLastCommittedURL());
  EXPECT_FALSE(popup_main_frame->has_committed_any_navigation());

  // Call LoadPostCommitErrorPage on the popup.
  std::string error_html = "Error page";
  TestNavigationObserver error_observer(popup_contents);
  controller.LoadPostCommitErrorPage(
      popup_main_frame, popup_main_frame->GetLastCommittedURL(), error_html);
  error_observer.Wait();

  // The post-commit error page committed an error page and sets the last
  // committed URL to about:blank.
  popup_main_frame = popup_contents->GetPrimaryMainFrame();
  EXPECT_EQ(popup_main_frame->GetLastCommittedURL(), GURL("about:blank"));
  EXPECT_FALSE(error_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, error_observer.last_net_error_code());
  EXPECT_EQ(error_html, EvalJs(popup_main_frame, "document.body.innerHTML"));

  // Verify that the error page committed in the error page process.
  scoped_refptr<SiteInstance> error_site_instance =
      popup_main_frame->GetSiteInstance();
  EXPECT_NE(original_site_instance, error_site_instance);
  EXPECT_EQ(GURL(kUnreachableWebDataURL), error_site_instance->GetSiteURL());

  // The URL displayed in the URL bar is about:blank.
  EXPECT_EQ(GURL("about:blank"), popup_contents->GetVisibleURL());
  // The opener frame can still access the popup window.
  EXPECT_EQ(false, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                          "!!(window.closed)"));
}

// Test to verify that LoadPostCommitErrorPage works correctly when done on an
// iframe without any committed entry.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       LoadPostCommitErrorPageFromFrameWithoutCommittedEntry) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  // Navigate to a page.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Add an iframe that never finishes loading and never commits any entry.
  GURL hung_url(embedded_test_server()->GetURL("/hung"));
  {
    TestNavigationManager hung_nav(contents(), hung_url);
    EXPECT_TRUE(ExecJs(shell(), R"(
          var iframe = document.createElement('iframe');
          iframe.src = '/hung';
          document.body.appendChild(iframe);
    )"));
    EXPECT_TRUE(hung_nav.WaitForRequestStart());
  }

  RenderFrameHostImpl* child = static_cast<RenderFrameHostImpl*>(
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0));
  scoped_refptr<SiteInstance> success_site_instance = child->GetSiteInstance();
  // The last committed URL is the empty URL, as it never committed.
  EXPECT_EQ(GURL(), child->GetLastCommittedURL());
  EXPECT_FALSE(child->has_committed_any_navigation());
  // The main frame can initially access the iframe's contentDocument.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         "!!(iframe.contentDocument)"));

  // Call LoadPostCommitErrorPage on the iframe.
  std::string error_html = "Error page";
  TestNavigationObserver error_observer(shell()->web_contents());
  controller.LoadPostCommitErrorPage(child, child->GetLastCommittedURL(),
                                     error_html);
  error_observer.Wait();

  // The post-commit error page committed an error page and sets the last
  // committed URL to about:blank.
  child = static_cast<RenderFrameHostImpl*>(
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0));
  EXPECT_EQ(child->GetLastCommittedURL(), GURL("about:blank"));
  EXPECT_FALSE(error_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, error_observer.last_net_error_code());
  EXPECT_EQ(error_html, EvalJs(child, "document.body.innerHTML"));

  // Verify that the subframe error page committed in the correct
  // SiteInstance.
  EXPECT_TRUE(IsExpectedSubframeErrorTransition(success_site_instance.get(),
                                                child->GetSiteInstance()));

  // Since the iframe is now showing an error page, the main frame can no longer
  // access its contentDocument.
  EXPECT_EQ(false, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                          "!!(iframe.contentDocument)"));
}

// Similar to LoadPostCommitErrorPageFromFrameWithoutCommittedEntry, but with
// the addition of CSP that will block the iframe from navigating to an empty
// URL (but not about:blank).
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    LoadPostCommitErrorPageFromFrameWithoutCommittedEntryBlockedByCSP) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  // Navigate to a page.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Add frame-src CSP via a new <meta> element that only allows same-origin
  // iframe.
  EXPECT_TRUE(ExecJs(shell(),
                     R"(var meta = document.createElement('meta');
             meta.httpEquiv = 'Content-Security-Policy';
             meta.content = "frame-src 'self'";
             document.getElementsByTagName('head')[0].appendChild(meta);)"));

  // Add an iframe that never finishes loading and never commits any entry.
  GURL hung_url(embedded_test_server()->GetURL("/hung"));
  {
    TestNavigationManager hung_nav(contents(), hung_url);
    EXPECT_TRUE(ExecJs(shell(), R"(
          var iframe = document.createElement('iframe');
          iframe.src = '/hung';
          document.body.appendChild(iframe);
    )"));
    EXPECT_TRUE(hung_nav.WaitForRequestStart());
  }

  RenderFrameHostImpl* child = static_cast<RenderFrameHostImpl*>(
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0));
  scoped_refptr<SiteInstance> success_site_instance = child->GetSiteInstance();
  // The last committed URL is the empty URL, as it never committed.
  EXPECT_EQ(GURL(), child->GetLastCommittedURL());
  EXPECT_FALSE(child->has_committed_any_navigation());
  // The main frame can initially access the iframe's contentDocument.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         "!!(iframe.contentDocument)"));

  // Call LoadPostCommitErrorPage on the iframe.
  std::string error_html = "Error page";
  TestNavigationObserver error_observer(shell()->web_contents());
  controller.LoadPostCommitErrorPage(child, child->GetLastCommittedURL(),
                                     error_html);
  error_observer.Wait();

  // The post-commit error page committed an error page and sets the last
  // committed URL to about:blank, which is allowed by CSP because it's the same
  // origin as the main frame (because of origin inheritance). So, the net error
  // code is still ERR_BLOCKED_BY_CLIENT instead of ERR_BLOCKED_BY_CSP.
  EXPECT_EQ(child->GetLastCommittedURL(), GURL("about:blank"));
  EXPECT_FALSE(error_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, error_observer.last_net_error_code());
  EXPECT_EQ(error_html, EvalJs(child, "document.body.innerHTML"));

  // Verify that the subframe error page committed in the correct
  // SiteInstance.
  EXPECT_TRUE(IsExpectedSubframeErrorTransition(success_site_instance.get(),
                                                child->GetSiteInstance()));

  // Since the iframe is now showing an error page, the main frame can no longer
  // access its contentDocument.
  EXPECT_EQ(false, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                          "!!(iframe.contentDocument)"));
}

// Starts a navigation to |url_to_start_| just before the DidCommitNavigation
// call for |url_to_intercept_| gets processed.
class NavigationStarterBeforeDidCommitNavigation
    : public DidCommitNavigationInterceptor {
 public:
  NavigationStarterBeforeDidCommitNavigation(WebContentsImpl* web_contents,
                                             Shell* shell,
                                             const GURL& url_to_intercept,
                                             const GURL& url_to_start)
      : DidCommitNavigationInterceptor(web_contents),
        shell_(shell),
        url_to_intercept_(url_to_intercept),
        url_to_start_(url_to_start) {}
  ~NavigationStarterBeforeDidCommitNavigation() override = default;

 private:
  // DidCommitNavigationInterceptor:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    if ((**params).url == *url_to_intercept_) {
      shell_->LoadURL(*url_to_start_);
    }
    return true;
  }

  raw_ptr<Shell> shell_;
  const raw_ref<const GURL> url_to_intercept_;
  const raw_ref<const GURL> url_to_start_;
};

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       MultipleSameDocumentNavigations) {
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  GURL url2(embedded_test_server()->GetURL("/title1.html#foo"));
  GURL url3(embedded_test_server()->GetURL("/title1.html#bar"));
  // Navigate to a page.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  // Do a same-document navigation to |url2|, and start a same-document
  // navigation to |url3| before the DidCommitNavigation message from the |url2|
  // navigation gets processed, causing the NavigationRequest stored for |url2|
  // to be replaced by the NavigationRequest for |url3| if we only allow saving
  // one NavigationRequest for same-document navigations at a time. This will
  // result in the re-creation of the NavigationRequest for |url2|, which might
  // get some important attributes wrong.
  NavigationStarterBeforeDidCommitNavigation url3_navigation_starter(
      contents(), shell(), url2, url3);
  TestNavigationObserver navigations_observer(shell()->web_contents(), 2);
  FrameNavigateParamsCapturer url2_capturer(
      contents()->GetPrimaryFrameTree().root());
  shell()->LoadURL(url2);
  url2_capturer.Wait();
  // The navigation to |url2| must be correctly recognized as a
  // browser-initiated same-document navigation.
  EXPECT_FALSE(url2_capturer.is_renderer_initiated());
  EXPECT_TRUE(url2_capturer.is_same_document());
  navigations_observer.Wait();
  EXPECT_EQ(url3, contents()->GetLastCommittedURL());
}

// Test to verify that after loading a post-commit error page, back is treated
// as navigating to the entry prior to the page that was active when the
// post-commit error page was triggered.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       BackOnBrowserInitiatedErrorPageNavigation) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  GURL url2(embedded_test_server()->GetURL("/title2.html"));

  // Navigate to a valid page.
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  int initial_entry_index = controller.GetLastCommittedEntryIndex();

  // Navigate to a different page.
  EXPECT_TRUE(NavigateToURL(shell(), url2));

  // Trigger a post-commit error page navigation.
  TestNavigationObserver error_observer(shell()->web_contents());
  controller.LoadPostCommitErrorPage(
      shell()->web_contents()->GetPrimaryMainFrame(), url2, "Error Page");
  error_observer.Wait();
  EXPECT_EQ(PAGE_TYPE_ERROR, controller.GetLastCommittedEntry()->GetPageType());
  EXPECT_EQ(2, controller.GetEntryCount());

  // Make sure back is treated as going back from the page that was visible when
  // the post-commit error page was loaded.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(initial_entry_index, controller.GetLastCommittedEntryIndex());
  // Check that the next forward entry has been replaced with the original visit
  // to the site (i.e. it shouldn't be the error page).
  EXPECT_EQ(PAGE_TYPE_NORMAL, controller.GetEntryAtOffset(1)->GetPageType());
  EXPECT_EQ(url2, controller.GetEntryAtOffset(1)->GetURL());
}

// Test to verify that after loading a post-commit error page, reload
// triggers a navigation to the previous page (the page that was active when
// the navigation to an error was triggered).
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ReloadOnBrowserInitiatedErrorPageNavigation) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL url(embedded_test_server()->GetURL("/title1.html"));

  // Navigate to a valid page.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  int initial_entry_index = controller.GetLastCommittedEntryIndex();
  int initial_entry_id = controller.GetLastCommittedEntry()->GetUniqueID();

  // Trigger a post-commit error page navigation.
  TestNavigationObserver error_observer(shell()->web_contents());
  controller.LoadPostCommitErrorPage(
      shell()->web_contents()->GetPrimaryMainFrame(), url, "Error Page");
  error_observer.Wait();
  EXPECT_EQ(PAGE_TYPE_ERROR, controller.GetLastCommittedEntry()->GetPageType());
  EXPECT_EQ(1, controller.GetEntryCount());

  // Make sure reload triggers a reload of the original page, not the error,
  // and that we get back to the original entry.
  controller.Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(initial_entry_index, controller.GetLastCommittedEntryIndex());

  // We should be in the initial entry and no longer be in an error page.
  EXPECT_EQ(initial_entry_id,
            controller.GetLastCommittedEntry()->GetUniqueID());
  EXPECT_EQ(PAGE_TYPE_NORMAL,
            controller.GetLastCommittedEntry()->GetPageType());

  // The error page entry shouldn't be available as a forward navigation.
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(1, controller.GetEntryCount());
}

// Test clone behavior of post-commit error page navigations.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       CloneOnBrowserInitiatedErrorPageNavigation) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL url(embedded_test_server()->GetURL("/title2.html"));

  // Navigate to a valid page.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  int initial_entry_id = controller.GetLastCommittedEntry()->GetUniqueID();
  std::u16string initial_title = controller.GetLastCommittedEntry()->GetTitle();
  // Trigger a post-commit error page navigation.
  TestNavigationObserver error_observer(shell()->web_contents());
  controller.LoadPostCommitErrorPage(
      shell()->web_contents()->GetPrimaryMainFrame(), url, "Error Page");
  error_observer.Wait();
  EXPECT_EQ(PAGE_TYPE_ERROR, controller.GetLastCommittedEntry()->GetPageType());
  EXPECT_EQ(1, controller.GetEntryCount());

  // Clone the tab and load the entry.
  std::unique_ptr<WebContents> new_tab = shell()->web_contents()->Clone();
  WebContentsImpl* new_tab_impl = static_cast<WebContentsImpl*>(new_tab.get());
  NavigationController& new_controller = new_tab_impl->GetController();
  EXPECT_TRUE(new_controller.IsInitialNavigation());
  EXPECT_TRUE(new_controller.NeedsReload());
  // TODO(carlosil): Before we load, the entry on the new controller is a clone
  // of the post commit error page entry. This is mostly ok since after the load
  // we end up in the right page, but causes navigation state to be lost,
  // ideally we should clone the entry replaced by the error page instead.
  EXPECT_EQ(PAGE_TYPE_ERROR,
            new_controller.GetLastCommittedEntry()->GetPageType());
  {
    TestNavigationObserver clone_observer(new_tab.get());
    new_controller.LoadIfNecessary();
    clone_observer.Wait();
  }
  // The entry on the new controller should be a new one.
  EXPECT_NE(initial_entry_id,
            new_controller.GetLastCommittedEntry()->GetUniqueID());
  // The new entry should keep the URL from the initial navigation, which means
  // after the load it should navigate to the initial page, not to the error.
  EXPECT_EQ(url, new_controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(initial_title, new_controller.GetLastCommittedEntry()->GetTitle());
  EXPECT_EQ(PAGE_TYPE_NORMAL,
            new_controller.GetLastCommittedEntry()->GetPageType());

  // Only one entry should exist in the controller of the cloned tab.
  EXPECT_EQ(1, new_controller.GetEntryCount());
}

// Tests that a same document navigation followed by a client redirect
// do not add any more session history entries and going to previous entry
// works.
// It replaces invalidly behaving unit test added for http://crbug.com/40395.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTestNoServer,
                       ClientRedirectAfterSameDocumentNavigation) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/foo.html");
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // Load an initial page, which the test will eventually go back to.
  const GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(start_url, controller.GetLastCommittedEntry()->GetURL());

  const GURL main_url(embedded_test_server()->GetURL("/foo.html"));
  const GURL last_url(embedded_test_server()->GetURL("/title3.html"));

  // Navigate to foo.html which will do a same document navigation and client
  // redirect.
  TestNavigationManager observer(shell()->web_contents(), last_url);
  shell()->LoadURL(main_url);
  response.WaitForRequest();
  response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<html><script>"
      "window.location.replace('#a');"
      "window.location='/title3.html';"
      "</script></html>");
  ASSERT_TRUE(observer.WaitForNavigationFinished());

  EXPECT_EQ(last_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_TRUE(controller.CanGoBack());

  TestNavigationObserver back_load_observer(shell()->web_contents());
  controller.GoBack();
  back_load_observer.Wait();
  EXPECT_EQ(start_url, controller.GetLastCommittedEntry()->GetURL());
}

class SandboxedNavigationControllerBrowserTest
    : public NavigationControllerBrowserTest {
 protected:
  void SetupNavigation() {
    NavigationControllerImpl& controller =
        static_cast<NavigationControllerImpl&>(
            shell()->web_contents()->GetController());
    GURL preload_url(embedded_test_server()->GetURL(
        "/navigation_controller/page_with_links.html"));
    EXPECT_TRUE(NavigateToURL(shell(), preload_url));
    ASSERT_EQ(1, controller.GetEntryCount());

    GURL main_url(embedded_test_server()->GetURL(
        "/navigation_controller/page_with_sandbox_iframe.html"));
    EXPECT_TRUE(NavigateToURL(shell(), main_url));
    ASSERT_EQ(2, controller.GetEntryCount());

    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();
    ASSERT_EQ(2U, root->child_count());
    ASSERT_NE(nullptr, root->child_at(0));
    ASSERT_NE(nullptr, root->child_at(1));
    ASSERT_NE(nullptr, root->child_at(1)->child_at(0));

    GURL sub_subframe_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    // Navigate sibling frame to simple_page_2.
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), sub_subframe_url));
    ASSERT_EQ(3, controller.GetEntryCount());

    // Navigate sandbox frame to simple_page_2.
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1)->child_at(0),
                                          sub_subframe_url));
    ASSERT_EQ(4, controller.GetEntryCount());

    // Click link inside sandboxed iframe.
    std::string script = "document.getElementById('test_anchor').click()";
    EXPECT_TRUE(ExecJs(root->child_at(1), script));
    ASSERT_EQ(5, controller.GetEntryCount());
    EXPECT_EQ(4, controller.GetCurrentEntryIndex());

    // History should now be:
    // [preload_url, main(simple1, sandbox(simple1)),
    // main(simple2, sandbox(simple1)), main(simple2, sandbox(simple2)),
    // *main(simple2, sandbox#test(simple2))]
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests navigations which occur from a sandboxed frame are prevented.
IN_PROC_BROWSER_TEST_P(SandboxedNavigationControllerBrowserTest,
                       TopLevelNavigationFromSandboxSource) {
  SetupNavigation();

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  std::string back_script = "history.back();";
  std::string forward_script = "history.forward();";

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  // Navigate sandbox frame back same-document.
  EXPECT_TRUE(ExecJs(root->child_at(1), back_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(3, controller.GetCurrentEntryIndex());

  // Navigate innermost frame back cross-document.
  EXPECT_TRUE(ExecJs(root->child_at(1), back_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());

  // Navigate sibling frame back cross-document. It should fail.
  EXPECT_TRUE(ExecJs(root->child_at(1), back_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());

  // Try it again and it should fail.
  EXPECT_TRUE(ExecJs(root->child_at(1), back_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());

  // Do it browser initiated. Make sure histograms don't change.
  ASSERT_TRUE(controller.CanGoBack());
  controller.GoBack();
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Go forward to reset state, then a mouse back button navigation.
  // Using the mouse back button should be allowed because it is a
  // UA level default action even though it originates from the
  // renderer side. The sandbox policy shouldn't be applied when it
  // doesn't originate from a script.
  controller.GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  root->child_at(1)
      ->current_frame_host()
      ->GetRenderWidgetHost()
      ->ForwardMouseEvent(blink::WebMouseEvent(
          blink::WebInputEvent::Type::kMouseUp, gfx::PointF(), gfx::PointF(),
          blink::WebPointerProperties::Button::kBack, 0, 0,
          base::TimeTicks::Now()));
  RunUntilInputProcessed(
      root->child_at(1)->current_frame_host()->GetRenderWidgetHost());
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
}

class SandboxedNavigationControllerWithBfcacheBrowserTest
    : public NavigationControllerBrowserTest {
 protected:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    NavigationControllerBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests navigations which occur from a sandboxed frame are prevented.
IN_PROC_BROWSER_TEST_P(SandboxedNavigationControllerWithBfcacheBrowserTest,
                       BackNavigationToCachedPageNotAllowed) {
  GURL cached_url(embedded_test_server()->GetURL("a.com", "/title1.html"));

  GURL main_url(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/page_with_sandbox_iframe.html"));

  EXPECT_TRUE(NavigateToURL(shell(), cached_url));
  RenderFrameHostImpl* cached_rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  content::RenderFrameDeletedObserver observer(cached_rfh);

  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  ASSERT_FALSE(observer.deleted());
  EXPECT_TRUE(cached_rfh->IsInBackForwardCache());

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(2UL, root->child_count());
  FrameTreeNode* sanboxed_iframe = root->child_at(1);

  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  // Navigation not allowed. It should fail.
  EXPECT_TRUE(ExecJs(sanboxed_iframe, "history.back();"));
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
}

class SandboxedNavigationControllerPopupBrowserTest
    : public NavigationControllerBrowserTest {
 protected:
  void SetupNavigation() {
    EXPECT_EQ(1u, Shell::windows().size());
    NavigationControllerImpl& controller =
        static_cast<NavigationControllerImpl&>(
            shell()->web_contents()->GetController());
    GURL preload_url(embedded_test_server()->GetURL(
        "/navigation_controller/page_with_sandboxed_iframe_popup.html"));
    EXPECT_TRUE(NavigateToURL(shell(), preload_url));
    ASSERT_EQ(1, controller.GetEntryCount());

    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();
    ASSERT_EQ(1U, root->child_count());
    ASSERT_NE(nullptr, root->child_at(0));
    ShellAddedObserver new_shell_observer;
    // Click link inside sandboxed iframe, causing popup open.
    std::string script = "document.getElementById('test_link').click()";
    EXPECT_TRUE(ExecJs(root->child_at(0), script));

    popup_shell_ = new_shell_observer.GetShell();
    EXPECT_TRUE(WaitForLoadStop(popup_shell_->web_contents()));
    FrameTreeNode* popup_root =
        static_cast<WebContentsImpl*>(popup_shell_->web_contents())
            ->GetPrimaryFrameTree()
            .root();
    // Click link inside sandboxed popup, causing the frame to have an
    // additional entry in history state.
    std::string popup_script = "document.getElementById('test_anchor').click()";
    EXPECT_TRUE(ExecJs(popup_root, popup_script));
  }

 protected:
  raw_ptr<Shell, DanglingUntriaged> popup_shell_ = nullptr;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests navigations that sandboxed top level frames still
// can navigate.
IN_PROC_BROWSER_TEST_P(SandboxedNavigationControllerPopupBrowserTest,
                       NavigateSelf) {
  SetupNavigation();

  std::string back_script = "history.back();";
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(popup_shell_->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      popup_shell_->web_contents()->GetController());
  ASSERT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  // Navigate sandboxed top frame back.
  EXPECT_TRUE(ExecJs(root, back_script));
  EXPECT_TRUE(WaitForLoadStop(popup_shell_->web_contents()));
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
}

class NavigationControllerMainDocumentSequenceNumberBrowserTest
    : public NavigationControllerBrowserTest,
      public WebContentsObserver {
 protected:
  void SetUpOnMainThread() override {
    NavigationControllerBrowserTest::SetUpOnMainThread();

    WebContentsObserver::Observe(shell()->web_contents());
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    main_frame_document_sequence_numbers_.push_back(
        shell()
            ->web_contents()
            ->GetController()
            .GetLastCommittedEntry()
            ->GetMainFrameDocumentSequenceNumber());
  }

  // Document sequence numbers monotonically increase during the entire lifetime
  // of the browser process. Renumber them starting at 1 to make testing easier.
  std::vector<int64_t> GetProcessedMainDocumentSequenceNumbers() {
    std::vector<int64_t> ids = main_frame_document_sequence_numbers_;
    std::sort(ids.begin(), ids.end());

    std::map<int64_t, int64_t> compressor;
    int current_id = 0;
    for (int64_t value : ids) {
      if (compressor.find(value) == compressor.end())
        compressor[value] = ++current_id;
    }

    std::vector<int64_t> result;
    for (int64_t value : main_frame_document_sequence_numbers_)
      result.push_back(compressor[value]);

    return result;
  }

 private:
  std::vector<int64_t> main_frame_document_sequence_numbers_;
};

IN_PROC_BROWSER_TEST_P(
    NavigationControllerMainDocumentSequenceNumberBrowserTest,
    SubframeNavigation) {
  const GURL url1(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  const GURL url2(embedded_test_server()->GetURL("/title1.html"));
  const GURL url3(embedded_test_server()->GetURL("/title2.html"));
  const char kChildFrameId[] = "child0";

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  // The navigation entries are:
  // [*url1(subframe)]

  EXPECT_TRUE(
      NavigateIframeToURL(shell()->web_contents(), kChildFrameId, url2));
  // The navigation entries are:
  // [url1(subframe), *url1(url2)]

  EXPECT_TRUE(NavigateToURL(shell(), url3));
  // The navigation entries are:
  // [url1(subframe), url1(url2), *url3]

  {
    // We are waiting for two navigations here: main frame and subframe.
    // However, when back/forward cache is enabled, back navigation to a page
    // with subframes will not trigger a subframe navigation (since the
    // subframe is cached with the page).
    TestNavigationObserver navigation_observer(
        shell()->web_contents(), IsBackForwardCacheEnabled() ? 1 : 2);
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url1(subframe), *url1(url2), url3]

  // Main document and child document navigation from the first NavigateToURL
  // and the subframe navigation from NavigateIframeToURL are related to the
  // first main document.
  // The second NavigateToURL navigates to a new main document.
  // The back navigation navigates back both main document and a child document
  // and they are related to the first main document (except when same-site
  // back/forward cache is enabled, where we only navigate the main frame, since
  // the subframe is cached and doesn't need reconstruction/navigation).
  if (!IsBackForwardCacheEnabled()) {
    EXPECT_THAT(GetProcessedMainDocumentSequenceNumbers(),
                ElementsAre(1, 1, 1, 2, 1, 1));
  } else {
    EXPECT_THAT(GetProcessedMainDocumentSequenceNumbers(),
                ElementsAre(1, 1, 1, 2, 1));
  }
}

IN_PROC_BROWSER_TEST_P(
    NavigationControllerMainDocumentSequenceNumberBrowserTest,
    SameDocument) {
  const GURL url1(embedded_test_server()->GetURL("/title1.html"));
  const GURL url1_fragment(embedded_test_server()->GetURL("/title1.html#id_1"));
  const GURL url2(embedded_test_server()->GetURL("/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));
  // The navigation entries are:
  // [*url1]

  EXPECT_TRUE(NavigateToURL(shell(), url1_fragment));
  // The navigation entries are:
  // [url1, *url1_fragment]

  EXPECT_TRUE(NavigateToURL(shell(), url2));
  // The navigation entries are:
  // [url1, url1_fragment, *url2]

  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.WaitForNavigationFinished();
  }
  // The navigation entries are:
  // [url1, *url1_fragment, url2]

  EXPECT_THAT(GetProcessedMainDocumentSequenceNumbers(),
              ElementsAre(1, 1, 2, 1));
}

namespace {

class DidCommitNavigationCanceller : public DidCommitNavigationInterceptor {
  using CallbackScriptRunner = base::OnceCallback<void()>;

 public:
  DidCommitNavigationCanceller(WebContents* web_contents,
                               const GURL& url_to_cancel,
                               CallbackScriptRunner callback)
      : DidCommitNavigationInterceptor(web_contents),
        url_to_cancel_(url_to_cancel) {
    callback_ = std::move(callback);
  }

  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    if (navigation_request->GetURL() == url_to_cancel_) {
      std::move(callback_).Run();
      return false;
    }
    return true;
  }

 private:
  CallbackScriptRunner callback_;
  GURL url_to_cancel_;
};

}  //  namespace

// When running OpenURL to an invalid URL on a frame proxy it should not spoof
// the url by canceling a main frame navigation.
// See https://crbug.com/966914.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       CrossProcessIframeToInvalidURLCancelsRedirectSpoof) {
  // This tests something that can only happened with out of process iframes.
  if (!AreAllSitesIsolatedForTesting())
    return;

  if (ShouldQueueNavigationsWhenPendingCommitRFHExists()) {
    // If navigation queueing is enabled, the first navigation will be stuck at
    // the "pending commit" stage forever, causing the second navigation to be
    // queued indefinitely, and the test will timeout.
    // TODO(https://crbug.com/1220337): Rewrite the test to defer the first
    // navigation instead, so that it won't cause the second navigation to be
    // stuck.
    return;
  }

  const GURL main_frame_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  const GURL main_frame_url_2(embedded_test_server()->GetURL("/title2.html"));

  // Load the initial page, containing a fully scriptable cross-site iframe.
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  FrameTreeNode* iframe = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root()
                              ->child_at(0);

  DidCommitNavigationCanceller canceller(
      shell()->web_contents(), main_frame_url_2,
      base::BindLambdaForTesting([iframe]() {
        EXPECT_TRUE(ExecJs(
            iframe, "parent.location.href = 'chrome-untrusted://1234';"));
      }));

  // This navigation will be raced by a navigation started in the iframe.
  // The NavigationRequest for the first navigation will already be in the
  // RenderFrameHost at this point, and the iframe proxy navigation will
  // proceed because we don't have a FrameTreeNode ongoing navigation.
  // So the main navigation will be cancelled first, by the iframe navigation
  // taking precedence, and the iframe navigation will not get passed network
  // because of the invalid url, getting cancelled as well.
  EXPECT_FALSE(NavigateToURL(shell(), main_frame_url_2));

  // Check that no spoof happened.
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(controller.GetVisibleEntry()->GetVirtualURL(),
            shell()->web_contents()->GetLastCommittedURL());
}

// Tests a renderer aborting the navigation it started, while still waiting on a
// long cross-process subframe beforeunload handler.
// Regression test: https://crbug.com/972154
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    NavigationAbortDuringLongCrossProcessIframeBeforeUnload) {
  // This test relies on the main frame and the iframe to live in different
  // processes. This allows one renderer process to cancel a navigation while
  // the other renderer process is busy executing its beforeunload handler.
  if (!AreAllSitesIsolatedForTesting())
    return;

  const GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  const GURL navigated_url(
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();

  // Have a first page with a cross process iframe.
  // The iframe itself does have a dialog-showing beforeunload handler.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  std::string script =
      "window.addEventListener('beforeunload', function (event) {"
      "  event.returnValue='blocked'"
      "});";
  EXPECT_TRUE(ExecJs(root->child_at(0), script));

  NavigationHandleObserver abort_observer(web_contents, navigated_url);
  BeforeUnloadBlockingDelegate beforeunload_pauser(web_contents);

  // Navigate to any page, renderer initiated.
  EXPECT_TRUE(ExecJs(shell(), "location.href = 'title1.html'"));

  // The previous navigation is paused while the beforeunloadhandler dialog is
  // shown to the user. In the meantime, the navigation is aborted:
  beforeunload_pauser.Wait();
  EXPECT_TRUE(ExecJs(shell(), "document.write()"));  // Cancel the navigation.

  // Wait for javascript to get processed, and its consequences (aborting the
  // navigation) to finish. To achieve that we simply run script again.
  EXPECT_EQ(42, EvalJs(shell(), "12 + 30").ExtractInt());

  // Verify that the navigation was aborted as expected.
  EXPECT_FALSE(abort_observer.has_committed());
}

namespace {

// A request handler that returns a simple page for requests using |method| to
// /handle-method-only, and closes the socket for any other method.
std::unique_ptr<net::test_server::HttpResponse> HandleMethodOnly(
    net::test_server::HttpMethod method,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/handle-method-only")
    return nullptr;

  if (request.method != method) {
    return std::make_unique<net::test_server::RawHttpResponse>("", "");
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  response->set_content("Success!");
  return response;
}

}  // namespace

// Tests that the navigation entry's method is updated to GET when following a
// 301 redirect that encounters an error page. See https://crbug.com/1041597.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTestNoServer,
                       UpdateMethodOn301RedirectError) {
  // HandleMethodOnly serves the final endpoint that the test ends up at. It
  // lets the test distinguish a GET from a POST by serving a response only for
  // POST requests.
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleMethodOnly, net::test_server::METHOD_POST));
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL start_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL post_only_url = embedded_test_server()->GetURL("/handle-method-only");
  GURL form_action_url(embedded_test_server()->GetURL("/server-redirect-301?" +
                                                      post_only_url.spec()));

  // Inject a form into the page and submit it, to create a POST request to
  // |form_action_url|. This POST request will redirect to |post_only_url|. The
  // request's method should change to GET while following the redirect,
  // resulting in an error page since |post_only_url| closes the connection on
  // GETs.
  TestNavigationObserver form_nav_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("var form = document.createElement('form');"
                               "form.method = 'POST';"
                               "form.action = $1;"
                               "document.body.appendChild(form);"
                               "form.submit();",
                               form_action_url.spec())));
  form_nav_observer.Wait();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(post_only_url, entry->GetURL());
  EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
  EXPECT_FALSE(entry->GetHasPostData());

  // When the error page is reloaded, the method should still be GET, resulting
  // in an error page. If https://crbug.com/1041597 regresses, the
  // NavigationEntry's method would be POST and this test would fail, seeing a
  // successful response instead of an error page from |post_only_url|.
  TestNavigationObserver reload_observer(shell()->web_contents(), 1);
  // Set |check_for_repost| to false to avoid hanging the test if the method is
  // improperly set to POST.
  controller.Reload(ReloadType::NORMAL, false);
  reload_observer.Wait();
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(post_only_url, entry->GetURL());
  EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
}

// Tests that the navigation entry's method is preserved as POST when following
// a 307 redirect that encounters an error page. This test is similar to the
// above UpdateMethodOn301RedirectError, but reversed: in this test, the method
// should be preserved as POST.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTestNoServer,
                       UpdateMethodOn307RedirectError) {
  // HandleMethodOnly serves the final endpoint that the test ends up at. It
  // lets the test distinguish a GET from a POST by serving a response only for
  // GET requests.
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleMethodOnly, net::test_server::METHOD_GET));
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL start_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL get_only_url = embedded_test_server()->GetURL("/handle-method-only");
  GURL form_action_url(embedded_test_server()->GetURL("/server-redirect-307?" +
                                                      get_only_url.spec()));

  // Inject a form into the page and submit it, to create a POST request to
  // |form_action_url|. This POST request will redirect to |get_only_url|. The
  // request's method should stay as POST while following the redirect,
  // resulting in an error page since |get_only_url| closes the connection on
  // POSTs.
  TestNavigationObserver form_nav_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("var form = document.createElement('form');"
                               "form.method = 'POST';"
                               "form.action = $1;"
                               "document.body.appendChild(form);"
                               "form.submit();",
                               form_action_url.spec())));
  form_nav_observer.Wait();
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(get_only_url, entry->GetURL());
  EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
  EXPECT_TRUE(entry->GetHasPostData());

  // When the error page is reloaded, the method should still be POST, resulting
  // in an error page.
  TestNavigationObserver reload_observer(shell()->web_contents(), 1);
  // Set |check_for_repost| to false to avoid hanging the test with the prompt.
  controller.Reload(ReloadType::NORMAL, false);
  reload_observer.Wait();
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(get_only_url, entry->GetURL());
  EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
}

// Test for a navigation that is
// 1) initiated by a cross-site frame
// 2) same-document
// 3) to a http URL with port 0.
//
// The history: before https://crbug.com/1065532 was fixed, this was a browser
// crash; afterwards, but before crbug.com/1136678 was fixed, it led to a
// renderer kill; now, it should just be a failed navigation (assuming port 0 is
// unreachable).
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       SameDocumentNavigationToHttpPortZero) {
  // The test server doesn't support port 0 (and, more generally, serving files
  // from a specific port), so we add a URLLoaderInterceptor that will provide
  // a response to our requests to port 0 later on.
  auto interceptor = URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
      "content/test/data", GURL("http://another-site.com:0"));

  GURL page_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), page_url));

  // Inject a HTTP subframe.
  const char kSubframeScriptTemplate[] = R"(
      var iframe = document.createElement('iframe');
      iframe.src = $1;
      document.body.appendChild(iframe);
  )";
  GURL subframe_initial_url =
      embedded_test_server()->GetURL("another-site.com", "/title2.html");
  {
    TestNavigationObserver subframe_injection_observer(shell()->web_contents(),
                                                       1);
    ASSERT_TRUE(ExecJs(
        shell(), JsReplace(kSubframeScriptTemplate, subframe_initial_url)));
    subframe_injection_observer.Wait();
    ASSERT_TRUE(subframe_injection_observer.last_navigation_succeeded());
  }

  // From the main page initiate a navigation of the cross-site subframe to a
  // http URL that has port=0.  Note that this is valid port according to the
  // URL spec (https://url.spec.whatwg.org/#port-state).
  //
  // Before the fix for SchemeHostPort's handling of port=0 the navigation
  // below would produce a browser process CHECK/crash, because port 0 confused
  // url::Origin::Resolve.
  GURL::Replacements replace_port_and_ref;
  replace_port_and_ref.SetPortStr("0");
  replace_port_and_ref.SetRefStr("someRef");
  GURL subframe_ref_url =
      subframe_initial_url.ReplaceComponents(replace_port_and_ref);

  FrameTreeNode* subframe_tree_node =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->child_at(0);
  // Shouldn't crash.
  ASSERT_TRUE(NavigateToURLFromRenderer(subframe_tree_node, subframe_ref_url));

  // As a reasonableness check, make sure we committed the right URL:
  EXPECT_EQ(subframe_tree_node->current_frame_host()->GetLastCommittedURL(),
            GURL("http://another-site.com:0/title2.html#someRef"));
  EXPECT_EQ(subframe_tree_node->current_frame_host()->GetLastCommittedOrigin(),
            url::Origin::Create(GURL("http://another-site.com:0")));
}

// Navigating a subframe to the same URL should not append a new history entry.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       NoHistoryOnNavigationToSameUrl) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // 1) Test navigating a same-site subframe to the same URL.
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(a)")));

  FrameTreeNode* child = contents()->GetPrimaryFrameTree().root()->child_at(0);
  GURL child_url = child->current_url();
  NavigationEntryImpl* previous_entry = controller.GetLastCommittedEntry();
  scoped_refptr<FrameNavigationEntry> previous_frame_entry =
      previous_entry->GetFrameEntry(child);
  EXPECT_EQ(1, controller.GetEntryCount());

  {
    // Navigate the subframe (renderer-initiated) to the same URL it's currently
    // on.
    FrameNavigateParamsCapturer capturer(child);
    ASSERT_TRUE(NavigateToURLFromRenderer(child, child_url));
    capturer.Wait();

    // We reused the previous NavigationEntry and FNE, but replaced the entry in
    // the renderer.
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(previous_frame_entry,
              controller.GetLastCommittedEntry()->GetFrameEntry(child));
    EXPECT_TRUE(capturer.did_replace_entry());
  }

  // 2) Test navigating a cross-site subframe to the same URL.
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b)")));
  EXPECT_EQ(2, controller.GetEntryCount());

  child = contents()->GetPrimaryFrameTree().root()->child_at(0);
  child_url = child->current_url();

  {
    // Replace history.state to "foo".
    ReplaceState(child, "foo");
    EXPECT_EQ("foo", EvalJs(child, "history.state"));

    previous_entry = controller.GetLastCommittedEntry();
    previous_frame_entry = previous_entry->GetFrameEntry(child);

    // Navigate the subframe (renderer-initiated) to the same URL it's currently
    // on.
    FrameNavigateParamsCapturer capturer(child);
    ASSERT_TRUE(NavigateToURLFromRenderer(child, child_url));
    capturer.Wait();

    // We reused the previous NavigationEntry and FNE, but replaced the entry in
    // the renderer.
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(previous_frame_entry,
              controller.GetLastCommittedEntry()->GetFrameEntry(child));
    EXPECT_TRUE(capturer.did_replace_entry());

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(child, "history.state"));
  }

  {
    // Replace history.state to "foo".
    ReplaceState(child, "foo");
    EXPECT_EQ("foo", EvalJs(child, "history.state"));

    previous_entry = controller.GetLastCommittedEntry();
    previous_frame_entry = previous_entry->GetFrameEntry(child);

    // Navigate the subframe (browser-initiated) to the same URL it's currently
    // on.
    FrameNavigateParamsCapturer capturer(child);
    ASSERT_TRUE(NavigateFrameToURL(child, child_url));
    capturer.Wait();

    // The navigation got converted into a reload - we reused the previous
    // NavigationEntry, FNE, and didn't do replacement in the renderer.
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(previous_frame_entry,
              controller.GetLastCommittedEntry()->GetFrameEntry(child));
    EXPECT_FALSE(capturer.did_replace_entry());

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(child, "history.state"));
  }

  // 3) Test navigating the subframe to the same URL, but it ends up in an error
  // page due to network error.
  {
    // Replace history.state to "foo".
    ReplaceState(child, "foo");
    EXPECT_EQ("foo", EvalJs(child, "history.state"));

    previous_entry = controller.GetLastCommittedEntry();
    previous_frame_entry = previous_entry->GetFrameEntry(child);

    // Navigate the subframe (browser-initiated) to the same URL it's currently
    // on, but end up in an error page instead.
    auto url_loader_interceptor = std::make_unique<URLLoaderInterceptor>(
        base::BindRepeating([](URLLoaderInterceptor::RequestParams* params) {
          network::URLLoaderCompletionStatus status;
          status.error_code = net::ERR_NOT_IMPLEMENTED;
          params->client->OnComplete(status);
          return true;
        }));

    FrameNavigateParamsCapturer capturer(child);
    ASSERT_FALSE(NavigateFrameToURL(child, child_url));
    capturer.Wait();

    // We reused the previous NavigationEntry and FNE, but replaced the entry in
    // the renderer.
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(previous_frame_entry,
              controller.GetLastCommittedEntry()->GetFrameEntry(child));
    EXPECT_TRUE(capturer.did_replace_entry());
  }

  // 4) Test successfully navigating the subframe to the same URL after a failed
  // navigation.
  {
    previous_entry = controller.GetLastCommittedEntry();
    previous_frame_entry = previous_entry->GetFrameEntry(child);

    // Navigate the subframe (browser-initiated) to the same URL it's currently
    // on successfully (instead of ending up in an error page again).
    TestNavigationObserver observer(shell()->web_contents());
    FrameNavigateParamsCapturer capturer(child);
    ASSERT_TRUE(NavigateFrameToURL(child, child_url));
    capturer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());

    // The navigation got converted into a reload - we reused the previous
    // NavigationEntry, FNE, and didn't do replacement in the renderer.
    // TODO(https://crbug.com/1188956): Once error-page isolation for subframes
    // is turned on, this should do replacement.
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(previous_frame_entry,
              controller.GetLastCommittedEntry()->GetFrameEntry(child));
    EXPECT_FALSE(capturer.did_replace_entry());

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(child, "history.state"));
  }
}

// Navigating a subframe to the same URL when the URL has a fragment
// should not append a new history entry.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       NoHistoryOnNavigationToSameURLWithFragment) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(a)")));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* child = contents()->GetPrimaryFrameTree().root()->child_at(0);
  GURL child_url = child->current_url();

  {
    // Navigate the subframe to a fragment.
    TestNavigationObserver observer(shell()->web_contents());
    FrameNavigateParamsCapturer capturer(child);
    EXPECT_TRUE(ExecJs(child, "location.href = '#bar';"));
    capturer.Wait();
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_TRUE(capturer.is_same_document());
    child_url = child->current_url();
  }

  // Replace history.state to "foo".
  ReplaceState(child, "foo");
  EXPECT_EQ("foo", EvalJs(child, "history.state"));

  NavigationEntryImpl* previous_entry = controller.GetLastCommittedEntry();
  scoped_refptr<FrameNavigationEntry> previous_frame_entry =
      previous_entry->GetFrameEntry(child);

  EXPECT_EQ(2, controller.GetEntryCount());

  {
    // Navigate to the same URL (browser-initiated).
    FrameNavigateParamsCapturer capturer(child);
    EXPECT_TRUE(NavigateFrameToURL(child, child_url));
    capturer.Wait();
    // We're classifying this as AUTO_SUBFRAME because the navigation got
    // converted into a reload.
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());

    // Since we did a reload, it's not classified as a same-document navigation.
    EXPECT_FALSE(capturer.is_same_document());

    // We reuse the last committed entry for this navigation.
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(previous_frame_entry,
              controller.GetLastCommittedEntry()->GetFrameEntry(child));
    EXPECT_EQ(2, controller.GetEntryCount());

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(child, "history.state"));
  }

  {
    // Replace history.state to "foo".
    ReplaceState(child, "foo");
    EXPECT_EQ("foo", EvalJs(child, "history.state"));

    previous_entry = controller.GetLastCommittedEntry();
    previous_frame_entry = previous_entry->GetFrameEntry(child);

    // Navigate to the same URL (renderer-initiated).
    FrameNavigateParamsCapturer capturer(child);
    EXPECT_TRUE(NavigateToURLFromRenderer(child, child_url));
    capturer.Wait();

    // We did a same-document navigation.
    EXPECT_TRUE(capturer.is_same_document());

    // We reused the previous NavigationEntry and FNE, but replaced the entry in
    // the renderer.
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(previous_frame_entry,
              controller.GetLastCommittedEntry()->GetFrameEntry(child));

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(child, "history.state"));

    EXPECT_EQ(2, controller.GetEntryCount());
  }
}

// Reloading a subframe should not append a new history entry.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       NoHistoryOnSubframeReload) {
  // Navigate to a page with a cross-site subframe.
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b)")));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  FrameTreeNode* child = root->child_at(0);

  // Replace history.state to "foo".
  ReplaceState(child, "foo");
  EXPECT_EQ("foo", EvalJs(child, "history.state"));

  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* previous_entry = controller.GetLastCommittedEntry();
  scoped_refptr<FrameNavigationEntry> previous_frame_entry =
      previous_entry->GetFrameEntry(child);

  {
    // Reload the subframe (browser-initiated).
    FrameNavigateParamsCapturer capturer(child);
    child->current_frame_host()->Reload();
    capturer.Wait();
    // We're classifying this as AUTO_SUBFRAME.
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());

    // We reused the previous NavigationEntry, FNE, and didn't do replacement in
    // the renderer.
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(previous_frame_entry,
              controller.GetLastCommittedEntry()->GetFrameEntry(child));
    EXPECT_EQ(1, controller.GetEntryCount());

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(child, "history.state"));
  }

  {
    // Replace history.state to "foo".
    ReplaceState(child, "foo");
    EXPECT_EQ("foo", EvalJs(child, "history.state"));

    previous_entry = controller.GetLastCommittedEntry();
    previous_frame_entry = previous_entry->GetFrameEntry(child);

    // Reload the subframe (renderer-initiated).
    FrameNavigateParamsCapturer capturer(child);
    EXPECT_TRUE(ExecJs(child, "location.reload()"));
    capturer.Wait();

    // We're classifying this as AUTO_SUBFRAME.
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());

    // We reused the previous NavigationEntry, FNE, and didn't do replacement in
    // the renderer.
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(previous_frame_entry,
              controller.GetLastCommittedEntry()->GetFrameEntry(child));
    EXPECT_EQ(1, controller.GetEntryCount());

    // We keep the same history.state value.
    EXPECT_EQ("foo", EvalJs(child, "history.state"));
  }

  {
    // Replace history.state to "foo".
    ReplaceState(child, "foo");
    EXPECT_EQ("foo", EvalJs(child, "history.state"));

    previous_entry = controller.GetLastCommittedEntry();
    previous_frame_entry = previous_entry->GetFrameEntry(child);

    // Reload the subframe (browser-initiated), but this time we hit a network
    // error and end up in an error page.
    auto url_loader_interceptor = std::make_unique<URLLoaderInterceptor>(
        base::BindRepeating([](URLLoaderInterceptor::RequestParams* params) {
          network::URLLoaderCompletionStatus status;
          status.error_code = net::ERR_NOT_IMPLEMENTED;
          params->client->OnComplete(status);
          return true;
        }));
    TestNavigationObserver reload_observer(shell()->web_contents());
    FrameNavigateParamsCapturer capturer(child);
    child->current_frame_host()->Reload();
    capturer.Wait();
    EXPECT_FALSE(reload_observer.last_navigation_succeeded());

    // We're classifying this as AUTO_SUBFRAME.
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());

    // We reused the previous NavigationEntry, FNE, and didn't do replacement in
    // the renderer.
    // TODO(https://crbug.com/1188956): Once error-page isolation for subframes
    // is turned on, this should do replacement.
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(previous_frame_entry,
              controller.GetLastCommittedEntry()->GetFrameEntry(child));
    EXPECT_EQ(1, controller.GetEntryCount());

    url_loader_interceptor.reset();
  }

  {
    previous_entry = controller.GetLastCommittedEntry();
    previous_frame_entry = previous_entry->GetFrameEntry(child);

    // Reload the subframe (browser-initiated) after a failed navigation.
    TestNavigationObserver reload_observer(shell()->web_contents());
    FrameNavigateParamsCapturer capturer(child);
    child->current_frame_host()->Reload();
    capturer.Wait();
    EXPECT_TRUE(reload_observer.last_navigation_succeeded());

    // We're classifying this as AUTO_SUBFRAME.
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());

    // We reused the previous NavigationEntry, FNE, and didn't do replacement in
    // the renderer.
    // TODO(https://crbug.com/1188956): Once error-page isolation for subframes
    // is turned on, this should do replacement.
    EXPECT_FALSE(capturer.did_replace_entry());
    EXPECT_EQ(previous_entry, controller.GetLastCommittedEntry());
    EXPECT_EQ(previous_frame_entry,
              controller.GetLastCommittedEntry()->GetFrameEntry(child));
    EXPECT_EQ(1, controller.GetEntryCount());

    // We keep the history.state value from before the failed navigation.
    // TODO(http://crbug.com/1188956): Ensure error page isolation correctly
    // maintains history.state as well.
    if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(false)) {
      EXPECT_EQ(nullptr, EvalJs(child, "history.state"));
    } else {
      EXPECT_EQ("foo", EvalJs(child, "history.state"));
    }
  }
}

// Verify that if a history navigation only affects a subframe that was
// removed, the main frame should not be reloaded.  See
// httos://crbug.com/705550.  This test checks the case where the attempted
// subframe navigation was same-document.
//
// TODO(alexmos, creis): Consider changing this behavior to auto-traverse
// history to the first entry which finds a frame to navigate.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       GoBackSameDocumentInRemovedSubframe) {
  GURL main_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  NavigationControllerImpl& controller = contents()->GetController();
  ASSERT_EQ(1, controller.GetEntryCount());

  FrameTreeNode* ftn_a = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* ftn_b = ftn_a->child_at(0);
  FrameTreeNode* ftn_c = ftn_a->child_at(1);

  // Set some state in the main frame that we can check later to make sure it
  // wasn't reloaded.
  EXPECT_TRUE(ExecJs(ftn_a, "window.state = 'a';"));

  // history.pushState() in the main frame.
  GURL ps2_url(embedded_test_server()->GetURL("a.com", "/ps2.html"));
  {
    FrameNavigateParamsCapturer capturer(ftn_a);
    ASSERT_TRUE(ExecJs(ftn_a, "history.pushState({}, 'page 2', 'ps2.html')"));
    capturer.Wait();
    EXPECT_EQ(2, controller.GetEntryCount());
    EXPECT_EQ(ps2_url, controller.GetLastCommittedEntry()->GetURL());
  }

  // history.pushState() twice in the c subframe.
  {
    FrameNavigateParamsCapturer capturer(ftn_c);
    ASSERT_TRUE(ExecJs(ftn_c, "history.pushState({}, 'page 3', 'ps3.html')"));
    capturer.Wait();
    EXPECT_EQ(3, controller.GetEntryCount());
  }
  {
    FrameNavigateParamsCapturer capturer(ftn_c);
    ASSERT_TRUE(ExecJs(ftn_c, "history.pushState({}, 'page 4', 'ps4.html')"));
    capturer.Wait();
    EXPECT_EQ(4, controller.GetEntryCount());
  }

  // Navigate frame b cross-document.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  {
    TestNavigationObserver navigation_observer(contents());
    EXPECT_TRUE(NavigateToURLFromRenderer(ftn_b, url_b));
    navigation_observer.WaitForNavigationFinished();
  }
  EXPECT_EQ(5, controller.GetEntryCount());

  // Go back.  This navigates frame b back, and we should be at next-to-last
  // entry (of 5 total) after that's done.
  EXPECT_EQ(4, controller.GetCurrentEntryIndex());
  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    shell()->GoBackOrForward(-1);
    navigation_observer.Wait();
  }
  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(3, controller.GetCurrentEntryIndex());

  // Last committed entry's URL should be ps2.html, corresponding to latest
  // navigation in the main frame.
  EXPECT_EQ(ps2_url, controller.GetLastCommittedEntry()->GetURL());

  // Set some state in frame b that we can check later to make sure it wasn't
  // reloaded.
  EXPECT_TRUE(ExecJs(ftn_b, "window.state='b';"));

  // Remove the c subframe.
  RenderFrameDeletedObserver deleted_observer(ftn_c->current_frame_host());
  EXPECT_TRUE(ExecJs(ftn_a,
                     "var f = document.querySelectorAll('iframe')[1];"
                     "f.parentNode.removeChild(f);"));
  deleted_observer.WaitUntilDeleted();

  // Try going back.  The target of this navigation had been a same-document
  // navigation in subframe c (to ps3.html), but since c has been removed, this
  // shouldn't reload frames a or b.  Frame |a| also shouldn't fire redundant
  // popstate events.
  EXPECT_TRUE(ExecJs(ftn_a, "window.popstateCalled = false"));
  EXPECT_TRUE(ExecJs(
      ftn_a, "window.onpopstate = () => { window.popstateCalled = true; }"));
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(WaitForLoadStop(contents()));
  EXPECT_EQ(ps2_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ("a", EvalJs(ftn_a, "window.state"));
  EXPECT_EQ("b", EvalJs(ftn_b, "window.state"));
  EXPECT_EQ(false, EvalJs(ftn_a, "window.popstateCalled"));

  // The corresponding NavigationEntry should now be the current one.
  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());

  // Try going back again.  Similarly, this would've gone back to c's original
  // URL when it was loaded from main_url, but since c is removed, this
  // shouldn't reload frames a or b or fire popstate events.
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(WaitForLoadStop(contents()));
  EXPECT_EQ(ps2_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ("a", EvalJs(ftn_a, "window.state"));
  EXPECT_EQ("b", EvalJs(ftn_b, "window.state"));
  EXPECT_EQ(false, EvalJs(ftn_a, "window.popstateCalled"));
  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());

  // Going back now should result in a same-document navigation in the main
  // frame to main_url.
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(WaitForLoadStop(contents()));
  EXPECT_EQ(main_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ("a", EvalJs(ftn_a, "window.state"));
  EXPECT_EQ("b", EvalJs(ftn_b, "window.state"));
  EXPECT_EQ(true, EvalJs(ftn_a, "window.popstateCalled"));
  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());

  // Go forward.  This should navigate the main frame same-document to
  // ps2.html.
  {
    FrameNavigateParamsCapturer capturer(ftn_a);
    shell()->GoBackOrForward(1);
    capturer.Wait();
  }
  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(ps2_url, ftn_a->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ("a", EvalJs(ftn_a, "window.state"));
  EXPECT_EQ("b", EvalJs(ftn_b, "window.state"));

  // Go forward three steps.  This should navigate the subframe b
  // cross-document to url_b (erasing its window.state).
  {
    FrameNavigateParamsCapturer capturer(ftn_b);
    shell()->GoBackOrForward(3);
    capturer.Wait();
  }
  EXPECT_EQ(url_b, ftn_b->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(4, controller.GetCurrentEntryIndex());
  EXPECT_EQ("a", EvalJs(ftn_a, "window.state"));
  EXPECT_EQ(nullptr, EvalJs(ftn_b, "window.state"));
}

// Verify that if a history navigation only affects a subframe that was
// removed, the main frame should not be reloaded. See
// httos://crbug.com/705550.  This test is similar to the one above, but checks
// the case where the attempted subframe navigation was cross-document rather
// than same-document.
//
// TODO(alexmos, creis): Consider changing this behavior to auto-traverse
// history to the first entry which finds a frame to navigate.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       GoBackCrossDocumentInRemovedSubframe) {
  GURL main_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  NavigationControllerImpl& controller = contents()->GetController();
  ASSERT_EQ(1, controller.GetEntryCount());

  FrameTreeNode* ftn_a = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* ftn_b = ftn_a->child_at(0);

  // Set some state in the main frame that we can check later to make sure it
  // wasn't reloaded.
  EXPECT_TRUE(ExecJs(ftn_a, "window.state = 'a';"));

  // Navigate frame b cross-document.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  {
    TestNavigationObserver navigation_observer(contents());
    EXPECT_TRUE(NavigateToURLFromRenderer(ftn_b, url_b));
    navigation_observer.WaitForNavigationFinished();
  }
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());

  // Delete frame b.
  RenderFrameDeletedObserver deleted_observer(
      ftn_a->child_at(0)->current_frame_host());
  EXPECT_TRUE(ExecJs(ftn_a,
                     "var f = document.querySelector('iframe');"
                     "f.parentNode.removeChild(f);"));
  deleted_observer.WaitUntilDeleted();
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());

  // Go back.  Since this history navigation targets a non-existent subframe,
  // the main frame shouldn't be reloaded, and the corresponding
  // NavigationEntry should become the current one.
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(WaitForLoadStop(contents()));
  EXPECT_EQ(main_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ("a", EvalJs(ftn_a, "window.state"));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_TRUE(controller.CanGoForward());

  // Go forward and expect similar behavior.
  shell()->GoBackOrForward(1);
  EXPECT_TRUE(WaitForLoadStop(contents()));
  EXPECT_EQ(main_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ("a", EvalJs(ftn_a, "window.state"));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// This test is similar to the one above, but checks the case where the first
// attempted navigation after subframe removal is a forward navigation
// rather than a back navigation.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       GoForwardCrossDocumentInRemovedSubframe) {
  GURL main_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  NavigationControllerImpl& controller = contents()->GetController();
  ASSERT_EQ(1, controller.GetEntryCount());

  FrameTreeNode* ftn_a = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* ftn_b = contents()->GetPrimaryFrameTree().root()->child_at(0);
  GURL orig_subframe_url(ftn_b->current_frame_host()->GetLastCommittedURL());

  // Set some state in the main frame that we can check later to make sure it
  // wasn't reloaded.
  EXPECT_TRUE(ExecJs(ftn_a, "window.state = 'a';"));

  // Navigate frame b cross-document twice.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  {
    TestNavigationObserver navigation_observer(contents());
    EXPECT_TRUE(NavigateToURLFromRenderer(ftn_b, url_b));
    navigation_observer.WaitForNavigationFinished();
  }
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title2.html"));
  {
    TestNavigationObserver navigation_observer(contents());
    EXPECT_TRUE(NavigateToURLFromRenderer(ftn_b, url_c));
    navigation_observer.WaitForNavigationFinished();
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());

  // history.pushState() in the main frame.
  GURL ps_url(embedded_test_server()->GetURL("a.com", "/ps.html"));
  {
    FrameNavigateParamsCapturer capturer(ftn_a);
    ASSERT_TRUE(
        ExecJs(ftn_a, "history.pushState({}, 'push state', 'ps.html')"));
    capturer.Wait();
    EXPECT_EQ(4, controller.GetEntryCount());
    EXPECT_EQ(ps_url, controller.GetLastCommittedEntry()->GetURL());
  }

  // Go back three stops, bringing back the original URL in the subframe.
  // (Note that going back by three steps all at once won't work as expected
  // due to https://crbug.com/542299, where finding that the main frame needs
  // to navigate same-document ignores any subframe navigations that should
  // also be part of the history entry navigation.)
  shell()->GoBackOrForward(-1);
  EXPECT_TRUE(WaitForLoadStop(contents()));
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(main_url, controller.GetLastCommittedEntry()->GetURL());
  shell()->GoBackOrForward(-2);
  EXPECT_TRUE(WaitForLoadStop(contents()));
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(orig_subframe_url,
            ftn_b->current_frame_host()->GetLastCommittedURL());

  // Delete frame b.
  RenderFrameDeletedObserver deleted_observer(
      ftn_a->child_at(0)->current_frame_host());
  EXPECT_TRUE(ExecJs(ftn_a,
                     "var f = document.querySelector('iframe');"
                     "f.parentNode.removeChild(f);"));
  deleted_observer.WaitUntilDeleted();
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());

  // Go forward.  Since this navigation attempt targets a non-existent
  // subframe, the main frame shouldn't be reloaded, and the corresponding
  // NavigationEntry should become current.  There should be no redundant
  // popstate events.
  EXPECT_TRUE(ExecJs(ftn_a, "window.popstateCalled = false"));
  EXPECT_TRUE(ExecJs(
      ftn_a, "window.onpopstate = () => { window.popstateCalled = true; }"));
  shell()->GoBackOrForward(1);
  EXPECT_TRUE(WaitForLoadStop(contents()));
  EXPECT_EQ("a", EvalJs(ftn_a, "window.state"));
  EXPECT_EQ(false, EvalJs(ftn_a, "window.popstateCalled"));
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());

  // Go forward again and expect similar behavior.
  shell()->GoBackOrForward(1);
  EXPECT_TRUE(WaitForLoadStop(contents()));
  EXPECT_EQ("a", EvalJs(ftn_a, "window.state"));
  EXPECT_EQ(false, EvalJs(ftn_a, "window.popstateCalled"));
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());

  // Go forward yet again.  This should result in a same-document navigation in
  // the main frame to ps.html.
  shell()->GoBackOrForward(1);
  EXPECT_TRUE(WaitForLoadStop(contents()));
  EXPECT_EQ(true, EvalJs(ftn_a, "window.popstateCalled"));
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(3, controller.GetCurrentEntryIndex());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(ps_url, controller.GetLastCommittedEntry()->GetURL());
}

// Check that if we ignore a history entry that targets a removed subframe, the
// entry still stays around and is used properly when the subframe gets
// recreated.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       RestoreRemovedSubframe) {
  // Start on a page with a same-site iframe.  It's important that this iframe
  // isn't dynamically inserted for history navigations in this test.
  GURL main_url =
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = contents()->GetController();
  FrameTreeNode* ftn_a = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* ftn_b = ftn_a->child_at(0);
  EXPECT_EQ(embedded_test_server()->GetURL("a.com", "/title1.html"),
            ftn_b->current_frame_host()->GetLastCommittedURL());

  // Set some state in the main frame that we can check later to make sure it
  // wasn't reloaded.
  EXPECT_TRUE(ExecJs(ftn_a, "window.state = 'a';"));

  // Navigate subframe cross-site twice.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  {
    TestNavigationObserver navigation_observer(contents());
    EXPECT_TRUE(NavigateToURLFromRenderer(ftn_b, url_b));
    navigation_observer.WaitForNavigationFinished();
  }

  GURL url_c(embedded_test_server()->GetURL("c.com", "/title3.html"));
  {
    TestNavigationObserver navigation_observer(contents());
    EXPECT_TRUE(NavigateToURLFromRenderer(ftn_b, url_c));
    navigation_observer.WaitForNavigationFinished();
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());

  // Remove subframe.
  RenderFrameDeletedObserver deleted_observer(
      ftn_a->child_at(0)->current_frame_host());
  EXPECT_TRUE(ExecJs(ftn_a,
                     "var f = document.querySelector('iframe');"
                     "f.parentNode.removeChild(f);"));
  deleted_observer.WaitUntilDeleted();

  // Go back.  This normally attempts to navigate the subframe from url_c to
  // url_b, but the subframe no longer exists.  Check that the main frame isn't
  // reloaded.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(contents()));
  EXPECT_EQ("a", EvalJs(ftn_a, "window.state"));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());

  // The test assumes the current page and its iframes gets deleted after
  // navigation and later recreated on the back navigation. Disable back/forward
  // cache to ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // Navigate main frame to another url.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title2.html")));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());

  // Now navigate back.  This should go back to |main_url|, reloading the
  // subframe at |url_b|, which is its URL in the history entry that we ignored
  // during the last back navigation above.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(contents()));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url_b,
            ftn_a->child_at(0)->current_frame_host()->GetLastCommittedURL());
}

// Check that when we go back in a subframe on a page that contains another
// frame which is crashed, we not only go back in the subframe but also reload
// the sad frame.  This restores restore the state covered by the corresponding
// NavigationEntry more faithfully.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ReloadSadFrameWithSubframeHistoryNavigation) {
  // Ensure this test runs in full site-per-process mode so that we can get a
  // sad frame on Android.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // Start on a page with two iframes.
  GURL main_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = contents()->GetController();
  FrameTreeNode* ftn_a = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* ftn_b = ftn_a->child_at(0);
  FrameTreeNode* ftn_c = ftn_a->child_at(1);
  GURL url_b(ftn_b->current_frame_host()->GetLastCommittedURL());
  GURL url_c(ftn_c->current_frame_host()->GetLastCommittedURL());

  // Navigate first subframe cross-site.
  GURL url_d(embedded_test_server()->GetURL("d.com", "/title2.html"));
  {
    TestNavigationObserver navigation_observer(contents());
    EXPECT_TRUE(NavigateToURLFromRenderer(ftn_b, url_d));
    navigation_observer.WaitForNavigationFinished();
  }
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url_d, ftn_b->current_frame_host()->GetLastCommittedURL());

  // Crash second subframe.
  RenderProcessHost* process_c = ftn_c->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process_c, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process_c->Shutdown(0);
  crash_observer.Wait();
  EXPECT_FALSE(ftn_c->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(ftn_c->current_frame_host()->GetLastCommittedURL().is_empty());

  // Go back.  This should navigate the first subframe back to b.com, and it
  // should also restore the subframe in c.com.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url_b, ftn_b->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(url_c, ftn_c->current_frame_host()->GetLastCommittedURL());
  EXPECT_TRUE(ftn_c->current_frame_host()->IsRenderFrameLive());
}

// Regression test for https://crbug.com/1088354, where a different-document
// load was incorrectly scheduled for a history navigation in a subframe that
// had no existing and no target FrameNavigationEntry.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       SubframeGoesBackAndSiblingHasNoFrameEntry) {
  // Start on a page with a same-site iframe.
  GURL main_url =
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = contents()->GetController();
  FrameTreeNode* ftn_a = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* ftn_b = ftn_a->child_at(0);
  EXPECT_EQ(embedded_test_server()->GetURL("a.com", "/title1.html"),
            ftn_b->current_frame_host()->GetLastCommittedURL());

  // Add a second subframe dynamically and set some state on it to ensure it's
  // not reloaded.  Using a javascript: URL results in the renderer not sending
  // a DidCommitNavigation IPC back for the new frame, leaving it without a
  // FrameNavigationEntry.
  EXPECT_TRUE(ExecJs(ftn_a,
                     "var f = document.createElement('iframe');"
                     "f.src = 'javascript:void(0)';"
                     "document.body.appendChild(f);"));
  FrameTreeNode* ftn_c = ftn_a->child_at(1);
  EXPECT_TRUE(ExecJs(ftn_c, "window.state='c';"));

  // Navigate first subframe same-document.
  {
    FrameNavigateParamsCapturer capturer(ftn_b);
    EXPECT_TRUE(ExecJs(ftn_b, "location.hash = 'foo'"));
    capturer.Wait();
    EXPECT_TRUE(capturer.is_same_document());
  }

  // Go back in the first subframe.  This should navigate the first subframe
  // back same-document, while the second subframe shouldn't be reloaded, and
  // the history navigation shouldn't crash while processing it.
  {
    FrameNavigateParamsCapturer capturer(ftn_b);
    controller.GoBack();
    capturer.Wait();
    EXPECT_TRUE(capturer.is_same_document());
    EXPECT_TRUE(WaitForLoadStop(contents()));
    EXPECT_EQ("c", EvalJs(ftn_c, "window.state"));
  }
}

// Checks that a browser-initiated same-document navigation on a page which has
// a valid base URL preserves the base URL.
// See https://crbug.com/1082141.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       LoadDataWithBaseURLSameDocumentNavigation) {
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;

  const GURL base_url("http://baseurl");
  const GURL history_url("http://history");
  const std::string data = "<html><title>One</title><body>foo</body></html>";
  const GURL data_url = GURL("data:text/html;charset=utf-8," + data);

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  {
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    shell()->LoadDataWithBaseURL(history_url, data, base_url);
    same_tab_observer.Wait();
  }

  // Verify the last committed NavigationEntry.
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, entry->GetVirtualURL());
  EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
  EXPECT_EQ(data_url, entry->GetURL());

  {
    // Make a same-document navigation via history.pushState.
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    EXPECT_TRUE(ExecJs(shell(), "history.pushState('', 'test', '#')"));
    same_tab_observer.Wait();
  }

  // Verify the last committed NavigationEntry.
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, entry->GetVirtualURL());
  EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
  EXPECT_EQ(data_url, entry->GetURL());

  {
    // Go back.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    controller.GoBack();
    back_load_observer.Wait();
  }

  // Verify the last committed NavigationEntry.
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, entry->GetVirtualURL());
  EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
  EXPECT_EQ(data_url, entry->GetURL());
  EXPECT_EQ(base_url, EvalJs(shell(), "document.URL"));
}

// Checks that a browser-initiated same-document navigation on a page which has
// a valid base URL preserves the base URL.
// See https://crbug.com/1082141.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       RendererInitiatedBackToLoadDataWithBaseURL) {
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting())
    return;

  const GURL base_url("http://baseurl");
  const GURL history_url("http://history");
  const std::string data = "<html><title>One</title><body>foo</body></html>";
  const GURL data_url = GURL("data:text/html;charset=utf-8," + data);

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  {
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    shell()->LoadDataWithBaseURL(history_url, data, base_url);
    same_tab_observer.Wait();
  }

  // Verify the last committed NavigationEntry.
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, entry->GetVirtualURL());
  EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
  EXPECT_EQ(data_url, entry->GetURL());

  {
    // Make a same-document navigation via history.pushState.
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    EXPECT_TRUE(ExecJs(shell(), "history.pushState('', 'test', '#')"));
    same_tab_observer.Wait();
  }

  // Verify the last committed NavigationEntry.
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, entry->GetVirtualURL());
  EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
  EXPECT_EQ(data_url, entry->GetURL());

  {
    // Renderer-initiated-back.
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.back()"));
    capturer.Wait();
  }

  // Verify the last committed NavigationEntry.
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, entry->GetVirtualURL());
  EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
  EXPECT_EQ(data_url, entry->GetURL());
  EXPECT_EQ(base_url, EvalJs(shell(), "document.URL"));
}

// Checks that a renderer-initiated back/forward navigation to a page which has
// a valid base URL does not crash during restore.
// See https://crbug.com/1082141.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    RendererInitiatedBackToLoadDataWithBaseURLDuringRestore) {
  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(https://crbug.com/962643): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  const GURL base_url("http://baseurl");
  const GURL history_url("http://history");
  const std::string data = "<html><title>One</title><body>foo</body></html>";
  const GURL data_url = GURL("data:text/html;charset=utf-8," + data);

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  {
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    shell()->LoadDataWithBaseURL(history_url, data, base_url);
    same_tab_observer.Wait();
  }

  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url2));

  // Restore into a new shell.
  Shell* restore_shell = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  NavigationControllerImpl& restore_controller =
      static_cast<NavigationControllerImpl&>(
          restore_shell->web_contents()->GetController());
  restore_controller.CopyStateFrom(&controller, true /* needs_reload */);
  EXPECT_EQ(2, restore_controller.GetEntryCount());
  EXPECT_EQ(1, restore_controller.GetLastCommittedEntryIndex());
  {
    TestNavigationObserver restore_observer(restore_shell->web_contents());
    restore_controller.LoadIfNecessary();
    restore_observer.Wait();
  }

  {
    // Renderer-initiated-back to data url with base url. This should not crash.
    FrameTreeNode* root =
        static_cast<WebContentsImpl*>(restore_shell->web_contents())
            ->GetPrimaryFrameTree()
            .root();
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecJs(root, "history.back()"));
    capturer.Wait();
  }
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       HistoryNavigationInNewSubframe) {
  // This test specifically observes behavior of creating a new frame during a
  // history navigation, so disable the back forward cache.
  DisableBackForwardCacheForTesting(contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Navigate to a page with an iframe.
  GURL url1 =
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html");
  ASSERT_TRUE(NavigateToURL(shell(), url1));
  GURL iframe_url = root->child_at(0)->current_url();

  // Navigate away so the iframe is destroyed.
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url2));

  // Go back (browser-initiated).
  ASSERT_TRUE(controller.CanGoBack());
  TestNavigationManager observer(shell()->web_contents(), iframe_url);
  controller.GoBack();
  EXPECT_TRUE(observer.WaitForRequestStart());

  // Check the initial navigation for the new iframe.
  NavigationRequest* navigation = root->child_at(0)->navigation_request();
  ASSERT_TRUE(navigation);
  EXPECT_TRUE(navigation->IsRendererInitiated());
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       HistoryNavigationInNewSubframeRendererInitiated) {
  // This test specifically observes behavior of creating a new frame during a
  // history navigation, so disable the back forward cache.
  DisableBackForwardCacheForTesting(contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Navigate to a page with an iframe.
  GURL url1 =
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html");
  ASSERT_TRUE(NavigateToURL(shell(), url1));
  GURL iframe_url = root->child_at(0)->current_url();

  // Navigate away so the iframe is destroyed.
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url2));

  // Go back (renderer-initiated).
  ASSERT_TRUE(controller.CanGoBack());
  TestNavigationManager observer(shell()->web_contents(), iframe_url);

  FrameNavigateParamsCapturer capturer(root);
  EXPECT_TRUE(ExecJs(root, "history.back()"));
  EXPECT_TRUE(observer.WaitForRequestStart());

  // Check the initial navigation for the new iframe.
  NavigationRequest* navigation = root->child_at(0)->navigation_request();
  ASSERT_TRUE(navigation);
  EXPECT_TRUE(navigation->IsRendererInitiated());
}

// Navigate an iframe, then reload it. Check the navigation and the
// FrameNavigationEntry are the same in both cases.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, ReloadFrame) {
  GURL main_url = embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_one_frame.html");
  GURL iframe_url = embedded_test_server()->GetURL("b.com", "/title1.html");
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* main_frame =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryMainFrame();

  // 1. Navigate the iframe using POST, initiated from the main frame.
  TestNavigationManager observer_1(shell()->web_contents(), iframe_url);
  EXPECT_TRUE(ExecJs(main_frame, JsReplace(R"(
    var form = document.createElement('form');
    form.method = 'POST';
    form.action = $1;
    form.target = "child-name-0";
    document.body.appendChild(form);
    form.submit();
  )",
                                           iframe_url)));

  EXPECT_TRUE(observer_1.WaitForRequestStart());

  // Check the navigation (initial navigation).
  NavigationRequest* navigation_1 =
      main_frame->child_at(0)->navigation_request();
  ASSERT_TRUE(navigation_1);
  EXPECT_EQ(main_url.DeprecatedGetOriginAsURL(),
            navigation_1->GetReferrer().url);
  EXPECT_EQ(network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
            navigation_1->GetReferrer().policy);
  EXPECT_TRUE(navigation_1->IsRendererInitiated());
  EXPECT_TRUE(navigation_1->IsPost());

  // Check the FrameNavigationEntry (initial navigation).
  ASSERT_TRUE(observer_1.WaitForNavigationFinished());
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  NavigationEntryImpl* entry_1 = controller.GetLastCommittedEntry();
  ASSERT_EQ(1U, entry_1->root_node()->children.size());
  scoped_refptr<FrameNavigationEntry> frame_entry_1 =
      entry_1->root_node()->children[0]->frame_entry.get();
  absl::optional<url::Origin> origin_1 = frame_entry_1->initiator_origin();
  ASSERT_TRUE(frame_entry_1->initiator_origin().has_value());
  EXPECT_EQ(url::Origin::Create(main_url),
            frame_entry_1->initiator_origin().value());
  content::Referrer referrer_1 = frame_entry_1->referrer();
  EXPECT_EQ(main_url.DeprecatedGetOriginAsURL(), frame_entry_1->referrer().url);
  EXPECT_EQ(network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
            frame_entry_1->referrer().policy);
  int item_sequence_number_1 = frame_entry_1->item_sequence_number();
  int document_sequence_number_1 = frame_entry_1->document_sequence_number();
  EXPECT_EQ(ReloadType::NONE, main_frame->reload_type());
  EXPECT_EQ(ReloadType::NONE,
            main_frame->child_at(0)->current_frame_host()->reload_type());

  // 2. Reload the document.
  TestNavigationManager observer_2(shell()->web_contents(), iframe_url);
  main_frame->child_at(0)->current_frame_host()->Reload();

  // Check the navigation (reload).
  EXPECT_TRUE(observer_2.WaitForRequestStart());
  NavigationRequest* navigation_2 =
      main_frame->child_at(0)->navigation_request();
  ASSERT_TRUE(navigation_2);
  EXPECT_EQ(main_url.DeprecatedGetOriginAsURL(),
            navigation_2->GetReferrer().url);
  EXPECT_EQ(network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
            navigation_2->GetReferrer().policy);
  EXPECT_FALSE(navigation_2->IsRendererInitiated());
  EXPECT_TRUE(navigation_2->IsPost());

  // Check the FrameNavigationEntry (reload).
  ASSERT_TRUE(observer_2.WaitForNavigationFinished());
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  NavigationEntryImpl* entry_2 = controller.GetLastCommittedEntry();
  ASSERT_EQ(1U, entry_1->root_node()->children.size());
  scoped_refptr<FrameNavigationEntry> frame_entry_2 =
      entry_2->root_node()->children[0]->frame_entry.get();
  absl::optional<url::Origin> origin_2 = frame_entry_2->initiator_origin();
  ASSERT_TRUE(frame_entry_2->initiator_origin().has_value());
  EXPECT_EQ(url::Origin::Create(main_url),
            frame_entry_2->initiator_origin().value());
  content::Referrer referrer_2 = frame_entry_1->referrer();
  EXPECT_EQ(main_url.DeprecatedGetOriginAsURL(), frame_entry_2->referrer().url);
  EXPECT_EQ(network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
            frame_entry_2->referrer().policy);

  int item_sequence_number_2 = frame_entry_1->item_sequence_number();
  int document_sequence_number_2 = frame_entry_1->document_sequence_number();
  EXPECT_EQ(item_sequence_number_1, item_sequence_number_2);
  EXPECT_EQ(document_sequence_number_1, document_sequence_number_2);
  EXPECT_EQ(ReloadType::NONE, main_frame->reload_type());
  EXPECT_EQ(ReloadType::NORMAL,
            main_frame->child_at(0)->current_frame_host()->reload_type());
}

// A history navigation only navigates the iframe that should be changed to
// update history. A grandchild iframe on the initial about:blank document will
// not commit any navigation and should not be modified by a history navigation
// in another frame.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       HistoryNavigationDoesntMoveFrameWithoutCommit) {
  WebContents* wc = shell()->web_contents();
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(wc->GetController());

  GURL main_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a)");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHostImpl* main_frame =
      static_cast<RenderFrameHostImpl*>(wc->GetPrimaryMainFrame());
  FrameTreeNode* b_frame = main_frame->child_at(0);
  FrameTreeNode* c_frame = main_frame->child_at(1);
  ASSERT_TRUE(b_frame);
  ASSERT_TRUE(c_frame);

  {
    LoadCommittedCapturer capturer(wc);
    EXPECT_TRUE(ExecJs(c_frame, kAddEmptyFrameScript));
    capturer.Wait();
  }

  FrameTreeNode* cc_frame = c_frame->current_frame_host()->child_at(0);
  ASSERT_TRUE(cc_frame);

  const char set_status_on_beforeunload[] = R"(
      window.top.testStatus = "STARTED";
      window.addEventListener(
        "beforeunload",
        () => { window.top.testStatus = "UNLOAD" },
        false);
      window.top.testStatus;
      )";
  EXPECT_EQ("STARTED",
            EvalJs(cc_frame, set_status_on_beforeunload).ExtractString());

  // Navigate frame 'b' creating a new history entry.
  GURL url2 = embedded_test_server()->GetURL("a.com", "/title2.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(b_frame, url2));

  // Frame 'cc' is a grandchild frame left at the initial about:blank document.
  // This results in it not being committed.
  EXPECT_FALSE(cc_frame->current_frame_host()->has_committed_any_navigation());

  scoped_refptr<FrameNavigationEntry> b_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(b_frame);
  int64_t b_isn = b_entry->item_sequence_number();

  // Go back.
  FrameNavigateParamsCapturer capturer(b_frame);
  controller.GoBack();
  capturer.Wait();

  scoped_refptr<FrameNavigationEntry> b2_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(b_frame);
  int64_t b2_isn = b2_entry->item_sequence_number();

  // Frame 'b' should have been navigated back.
  EXPECT_NE(b_isn, b2_isn);
  // Frame 'c' should not have committed due to 'b' navigating.
  EXPECT_FALSE(cc_frame->current_frame_host()->has_committed_any_navigation());

  // We bounce through frame 'cc' in order to avoid races with beforeunload.
  //
  // If frame 'b' navigating caused the un-committed about:blank frame to do a
  // navigation, then it would have to finish its commit and fire beforeunload,
  // which would result in getting "UNLOAD" here. This comes from the original
  // repro at https://crbug.com/1192709.
  const char set_status_done[] = R"(
      if (window.top.testStatus == "STARTED")
        window.top.testStatus = "DONE";
      window.top.testStatus;
      )";
  EXPECT_EQ("DONE", EvalJs(cc_frame, set_status_done).ExtractString());
}

// If the NavigationEntry |is_overriding_user_agent_| attribute is updated and
// the entry reloaded, the new document will load with the user agent
// overridden. Consecutive navigation will continue with the user agent
// overridden.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       OverrideUserAgentThenReload) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/empty.html")));
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());

  // Update the entry and load it.
  controller.GetLastCommittedEntry()->SetIsOverridingUserAgent(true);
  EXPECT_TRUE(controller.GetLastCommittedEntry()->GetIsOverridingUserAgent());
  controller.LoadOriginalRequestURL();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(controller.GetLastCommittedEntry()->GetIsOverridingUserAgent());

  // Consecutive same-document navigation uses the new state.
  EXPECT_TRUE(ExecJs(shell(), "history.pushState('', 'test', '#foo2')"));
  EXPECT_TRUE(controller.GetLastCommittedEntry()->GetIsOverridingUserAgent());

  // Consecutive cross-document same-origin navigation uses the new state.
  EXPECT_TRUE(ExecJs(shell(), "location.href = '/title1.html';"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(controller.GetLastCommittedEntry()->GetIsOverridingUserAgent());

  // Consecutive cross-document cross-origin navigation uses the new state.
  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("location.href = $1;", embedded_test_server()->GetURL(
                                                    "b.com", "/empty.html"))));
  EXPECT_TRUE(controller.GetLastCommittedEntry()->GetIsOverridingUserAgent());
}

// If the NavigationEntry |is_overriding_user_agent_| attribute is updated and
// the document is still the one loaded without overriding user agent, then
// committing a same-document navigation will restore the value used by the
// document.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       OverrideUserAgentThenSameDocument) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/empty.html")));
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());

  controller.GetLastCommittedEntry()->SetIsOverridingUserAgent(true);
  EXPECT_TRUE(controller.GetLastCommittedEntry()->GetIsOverridingUserAgent());
  EXPECT_TRUE(ExecJs(shell(), "history.pushState('', 'test', '#foo1')"));
  EXPECT_FALSE(controller.GetLastCommittedEntry()->GetIsOverridingUserAgent());
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       URLLoadReturnsNavigationHandle) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));

  TestNavigationManager navigation_manager(shell()->web_contents(), url);
  base::WeakPtr<NavigationHandle> navigation =
      shell()->web_contents()->GetController().LoadURLWithParams(
          NavigationController::LoadURLParams(url));

  // The returned NavigationHandle should be valid.
  EXPECT_TRUE(navigation);

  // Start the navigation and ensure that the NavigationHandle we saw matches
  // the one TestNavigationManager saw.
  ASSERT_TRUE(navigation_manager.WaitForRequestStart());
  EXPECT_TRUE(navigation);
  EXPECT_EQ(navigation->GetNavigationId(),
            navigation_manager.GetNavigationHandle()->GetNavigationId());

  // Commit navigation and ensure that the weak ptr to NavigationHandle was
  // invalidated.
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  EXPECT_FALSE(navigation);
}

// All non-about:blank/about:srcdoc about: URLs reported by the renderer should
// get rewritten to about:blank#blocked (browser-initiated navigations will be
// blocked from starting entirely, and trying to trigger the navigation in test
// will hit a NOTREACHED() in the NavigationRequest constructor).
// See ChildProcessSecurityPolicyImpl::CanRequestURL() for a discussion.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, FilterAbout) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  // Navigate to the initial page.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/empty.html")));
  EXPECT_EQ(1, controller.GetEntryCount());

  // Renderer-initiated navigation will be rewritten to "about:blank#blocked".
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), GURL("about:cache"),
                                        GURL(kBlockedURL)));
  EXPECT_EQ(2, controller.GetEntryCount());
  ASSERT_TRUE(controller.GetVisibleEntry());
  EXPECT_EQ(GURL(kBlockedURL), controller.GetVisibleEntry()->GetURL());
}

// Tests that reload() does not create a new NavigationEntry.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, WindowReload) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  // Navigate to the initial page.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_EQ(1, controller.GetEntryCount());

  TestNavigationObserver observer(contents());
  EXPECT_TRUE(ExecJs(current_main_frame_host(), "window.location.reload();"));
  observer.WaitForNavigationFinished();
  // Reloads in the main frame have a valid nav_entry_id.
  EXPECT_NE(0, observer.last_nav_entry_id());
  EXPECT_EQ(1, controller.GetEntryCount());
}

// Tests that reload() in iframes does not create a new NavigationEntry.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, WindowReloadInIframe) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  // Navigate to the initial page.
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "a.com", "/page_with_iframe.html")));
  EXPECT_EQ(1, controller.GetEntryCount());

  TestNavigationObserver observer(contents());
  EXPECT_TRUE(ExecJs(current_main_frame_host()->child_at(0),
                     "window.location.reload();"));
  observer.WaitForNavigationFinished();
  // Reloads in subframes do not have a valid nav_entry_id.
  EXPECT_EQ(0, observer.last_nav_entry_id());
  EXPECT_EQ(1, controller.GetEntryCount());
}

namespace {
// Tracks how many NavigationStateChanged call were triggered with the
// INVALIDATE_TYPE_ALL flag.
class AllNavigationStateChangedDelegate : public WebContentsDelegate {
 public:
  AllNavigationStateChangedDelegate() = default;

  AllNavigationStateChangedDelegate(const AllNavigationStateChangedDelegate&) =
      delete;
  AllNavigationStateChangedDelegate& operator=(
      const AllNavigationStateChangedDelegate&) = delete;

  ~AllNavigationStateChangedDelegate() override = default;

  void NavigationStateChanged(WebContents* source,
                              InvalidateTypes changed_flags) override {
    if (changed_flags == INVALIDATE_TYPE_ALL)
      call_count_++;
  }

  int call_count() { return call_count_; }

 private:
  int call_count_ = 0;
};
}  // namespace

// Test to highlight the difference in behavior for relative urls in an
// about:blank popup. The expectations in this test can be directly compared to
// those for the "Navigate the popup main frame to a same-document URL." case
// in the test NavigationStateChangedForInitialNavigationEntry immediately
// below.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       RelativeURLInAboutBlankPopup) {
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  // Pop open a new window without specifying a URL.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(root, "var w = window.open()"));
  Shell* new_shell = new_shell_observer.GetShell();
  WebContentsImpl* new_contents =
      static_cast<WebContentsImpl*>(new_shell->web_contents());
  EXPECT_TRUE(WaitForLoadStop(new_contents));

  // Observe NavigationStateChanged calls.
  AllNavigationStateChangedDelegate all_navigation_state_changed_delegate;
  new_contents->SetDelegate(&all_navigation_state_changed_delegate);
  EXPECT_EQ(0, all_navigation_state_changed_delegate.call_count());
  ASSERT_NE(new_contents, shell()->web_contents());
  FrameTreeNode* new_root = new_contents->GetPrimaryFrameTree().root();

  NavigationControllerImpl& controller = new_contents->GetController();
  // The new window is on the initial NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_TRUE(controller.GetLastCommittedEntry()->IsInitialEntry());

  if (blink::features::IsNewBaseUrlInheritanceBehaviorEnabled()) {
    EXPECT_NE("about:blank",
              EvalJs(new_root, "document.baseURI").ExtractString());
  } else {
    EXPECT_EQ("about:blank",
              EvalJs(new_root, "document.baseURI").ExtractString());
  }

  // Navigate the popup main frame to a same-document relative URL.
  FrameNavigateParamsCapturer capturer(new_root);
  EXPECT_TRUE(ExecJs(new_root, "location.href = '#foo';"));
  capturer.Wait();

  if (blink::features::IsNewBaseUrlInheritanceBehaviorEnabled()) {
    // When an about:blank popup inherits its base url from the initiator,
    // it will not be able to do same-document navigations without specifying an
    // absolute url.

    EXPECT_EQ(embedded_test_server()->GetURL("/title1.html#foo"),
              new_root->current_url());
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_FALSE(capturer.is_same_document());

    // The navigation is a cross-document navigation from the initial empty
    // document, so we committed a new non-initial NavigationEntry that replaced
    // the initial entry.
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_FALSE(controller.GetLastCommittedEntry()->IsInitialEntry());
  } else {
    // When the new inheritance behavior is disabled, an about:blank popup will
    // have about:blank for the baseURI also, and so relative URLs can be used
    // for same document navigations.

    EXPECT_EQ(GURL("about:blank#foo"), new_root->current_url());
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());

    // The navigation is a same-document navigation from the initial empty
    // document, so we committed a new initial NavigationEntry that replaced the
    // previous initial NavigationEntry.
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_TRUE(controller.GetLastCommittedEntry()->IsInitialEntry());
  }
}

// Tests that committing iframes and doing same-document navigations with the
// initial NavigationEntry dispatches the expected NavigationStateChanged calls.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       NavigationStateChangedForInitialNavigationEntry) {
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  // Pop open a new window without specifying a URL.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(root, "var w = window.open()"));
  Shell* new_shell = new_shell_observer.GetShell();
  WebContentsImpl* new_contents =
      static_cast<WebContentsImpl*>(new_shell->web_contents());
  EXPECT_TRUE(WaitForLoadStop(new_contents));

  // Observe NavigationStateChanged calls.
  AllNavigationStateChangedDelegate all_navigation_state_changed_delegate;
  new_contents->SetDelegate(&all_navigation_state_changed_delegate);
  EXPECT_EQ(0, all_navigation_state_changed_delegate.call_count());
  ASSERT_NE(new_contents, shell()->web_contents());
  FrameTreeNode* new_root = new_contents->GetPrimaryFrameTree().root();

  NavigationControllerImpl& controller = new_contents->GetController();
  // The new window is on the initial NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_TRUE(controller.GetLastCommittedEntry()->IsInitialEntry());

  {
    // Make a new iframe in the popup.
    LoadCommittedCapturer capturer(new_contents);
    std::string script = JsReplace(kAddFrameWithSrcScript, url1);
    EXPECT_TRUE(ExecJs(new_root, script));
    capturer.Wait();
    EXPECT_EQ(url1, new_root->child_at(0)->current_url());

    // The new window is still on the initial NavigationEntry, as the previous
    // navigation modified the existing entry.
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_TRUE(controller.GetLastCommittedEntry()->IsInitialEntry());

    // No new NavigationEntry was committed as the last navigation only modifies
    // the existing NavigationEntry, so no NavigationStateChanged calls were
    // triggered.
    EXPECT_EQ(0, all_navigation_state_changed_delegate.call_count());
  }

  {
    //  Navigate the iframe to another URL.
    FrameNavigateParamsCapturer capturer(new_root->child_at(0));
    NavigateFrameToURL(new_root->child_at(0), url2);
    capturer.Wait();
    EXPECT_EQ(url2, new_root->child_at(0)->current_url());
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.navigation_type());
    // The new window is still on the initial NavigationEntry, as the previous
    // navigation modified the existing entry.
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_TRUE(controller.GetLastCommittedEntry()->IsInitialEntry());

    // No new NavigationEntry was committed as the last navigation only modifies
    // the existing NavigationEntry, so no NavigationStateChanged calls were
    // triggered.
    EXPECT_EQ(0, all_navigation_state_changed_delegate.call_count());
  }

  {
    // Navigate the popup main frame to a same-document URL.
    FrameNavigateParamsCapturer capturer(new_root);
    EXPECT_TRUE(ExecJs(new_root, "location.href = 'about:blank#foo';"));
    capturer.Wait();
    EXPECT_EQ(GURL("about:blank#foo"), new_root->current_url());
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
              capturer.navigation_type());
    EXPECT_TRUE(capturer.is_same_document());

    // The navigation is a same-document navigation from the initial empty
    // document, so we committed a new initial NavigationEntry that replaced the
    // previous initial NavigationEntry.
    EXPECT_TRUE(capturer.did_replace_entry());
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_TRUE(controller.GetLastCommittedEntry()->IsInitialEntry());

    // 2 INVALIDATE_TYPE_ALL NavigationStateChanged calls were triggered when
    // committing the new NavigationEntry which keeps the "initial" status:
    // #1 was triggered by DiscardNonCommittedEntries().
    // #2 is triggered by NotifyNavigationEntryCommitted().
    // Note that this is different from the _Ignore test below, which wouldn't
    // fire the events because the client chooses to ignore the updates.
    EXPECT_EQ(2, all_navigation_state_changed_delegate.call_count());
  }

  {
    // Navigate the popup main frame to another URL.
    FrameNavigateParamsCapturer capturer(new_root);
    EXPECT_TRUE(ExecJs(new_root, JsReplace("location.href = $1;", url2)));
    capturer.Wait();
    EXPECT_EQ(url2, new_root->current_url());
    EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, capturer.navigation_type());
    EXPECT_TRUE(capturer.did_replace_entry());

    // The navigation is a cross-document navigation from the initial empty
    // document, so we committed a new non-initial NavigationEntry that
    // replaced the initial entry.
    EXPECT_EQ(1, controller.GetEntryCount());
    EXPECT_FALSE(controller.GetLastCommittedEntry()->IsInitialEntry());

    // 2 additional INVALIDATE_TYPE_ALL NavigationStateChanged calls were
    // triggered (increasing the count to 4), and they're not for the initial
    // NavigationEntry.
    // #1 was triggered by DiscardNonCommittedEntries().
    // #2 is triggered by NotifyNavigationEntryCommitted().
    EXPECT_EQ(4, all_navigation_state_changed_delegate.call_count());
  }
}

namespace {
// WebContentsDelegate that ensures CanGoBack returns false during all
// NavigationStateChanged notifications.
class CantGoBackNavigationStateChangedDelegate : public WebContentsDelegate {
 public:
  CantGoBackNavigationStateChangedDelegate() = default;

  CantGoBackNavigationStateChangedDelegate(
      const CantGoBackNavigationStateChangedDelegate&) = delete;
  CantGoBackNavigationStateChangedDelegate& operator=(
      const CantGoBackNavigationStateChangedDelegate&) = delete;

  ~CantGoBackNavigationStateChangedDelegate() override = default;

  void NavigationStateChanged(WebContents* source,
                              InvalidateTypes changed_flags) override {
    if (changed_flags == INVALIDATE_TYPE_ALL) {
      EXPECT_FALSE(source->GetController().CanGoBack());
    }
  }
};
}  // namespace

// Test that CanGoBack does not return true after it starts returning false
// during a back navigation. See https://crbug.com/1439948.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       CanGoBackConsistencyForDelegate) {
  DisableBackForwardCacheForTesting(contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // Start a back navigation but do not allow it to commit.  CanGoBack should
  // change from true before the navigation starts to false after.
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  TestNavigationManager nav_manager(contents(), url_1);
  ASSERT_TRUE(controller.CanGoBack());
  controller.GoBack();
  EXPECT_TRUE(nav_manager.WaitForRequestStart());
  EXPECT_FALSE(controller.CanGoBack());

  // Observe any further NavigationStateChanged calls as the navigation resumes.
  // If CanGoBack starts returning true again, the test will fail.
  CantGoBackNavigationStateChangedDelegate navigation_state_changed_delegate;
  contents()->SetDelegate(&navigation_state_changed_delegate);
  nav_manager.ResumeNavigation();
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_FALSE(controller.CanGoBack());
}

// Verify that calling Restore() with a non-empty entry list on a new
// NavigationController will not result in an initial NavigationEntry.
// See https://crbug.com/1295723.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    RestoreRemovesInitialEmptyDocumentOnFreshNavigationController) {
  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // 1. Start in a tab with 1 NavigationEntry.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(url_1, root->current_url());

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();

  // 2. Create a NavigationEntry with the same PageState as |entry1|.
  std::unique_ptr<NavigationEntryImpl> restored_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              url_1, Referrer(), absl::nullopt /* initiator_origin= */,
              /* initiator_base_url= */ absl::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  EXPECT_EQ(0U, restored_entry->root_node()->children.size());
  std::unique_ptr<NavigationEntryRestoreContextImpl> context =
      std::make_unique<NavigationEntryRestoreContextImpl>();
  restored_entry->SetPageState(entry1->GetPageState(), context.get());

  // 3. Create a new tab and don't navigate it anywhere.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  Shell* new_shell = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(
          new_shell->web_contents()->GetController());
  // The new tab's root is on the initial NavigationEntry and is marked as
  // "on initial empty document" as it hasn't navigated anywhere.
  EXPECT_TRUE(new_root->is_on_initial_empty_document());
  EXPECT_TRUE(new_controller.GetLastCommittedEntry()->IsInitialEntry());

  // 4. Call Restore() with the entry from the old tab.
  new_controller.Restore(entries.size() - 1, RestoreType::kRestored, &entries);
  EXPECT_EQ(0u, entries.size());
  EXPECT_EQ(1, new_controller.GetEntryCount());
  // The new tab's root is no longer on the initial NavigationEntry but is still
  // marked as "on initial empty document" after calling Restore().
  EXPECT_TRUE(new_root->is_on_initial_empty_document());
  EXPECT_FALSE(new_controller.GetLastCommittedEntry()->IsInitialEntry());

  // 5. Navigate the new tab to another URL.
  FrameNavigateParamsCapturer capturer(new_root);
  EXPECT_TRUE(NavigateToURL(new_shell, url_2));
  capturer.Wait();
  // The navigation should append a new entry instead of replacing the restored
  // entry (which is what it would've done if it was still on the
  // initial NavigationEntry).
  EXPECT_FALSE(capturer.did_replace_entry());
  EXPECT_EQ(2, new_controller.GetEntryCount());
  EXPECT_EQ(url_1, new_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url_2, new_controller.GetLastCommittedEntry()->GetURL());
}

// Verify that calling Restore with an empty set of entries on a fresh/unused
// NavigationController does not crash, since this may be possible via Android
// WebView's restoreState() API. See https://crbug.com/1287624.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       RestoreEmptyAndLoadOnFreshNavigationController) {
  // 1. Create a new tab with a new NavigationController, and don't navigate it
  // anywhere.
  Shell* new_shell =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             GURL::EmptyGURL(), nullptr, gfx::Size());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(
          new_shell->web_contents()->GetController());
  // The new tab is on the initial NavigationEntry.
  EXPECT_EQ(1, new_controller.GetEntryCount());
  EXPECT_EQ(0, new_controller.GetLastCommittedEntryIndex());
  EXPECT_TRUE(new_controller.GetLastCommittedEntry()->IsInitialEntry());

  // 2. Restore an empty set of entries in the new tab, which is possible via
  // Android WebView's restoreState API.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  new_controller.Restore(-1, RestoreType::kRestored, &entries);
  // There is always a valid index, and the initial NavigationEntry is kept to
  // ensure that there is always at least 1 entry.
  EXPECT_EQ(1, new_controller.GetEntryCount());
  EXPECT_EQ(0, new_controller.GetLastCommittedEntryIndex());
  EXPECT_TRUE(new_controller.GetLastCommittedEntry()->IsInitialEntry());

  // 3. Call LoadIfNecessary() in the new tab.
  ASSERT_EQ(0u, entries.size());
  new_controller.LoadIfNecessary();
  // No navigation had started, as there's no non-initial NavigationEntry to
  // load.
  EXPECT_FALSE(new_root->navigation_request());
  EXPECT_EQ(nullptr, new_controller.GetPendingEntry());

  // 4. Navigate the new tab to another URL.
  FrameNavigateParamsCapturer capturer(new_root);
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(new_shell, url));
  capturer.Wait();
  // The navigation should replace the initial NavigationEntry .
  EXPECT_TRUE(capturer.did_replace_entry());
  EXPECT_EQ(1, new_controller.GetEntryCount());
  EXPECT_EQ(url, new_controller.GetLastCommittedEntry()->GetURL());
}

// A test to verify that a navigation with |force_new_browsing_instance| set to
// true results in a new BrowsingInstance, even if it's to a URL compatible with
// the original SiteInstance.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ResetForTestForcesNewBrowsingInstance) {
  GURL test_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  SiteInstance* initial_site_instance = contents()->GetSiteInstance();

  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  DisableProactiveBrowsingInstanceSwapFor(root->current_frame_host());

  TestNavigationObserver navigation_observer(contents());
  NavigationController::LoadURLParams params(test_url);
  params.force_new_browsing_instance = true;
  contents()->GetController().LoadURLWithParams(params);
  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());

  SiteInstance* post_reset_site_instance = contents()->GetSiteInstance();
  EXPECT_NE(initial_site_instance, post_reset_site_instance);
  EXPECT_FALSE(
      post_reset_site_instance->IsRelatedSiteInstance(initial_site_instance));
}

// Test that unload timing of old document is accessible after same-origin
// navigation, but not after cross-origin navigation.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest, UnloadTiming) {
  // With BackForwardCache, old document doesn't fire unload handlers as the
  // page is stored in BackForwardCache on navigation.
  DisableBackForwardCacheForTesting(contents(),
                                    BackForwardCache::TEST_USES_UNLOAD_EVENT);

  GURL initial_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  // Navigate to a page with an iframe.
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));

  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0u);

  // Navigate the subframe same-origin.
  ASSERT_TRUE(NavigateFrameToURL(child, url_a));

  // The unloadEventStart and unloadEventEnd timings should be set.
  EXPECT_TRUE(ExecJs(
      child,
      "var navigationTiming = performance.getEntriesByType('navigation')"));
  ASSERT_EQ(1, EvalJs(child, "navigationTiming.length"));
  EXPECT_NE(0, EvalJs(child, "navigationTiming[0].unloadEventStart"));
  EXPECT_NE(0, EvalJs(child, "navigationTiming[0].unloadEventEnd"));

  // Navigate the subframe cross-origin.
  {
    FrameNavigateParamsCapturer capturer(child);
    ASSERT_TRUE(NavigateFrameToURL(child, url_b));
    capturer.Wait();
  }
  // The unloadEventStart and unloadEventEnd timings should not be set.
  EXPECT_TRUE(ExecJs(
      child,
      "var navigationTiming = performance.getEntriesByType('navigation')"));
  ASSERT_EQ(1, EvalJs(child, "navigationTiming.length"));
  EXPECT_EQ(0, EvalJs(child, "navigationTiming[0].unloadEventStart"));
  EXPECT_EQ(0, EvalJs(child, "navigationTiming[0].unloadEventEnd"));

  // Navigate the main frame same-origin.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  // The unloadEventStart and unloadEventEnd timings should be set.
  EXPECT_TRUE(ExecJs(
      root,
      "var navigationTiming = performance.getEntriesByType('navigation')"));
  ASSERT_EQ(1, EvalJs(root, "navigationTiming.length"));
  EXPECT_NE(0, EvalJs(root, "navigationTiming[0].unloadEventStart"));
  EXPECT_NE(0, EvalJs(root, "navigationTiming[0].unloadEventEnd"));

  // Navigate the main frame cross-origin.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  // The unloadEventStart and unloadEventEnd timings should not be set.
  EXPECT_TRUE(ExecJs(
      root,
      "var navigationTiming = performance.getEntriesByType('navigation')"));
  ASSERT_EQ(1, EvalJs(root, "navigationTiming.length"));
  EXPECT_EQ(0, EvalJs(root, "navigationTiming[0].unloadEventStart"));
  EXPECT_EQ(0, EvalJs(root, "navigationTiming[0].unloadEventEnd"));
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       SameURLNavigationRetainsHistoryState) {
  GURL main_frame_url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  GURL subframe_url(embedded_test_server()->GetURL("/title1.html"));
  GURL other_url(embedded_test_server()->GetURL("/title2.html"));
  // Navigate to a page with an iframe.
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0u);
  EXPECT_EQ(main_frame_url, root->current_url());
  EXPECT_EQ(subframe_url, child->current_url());

  // Do a replaceState on the subframe to set the history.state to "foo".
  ReplaceState(child, "foo");

  // Navigate the subframe to the same URL it is currently on.
  {
    FrameNavigateParamsCapturer capturer(child);
    ASSERT_TRUE(NavigateFrameToURL(child, subframe_url));
    capturer.Wait();
    EXPECT_EQ(subframe_url, child->current_url());
  }
  // Check that the history.state is retained after the navigation.
  EXPECT_EQ("foo", EvalJs(child, "history.state"));

  // Navigate the subframe to a different URL.
  {
    FrameNavigateParamsCapturer capturer(child);
    ASSERT_TRUE(NavigateFrameToURL(child, other_url));
    capturer.Wait();
    EXPECT_EQ(other_url, child->current_url());
  }
  // Check that the history.state is not retained after the navigation.
  EXPECT_EQ(nullptr, EvalJs(child, "history.state"));

  // Do a replaceState on the main frame to set the history.state to "bar".
  ReplaceState(root, "bar");
  EXPECT_EQ("bar", EvalJs(root, "history.state"));

  // Navigate the main frame to the same URL it's currently on.
  ASSERT_TRUE(NavigateToURL(shell(), GURL(root->current_url())));
  // Check that the history.state is retained after the navigation.
  EXPECT_EQ("bar", EvalJs(root, "history.state"));

  // Navigate the main frame to a different URL.
  ASSERT_TRUE(NavigateToURL(shell(), other_url));
  // Check that the history.state is not retained after the navigation.
  EXPECT_EQ(nullptr, EvalJs(root, "history.state"));
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       GoForwardAfterInitialHistoryInChildFrame) {
  // Start on a page with one iframe.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = contents()->GetController();
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Navigate to a real page in the subframe, so that the next navigation will
  // be MANUAL_SUBFRAME.
  GURL subframe_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  {
    TestNavigationObserver navigation_observer(contents());
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), subframe_url));
    navigation_observer.Wait();
  }

  // Navigate subframe same-document.
  {
    TestNavigationObserver navigation_observer(contents());
    std::string script = "location.href = '#foo';";
    EXPECT_TRUE(ExecJs(child, script));
    navigation_observer.Wait();
  }
  GURL child_url(child->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());

  // Navigate root cross-document.
  GURL cross_doc_url(embedded_test_server()->GetURL("a.com", "/title2.html"));
  {
    TestNavigationObserver navigation_observer(contents());
    EXPECT_TRUE(NavigateToURLFromRenderer(root, cross_doc_url));
    navigation_observer.Wait();
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());

  // Go back 2, this will restore the subframe.
  {
    TestNavigationObserver navigation_observer(contents());
    controller.GoToOffset(-2);
    navigation_observer.Wait();
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());

  // The NavigationApi in the renderer validates same-document back/forward
  // navigations, and cancels them if it is not aware of the existence of the
  // entry being navigated to. Restoring an iframe during a history traversal of
  // its parent used to cause
  // NavigationControllerImpl::GetNavigationApiHistoryEntryVectors() to
  // miscalculate the committing entry index (thinking it was 1 instead of 0).
  // This meant that index 0 was treated as a back entry (when it should have
  // been current) and the index 1 entry was omitted (when it should have been a
  // forward entry). The NavigationApi therefore had incorrect data to refer to
  // when validating back/forward navigations and was entirely unaware of the
  // entry at index 1, so it would incorrectly block a forward navigation to
  // index 1.
  {
    // Go forward.
    TestNavigationObserver navigation_observer(shell()->web_contents());
    controller.GoForward();
    navigation_observer.Wait();
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(child_url,
            root->child_at(0)->current_frame_host()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       GoBackInMultipleIframesOneFastOneSlow) {
  // Start on a page with two iframes.
  GURL main_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  NavigationControllerImpl& controller = contents()->GetController();
  FrameTreeNode* ftn_a = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* ftn_b = ftn_a->child_at(0);
  FrameTreeNode* ftn_c = ftn_a->child_at(1);
  GURL url_b(ftn_b->current_frame_host()->GetLastCommittedURL());
  GURL url_c(ftn_c->current_frame_host()->GetLastCommittedURL());

  // Navigate second subframe same-document.
  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    std::string script = "location.href = '#foo';";
    EXPECT_TRUE(ExecJs(ftn_c, script));
    navigation_observer.Wait();
  }
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());

  // Navigate first subframe cross-document.
  GURL url_b2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  {
    TestNavigationObserver navigation_observer(contents());
    EXPECT_TRUE(NavigateToURLFromRenderer(ftn_b, url_b2));
    navigation_observer.Wait();
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url_b2, ftn_b->current_frame_host()->GetLastCommittedURL());

  // Navigate second subframe cross-document.
  GURL url_c2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  {
    TestNavigationObserver navigation_observer(contents());
    EXPECT_TRUE(NavigateToURLFromRenderer(ftn_c, url_c2));
    navigation_observer.Wait();
  }
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(3, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url_c2, ftn_c->current_frame_host()->GetLastCommittedURL());

  // Go back 3 so that both subframes navigate, but ensure that ftn_c doesn't
  // commit until after ftn_b is entirely finished.
  FrameTestNavigationManager b_delayer(ftn_b->frame_tree_node_id(),
                                       shell()->web_contents(), url_b);
  FrameTestNavigationManager c_delayer(ftn_c->frame_tree_node_id(),
                                       shell()->web_contents(), url_c);
  controller.GoToOffset(-3);
  ASSERT_TRUE(b_delayer.WaitForNavigationFinished());
  ASSERT_TRUE(c_delayer.WaitForNavigationFinished());

  EXPECT_TRUE(WaitForLoadStop(contents()));
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url_b, ftn_b->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(url_c, ftn_c->current_frame_host()->GetLastCommittedURL());

  // The NavigationApi in the renderer validates same-document back/forward
  // navigations, and cancels them if it is not aware of the existence of the
  // entry being navigated to. The back traversal above with multiple iframes
  // committing at different times used to cause
  // NavigationControllerImpl::GetNavigationApiHistoryEntryVectors() to
  // miscalculate the committing entry index (thinking it was 1 instead of 0).
  // This meant that index 0 was treated as a back entry (when it should have
  // been current) and the index 1 entry was omitted (when it should have been a
  // forward entry). The NavigationApi therefore had incorrect data to refer to
  // when validating back/forward navigations and was entirely unaware of the
  // entry at index 1, so it would incorrectly block a forward navigation to
  // index 1.
  {
    // Go forward.
    TestNavigationObserver navigation_observer(shell()->web_contents());
    controller.GoForward();
    navigation_observer.Wait();
  }
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       DeleteNavigationEntriesFiresNavigationApiDisposeEvent) {
  GURL url1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL url2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url3(embedded_test_server()->GetURL("a.com", "/title3.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url1));
  ASSERT_TRUE(NavigateToURL(shell(), url2));
  ASSERT_TRUE(NavigateToURL(shell(), url3));
  NavigationControllerImpl& controller = contents()->GetController();

  // Go back.
  TestNavigationObserver back_load_observer(shell()->web_contents());
  controller.GoBack();
  back_load_observer.Wait();
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());

  // Insert dispose events on all navigation API entries.
  EXPECT_TRUE(ExecJs(contents(), R"(
      window.dispose0_fired = false;
      window.dispose1_fired = false;
      window.dispose2_fired = false;
      navigation.entries()[0].ondispose = () => window.dispose0_fired = true;
      navigation.entries()[1].ondispose = () => window.dispose2_fired = true;
      navigation.entries()[2].ondispose = () => window.dispose2_fired = true;
    )"));

  // Delete the NavigationEntry for |url1| and ensure the appropriate dispose
  // event fires.
  controller.DeleteNavigationEntries(
      base::BindLambdaForTesting([&](content::NavigationEntry* entry) {
        return entry->GetURL() == url1;
      }));
  EXPECT_EQ(true, EvalJs(shell(), "window.dispose0_fired").ExtractBool());
  EXPECT_EQ(false, EvalJs(shell(), "window.dispose1_fired").ExtractBool());
  EXPECT_EQ(false, EvalJs(shell(), "window.dispose2_fired").ExtractBool());

  // Do the same for |url3|.
  controller.DeleteNavigationEntries(
      base::BindLambdaForTesting([&](content::NavigationEntry* entry) {
        return entry->GetURL() == url3;
      }));
  EXPECT_EQ(true, EvalJs(shell(), "window.dispose0_fired").ExtractBool());
  EXPECT_EQ(false, EvalJs(shell(), "window.dispose1_fired").ExtractBool());
  EXPECT_EQ(true, EvalJs(shell(), "window.dispose2_fired").ExtractBool());
}

IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       NavigationApiTraverseCancelledByRaceCondition) {
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(contents(), url1));
  EXPECT_TRUE(NavigateToURL(contents(), url2));

  // Request navigation.back() in the renderer.
  ExecuteScriptAsync(contents()->GetPrimaryFrameTree().root(),
                     "navigation.back().committed.then("
                     "() => document.title = 'FAIL',"
                     "e => document.title = e.name === 'InvalidStateError'"
                     "   ? 'PASS' : 'WRONG_ERROR_TYPE');");

  // Before the navigation.back() is processed and sent to the browser, remove
  // the NavigationEntry that navigation.back() will request to be navigated to.
  contents()->GetController().PruneAllButLastCommitted();

  // The browser process should notify the renderer that an appropriate entry
  // could not be found, and the renderer should fire a navigateerror event.
  TitleWatcher title_watcher(shell()->web_contents(), u"PASS");
  EXPECT_EQ(u"PASS", title_watcher.WaitAndGetTitle());
}

// Verify that navigation API can be used to traverse to same-origin urls, even
// if they are cross site instance. Regression test for
// https://crbug.com/1431412.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       NavigationApiBackSameOriginDifferentSiteInstance) {
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  scoped_refptr<SiteInstance> initial_site_instance =
      contents()->GetSiteInstance();

  TestNavigationObserver navigation_observer(contents());
  NavigationController::LoadURLParams params(url2);
  params.force_new_browsing_instance = true;
  controller.LoadURLWithParams(params);
  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_NE(initial_site_instance, contents()->GetSiteInstance());

  EXPECT_TRUE(EvalJs(shell(), "navigation.canGoBack").ExtractBool());
  TestNavigationObserver navigation_observer2(contents());
  ExecuteScriptAsync(contents()->GetPrimaryFrameTree().root(),
                     "navigation.back()");
  navigation_observer2.Wait();

  EXPECT_EQ(url1, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(initial_site_instance, contents()->GetSiteInstance());
}

// Tests that renderer-initiated navigation cancellation from the same JS task
// that created the navigation can still be triggered after WillProcessResponse,
// as the browser defers the navigation until the JS task that started it
// finishes.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTestNoServer,
                       RendererInitiatedCancellationDueToJS) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");

  ASSERT_TRUE(embedded_test_server()->Start());
  // Navigate to a page with an iframe.
  GURL url1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(contents(), url1));

  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  GURL child_url = child->current_url();
  GURL child_url_2(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Now navigate the child frame to another same-site document, but cancel it
  // from the JS task after a synchronous XHR request that we can control.
  TestNavigationManager nav_manager(contents(), child_url_2);
  ExecuteScriptAsync(child, R"(
      location.href = '/title1.html';
      var request = new XMLHttpRequest();
      request.open("GET", "/fetch", /*async=*/false);
      request.send("");
      var fetch_success = (request.readyState == 4 && request.status == 200);
      window.stop();
    )");

  // The navigation should be able to start, as the synchronous XHR request
  // hasn't completed, and the navigation hasn't been canceled yet.
  EXPECT_TRUE(nav_manager.WaitForRequestStart());
  nav_manager.ResumeNavigation();
  NavigationRequest* request =
      static_cast<NavigationRequest*>(nav_manager.GetNavigationHandle());
  EXPECT_EQ(request, child->navigation_request());

  // The navigation should be able to reach the WillProcessResponse stage,
  // and gets deferred by RendererCancellationThrottle after that. Wait for the
  // first NavigationThrottle deferral.
  base::RunLoop run_loop;
  NavigationThrottleRunner* throttle_runner =
      request->GetNavigationThrottleRunnerForTesting();
  throttle_runner->set_first_deferral_callback_for_testing(
      run_loop.QuitClosure());
  run_loop.Run();

  // Check that the deferral is caused by RendererCancellationThrottle.
  EXPECT_TRUE(request->IsDeferredForTesting());
  EXPECT_STREQ("RendererCancellationThrottle",
               throttle_runner->GetDeferringThrottle()->GetNameForLogging());
  EXPECT_EQ(request->state(), NavigationRequest::WILL_PROCESS_RESPONSE);

  // Unblock the JS task in the renderer by sending the response for the sync
  // XHR request. After that, the renderer will trigger navigation cancellation
  // due to the window.stop() call.
  fetch_response.WaitForRequest();
  fetch_response.Send(net::HTTP_OK, "foo");
  fetch_response.Done();

  // The navigation got canceled without committing.
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_FALSE(nav_manager.was_successful());
  EXPECT_EQ(child_url, child->current_url());
  EXPECT_EQ(true, EvalJs(child, "fetch_success"));
}

// Tests that the crash of the renderer that created a navigation will cancel
// the navigation.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTestNoServer,
                       RendererInitiatedCancellationDueToCrash) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");

  ASSERT_TRUE(embedded_test_server()->Start());
  // Navigate to a page with an iframe.
  GURL url1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(contents(), url1));

  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  GURL child_url = child->current_url();
  GURL child_url_2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Now navigate the child frame to another same-site page, but cancel it
  // from the JS task after a synchronous XHR request that we can control.
  TestNavigationManager nav_manager(contents(), child_url_2);
  ExecuteScriptAsync(child, R"(
      location.href = '/title1.html';
      var request = new XMLHttpRequest();
      request.open("GET", "/fetch", /*async=*/false);
      request.send("");
      var fetch_success = (request.readyState == 4 && request.status == 200);
      window.stop();
    )");

  // The navigation should be able to start, as the synchronous XHR request
  // hasn't completed, and the navigation hasn't been canceled yet.
  EXPECT_TRUE(nav_manager.WaitForRequestStart());
  nav_manager.ResumeNavigation();
  NavigationRequest* request =
      static_cast<NavigationRequest*>(nav_manager.GetNavigationHandle());
  EXPECT_EQ(request, child->navigation_request());

  // The navigation should be able to reach the WillProcessResponse stage,
  // and gets deferred by RendererCancellationThrottle after that. Wait for the
  // first NavigationThrottle deferral.
  base::RunLoop run_loop;
  NavigationThrottleRunner* throttle_runner =
      request->GetNavigationThrottleRunnerForTesting();
  throttle_runner->set_first_deferral_callback_for_testing(
      run_loop.QuitClosure());
  run_loop.Run();

  // Check that the deferral is caused by RendererCancellationThrottle.
  EXPECT_TRUE(request->IsDeferredForTesting());
  EXPECT_STREQ("RendererCancellationThrottle",
               throttle_runner->GetDeferringThrottle()->GetNameForLogging());
  EXPECT_EQ(request->state(), NavigationRequest::WILL_PROCESS_RESPONSE);

  // Kill the renderer process that started the navigation.
  RenderProcessHost* child_process = child->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();

  // The navigation got canceled without committing.
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_FALSE(nav_manager.was_successful());
  // If the process is not shared with the main frame, the current URL of the
  // child frame is empty after the process crashed. If the process is shared,
  // the child frame will be gone because the main frame's process crashed.
  if (AreAllSitesIsolatedForTesting())
    EXPECT_EQ(GURL(), child->current_url());
}

// Tests that if waiting for a renderer-initiated navigation cancellation takes
// too long, a warning for an unresponsive renderer will be sent.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTestNoServer,
    RendererInitiatedCancellationTimeoutWarnsUnresponsiveRenderer) {
  net::test_server::ControllableHttpResponse fetch_response(
      embedded_test_server(), "/fetch");

  ASSERT_TRUE(embedded_test_server()->Start());
  // Navigate to a page with an iframe.
  GURL url1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(contents(), url1));

  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  GURL child_url = child->current_url();
  GURL child_url_2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Set the cancellation timeout to a low enough value to wait in the test.
  RendererCancellationThrottle::SetCancellationTimeoutForTesting(
      base::Milliseconds(100));
  UnresponsiveRendererObserver unresponsive_renderer_observer(contents());

  // Now navigate the child frame to another same-site page, but cancel it
  // from the JS task after a synchronous XHR request that we can control.
  TestNavigationManager nav_manager(contents(), child_url_2);
  ExecuteScriptAsync(child, R"(
      location.href = '/title1.html';
      var request = new XMLHttpRequest();
      request.open("GET", "/fetch", /*async=*/false);
      request.send("");
      var fetch_success = (request.readyState == 4 && request.status == 200);
      window.stop();
    )");

  // The navigation should be able to start, as the synchronous XHR request
  // hasn't completed, and the navigation hasn't been canceled yet.
  EXPECT_TRUE(nav_manager.WaitForRequestStart());
  nav_manager.ResumeNavigation();
  NavigationRequest* request =
      static_cast<NavigationRequest*>(nav_manager.GetNavigationHandle());
  EXPECT_EQ(request, child->navigation_request());

  // The navigation should be able to reach the WillProcessResponse stage,
  // and gets deferred by RendererCancellationThrottle after that. Wait for the
  // first NavigationThrottle deferral.
  base::RunLoop run_loop;
  NavigationThrottleRunner* throttle_runner =
      request->GetNavigationThrottleRunnerForTesting();
  throttle_runner->set_first_deferral_callback_for_testing(
      run_loop.QuitClosure());
  run_loop.Run();

  // Check that the deferral is caused by RendererCancellationThrottle.
  EXPECT_TRUE(request->IsDeferredForTesting());
  EXPECT_STREQ("RendererCancellationThrottle",
               throttle_runner->GetDeferringThrottle()->GetNameForLogging());
  EXPECT_EQ(request->state(), NavigationRequest::WILL_PROCESS_RESPONSE);

  // Verify that we will be notified about the unresponsive renderer.
  RenderProcessHost* hung_process = unresponsive_renderer_observer.Wait();
  EXPECT_EQ(hung_process, child->current_frame_host()->GetProcess());

  // Verify that the throttle still waits for the renderer-initiated
  // cancellation.
  EXPECT_TRUE(request->IsDeferredForTesting());

  // Reset the cancellation timeout to the default value.
  RendererCancellationThrottle::SetCancellationTimeoutForTesting(
      base::TimeDelta());
}

// Tests that navigating to a document that was previously sandboxed but later
// on is loaded again without being marked as sandboxed would not reuse the
// opaque origin that was used when the document was first loaded, and instead
// recalculate the origin using the latest state.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTestNoServer,
                       HistoryNavigationToPreviouslySandboxedDocument) {
  net::test_server::ControllableHttpResponse response1(
      embedded_test_server(), "/sandboxed_on_first_load_only");
  net::test_server::ControllableHttpResponse response2(
      embedded_test_server(), "/sandboxed_on_first_load_only");
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents())->GetPrimaryFrameTree().root();

  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/sandboxed_on_first_load_only"));
  {
    // Navigate to a page that is sandboxed when it's first loaded.
    TestNavigationObserver nav_observer(contents());
    shell()->LoadURL(main_url);
    response1.WaitForRequest();
    // Note: we use Cache-Control: no-store to prevent the HTTP cache, so that
    // we can change the response on the next navigation to this page.
    response1.Send(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Security-Policy: sandbox\r\n"
        "Cache-Control: no-store\r\n"
        "\r\n"
        "sandboxed document");
    response1.Done();
    nav_observer.Wait();
    EXPECT_EQ(main_url, controller.GetLastCommittedEntry()->GetURL());
    // The document is sandboxed, and has an opaque origin derived from
    // `main_url`.
    EXPECT_TRUE(root->current_frame_host()->IsSandboxed(
        network::mojom::WebSandboxFlags::kAll));
    EXPECT_TRUE(root->current_frame_host()->GetLastCommittedOrigin().opaque());
    EXPECT_TRUE(
        root->current_frame_host()->GetLastCommittedOrigin().CanBeDerivedFrom(
            main_url));
  }

  {
    // Navigate to another document.
    TestNavigationObserver back_load_observer(contents());
    GURL url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));
    shell()->LoadURL(url_2);
    back_load_observer.Wait();
    EXPECT_EQ(url_2, controller.GetLastCommittedEntry()->GetURL());
  }

  {
    // Navigate back to a page that was sandboxed before, but is no longer
    // sandboxed now (as the HTTP response had changed).
    TestNavigationObserver nav_observer(contents());
    controller.GoBack();
    response2.WaitForRequest();
    response2.Send(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "\r\n"
        "non-sandboxed document");
    response2.Done();
    nav_observer.Wait();
    EXPECT_EQ(main_url, controller.GetLastCommittedEntry()->GetURL());
    // The document is not sandboxed, and has a non-opaque origin based on
    // `main_url`.
    EXPECT_FALSE(root->current_frame_host()->IsSandboxed(
        network::mojom::WebSandboxFlags::kAll));
    EXPECT_FALSE(root->current_frame_host()->GetLastCommittedOrigin().opaque());
    EXPECT_EQ(root->current_frame_host()->GetLastCommittedOrigin(),
              url::Origin::Create(main_url));
  }
}

// Same with above, but with an iframe and the sandbox attribute instead.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       HistoryNavigationToPreviouslySandboxedDocumentOnIframe) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  GURL main_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents())->GetPrimaryFrameTree().root();

  {
    // Create an iframe and navigate it somewhere, to avoid initial empty
    // document replacement on subsequent navigations.
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, R"(
        let iframe = document.createElement('iframe');
        iframe.id = 'child';
        iframe.src = location.href;
        document.body.appendChild(iframe);)"));
    capturer.Wait();
  }
  FrameTreeNode* child = root->child_at(0);

  {
    // Set the sandbox attribute on the iframe and navigate it to about:blank.
    FrameNavigateParamsCapturer capturer(child);
    EXPECT_TRUE(ExecJs(root, R"(
        document.getElementById('child').setAttribute('sandbox', '');
        document.getElementById('child').src = 'about:blank';)"));
    capturer.Wait();

    // The document is sandboxed, and has an opaque origin derived from
    // `main_url` (the initiator origin).
    EXPECT_TRUE(child->current_frame_host()->IsSandboxed(
        network::mojom::WebSandboxFlags::kAll));
    EXPECT_TRUE(child->current_frame_host()->GetLastCommittedOrigin().opaque());
    EXPECT_TRUE(
        child->current_frame_host()->GetLastCommittedOrigin().CanBeDerivedFrom(
            main_url));
  }

  {
    GURL url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));
    // Navigate the iframe to somewhere else.
    FrameNavigateParamsCapturer capturer(child);
    NavigateFrameToURL(child, url_2);
    capturer.Wait();
  }

  {
    // Remove the sandbox attribute and navigate the iframe back.
    EXPECT_TRUE(ExecJs(
        root, "document.getElementById('child').removeAttribute('sandbox');"));
    FrameNavigateParamsCapturer capturer(child);
    controller.GoBack();
    capturer.Wait();
    // The document is not sandboxed, and has a non-opaque origin based on
    // `main_url`.
    EXPECT_FALSE(child->current_frame_host()->IsSandboxed(
        network::mojom::WebSandboxFlags::kAll));
    EXPECT_FALSE(
        child->current_frame_host()->GetLastCommittedOrigin().opaque());
    EXPECT_EQ(child->current_frame_host()->GetLastCommittedOrigin(),
              url::Origin::Create(main_url));
  }
}

// Tests that navigating to a document that was previously not sandboxed but
// later on is loaded again while being marked as sandboxed would use an opaque
// origin, and the document is marked as sandboxed.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTestNoServer,
                       HistoryNavigationToPreviouslyNonSandboxedDocument) {
  net::test_server::ControllableHttpResponse response1(
      embedded_test_server(), "/sandboxed_on_second_load_only");
  net::test_server::ControllableHttpResponse response2(
      embedded_test_server(), "/sandboxed_on_second_load_only");
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents())->GetPrimaryFrameTree().root();

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/sandboxed_on_second_load_only"));
  {
    // Navigate to a page that is not sandboxed when it's first loaded.
    TestNavigationObserver nav_observer(contents());
    shell()->LoadURL(main_url);
    response1.WaitForRequest();
    // Note: we use Cache-Control: no-store to prevent the HTTP cache, so that
    // we can change the response on the next navigation to this page.
    response1.Send(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Cache-Control: no-store\r\n"
        "\r\n"
        "non-sandboxed document");
    response1.Done();
    nav_observer.Wait();
    EXPECT_EQ(main_url, controller.GetLastCommittedEntry()->GetURL());
    // The document is not sandboxed, and should have an origin based on
    // `main_url`.
    EXPECT_FALSE(root->current_frame_host()->IsSandboxed(
        network::mojom::WebSandboxFlags::kAll));
    EXPECT_FALSE(root->current_frame_host()->GetLastCommittedOrigin().opaque());
    EXPECT_EQ(url::Origin::Create(main_url),
              root->current_frame_host()->GetLastCommittedOrigin());
  }

  {
    // Navigate to another document.
    TestNavigationObserver back_load_observer(contents());
    GURL url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));
    shell()->LoadURL(url_2);
    back_load_observer.Wait();
    EXPECT_EQ(url_2, controller.GetLastCommittedEntry()->GetURL());
  }

  {
    // Navigate back to a page that was not sandboxed before, but is now
    // sandboxed (as the HTTP response had changed).
    TestNavigationObserver nav_observer(contents());
    controller.GoBack();
    response2.WaitForRequest();
    response2.Send(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Security-Policy: sandbox\r\n"
        "\r\n"
        "sandboxed document");
    response2.Done();
    nav_observer.Wait();
    EXPECT_EQ(main_url, controller.GetLastCommittedEntry()->GetURL());
    // The document is sandboxed, and has an opaque origin derived from
    // `main_url`.
    // TODO(https://crbug.com/1359351): The origin should be recalculated and
    // result in a non-opaque origin instead.
    EXPECT_TRUE(root->current_frame_host()->IsSandboxed(
        network::mojom::WebSandboxFlags::kAll));
    EXPECT_TRUE(root->current_frame_host()->GetLastCommittedOrigin().opaque());
    EXPECT_TRUE(
        root->current_frame_host()->GetLastCommittedOrigin().CanBeDerivedFrom(
            main_url));
  }
}

// Same with above, but with an iframe and the sandbox attribute instead.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    HistoryNavigationToPreviouslyNonSandboxedDocumentOnIframe) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  GURL main_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents())->GetPrimaryFrameTree().root();

  {
    // Create an iframe and navigate it somewhere, to avoid initial empty
    // document replacement on subsequent navigations.
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, R"(
        let iframe = document.createElement('iframe');
        iframe.id = 'child';
        iframe.src = location.href;
        document.body.appendChild(iframe);)"));
    capturer.Wait();
  }
  FrameTreeNode* child = root->child_at(0);

  {
    // Navigate the iframe to about:blank.
    FrameNavigateParamsCapturer capturer(child);
    EXPECT_TRUE(ExecJs(root, R"(
        document.getElementById('child').src = 'about:blank';)"));
    capturer.Wait();

    // The document is not sandboxed, and it's origin is the same as the main
    // frame's origin (the initiator origin).
    EXPECT_FALSE(child->current_frame_host()->IsSandboxed(
        network::mojom::WebSandboxFlags::kAll));
    EXPECT_FALSE(
        child->current_frame_host()->GetLastCommittedOrigin().opaque());
    EXPECT_EQ(child->current_frame_host()->GetLastCommittedOrigin(),
              root->current_frame_host()->GetLastCommittedOrigin());
  }

  {
    GURL url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));
    // Navigate the iframe to somewhere else.
    FrameNavigateParamsCapturer capturer(child);
    NavigateFrameToURL(child, url_2);
    capturer.Wait();
  }

  {
    // Add the sandbox attribute and navigate the iframe back.
    EXPECT_TRUE(ExecJs(
        root, "document.getElementById('child').setAttribute('sandbox', '');"));
    FrameNavigateParamsCapturer capturer(child);
    controller.GoBack();
    capturer.Wait();
    // The document is sandboxed, and has an opaque origin derived from
    // `main_url` (the initiator origin).
    EXPECT_TRUE(child->current_frame_host()->IsSandboxed(
        network::mojom::WebSandboxFlags::kAll));
    EXPECT_TRUE(child->current_frame_host()->GetLastCommittedOrigin().opaque());
    EXPECT_TRUE(
        child->current_frame_host()->GetLastCommittedOrigin().CanBeDerivedFrom(
            main_url));
  }
}

// Verifies that deletion of speculative RenderFrameHost due to navigation
// cancellation doesn't crash.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    DeleteSpeculativeRenderFrameHostDueToNavigationCancellation_MainFrame) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title3.html"));
  // Navigate to the original page.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();

  if (ShouldCreateNewHostForAllFrames()) {
    SCOPED_TRACE(testing::Message()
                 << " Testing main frame same-site navigation.");
    // Main frame same-site navigation creates a speculative RenderFrameHost.
    TestNavigationManager navigation_manager(contents(), url_a);
    EXPECT_TRUE(ExecJs(root, JsReplace("location.href = $1;", url_a)));
    EXPECT_TRUE(navigation_manager.WaitForRequestStart());
    EXPECT_TRUE(root->render_manager()->speculative_frame_host());

    // Cancel the navigation. This should trigger the deletion of the
    // speculative RenderFrameHost.
    root->ResetNavigationRequest(NavigationDiscardReason::kCancelled);
    EXPECT_FALSE(root->render_manager()->speculative_frame_host());
  }

  if (AreAllSitesIsolatedForTesting() || ShouldCreateNewHostForAllFrames()) {
    SCOPED_TRACE(testing::Message()
                 << " Testing main frame cross-site navigation.");
    // Main frame cross-site navigation creates a speculative RenderFrameHost.
    TestNavigationManager navigation_manager(contents(), url_b);
    EXPECT_TRUE(ExecJs(root, JsReplace("location.href = $1;", url_b)));
    EXPECT_TRUE(navigation_manager.WaitForRequestStart());
    EXPECT_TRUE(root->render_manager()->speculative_frame_host());
    // Cancel the navigation. This should trigger the deletion of the
    // speculative RenderFrameHost.
    root->ResetNavigationRequest(NavigationDiscardReason::kCancelled);
    EXPECT_FALSE(root->render_manager()->speculative_frame_host());
  }
}

// Same as above but for subframes.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    DeleteSpeculativeRenderFrameHostDueToNavigationCancellation_Subframe) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title3.html"));
  // Navigate to a page with an iframe.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  if (child->current_frame_host()
          ->ShouldChangeRenderFrameHostOnSameSiteNavigation()) {
    SCOPED_TRACE(testing::Message()
                 << " Testing subframe same-site navigation.");
    // Subframe same-site navigation creates a speculative RenderFrameHost.
    TestNavigationManager navigation_manager(contents(), url_a);
    EXPECT_TRUE(ExecJs(child, JsReplace("location.href = $1;", url_a)));
    EXPECT_TRUE(navigation_manager.WaitForRequestStart());
    EXPECT_TRUE(child->render_manager()->speculative_frame_host());
    // Cancel the navigation. This should trigger the deletion of the
    // speculative RenderFrameHost.
    child->ResetNavigationRequest(NavigationDiscardReason::kCancelled);
    EXPECT_FALSE(child->render_manager()->speculative_frame_host());
  }

  if (AreAllSitesIsolatedForTesting() ||
      child->current_frame_host()
          ->ShouldChangeRenderFrameHostOnSameSiteNavigation()) {
    SCOPED_TRACE(testing::Message()
                 << " Testing subframe same-site navigation.");
    // Subframe cross-site navigation creates a speculative RenderFrameHost.
    TestNavigationManager navigation_manager(contents(), url_b);
    EXPECT_TRUE(ExecJs(child, JsReplace("location.href = $1;", url_b)));
    EXPECT_TRUE(navigation_manager.WaitForRequestStart());
    EXPECT_TRUE(child->render_manager()->speculative_frame_host());
    child->ResetNavigationRequest(NavigationDiscardReason::kCancelled);

    // Cancel the navigation. This should trigger the deletion of the
    // speculative RenderFrameHost.
    EXPECT_FALSE(child->render_manager()->speculative_frame_host());
  }
}

// Verifies that deletion of speculative RenderFrameHost due to deletion of
// the frame it's committing in doesn't crash.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    DeleteSpeculativeRenderFrameHostDueToFrameDeletion_MainFrame) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title3.html"));
  // Navigate to the original page.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* original_root = contents()->GetPrimaryFrameTree().root();

  if (ShouldCreateNewHostForAllFrames()) {
    SCOPED_TRACE(testing::Message()
                 << " Testing main frame same-site navigation.");

    // Open a new window.
    ShellAddedObserver new_shell_observer;
    EXPECT_TRUE(ExecJs(original_root,
                       "var w = window.open('" + main_url.spec() + "');"));
    Shell* new_shell = new_shell_observer.GetShell();
    ASSERT_NE(new_shell->web_contents(), shell()->web_contents());
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));
    FrameTreeNode* new_window_root =
        static_cast<WebContentsImpl*>(new_shell->web_contents())
            ->GetPrimaryFrameTree()
            .root();

    // Main frame same-site navigation in the new window creates a speculative
    // RenderFrameHost.
    TestNavigationManager navigation_manager(new_shell->web_contents(), url_a);
    EXPECT_TRUE(
        ExecJs(new_window_root, JsReplace("location.href = $1;", url_a)));
    EXPECT_TRUE(navigation_manager.WaitForRequestStart());
    EXPECT_TRUE(new_window_root->render_manager()->speculative_frame_host());

    // Close the window. This should trigger the deletion of the
    // speculative RenderFrameHost.
    EXPECT_TRUE(ExecJs(original_root, "w.close()"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
    EXPECT_FALSE(navigation_manager.was_committed());
  }

  if (AreAllSitesIsolatedForTesting() || ShouldCreateNewHostForAllFrames()) {
    SCOPED_TRACE(testing::Message()
                 << " Testing main frame cross-site navigation.");

    // Open a new window.
    ShellAddedObserver new_shell_observer;
    EXPECT_TRUE(ExecJs(original_root,
                       "var w = window.open('" + main_url.spec() + "');"));
    Shell* new_shell = new_shell_observer.GetShell();
    ASSERT_NE(new_shell->web_contents(), shell()->web_contents());
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));
    FrameTreeNode* new_window_root =
        static_cast<WebContentsImpl*>(new_shell->web_contents())
            ->GetPrimaryFrameTree()
            .root();

    // Main frame cross-site navigation in the new window creates a speculative
    // RenderFrameHost.
    TestNavigationManager navigation_manager(new_shell->web_contents(), url_b);
    EXPECT_TRUE(
        ExecJs(new_window_root, JsReplace("location.href = $1;", url_b)));
    EXPECT_TRUE(navigation_manager.WaitForRequestStart());
    EXPECT_TRUE(new_window_root->render_manager()->speculative_frame_host());

    // Close the window. This should trigger the deletion of the
    // speculative RenderFrameHost.
    EXPECT_TRUE(ExecJs(original_root, "w.close()"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
    EXPECT_FALSE(navigation_manager.was_committed());
  }
}

// Same as above but for subframes.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    DeleteSpeculativeRenderFrameHostDueToFrameDeletion_Subframe) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title3.html"));
  // Navigate to the original page.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  {
    // Create a new iframe.
    LoadCommittedCapturer capturer(contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, main_url)));
    capturer.Wait();
  }

  if (root->child_at(0)
          ->current_frame_host()
          ->ShouldChangeRenderFrameHostOnSameSiteNavigation()) {
    SCOPED_TRACE(testing::Message()
                 << " Testing subframe same-site navigation.");
    // Subframe same-site navigation in the new iframe creates a speculative
    // RenderFrameHost.
    TestNavigationManager navigation_manager(contents(), url_a);
    EXPECT_TRUE(
        ExecJs(root->child_at(0), JsReplace("location.href = $1;", url_a)));
    EXPECT_TRUE(navigation_manager.WaitForRequestStart());
    EXPECT_TRUE(root->child_at(0)->render_manager()->speculative_frame_host());

    // Close the window. This should trigger the deletion of the
    // speculative RenderFrameHost.
    EXPECT_TRUE(
        ExecJs(root, "document.getElementsByTagName('iframe')[0].remove()"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
    EXPECT_FALSE(navigation_manager.was_committed());
  }

  {
    // Create a new iframe.
    LoadCommittedCapturer capturer(contents());
    EXPECT_TRUE(ExecJs(root, JsReplace(kAddFrameWithSrcScript, main_url)));
    capturer.Wait();
  }

  if (AreAllSitesIsolatedForTesting() ||
      root->child_at(0)
          ->current_frame_host()
          ->ShouldChangeRenderFrameHostOnSameSiteNavigation()) {
    SCOPED_TRACE(testing::Message()
                 << " Testing subframe cross-site navigation.");
    // Subframe cross-site navigation in the new iframe creates a speculative
    // RenderFrameHost.
    TestNavigationManager navigation_manager(contents(), url_b);
    EXPECT_TRUE(
        ExecJs(root->child_at(0), JsReplace("location.href = $1;", url_b)));
    EXPECT_TRUE(navigation_manager.WaitForRequestStart());
    EXPECT_TRUE(root->child_at(0)->render_manager()->speculative_frame_host());

    // Close the iframe. This should trigger the deletion of the
    // speculative RenderFrameHost.
    EXPECT_TRUE(
        ExecJs(root, "document.getElementsByTagName('iframe')[0].remove()"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
    EXPECT_FALSE(navigation_manager.was_committed());
  }
}

// Test that starting a new navigation will cancel a previous navigation that
// had started earlier if one of the navigations uses a speculative
// RenderFrameHost and the other one does not.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       StartNewNavigationWithExistingNavigation_RequestStart) {
  if (ShouldCreateNewHostForAllFrames()) {
    // This test expects cross-site main frame navigations to change
    // RenderFrameHosts, and same-site main frame navigations to reuse the
    // current RenderFrameHosts.
    return;
  }
  // Ensure same-site navigation won't change RenderFrameHosts due to BFCache.
  DisableBackForwardCacheForTesting(
      contents(), BackForwardCacheImpl::TEST_ASSUMES_NO_RENDER_FRAME_CHANGE);

  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_a3(embedded_test_server()->GetURL("a.com", "/title3.html"));
  GURL url_b1(embedded_test_server()->GetURL("b.com", "/title1.html"));
  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));

  // 2) Start a same-RFH navigation to A2, but don't commit it yet.
  TestNavigationManager a2_navigation(shell()->web_contents(), url_a2);
  shell()->LoadURL(url_a2);
  EXPECT_TRUE(a2_navigation.WaitForRequestStart());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(root->navigation_request(), a2_navigation.GetNavigationHandle());

  // 3) Start a cross-RFH navigation to B1, which will cancel the navigation to
  // A2.
  TestNavigationManager b1_navigation(shell()->web_contents(), url_b1);
  shell()->LoadURL(url_b1);
  EXPECT_TRUE(b1_navigation.WaitForRequestStart());
  EXPECT_EQ(url_b1, b1_navigation.GetNavigationHandle()->GetURL());
  EXPECT_EQ(root->navigation_request(), b1_navigation.GetNavigationHandle());

  // Assert that the navigation to A2 gets cancelled.
  ASSERT_TRUE(a2_navigation.WaitForNavigationFinished());
  EXPECT_FALSE(a2_navigation.was_committed());

  // 4) Start another same-RFH navigation to A3, which will cancel the
  // previous cross-RFH navigation to B1.
  TestNavigationManager a3_navigation(shell()->web_contents(), url_a3);
  shell()->LoadURL(url_a3);
  EXPECT_TRUE(a3_navigation.WaitForRequestStart());
  EXPECT_EQ(url_a3, a3_navigation.GetNavigationHandle()->GetURL());
  EXPECT_EQ(root->navigation_request(), a3_navigation.GetNavigationHandle());

  // Assert that the navigation to B1 gets cancelled.
  ASSERT_TRUE(b1_navigation.WaitForNavigationFinished());
  EXPECT_FALSE(b1_navigation.was_committed());
}

IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    StartNewNavigationWithExistingNavigation_RequestStart_History) {
  if (ShouldCreateNewHostForAllFrames()) {
    // This test expects cross-site main frame navigations to change
    // RenderFrameHosts, and same-site main frame navigations to reuse the
    // current RenderFrameHosts.
    return;
  }
  // Ensure same-site navigation won't change RenderFrameHosts due to BFCache.
  DisableBackForwardCacheForTesting(
      contents(), BackForwardCacheImpl::TEST_ASSUMES_NO_RENDER_FRAME_CHANGE);

  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b1(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_b2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // 1) Navigate to A1, then B1, then B2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  NavigationEntryImpl* entry_a1 = controller.GetLastCommittedEntry();
  SiteInstance* site_instance_a1 =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(site_instance_a1, entry_a1->site_instance());

  EXPECT_TRUE(NavigateToURL(shell(), url_b1));
  NavigationEntryImpl* entry_b1 = controller.GetLastCommittedEntry();
  SiteInstance* site_instance_b1 =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(site_instance_b1, entry_b1->site_instance());

  EXPECT_TRUE(NavigateToURL(shell(), url_b2));
  NavigationEntryImpl* entry_b2 = controller.GetLastCommittedEntry();
  SiteInstance* site_instance_b2 =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(site_instance_b2, entry_b2->site_instance());

  // 2) Go back to trigger a same-RFH history navigation to B1, but don't commit
  // it yet.
  TestNavigationManager b1_navigation(shell()->web_contents(), url_b1);
  controller.GoBack();
  EXPECT_TRUE(b1_navigation.WaitForRequestStart());
  EXPECT_EQ(root->navigation_request(), b1_navigation.GetNavigationHandle());

  // 3) Go back again to trigger a cross-RFH navigation to A1, which will cancel
  // the navigation to B1.
  TestNavigationManager a1_navigation(shell()->web_contents(), url_a1);
  controller.GoBack();
  EXPECT_TRUE(a1_navigation.WaitForRequestStart());
  EXPECT_EQ(url_a1, a1_navigation.GetNavigationHandle()->GetURL());
  EXPECT_EQ(root->navigation_request(), a1_navigation.GetNavigationHandle());

  // Assert that the navigation to B1 gets cancelled.
  ASSERT_TRUE(b1_navigation.WaitForNavigationFinished());
  EXPECT_FALSE(b1_navigation.was_committed());

  // Assert that the navigation to A1 successfully commits.
  ASSERT_TRUE(a1_navigation.WaitForNavigationFinished());
  EXPECT_TRUE(a1_navigation.was_successful());
  // Assert that the correct NavigationEntry is used, and no entry gets
  // corrupted.
  EXPECT_EQ(entry_a1, controller.GetLastCommittedEntry());
  EXPECT_EQ(site_instance_a1, entry_a1->site_instance());
  EXPECT_EQ(site_instance_b1, entry_b1->site_instance());
  EXPECT_EQ(site_instance_b2, entry_b2->site_instance());
}

namespace {
// Starts a navigation to `new_navigation_url` when the navigation mamnaged by
// `prev_navigation_manager` gets to the ReadyToCommit stage.
void StartNavigationOnReadyToCommit(
    Shell* shell,
    TestNavigationManager& prev_navigation_manager,
    GURL& new_navigation_url) {
  base::RunLoop run_loop;
  static_cast<NavigationRequest*>(prev_navigation_manager.GetNavigationHandle())
      ->set_ready_to_commit_callback_for_testing(run_loop.QuitClosure());
  prev_navigation_manager.ResumeNavigation();
  run_loop.Run();
  shell->LoadURL(new_navigation_url);
}
}  // namespace

// Same as StartNewNavigationWithExistingNavigation_RequestStart but start the
// new navigation when the previous navigation is at the "ReadyToCommit" stage,
// after the NavigationRequest's ownership had moved to the committing
// RenderFrameHost. The behavior is a bit different as the cancellation behavior
// differs depending on which navigation starts or commits first.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       StartNewNavigationWithExistingNavigation_ReadyToCommit) {
  if (ShouldCreateNewHostForAllFrames()) {
    // This test expects cross-site main frame navigations to change
    // RenderFrameHosts, and same-site main frame navigations to reuse the
    // current RenderFrameHosts.
    return;
  }
  // Ensure same-site navigation won't change RenderFrameHosts due to BFCache.
  DisableBackForwardCacheForTesting(
      contents(), BackForwardCacheImpl::TEST_ASSUMES_NO_RENDER_FRAME_CHANGE);

  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_a3(embedded_test_server()->GetURL("a.com", "/title3.html"));
  GURL url_b1(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_b2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));

  // 2) Start a same-RFH navigation to A2, but don't commit it yet.
  TestNavigationManager a2_navigation(shell()->web_contents(), url_a2);
  shell()->LoadURL(url_a2);
  EXPECT_TRUE(a2_navigation.WaitForResponse());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(root->navigation_request(), a2_navigation.GetNavigationHandle());

  // 3) Start a cross-RFH navigation to B1 after A2 gets to "pending commit"
  // stage, which will not cancel the navigation to A2 as A2's NavigationRequest
  // had moved.
  TestNavigationManager b1_navigation(shell()->web_contents(), url_b1);
  StartNavigationOnReadyToCommit(shell(), a2_navigation, url_b1);
  EXPECT_TRUE(b1_navigation.WaitForRequestStart());
  EXPECT_EQ(url_b1, b1_navigation.GetNavigationHandle()->GetURL());
  EXPECT_EQ(root->navigation_request(), b1_navigation.GetNavigationHandle());

  // Assert that the navigation to A2 didn't get cancelled, and finish
  // committing A2. This shouldn't cancel the navigation to B1.
  ASSERT_TRUE(a2_navigation.WaitForNavigationFinished());
  EXPECT_TRUE(a2_navigation.was_successful());

  // A2's navigation commit didn't cancel B1's navigation.
  EXPECT_TRUE(b1_navigation.GetNavigationHandle());

  // 4) Start a same-RFH navigation to A3 after B1 gets to "pending commit"
  // stage. The behavior depends on whether the navigation queueing feature
  // level is at least kAvoidRedundantCancellations:
  // - If it is at least kAvoidReundantCancellations, A3's navigation won't
  //   cancel the previous cross-RFH navigation to B1, as B1's NavigationRequest
  //   had moved.
  // - Otherwise, A3's navigation will cancel the previous cross-RFH navigation
  //   to B1, because when a same-RFH navigation starts it will delete the
  //   speculative RFH.
  TestNavigationManager a3_navigation(shell()->web_contents(), url_a3);
  EXPECT_TRUE(b1_navigation.WaitForResponse());
  StartNavigationOnReadyToCommit(shell(), b1_navigation, url_a3);

  if (ShouldAvoidRedundantNavigationCancellations()) {
    // Assert that the navigation to B1 didn't get cancelled, and finish
    // committing B1. This shouldn't cancel the navigation to A3.
    ASSERT_TRUE(b1_navigation.WaitForNavigationFinished());
    EXPECT_TRUE(b1_navigation.was_successful());

    // B1's navigation commit didn't cancel A3's navigation.
    EXPECT_TRUE(a3_navigation.WaitForResponse());
    EXPECT_TRUE(a3_navigation.GetNavigationHandle());
    ASSERT_TRUE(a3_navigation.WaitForNavigationFinished());
    EXPECT_TRUE(a3_navigation.was_successful());
  } else {
    EXPECT_TRUE(a3_navigation.WaitForResponse());
    EXPECT_EQ(url_a3, a3_navigation.GetNavigationHandle()->GetURL());
    EXPECT_EQ(root->navigation_request(), a3_navigation.GetNavigationHandle());

    // Assert that the navigation to B1 gets cancelled.
    EXPECT_TRUE(b1_navigation.WaitForNavigationFinished());
    EXPECT_FALSE(b1_navigation.was_committed());

    // 5) Start a cross-RFH navigation to B2 after A3 gets to "pending commit"
    // stage, which will cancel the previous same-RFH navigation to A3 when B2
    // commits first, because when the previous RFH gets unloaded it will
    // cancel all ongoing navigations in the pending deletion RFH.
    TestNavigationManager b2_navigation(shell()->web_contents(), url_b2);
    // Ignore A3's commit so that B2's navigation can start and finish
    // committing before A3 finishes committing.
    DidCommitNavigationCanceller ignore_a3_commit(
        shell()->web_contents(), url_a3,
        base::BindLambdaForTesting([&]() { shell()->LoadURL(url_b2); }));
    // Continue the A3 navigation, but its commit will be dropped.
    a3_navigation.ResumeNavigation();
    // The navigation to B2 will start, but won't cancel A3's navigation just
    // yet.
    EXPECT_TRUE(b2_navigation.WaitForResponse());
    EXPECT_TRUE(a3_navigation.GetNavigationHandle());

    // The navigation to B2 finished committing, and cancels A3's navigation.
    EXPECT_TRUE(b2_navigation.WaitForNavigationFinished());
    EXPECT_TRUE(b2_navigation.was_successful());
    // Assert A3's navigation finished but didn't get committed.
    EXPECT_TRUE(a3_navigation.WaitForNavigationFinished());
    EXPECT_FALSE(a3_navigation.was_committed());
  }
}

// Tests that calling FrameTreeNode::ResetNavigationRequest() cancels the
// navigation owned by the FrameTreeNode but won't reset a speculative RFH that
// has a pending commit navigation, even if it was used by the deleted
// navigation.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       ResetNavigationRequestWontDeletePendingCommitRFH) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b1(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_b2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  // Load `main_url`.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderFrameHostImplWrapper rfh_a(current_main_frame_host());

  // Start the cross-site navigation to `url_b1`. Then, start a cross-site
  // navigation to `url_b2` after the `url_b1` navigation is in the
  // "pending commit" stage, so that both navigations can exist at the same
  // time (the previous NavigationRequest had already been moved to the
  //"pending commit" speculative RFH, and they both use the same speculative
  // RFH).
  TestNavigationManager b1_nav(shell()->web_contents(), url_b1);
  TestNavigationManager b2_nav(shell()->web_contents(), url_b2);
  // Start the navigation to `url_b1` then drop its DidCommit message from the
  // renderer, so that it will stay in the "pending commit" stage for the rest
  // of the test. Then, start a navigation to `url_b2`.
  DidCommitNavigationCanceller ignore_b1_commit(
      shell()->web_contents(), url_b1,
      base::BindLambdaForTesting([&]() { shell()->LoadURL(url_b2); }));
  shell()->LoadURL(url_b1);
  // Assert tht the `url_b1` navigation had started.
  EXPECT_TRUE(b1_nav.WaitForResponse());
  EXPECT_EQ(b1_nav.GetNavigationHandle(), root->navigation_request());

  b1_nav.ResumeNavigation();
  // Assert that the `url_b2` navigation had started, and didn't cancel the
  // `url_b1` navigation.
  EXPECT_TRUE(b2_nav.WaitForRequestStart());
  EXPECT_EQ(b2_nav.GetNavigationHandle(), root->navigation_request());
  EXPECT_TRUE(b1_nav.GetNavigationHandle());

  // Cancel the navigation to `url_b2` by calling ResetNavigationRequest().
  // This shouldn't cancel the navigation to `url_b1`.
  root->ResetNavigationRequest(NavigationDiscardReason::kCancelled);
  ASSERT_TRUE(b2_nav.WaitForNavigationFinished());
  EXPECT_FALSE(b2_nav.was_committed());
  EXPECT_FALSE(b2_nav.GetNavigationHandle());

  // Assert that the `url_b1` navigation and the speculative RenderFrameHost is
  // still around. Note that we can't wait for the navigation to finish here
  // because we've dropped the DidCommit message for this navigation.
  EXPECT_TRUE(b1_nav.GetNavigationHandle());
  EXPECT_TRUE(root->render_manager()->speculative_frame_host());
  EXPECT_EQ(b1_nav.GetNavigationHandle()->GetRenderFrameHost(),
            root->render_manager()->speculative_frame_host());
}

// Tests that when a RenderFrameHost gets into the "pending deletion" state due
// to another RenderFrameHost committing in the same FrameTreeNode, it won't
// cancel other navigations happening in the same FrameTreeNode.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       UnloadingPreviousRFHOnCommitWontCancelNavigation) {
  if (!ShouldAvoidRedundantNavigationCancellations()) {
    return;
  }
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b1(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_b2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  // Load `main_url`.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderFrameHostImplWrapper rfh_a(current_main_frame_host());

  // Start the cross-site navigation to `url_b1`, then start the cross-site
  // navigation to `url_b2` just before the navigation to `url_b1` processes its
  // DidCommitNavigation message, so that both navigation can exist at the same
  // time (the previous NavigationRequest has already been moved to the pending
  // commit speculative RFH, and they both use the same speculative RFH because
  // they use the same SiteInstance).
  TestNavigationManager b1_nav(shell()->web_contents(), url_b1);
  TestNavigationManager b2_nav(shell()->web_contents(), url_b2);
  NavigationStarterBeforeDidCommitNavigation urlb2_navigation_starter(
      contents(), shell(), url_b1, url_b2);
  shell()->LoadURL(url_b1);

  // Check the navigation to `url_b1` started.
  EXPECT_TRUE(b1_nav.WaitForResponse());
  EXPECT_EQ(b1_nav.GetNavigationHandle(), root->navigation_request());
  b1_nav.ResumeNavigation();

  // Check the navigation to `url_b2` started.
  EXPECT_TRUE(b2_nav.WaitForRequestStart());
  EXPECT_EQ(b2_nav.GetNavigationHandle(), root->navigation_request());

  // Wait for the `url_b1` navigation to finish.
  ASSERT_TRUE(b1_nav.WaitForNavigationFinished());
  EXPECT_TRUE(b1_nav.was_successful());

  // The RenderFrameHost had changed, which means we have started to unload the
  // previous RenderFrameHost or saved it in BFCache.
  RenderFrameHostImplWrapper rfh_b(current_main_frame_host());
  EXPECT_NE(rfh_b.get(), rfh_a.get());
  EXPECT_TRUE(!rfh_a.get() || !rfh_a->IsActive());

  // Check that the `url_b2` navigation didn't get cancelled and its associate
  // RFH type is set to NONE.
  EXPECT_NE(nullptr, root->navigation_request());
  EXPECT_EQ(b2_nav.GetNavigationHandle(), root->navigation_request());
  EXPECT_EQ(root->navigation_request()->GetAssociatedRFHType(),
            NavigationRequest::AssociatedRenderFrameHostType::NONE);

  // Check that the `url_b2` navigation's associated RFH type gets updated when
  // it gets its final RenderFrameHost.
  b2_nav.ResumeNavigation();
  EXPECT_TRUE(b2_nav.WaitForResponse());
  if (IsBackForwardCacheEnabled() || ShouldCreateNewHostForAllFrames()) {
    // When BFCache or RenderDocument is enabled, the `url_b2` navigation won't
    // reuse the current RFH.
    EXPECT_EQ(root->navigation_request()->GetAssociatedRFHType(),
              NavigationRequest::AssociatedRenderFrameHostType::SPECULATIVE);
  } else {
    EXPECT_EQ(root->navigation_request()->GetAssociatedRFHType(),
              NavigationRequest::AssociatedRenderFrameHostType::CURRENT);
  }

  // Assert that the `url_b2` navigation committed successfully.
  ASSERT_TRUE(b2_nav.WaitForNavigationFinished());
  EXPECT_TRUE(b2_nav.was_successful());
}

// Test that ConcurrentNavigationsCommitDeferringCondition will queue BFCache
// restore navigations as long as there is a pending commit RenderFrameHost.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       BFCacheRestoreDeferredWhenPendingCommitRFHExists) {
  if (!ShouldAvoidRedundantNavigationCancellations() ||
      !IsBackForwardCacheEnabled()) {
    return;
  }

  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url_3(embedded_test_server()->GetURL("c.com", "/title3.html"));

  // Load `url_1`, then `url_2`.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();

  // Start loading `url_3`, but ignore its DidCommit IPC, so that it will stay
  // at the "pending commit" stage indefinitely. Then, start a back navigation
  // to `url_1`, which should try to do a back/forward cache restore.
  TestNavigationManager pending_commit_nav_manager(contents(), url_3);
  TestActivationManager bfcache_nav_manager(contents(), url_1);
  {
    // Note that we need to scope the lifetime of DidCommitNavigationCanceller
    // to avoid crashing during the same-document navigation later on.
    DidCommitNavigationCanceller ignore_url_3_commit(
        contents(), url_3,
        base::BindLambdaForTesting([&]() { controller.GoBack(); }));
    shell()->LoadURL(url_3);
    EXPECT_TRUE(pending_commit_nav_manager.WaitForResponse());
    // Check that a speculative RenderFrameHost was created for the navigation
    // to `url_3`. Continue the `url_3` navigation, which will stay at the
    // "pending commit" stage.
    RenderFrameHostImplWrapper spec_rfh(
        root->render_manager()->speculative_frame_host());
    EXPECT_TRUE(spec_rfh.get());

    pending_commit_nav_manager.ResumeNavigation();

    // Ensure that the BFCache restore navigation starts correctly.
    EXPECT_TRUE(bfcache_nav_manager.WaitForBeforeChecks());

    // The pending commit RenderFrameHost for `url_3` is still around.
    EXPECT_EQ(spec_rfh.get(), root->render_manager()->speculative_frame_host());
    EXPECT_TRUE(spec_rfh->HasPendingCommitForCrossDocumentNavigation());
  }

  // The BFCache restore got queued as there is a pending commit RFH.
  bfcache_nav_manager.ResumeActivation();
  NavigationRequest* bfcache_nav = static_cast<NavigationRequest*>(
      bfcache_nav_manager.GetNavigationHandle());
  EXPECT_TRUE(bfcache_nav->IsQueued());

  // Do a same-document navigation in the renderer, which will create and
  // destruct a NavigationRequest on the browser side. This should not cause the
  // queued BFCache restore to continue, as there is still a pending commit
  // RFH.
  GURL url_2_foo(embedded_test_server()->GetURL("b.com", "/title2.html#foo"));
  EXPECT_TRUE(ExecJs(contents(), "location.href = '#foo'"));
  EXPECT_EQ(url_2_foo, contents()->GetLastCommittedURL());
  EXPECT_TRUE(bfcache_nav->IsQueued());

  // Trigger the pending commit RFH & NavigationRequest deletion, so that the
  // BFCache restore can commit. Note that the pending commit RFH deletion can't
  // actually happen in real life, but this is the best we can do since we've
  // dropped the DidCommit IPC before. Ideally, we would just re-trigger the
  // DidCommit IPC that we dropped, so that the pending commit RFH &
  // NavigationRequest will finish the commit and trigger the resume callback of
  // the queued navigation, but there's currently no way to do that in test.
  // Also, normally we discourage the deletion of pending commit RFHs as that
  // will result in a confused renderer, but in this case the renderer for
  // `url_3` is a brand new renderer that we will never reuse anyways, so it's
  // fine to discard the pending commit RFH.
  // TODO(https://crbug.com/1220337): Use ResumeCommitClosureSetObserver and
  // BeginNavigationInCommitCallbackInterceptor instead of doing this.
  root->render_manager()->DiscardSpeculativeRenderFrameHostForShutdown();
  EXPECT_TRUE(pending_commit_nav_manager.WaitForNavigationFinished());
  EXPECT_FALSE(pending_commit_nav_manager.was_committed());

  // Ensure that the BFCache restore can commit successfully now.
  bfcache_nav_manager.WaitForNavigationFinished();
  EXPECT_TRUE(bfcache_nav_manager.was_successful());
  EXPECT_EQ(url_1, contents()->GetLastCommittedURL());
}

// Same as above, but the BFCached RenderFrameHost gets evicted while it is
// queued, causing the NavigationRequest to not resume after the pending commit
// navigation finished committing.
IN_PROC_BROWSER_TEST_P(
    NavigationControllerBrowserTest,
    BFCacheRestoreDeferredAndEvictedWhenPendingCommitRFHExists) {
  if (!ShouldAvoidRedundantNavigationCancellations() ||
      !IsBackForwardCacheEnabled()) {
    return;
  }

  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url_3(embedded_test_server()->GetURL("c.com", "/title3.html"));

  // Load `url_1`, then `url_2`.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(contents()->GetController());
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();
  RenderFrameHostImplWrapper url_1_rfh(root->current_frame_host());

  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  // The previous RenderFrameHost should be BFCached.
  EXPECT_TRUE(url_1_rfh->IsInBackForwardCache());

  // Start loading `url_3`, but ignore its DidCommit IPC, so that it will stay
  // at the "pending commit" stage indefinitely. Then, start a back navigation
  // to `url_1`, which should try to do a back/forward cache restore.
  TestNavigationManager pending_commit_nav_manager(contents(), url_3);
  TestActivationManager bfcache_nav_manager(contents(), url_1);
  {
    DidCommitNavigationCanceller ignore_url_3_commit(
        contents(), url_3,
        base::BindLambdaForTesting([&]() { controller.GoBack(); }));
    shell()->LoadURL(url_3);
    EXPECT_TRUE(pending_commit_nav_manager.WaitForResponse());
    // Check that a speculative RenderFrameHost was created for the navigation
    // to `url_3`. Continue the `url_3` navigation, which will stay at the
    // "pending commit" stage.
    RenderFrameHostImplWrapper spec_rfh(
        root->render_manager()->speculative_frame_host());
    EXPECT_TRUE(spec_rfh.get());

    pending_commit_nav_manager.ResumeNavigation();

    // Ensure that the BFCache restore navigation starts correctly.
    EXPECT_TRUE(bfcache_nav_manager.WaitForBeforeChecks());

    // The pending commit RenderFrameHost for `url_3` is still around.
    EXPECT_EQ(spec_rfh.get(), root->render_manager()->speculative_frame_host());
    EXPECT_TRUE(spec_rfh->HasPendingCommitForCrossDocumentNavigation());
  }

  // The BFCache restore got queued as there is a pending commit RFH.
  bfcache_nav_manager.ResumeActivation();
  base::WeakPtr<NavigationRequest> bfcache_nav =
      static_cast<NavigationRequest*>(bfcache_nav_manager.GetNavigationHandle())
          ->GetWeakPtr();
  EXPECT_TRUE(bfcache_nav->IsQueued());

  // Evict the BFCache entry for `url_1_rfh`. This will cause the BFCache
  // restore navigation to be marked for "restart", posting a task to start
  // a new history navigation that won't try to do a BFCache restore.
  DisableBFCacheForRFHForTesting(url_1_rfh.get());

  // Trigger the pending commit RFH & NavigationRequest deletion, so that the
  // BFCache restore can continue. Note that the pending commit RFH deletion
  // can't actually happen in real life, but this is the best we can do since
  // we've dropped the DidCommit IPC before. Ideally, we would just re-trigger
  // the DidCommit IPC that we dropped, so that the pending commit RFH &
  // NavigationRequest will finish the commit and trigger the resume callback of
  // the queued navigation, but there's currently no way to do that in test.
  // Also, normally we discourage the deletion of pending commit RFHs as that
  // will result in a confused renderer, but in this case the renderer for
  // `url_3` is a brand new renderer that we will never reuse anyways, so it's
  // fine to discard the pending commit RFH.
  // TODO(https://crbug.com/1220337): Use ResumeCommitClosureSetObserver and
  // BeginNavigationInCommitCallbackInterceptor instead of doing this.
  TestNavigationManager new_history_navigation_manager(contents(), url_1);
  root->render_manager()->DiscardSpeculativeRenderFrameHostForShutdown();
  EXPECT_TRUE(pending_commit_nav_manager.WaitForNavigationFinished());
  EXPECT_FALSE(pending_commit_nav_manager.was_committed());

  // The BFCache restore should be cancelled and the navigation is reset because
  // the corresponding BFCache entry no longer exists.
  EXPECT_FALSE(bfcache_nav);
  bfcache_nav_manager.WaitForNavigationFinished();
  EXPECT_FALSE(bfcache_nav_manager.was_committed());

  // A new history navigation to `url_1` was triggered after the BFCache restore
  // failed, and it commits successfully.
  EXPECT_TRUE(new_history_navigation_manager.WaitForResponse());
  EXPECT_TRUE(new_history_navigation_manager.WaitForNavigationFinished());
  EXPECT_TRUE(new_history_navigation_manager.was_successful());
  EXPECT_EQ(url_1, contents()->GetLastCommittedURL());
}

// Tests that when a WebUI navigation couldn't create a speculative
// RenderFrameHost yet due to navigation queueing, a WebUI object is still
// created successfully and eventually gets moved to the final RenderFrameHost.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       WebUICreatedForQueuedNavigation) {
  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url_webui(std::string(kChromeUIScheme) + "://" +
                 std::string(kChromeUIGpuHost));

  // Load `url_1`.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  FrameTreeNode* root = contents()->GetPrimaryFrameTree().root();

  // Start loading `url_2`, but ignore its DidCommit IPC, so that it will stay
  // at the "pending commit" stage indefinitely. Then, start a navigation to
  // `url_webui`, which should create WebUI Objects.
  TestNavigationManager pending_commit_nav_manager(contents(), url_2);
  TestNavigationManager webui_nav_manager(contents(), url_webui);
  WebUIImpl* created_webui = nullptr;
  {
    DidCommitNavigationCanceller ignore_url_2_commit(
        contents(), url_2,
        base::BindLambdaForTesting([&]() { shell()->LoadURL(url_webui); }));
    shell()->LoadURL(url_2);
    EXPECT_TRUE(pending_commit_nav_manager.WaitForResponse());

    // Check that a speculative RenderFrameHost was created for the navigation
    // to `url_2`. Continue the `url_2` navigation, which will stay at the
    // "pending commit" stage.
    RenderFrameHostImplWrapper spec_rfh(
        root->render_manager()->speculative_frame_host());
    EXPECT_TRUE(spec_rfh.get());

    pending_commit_nav_manager.ResumeNavigation();

    // Ensure that the WebUI navigation starts correctly and created a WebUI
    // object even though it hasn't created a RenderFrameHost yet.
    EXPECT_TRUE(webui_nav_manager.WaitForRequestStart());
    NavigationRequest* webui_nav = static_cast<NavigationRequest*>(
        webui_nav_manager.GetNavigationHandle());
    EXPECT_FALSE(webui_nav->HasRenderFrameHost());
    EXPECT_TRUE(webui_nav->HasWebUI());
    created_webui = webui_nav->web_ui();
    EXPECT_FALSE(created_webui->HasRenderFrameHost());

    // The pending commit RenderFrameHost for `url_2` is still around and
    // doesn't have a WebUI Object.
    EXPECT_EQ(spec_rfh.get(), root->render_manager()->speculative_frame_host());
    EXPECT_TRUE(spec_rfh->HasPendingCommitForCrossDocumentNavigation());
    EXPECT_NE(webui_nav->dest_site_instance(), spec_rfh->GetSiteInstance());
    EXPECT_FALSE(spec_rfh->web_ui());
  }

  // Trigger the `url_2` pending commit RFH & NavigationRequest deletion, so
  // that the WebUI navigation can continue. Note that the pending commit RFH
  // deletion can't actually happen in real life, but this is the best we can
  // do since we've dropped the DidCommit IPC before. Ideally, we would just
  // re-trigger the DidCommit IPC that we dropped, so that the pending commit
  // RFH & NavigationRequest will finish the commit and trigger the resume
  // callback of the queued navigation, but there's currently no way to do that
  // in test (we have BeginNavigationInCommitCallbackInterceptor but that only
  // triggers renderer-initiated navigations, which can't navigate to WebUI).
  // Also, normally we discourage the deletion of pending commit RFHs as that
  // will result in a confused renderer, but in this case the renderer for
  // `url_2` is a brand new renderer that we will never reuse anyways, so it's
  // fine to discard the pending commit RFH.
  // TODO(https://crbug.com/1220337): Use ResumeCommitClosureSetObserver and
  // BeginNavigationInCommitCallbackInterceptor instead of doing this.
  root->render_manager()->DiscardSpeculativeRenderFrameHostForShutdown();
  EXPECT_TRUE(pending_commit_nav_manager.WaitForNavigationFinished());
  EXPECT_FALSE(pending_commit_nav_manager.was_committed());

  // Thew WebUI navigation continues successfully and the WebUI object
  // successfully moved to the newly created RenderFrameHost.
  webui_nav_manager.ResumeNavigation();
  EXPECT_TRUE(webui_nav_manager.WaitForResponse());
  NavigationRequest* webui_nav =
      static_cast<NavigationRequest*>(webui_nav_manager.GetNavigationHandle());
  EXPECT_TRUE(webui_nav->HasRenderFrameHost());
  EXPECT_FALSE(webui_nav->HasWebUI());
  EXPECT_EQ(created_webui, webui_nav->GetRenderFrameHost()->web_ui());
  EXPECT_TRUE(created_webui->HasRenderFrameHost());
  EXPECT_EQ(created_webui->GetRenderFrameHost(),
            webui_nav->GetRenderFrameHost());
  EXPECT_EQ(webui_nav->GetRenderFrameHost(),
            root->render_manager()->speculative_frame_host());

  // Commit the navigation.
  webui_nav_manager.ResumeNavigation();
  EXPECT_TRUE(webui_nav_manager.WaitForNavigationFinished());
  EXPECT_TRUE(webui_nav_manager.was_successful());
  EXPECT_EQ(url_webui, contents()->GetLastCommittedURL());
  EXPECT_EQ(created_webui, current_main_frame_host()->web_ui());
  EXPECT_EQ(created_webui->GetRenderFrameHost(), current_main_frame_host());
}

// Ensure that we use the browser-calculated origin_to_commit for data: URLs.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       DataURLHasCommittedOrigin) {
  const std::string data = "<html><title>One</title><body>foo</body></html>";
  const GURL data_url = GURL("data:text/html;charset=utf-8," + data);
  TestNavigationObserver navigation_observer(contents());
  DataURLOriginToCommitObserver observer(contents());
  shell()->LoadURL(data_url);
  navigation_observer.Wait();

  // Check that the browser-calculated origin_to_commit is used and matches the
  // committed origin.
  absl::optional<url::Origin> origin_to_commit = observer.origin_to_commit();
  EXPECT_TRUE(origin_to_commit.has_value());
  url::Origin committed_origin =
      shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  EXPECT_EQ(origin_to_commit.value(), committed_origin);
}

// This tests a regression found in https://crbug.com/1487803.
// This test loads a page with sandbox flags set in its header, and then opens
// a new page in a new tab. This ensures that the new page is also loaded with
// sandbox flags. After the new page commits, it loads a post-commit error page
// and checks that it loads without issue.
IN_PROC_BROWSER_TEST_P(NavigationControllerBrowserTest,
                       LoadPostCommitErrorPageSandboxedTopLevel) {
  GURL url(embedded_test_server()->GetURL(
      "/set-header?Content-Security-Policy: sandbox allow-top-navigation "
      "allow-scripts allow-popups"));
  GURL new_url(embedded_test_server()->GetURL("/title1.html"));

  // Navigate to the main page
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Open a new page in a new tab. The sandbox flags are copied over.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(current_main_frame_host(),
                     JsReplace(R"(window.open($1, '_blank');)", new_url)));
  Shell* new_shell = new_shell_observer.GetShell();
  ASSERT_NE(new_shell->web_contents(), shell()->web_contents());
  EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      new_shell->web_contents()->GetController());

  // Navigate the page in the new tab to a post-commit error page.
  std::string error_html = "Error page";
  TestNavigationObserver error_observer(new_shell->web_contents());
  controller.LoadPostCommitErrorPage(new_root->current_frame_host(), new_url,
                                     error_html);
  error_observer.Wait();

  EXPECT_FALSE(error_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, error_observer.last_net_error_code());
  EXPECT_EQ(PAGE_TYPE_ERROR, controller.GetLastCommittedEntry()->GetPageType());
  EXPECT_EQ(error_html, EvalJs(new_shell, "document.body.innerHTML"));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NavigationControllerAlertDialogBrowserTest,
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool()),
    NavigationControllerBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(
    All,
    NavigationControllerBrowserTest,
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool()),
    NavigationControllerBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(
    All,
    NavigationControllerBrowserTestNoServer,
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool()),
    NavigationControllerBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(
    All,
    NavigationControllerMainDocumentSequenceNumberBrowserTest,
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool()),
    NavigationControllerBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(
    All,
    RequestMonitoringNavigationBrowserTest,
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool()),
    NavigationControllerBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(
    All,
    SandboxedNavigationControllerBrowserTest,
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool()),
    NavigationControllerBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(
    All,
    SandboxedNavigationControllerWithBfcacheBrowserTest,
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Values(true)),
    NavigationControllerBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(
    All,
    SandboxedNavigationControllerPopupBrowserTest,
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool()),
    NavigationControllerBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(
    All,
    InitialEmptyDocNavigationControllerBrowserTest,
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool()),
    InitialEmptyDocNavigationControllerBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(
    All,
    LoadDataWithBaseURLWithPossiblyEmptyURLsBrowserTest,
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool(),
                     testing::Bool(),
                     testing::Bool()),
    LoadDataWithBaseURLWithPossiblyEmptyURLsBrowserTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(
    All,
    LoadDataWithBaseURLBrowserTest,
    testing::Combine(testing::ValuesIn(RenderDocumentFeatureLevelValues()),
                     testing::Bool()),
    LoadDataWithBaseURLBrowserTest::DescribeParams);
}  // namespace content
