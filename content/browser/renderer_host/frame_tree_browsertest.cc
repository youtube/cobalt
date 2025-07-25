// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "url/url_constants.h"

namespace content {

namespace {

EvalJsResult GetOriginFromRenderer(FrameTreeNode* node) {
  return EvalJs(node, "self.origin");
}

// Expect that frame_name, id and src match the node's values.
void ExpectAttributesEq(FrameTreeNode* node,
                        const std::string& frame_name,
                        const absl::optional<std::string> id,
                        const absl::optional<std::string> src) {
  EXPECT_EQ(frame_name, node->frame_name());
  EXPECT_EQ(id, node->html_id());
  EXPECT_EQ(src, node->html_src());
}

}  // namespace

class FrameTreeBrowserTest : public ContentBrowserTest {
 public:
  FrameTreeBrowserTest() = default;

  FrameTreeBrowserTest(const FrameTreeBrowserTest&) = delete;
  FrameTreeBrowserTest& operator=(const FrameTreeBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Ensures FrameTree correctly reflects page structure during navigations.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, FrameTreeShape) {
  GURL base_url = embedded_test_server()->GetURL("A.com", "/site_isolation/");

  // Load doc without iframes. Verify FrameTree just has root.
  // Frame tree:
  //   Site-A Root
  EXPECT_TRUE(NavigateToURL(shell(), base_url.Resolve("blank.html")));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(0U, root->child_count());

  // Add 2 same-site frames. Verify 3 nodes in tree with proper names.
  // Frame tree:
  //   Site-A Root -- Site-A frame1
  //              \-- Site-A frame2
  LoadStopObserver observer1(shell()->web_contents());
  EXPECT_TRUE(NavigateToURL(shell(), base_url.Resolve("frames-X-X.html")));
  observer1.Wait();
  ASSERT_EQ(2U, root->child_count());
  EXPECT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(0U, root->child_at(1)->child_count());
}

// TODO(ajwong): Talk with nasko and merge this functionality with
// FrameTreeShape.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, FrameTreeShape2) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/frame_tree/top.html")));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetPrimaryFrameTree().root();

  // Check that the root node is properly created.
  ASSERT_EQ(3UL, root->child_count());
  ExpectAttributesEq(root, std::string(), absl::nullopt, absl::nullopt);

  ASSERT_EQ(2UL, root->child_at(0)->child_count());
  ExpectAttributesEq(root->child_at(0), "1-1-name", "1-1-id", "1-1.html");

  // Verify the deepest node exists and has the right name.
  ASSERT_EQ(2UL, root->child_at(2)->child_count());
  EXPECT_EQ(1UL, root->child_at(2)->child_at(1)->child_count());
  EXPECT_EQ(0UL, root->child_at(2)->child_at(1)->child_at(0)->child_count());
  ExpectAttributesEq(root->child_at(2)->child_at(1)->child_at(0), "3-1-name",
                     "3-1-id", "3-1.html");

  // Navigate to about:blank, which should leave only the root node of the frame
  // tree in the browser process.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  root = wc->GetPrimaryFrameTree().root();
  EXPECT_EQ(0UL, root->child_count());
  ExpectAttributesEq(root, std::string(), absl::nullopt, absl::nullopt);
}

// Frame attributes of iframe elements are correctly tracked in FrameTree.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, FrameTreeAttributesUpdate) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/frame_tree/top.html")));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetPrimaryFrameTree().root();

  // Check that the root node is properly created.
  ASSERT_EQ(3UL, root->child_count());
  ExpectAttributesEq(root, std::string(), absl::nullopt, absl::nullopt);

  ASSERT_EQ(2UL, root->child_at(0)->child_count());
  ExpectAttributesEq(root->child_at(0), "1-1-name", "1-1-id", "1-1.html");

  // Change id, name and src of the iframe.
  EXPECT_TRUE(ExecJs(root->current_frame_host(), R"(
    let iframe = document.getElementById('1-1-id');
    iframe.id = '1-1-updated-id';
    iframe.name = '1-1-updated-name';
    iframe.src = '1-1-updated.html';
  )"));
  // |html_name()| gets updated whenever the name attribute gets updated.
  EXPECT_EQ("1-1-updated-name", root->child_at(0)->html_name());
  ExpectAttributesEq(root->child_at(0), "1-1-name", "1-1-updated-id",
                     "1-1-updated.html");
}

// Ensures that frames' name attributes and their updates are tracked in
// |html_name()| and window.name and its updates are tracked in |frame_name()|.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, FrameNameVSWindowName) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/frame_tree/top.html")));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetPrimaryFrameTree().root();

  // Check that the root node is properly created.
  ASSERT_EQ(3UL, root->child_count());
  EXPECT_EQ(absl::nullopt, root->html_name());
  EXPECT_EQ(std::string(), root->frame_name());

  ASSERT_EQ(2UL, root->child_at(0)->child_count());
  EXPECT_EQ("1-1-name", root->child_at(0)->html_name());
  EXPECT_EQ("1-1-name", root->child_at(0)->frame_name());

  // Change the name attribute of the iframe.
  EXPECT_TRUE(ExecJs(root->current_frame_host(), R"(
    let iframe = document.getElementById('1-1-id');
    iframe.name = '1-1-updated-name';
  )"));
  // |html_name()| gets updated whenever the name attribute gets updated.
  EXPECT_EQ("1-1-updated-name", root->child_at(0)->html_name());
  // |frame_name()| stays the same.
  EXPECT_EQ("1-1-name", root->child_at(0)->frame_name());

  // Change the window.name of the iframe.
  EXPECT_TRUE(ExecJs(root->current_frame_host(), R"(
    let iframe = document.getElementById('1-1-id');
    iframe.contentWindow.name = '1-1-updated-name-2';
  )"));
  // |html_name()| stays the same.
  EXPECT_EQ("1-1-updated-name", root->child_at(0)->html_name());
  // |frame_name()| gets updated.
  EXPECT_EQ("1-1-updated-name-2", root->child_at(0)->frame_name());
}

// Ensures that long attributes are cut down to the max length.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, LongAttributesCutDown) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/frame_tree/top.html")));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetPrimaryFrameTree().root();

  // Check that the root node is properly created.
  ASSERT_EQ(3UL, root->child_count());
  ASSERT_EQ(2UL, root->child_at(0)->child_count());
  EXPECT_EQ("1-1-name", root->child_at(0)->html_name());

  // Change the name attribute of the iframe.
  EXPECT_TRUE(ExecJs(root->current_frame_host(), R"(
    let iframe = document.getElementById('1-1-id');
    iframe.id += 'a'.repeat(1200);
    iframe.name += 'b'.repeat(1200);
    iframe.src += 'c'.repeat(1200) + '.html';
  )"));
  // Long attribute is cut down to the maximum length.
  EXPECT_EQ(1024UL, root->child_at(0)->html_id()->size());
  EXPECT_EQ(1024UL, root->child_at(0)->html_name()->size());
  EXPECT_EQ(1024UL, root->child_at(0)->html_src()->size());
}

// Insert a frame into the frame tree and ensure that the inserted frame's
// attributes are correctly captured.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, InsertFrameInTree) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/frame_tree/top.html")));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetPrimaryFrameTree().root();

  // Check that the root node is properly created.
  ASSERT_EQ(3UL, root->child_count());
  ExpectAttributesEq(root, std::string(), absl::nullopt, absl::nullopt);

  ASSERT_EQ(2UL, root->child_at(0)->child_count());
  ExpectAttributesEq(root->child_at(0), "1-1-name", "1-1-id", "1-1.html");

  // Insert a child iframe.
  EXPECT_TRUE(ExecJs(root->current_frame_host(), R"(
    let new_iframe = document.createElement('iframe');
    new_iframe.id = '1-1-child-id';
    new_iframe.src = '1-1-child.html';
    new_iframe.name = '1-1-child-name';

    document.body.appendChild(new_iframe);
  )"));
  // Check that the new iframe is inserted and their attributes are correct.
  ASSERT_EQ(4UL, root->child_count());
  ExpectAttributesEq(root->child_at(3), "1-1-child-name", "1-1-child-id",
                     "1-1-child.html");
}

// Test that we can navigate away if the previous renderer doesn't clean up its
// child frames.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, FrameTreeAfterCrash) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/frame_tree/top.html")));

  // Ensure the view and frame are live.
  RenderFrameHostImpl* rfh1 = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  RenderViewHostImpl* rvh = rfh1->render_view_host();
  EXPECT_TRUE(rvh->IsRenderViewLive());
  EXPECT_TRUE(rfh1->IsRenderFrameLive());

  // Crash the renderer so that it doesn't send any FrameDetached messages.
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  ASSERT_TRUE(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->Shutdown(
          0));
  crash_observer.Wait();

  // The frame tree should be cleared.
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetPrimaryFrameTree().root();
  EXPECT_EQ(0UL, root->child_count());

  // Ensure the view and frame aren't live anymore.
  EXPECT_FALSE(rvh->IsRenderViewLive());
  EXPECT_FALSE(rfh1->IsRenderFrameLive());

  // Navigate to a new URL.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(0UL, root->child_count());
  EXPECT_EQ(url, root->current_url());

  RenderFrameHostImpl* rfh2 = root->current_frame_host();
  // Ensure the view and frame are live again.
  EXPECT_TRUE(rvh->IsRenderViewLive());
  EXPECT_TRUE(rfh2->IsRenderFrameLive());
}

// Test that we can navigate away if the previous renderer doesn't clean up its
// child frames.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, NavigateWithLeftoverFrames) {
  GURL base_url = embedded_test_server()->GetURL("A.com", "/site_isolation/");

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/frame_tree/top.html")));

  // Hang the renderer so that it doesn't send any FrameDetached messages.
  // (This navigation will never complete, so don't wait for it.)
  shell()->LoadURL(GURL(blink::kChromeUIHangURL));

  // Check that the frame tree still has children.
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetPrimaryFrameTree().root();
  ASSERT_EQ(3UL, root->child_count());

  // Navigate to a new URL.  We use LoadURL because NavigateToURL will try to
  // wait for the previous navigation to stop.
  TestNavigationObserver tab_observer(wc, 1);
  shell()->LoadURL(base_url.Resolve("blank.html"));
  tab_observer.Wait();

  // The frame tree should now be cleared.
  EXPECT_EQ(0UL, root->child_count());
}

// Ensure that IsRenderFrameLive is true for main frames and same-site iframes.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, IsRenderFrameLive) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // The root and subframe should each have a live RenderFrame.
  EXPECT_TRUE(
      root->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());

  // Load a same-site page into iframe and it should still be live.
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), http_url));
  EXPECT_TRUE(
      root->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());
}

// Ensure that origins are correctly set on navigations.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, OriginSetOnNavigation) {
  GURL about_blank(url::kAboutBlankURL);
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContents* contents = shell()->web_contents();

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetPrimaryFrameTree().root();

  // Extra '/' is added because the replicated origin is serialized in RFC 6454
  // format, which dictates no trailing '/', whereas GURL::GetOrigin does put a
  // '/' at the end.
  EXPECT_EQ(main_url.DeprecatedGetOriginAsURL().spec(),
            root->current_origin().Serialize() + '/');
  EXPECT_EQ(
      main_url.DeprecatedGetOriginAsURL().spec(),
      root->current_frame_host()->GetLastCommittedOrigin().Serialize() + '/');

  // The iframe is inititially same-origin.
  EXPECT_TRUE(
      root->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
          root->child_at(0)->current_frame_host()->GetLastCommittedOrigin()));
  EXPECT_EQ(root->current_origin().Serialize(), GetOriginFromRenderer(root));
  EXPECT_EQ(root->child_at(0)->current_origin().Serialize(),
            GetOriginFromRenderer(root->child_at(0)));

  // Navigate the iframe cross-origin.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
  EXPECT_EQ(frame_url, root->child_at(0)->current_url());
  EXPECT_EQ(frame_url.DeprecatedGetOriginAsURL().spec(),
            root->child_at(0)->current_origin().Serialize() + '/');
  EXPECT_FALSE(
      root->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
          root->child_at(0)->current_frame_host()->GetLastCommittedOrigin()));
  EXPECT_EQ(root->current_origin().Serialize(), GetOriginFromRenderer(root));
  EXPECT_EQ(root->child_at(0)->current_origin().Serialize(),
            GetOriginFromRenderer(root->child_at(0)));

  // Parent-initiated about:blank navigation should inherit the parent's a.com
  // origin.
  NavigateIframeToURL(contents, "1-1-id", about_blank);
  EXPECT_EQ(about_blank, root->child_at(0)->current_url());
  EXPECT_EQ(main_url.DeprecatedGetOriginAsURL().spec(),
            root->child_at(0)->current_origin().Serialize() + '/');
  EXPECT_EQ(root->current_frame_host()->GetLastCommittedOrigin().Serialize(),
            root->child_at(0)
                ->current_frame_host()
                ->GetLastCommittedOrigin()
                .Serialize());
  EXPECT_TRUE(
      root->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
          root->child_at(0)->current_frame_host()->GetLastCommittedOrigin()));
  EXPECT_EQ(root->current_origin().Serialize(), GetOriginFromRenderer(root));
  EXPECT_EQ(root->child_at(0)->current_origin().Serialize(),
            GetOriginFromRenderer(root->child_at(0)));

  GURL data_url("data:text/html,foo");
  EXPECT_TRUE(NavigateToURL(shell(), data_url));

  // Navigating to a data URL should set a unique origin.  This is represented
  // as "null" per RFC 6454.
  EXPECT_EQ("null", root->current_origin().Serialize());
  EXPECT_TRUE(
      contents->GetPrimaryMainFrame()->GetLastCommittedOrigin().opaque());
  EXPECT_EQ("null", GetOriginFromRenderer(root));

  // Re-navigating to a normal URL should update the origin.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(main_url.DeprecatedGetOriginAsURL().spec(),
            root->current_origin().Serialize() + '/');
  EXPECT_EQ(
      main_url.DeprecatedGetOriginAsURL().spec(),
      contents->GetPrimaryMainFrame()->GetLastCommittedOrigin().Serialize() +
          '/');
  EXPECT_FALSE(
      contents->GetPrimaryMainFrame()->GetLastCommittedOrigin().opaque());
  EXPECT_EQ(root->current_origin().Serialize(), GetOriginFromRenderer(root));
}

// Tests a cross-origin navigation to a blob URL. The main frame initiates this
// navigation on its grandchild. It should wind up in the main frame's process.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, NavigateGrandchildToBlob) {
  WebContents* contents = shell()->web_contents();
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetPrimaryFrameTree().root();

  // First, snapshot the FrameTree for a normal A(B(A)) case where all frames
  // are served over http. The blob test should result in the same structure.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b(a))")));
  std::string reference_tree = DepictFrameTree(*root);

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // The root node will initiate the navigation; its grandchild node will be the
  // target of the navigation.
  FrameTreeNode* target = root->child_at(0)->child_at(0);

  RenderFrameDeletedObserver deleted_observer(target->current_frame_host());
  std::string html =
      "<html><body><div>This is blob content.</div>"
      "<script>"
      "window.parent.parent.postMessage('HI', self.origin);"
      "</script></body></html>";
  std::string script = JsReplace(
      "new Promise((resolve) => {"
      "  window.addEventListener('message', resolve, false);"
      "  var blob = new Blob([$1], {type: 'text/html'});"
      "  var blob_url = URL.createObjectURL(blob);"
      "  frames[0][0].location.href = blob_url;"
      "}).then((event) => {"
      "  document.body.appendChild(document.createTextNode(event.data));"
      "  return event.source.location.href;"
      "});",
      html);
  std::string blob_url_string = EvalJs(root, script).ExtractString();
  // Wait for the RenderFrame to go away, if this will be cross-process.
  if (AreAllSitesIsolatedForTesting())
    deleted_observer.WaitUntilDeleted();
  EXPECT_EQ(GURL(blob_url_string), target->current_url());
  EXPECT_EQ(url::kBlobScheme, target->current_url().scheme());
  EXPECT_FALSE(target->current_origin().opaque());
  EXPECT_EQ("a.com", target->current_origin().host());
  EXPECT_EQ(url::kHttpScheme, target->current_origin().scheme());
  EXPECT_EQ("This is blob content.",
            EvalJs(target, "document.body.children[0].innerHTML"));
  EXPECT_EQ(reference_tree, DepictFrameTree(*root));
}

IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, NavigateChildToAboutBlank) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // The leaf node (c.com) will be navigated. Its parent node (b.com) will
  // initiate the navigation.
  FrameTreeNode* target =
      contents->GetPrimaryFrameTree().root()->child_at(0)->child_at(0);
  RenderFrameHost* initiator_rfh = target->parent();

  // Give the target a name.
  EXPECT_TRUE(ExecJs(target, "window.name = 'target';"));

  // Use window.open(about:blank), then poll the document for access.
  EvalJsResult about_blank_origin = EvalJs(
      initiator_rfh,
      "new Promise(resolve => {"
      "  var didNavigate = false;"
      "  var intervalID = setInterval(function() {"
      "    if (!didNavigate) {"
      "      didNavigate = true;"
      "      window.open('about:blank', 'target');"
      "    }"
      "    // Poll the document until it doesn't throw a SecurityError.\n"
      "    try {"
      "      frames[0].document.write('Hi from ' + document.domain);"
      "    } catch (e) { return; }"
      "    clearInterval(intervalID);"
      "    resolve(frames[0].self.origin);"
      "  }, 16);"
      "});");
  EXPECT_EQ(target->current_origin(), about_blank_origin);
  EXPECT_EQ(GURL(url::kAboutBlankURL), target->current_url());
  EXPECT_EQ(url::kAboutScheme, target->current_url().scheme());
  EXPECT_FALSE(target->current_origin().opaque());
  EXPECT_EQ("b.com", target->current_origin().host());
  EXPECT_EQ(url::kHttpScheme, target->current_origin().scheme());

  EXPECT_EQ("Hi from b.com", EvalJs(target, "document.body.innerHTML"));
}

// Nested iframes, three origins: A(B(C)). Frame A navigates C to about:blank
// (via window.open). This should wind up in A's origin per the spec. Test fails
// because of http://crbug.com/564292
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest,
                       DISABLED_NavigateGrandchildToAboutBlank) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // The leaf node (c.com) will be navigated. Its grandparent node (a.com) will
  // initiate the navigation.
  FrameTreeNode* target =
      contents->GetPrimaryFrameTree().root()->child_at(0)->child_at(0);
  RenderFrameHost* initiator_rfh = target->parent()->GetParent();

  // Give the target a name.
  EXPECT_TRUE(ExecJs(target, "window.name = 'target';"));

  // Use window.open(about:blank), then poll the document for access.
  EvalJsResult about_blank_origin =
      EvalJs(initiator_rfh,
             "new Promise((resolve) => {"
             "  var didNavigate = false;"
             "  var intervalID = setInterval(() => {"
             "    if (!didNavigate) {"
             "      didNavigate = true;"
             "      window.open('about:blank', 'target');"
             "    }"
             "    // May raise a SecurityError, that's expected.\n"
             "    try {"
             "      frames[0][0].document.write('Hi from ' + document.domain);"
             "    } catch (e) { return; }"
             "    clearInterval(intervalID);"
             "    resolve(frames[0][0].self.origin);"
             "  }, 16);"
             "});");
  EXPECT_EQ(target->current_origin(), about_blank_origin);
  EXPECT_EQ(GURL(url::kAboutBlankURL), target->current_url());
  EXPECT_EQ(url::kAboutScheme, target->current_url().scheme());
  EXPECT_FALSE(target->current_origin().opaque());
  EXPECT_EQ("a.com", target->current_origin().host());
  EXPECT_EQ(url::kHttpScheme, target->current_origin().scheme());

  EXPECT_EQ("Hi from a.com", EvalJs(target, "document.body.innerHTML"));
}

// Tests a cross-origin navigation to a data: URL. The main frame initiates this
// navigation on its grandchild. It should wind up in the main frame's process
// and have precursor origin of the main frame origin.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, NavigateGrandchildToDataUrl) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // The leaf node (c.com) will be navigated. Its grandparent node (a.com) will
  // initiate the navigation.
  FrameTreeNode* target =
      contents->GetPrimaryFrameTree().root()->child_at(0)->child_at(0);
  RenderFrameHostImpl* initiator_rfh = target->parent()->GetParent();

  // Give the target a name.
  EXPECT_TRUE(ExecJs(target, "window.name = 'target';"));

  // Navigate the target frame through the initiator frame.
  {
    TestFrameNavigationObserver observer(target);
    EXPECT_TRUE(ExecJs(initiator_rfh,
                       "window.open('data:text/html,content', 'target');"));
    observer.Wait();
  }

  url::Origin original_target_origin =
      target->current_frame_host()->GetLastCommittedOrigin();
  EXPECT_TRUE(original_target_origin.opaque());
  EXPECT_EQ(original_target_origin.GetTupleOrPrecursorTupleIfOpaque(),
            url::SchemeHostPort(main_url));

  // Navigate the grandchild frame again cross-process to foo.com, then
  // go back in session history. The frame should commit a new opaque origin,
  // but it will still have the same precursor origin (the main frame origin).
  {
    TestFrameNavigationObserver observer(target);
    EXPECT_TRUE(ExecJs(target, JsReplace("window.location = $1",
                                         embedded_test_server()->GetURL(
                                             "foo.com", "/title2.html"))));
    observer.Wait();
  }
  EXPECT_NE(original_target_origin,
            target->current_frame_host()->GetLastCommittedOrigin());
  {
    TestFrameNavigationObserver observer(target);
    contents->GetController().GoBack();
    observer.Wait();
  }

  url::Origin target_origin =
      target->current_frame_host()->GetLastCommittedOrigin();
  EXPECT_NE(target_origin, original_target_origin);
  EXPECT_TRUE(target_origin.opaque());
  EXPECT_EQ(target_origin.GetTupleOrPrecursorTupleIfOpaque(),
            original_target_origin.GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_EQ(target_origin.GetTupleOrPrecursorTupleIfOpaque(),
            url::SchemeHostPort(main_url));
}

// Ensures that iframe with srcdoc is always put in the same origin as its
// parent frame.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, ChildFrameWithSrcdoc) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();
  EXPECT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);
  std::string frame_origin = EvalJs(child, "self.origin;").ExtractString();
  EXPECT_TRUE(
      child->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
          url::Origin::Create(GURL(frame_origin))));
  EXPECT_FALSE(
      root->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
          url::Origin::Create(GURL(frame_origin))));

  // Create a new iframe with srcdoc and add it to the main frame. It should
  // be created in the same SiteInstance as the parent.
  {
    std::string script(
        "var f = document.createElement('iframe');"
        "f.srcdoc = 'some content';"
        "document.body.appendChild(f)");
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, script));
    EXPECT_EQ(2U, root->child_count());
    observer.Wait();

    EXPECT_TRUE(root->child_at(1)->current_url().IsAboutSrcdoc());
    EvalJsResult js_result = EvalJs(root->child_at(1), "self.origin");
    EXPECT_EQ(root->current_frame_host()
                  ->GetLastCommittedURL()
                  .DeprecatedGetOriginAsURL(),
              GURL(js_result.ExtractString()));
    EXPECT_NE(child->current_frame_host()
                  ->GetLastCommittedURL()
                  .DeprecatedGetOriginAsURL(),
              GURL(js_result.ExtractString()));
  }

  // Set srcdoc on the existing cross-site frame. It should navigate the frame
  // back to the origin of the parent.
  {
    std::string script(
        "var f = document.getElementById('child-0');"
        "f.srcdoc = 'some content';");
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, script));
    observer.Wait();

    EXPECT_TRUE(child->current_url().IsAboutSrcdoc());
    EXPECT_EQ(root->current_frame_host()->GetLastCommittedOrigin().Serialize(),
              EvalJs(child, "self.origin"));
  }
}

// Ensure that sandbox flags are correctly set in the main frame when set by
// Content-Security-Policy header.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, SandboxFlagsSetForMainFrame) {
  GURL main_url(embedded_test_server()->GetURL("/csp_sandboxed_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Verify that sandbox flags are set properly for the root FrameTreeNode and
  // RenderFrameHost. Root frame is sandboxed with "allow-scripts".
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
            root->active_sandbox_flags());
  EXPECT_EQ(root->active_sandbox_flags(),
            root->current_frame_host()->active_sandbox_flags());

  // Verify that child frames inherit sandbox flags from the root. First frame
  // has no explicitly set flags of its own, and should inherit those from the
  // root. Second frame is completely sandboxed.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
            root->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
            root->child_at(0)->active_sandbox_flags());
  EXPECT_EQ(root->child_at(0)->active_sandbox_flags(),
            root->child_at(0)->current_frame_host()->active_sandbox_flags());
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            root->child_at(1)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            root->child_at(1)->active_sandbox_flags());
  EXPECT_EQ(root->child_at(1)->active_sandbox_flags(),
            root->child_at(1)->current_frame_host()->active_sandbox_flags());

  // Navigating the main frame to a different URL should clear sandbox flags.
  GURL unsandboxed_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root, unsandboxed_url));

  // Verify that sandbox flags are cleared properly for the root FrameTreeNode
  // and RenderFrameHost.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->active_sandbox_flags());
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->current_frame_host()->active_sandbox_flags());
}

// Ensure that sandbox flags are correctly set when child frames are created.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, SandboxFlagsSetForChildFrames) {
  GURL main_url(embedded_test_server()->GetURL("/sandboxed_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Verify that sandbox flags are set properly for all FrameTreeNodes.
  // First frame is completely sandboxed; second frame uses "allow-scripts",
  // which resets both SandboxFlags::Scripts and
  // SandboxFlags::AutomaticFeatures bits per blink::parseSandboxPolicy(), and
  // third frame has "allow-scripts allow-same-origin".
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            root->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
            root->child_at(1)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
                ~network::mojom::WebSandboxFlags::kOrigin,
            root->child_at(2)->effective_frame_policy().sandbox_flags);

  // Sandboxed frames should set a unique origin unless they have the
  // "allow-same-origin" directive.
  EXPECT_EQ("null", root->child_at(0)->current_origin().Serialize());
  EXPECT_EQ("null", root->child_at(1)->current_origin().Serialize());
  EXPECT_EQ(main_url.DeprecatedGetOriginAsURL().spec(),
            root->child_at(2)->current_origin().Serialize() + "/");

  // Navigating to a different URL should not clear sandbox flags.
  GURL frame_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll,
            root->child_at(0)->effective_frame_policy().sandbox_flags);
}

// Ensure that sandbox flags are correctly set in the child frames when set by
// Content-Security-Policy header, and in combination with the sandbox iframe
// attribute.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest,
                       SandboxFlagsSetByCSPForChildFrames) {
  GURL main_url(embedded_test_server()->GetURL("/sandboxed_frames_csp.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Verify that sandbox flags are set properly for all FrameTreeNodes.
  // First frame has no iframe sandbox flags, but the framed document is served
  // with a CSP header which sets "allow-scripts", "allow-popups" and
  // "allow-pointer-lock".
  // Second frame is sandboxed with "allow-scripts", "allow-pointer-lock" and
  // "allow-orientation-lock", and the framed document is also served with a CSP
  // header which uses "allow-popups" and "allow-pointer-lock". The resulting
  // sandbox for the frame should only have "allow-pointer-lock".
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->active_sandbox_flags());
  EXPECT_EQ(root->active_sandbox_flags(),
            root->current_frame_host()->active_sandbox_flags());
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
          ~network::mojom::WebSandboxFlags::kPointerLock &
          ~network::mojom::WebSandboxFlags::kPopups &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols,
      root->child_at(0)->active_sandbox_flags());
  EXPECT_EQ(root->child_at(0)->active_sandbox_flags(),
            root->child_at(0)->current_frame_host()->active_sandbox_flags());
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
                ~network::mojom::WebSandboxFlags::kPointerLock &
                ~network::mojom::WebSandboxFlags::kOrientationLock,
            root->child_at(1)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
                ~network::mojom::WebSandboxFlags::kPointerLock,
            root->child_at(1)->active_sandbox_flags());
  EXPECT_EQ(root->child_at(1)->active_sandbox_flags(),
            root->child_at(1)->current_frame_host()->active_sandbox_flags());

  // Navigating to a different URL *should* clear CSP-set sandbox flags, but
  // should retain those flags set by the frame owner.
  GURL frame_url(embedded_test_server()->GetURL("/title1.html"));

  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->child_at(0)->active_sandbox_flags());
  EXPECT_EQ(root->child_at(0)->active_sandbox_flags(),
            root->child_at(0)->current_frame_host()->active_sandbox_flags());

  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), frame_url));
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
                ~network::mojom::WebSandboxFlags::kPointerLock &
                ~network::mojom::WebSandboxFlags::kOrientationLock,
            root->child_at(1)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
                ~network::mojom::WebSandboxFlags::kPointerLock &
                ~network::mojom::WebSandboxFlags::kOrientationLock,
            root->child_at(1)->active_sandbox_flags());
  EXPECT_EQ(root->child_at(1)->active_sandbox_flags(),
            root->child_at(1)->current_frame_host()->active_sandbox_flags());
}

// Ensure that a popup opened from a subframe sets its opener to the subframe's
// FrameTreeNode, and that the opener is cleared if the subframe is destroyed.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, SubframeOpenerSetForNewWindow) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Open a new window from a subframe.
  ShellAddedObserver new_shell_observer;
  GURL popup_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(
      ExecJs(root->child_at(0), JsReplace("window.open($1);", popup_url)));
  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* new_contents = new_shell->web_contents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));

  // Check that the new window's opener points to the correct subframe on
  // original window.
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_contents)->GetPrimaryFrameTree().root();
  EXPECT_EQ(root->child_at(0), popup_root->opener());

  // Close the original window.  This should clear the new window's opener.
  shell()->Close();
  EXPECT_EQ(nullptr, popup_root->opener());
}

// Tests that the user activation bits get cleared when a same-site document is
// installed in the frame.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest,
                       ClearUserActivationForNewDocument) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Set the user activation bits.
  root->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(root->HasStickyUserActivation());
  EXPECT_TRUE(root->HasTransientUserActivation());

  // Install a new same-site document to check the clearing of user activation
  // bits.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());
}

class CrossProcessFrameTreeBrowserTest : public ContentBrowserTest {
 public:
  CrossProcessFrameTreeBrowserTest() = default;

  CrossProcessFrameTreeBrowserTest(const CrossProcessFrameTreeBrowserTest&) =
      delete;
  CrossProcessFrameTreeBrowserTest& operator=(
      const CrossProcessFrameTreeBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Ensure that we can complete a cross-process subframe navigation.
IN_PROC_BROWSER_TEST_F(CrossProcessFrameTreeBrowserTest,
                       CreateCrossProcessSubframeProxies) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // There should not be a proxy for the root's own SiteInstance.
  SiteInstanceImpl* root_instance =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(root->current_frame_host()
                   ->browsing_context_state()
                   ->GetRenderFrameProxyHost(root_instance->group()));

  // Load same-site page into iframe.
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), http_url));

  // Load cross-site page into iframe.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), cross_site_url));

  // Ensure that we have created a new process for the subframe.
  ASSERT_EQ(2U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  SiteInstanceImpl* child_instance =
      child->current_frame_host()->GetSiteInstance();
  RenderViewHost* rvh = child->current_frame_host()->render_view_host();
  RenderProcessHost* rph = child->current_frame_host()->GetProcess();

  EXPECT_NE(shell()->web_contents()->GetPrimaryMainFrame()->GetRenderViewHost(),
            rvh);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), child_instance);
  EXPECT_NE(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(), rph);

  // Ensure that the root node has a proxy for the child node's SiteInstance.
  EXPECT_TRUE(root->current_frame_host()
                  ->browsing_context_state()
                  ->GetRenderFrameProxyHost(child_instance->group()));

  // Also ensure that the child has a proxy for the root node's SiteInstance.
  EXPECT_TRUE(child->current_frame_host()
                  ->browsing_context_state()
                  ->GetRenderFrameProxyHost(root_instance->group()));

  // The nodes should not have proxies for their own SiteInstance.
  EXPECT_FALSE(root->current_frame_host()
                   ->browsing_context_state()
                   ->GetRenderFrameProxyHost(root_instance->group()));
  EXPECT_FALSE(child->current_frame_host()
                   ->browsing_context_state()
                   ->GetRenderFrameProxyHost(child_instance->group()));

  // Ensure that the RenderViews and RenderFrames are all live.
  EXPECT_TRUE(
      root->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(
      child->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());
}

IN_PROC_BROWSER_TEST_F(CrossProcessFrameTreeBrowserTest,
                       OriginSetOnNavigations) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(root->current_origin().Serialize() + '/',
            main_url.DeprecatedGetOriginAsURL().spec());

  // First frame is an about:blank frame.  Check that its origin is correctly
  // inherited from the parent.
  EXPECT_EQ(root->child_at(0)->current_origin().Serialize() + '/',
            main_url.DeprecatedGetOriginAsURL().spec());

  // Second frame loads a same-site page.  Its origin should also be the same
  // as the parent.
  EXPECT_EQ(root->child_at(1)->current_origin().Serialize() + '/',
            main_url.DeprecatedGetOriginAsURL().spec());

  // Load cross-site page into the first frame.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), cross_site_url));

  EXPECT_EQ(root->child_at(0)->current_origin().Serialize() + '/',
            cross_site_url.DeprecatedGetOriginAsURL().spec());

  // The root's origin shouldn't have changed.
  EXPECT_EQ(root->current_origin().Serialize() + '/',
            main_url.DeprecatedGetOriginAsURL().spec());

  {
    GURL data_url("data:text/html,foo");
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(
        ExecJs(root->child_at(1), JsReplace("window.location = $1", data_url)));
    observer.Wait();
  }

  // Navigating to a data URL should set a unique origin.  This is represented
  // as "null" per RFC 6454.  A frame navigating itself to a data: URL does not
  // require a process transfer, but should retain the original origin
  // as its precursor.
  EXPECT_EQ(root->child_at(1)->current_origin().Serialize(), "null");
  EXPECT_TRUE(root->child_at(1)->current_origin().opaque());
  ASSERT_EQ(
      url::SchemeHostPort(main_url),
      root->child_at(1)->current_origin().GetTupleOrPrecursorTupleIfOpaque())
      << "Expected the precursor origin to be preserved; should be the "
         "initiator of a data: navigation.";

  // Adding an <iframe sandbox srcdoc=> frame should result in a unique origin
  // that is different-origin from its data: URL parent.
  {
    TestNavigationObserver observer(shell()->web_contents());

    ASSERT_EQ(0U, root->child_at(1)->child_count());
    EXPECT_TRUE(
        ExecJs(root->child_at(1), JsReplace(
                                      R"(
                var iframe = document.createElement('iframe');
                iframe.setAttribute('sandbox', 'allow-scripts');
                iframe.srcdoc = $1;
                document.body.appendChild(iframe);
            )",
                                      "<html><body>This sandboxed doc should "
                                      "be different-origin.</body></html>")));
    observer.Wait();
    ASSERT_EQ(1U, root->child_at(1)->child_count());
  }

  url::Origin root_origin = root->current_origin();
  url::Origin child_1 = root->child_at(1)->current_origin();
  url::Origin child_1_0 = root->child_at(1)->child_at(0)->current_origin();
  EXPECT_FALSE(root_origin.opaque());
  EXPECT_TRUE(child_1.opaque());
  EXPECT_TRUE(child_1_0.opaque());
  EXPECT_NE(child_1, child_1_0);
  EXPECT_EQ(url::SchemeHostPort(main_url),
            root_origin.GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_EQ(url::SchemeHostPort(main_url),
            child_1.GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_EQ(url::SchemeHostPort(main_url),
            child_1_0.GetTupleOrPrecursorTupleIfOpaque());

  {
    TestNavigationObserver observer(shell()->web_contents());

    ASSERT_EQ(1U, root->child_at(1)->child_count());
    EXPECT_TRUE(
        ExecJs(root->child_at(1), JsReplace(
                                      R"(
                var iframe = document.createElement('iframe');
                iframe.srcdoc = $1;
                document.body.appendChild(iframe);
            )",
                                      "<html><body>This srcdoc document should "
                                      "be same-origin.</body></html>")));
    observer.Wait();
    ASSERT_EQ(2U, root->child_at(1)->child_count());
  }
  EXPECT_EQ(root_origin, root->current_origin());
  EXPECT_EQ(child_1, root->child_at(1)->current_origin());
  EXPECT_EQ(child_1_0, root->child_at(1)->child_at(0)->current_origin());
  url::Origin child_1_1 = root->child_at(1)->child_at(1)->current_origin();
  EXPECT_EQ(child_1, child_1_1);
  EXPECT_NE(child_1_0, child_1_1);

  {
    TestNavigationObserver observer(shell()->web_contents());

    ASSERT_EQ(2U, root->child_at(1)->child_count());
    EXPECT_TRUE(
        ExecJs(root->child_at(1), JsReplace(
                                      R"(
                var iframe = document.createElement('iframe');
                iframe.src = 'data:text/html;base64,' + btoa($1);
                document.body.appendChild(iframe);
            )",
                                      "<html><body>This data: doc should be "
                                      "different-origin.</body></html>")));
    observer.Wait();
    ASSERT_EQ(3U, root->child_at(1)->child_count());
  }
  EXPECT_EQ(root_origin, root->current_origin());
  EXPECT_EQ(child_1, root->child_at(1)->current_origin());
  EXPECT_EQ(child_1_0, root->child_at(1)->child_at(0)->current_origin());
  EXPECT_EQ(child_1_1, root->child_at(1)->child_at(1)->current_origin());
  url::Origin child_1_2 = root->child_at(1)->child_at(2)->current_origin();
  EXPECT_NE(child_1, child_1_2);
  EXPECT_NE(child_1_0, child_1_2);
  EXPECT_NE(child_1_1, child_1_2);
  EXPECT_EQ(url::SchemeHostPort(main_url),
            child_1_2.GetTupleOrPrecursorTupleIfOpaque());

  // If the parent navigates its child to a data URL, it should transfer
  // to the parent's process, and the precursor origin should track the
  // parent's origin.
  {
    GURL data_url("data:text/html,foo2");
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, JsReplace("frames[0].location = $1", data_url)));
    observer.Wait();
    EXPECT_EQ(data_url, root->child_at(0)->current_url());
  }

  EXPECT_EQ(root->child_at(0)->current_origin().Serialize(), "null");
  EXPECT_TRUE(root->child_at(0)->current_origin().opaque());
  EXPECT_EQ(
      url::SchemeHostPort(main_url),
      root->child_at(0)->current_origin().GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            root->child_at(0)->current_frame_host()->GetProcess());
}

// Test to verify that a blob: URL that is created by a unique opaque origin
// will correctly set the origin_to_commit on a session history navigation.
IN_PROC_BROWSER_TEST_F(CrossProcessFrameTreeBrowserTest,
                       OriginForBlobUrlsFromUniqueOpaqueOrigin) {
  // Start off with a navigation to data: URL in the main frame. It should
  // result in a unique opaque origin without any precursor information.
  GURL data_url("data:text/html,foo<iframe id='child' src='" +
                embedded_test_server()->GetURL("/title1.html").spec() +
                "'></iframe>");
  EXPECT_TRUE(NavigateToURL(shell(), data_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_TRUE(root->current_origin().opaque());
  EXPECT_FALSE(
      root->current_origin().GetTupleOrPrecursorTupleIfOpaque().IsValid());
  EXPECT_EQ(1UL, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  // Create a blob: URL and navigate the child frame to it.
  std::string html = "<html><body>This is blob content.</body></html>";
  std::string script = JsReplace(
      "var blob = new Blob([$1], {type: 'text/html'});"
      "var blob_url = URL.createObjectURL(blob);"
      "document.getElementById('child').src = blob_url;"
      "blob_url;",
      html);
  GURL blob_url;
  {
    TestFrameNavigationObserver observer(child);
    blob_url = GURL(EvalJs(root, script).ExtractString());
    observer.Wait();
    EXPECT_EQ(blob_url, child->current_frame_host()->GetLastCommittedURL());
  }

  // We expect the frame to have committed in an opaque origin which contains
  // the same precursor information - none.
  url::Origin blob_origin = child->current_origin();
  EXPECT_TRUE(blob_origin.opaque());
  EXPECT_EQ(root->current_origin().GetTupleOrPrecursorTupleIfOpaque(),
            blob_origin.GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_FALSE(
      child->current_origin().GetTupleOrPrecursorTupleIfOpaque().IsValid());

  // Navigate the frame away to any web URL.
  {
    GURL url(embedded_test_server()->GetURL("/title2.html"));
    TestFrameNavigationObserver observer(child);
    EXPECT_TRUE(ExecJs(child, JsReplace("window.location = $1", url)));
    observer.Wait();
    EXPECT_EQ(url, child->current_frame_host()->GetLastCommittedURL());
  }
  EXPECT_FALSE(child->current_origin().opaque());
  EXPECT_TRUE(shell()->web_contents()->GetController().CanGoBack());
  EXPECT_EQ(3, shell()->web_contents()->GetController().GetEntryCount());
  EXPECT_EQ(
      2, shell()->web_contents()->GetController().GetLastCommittedEntryIndex());

  // Verify the blob URL still exists in the main frame, which keeps it alive
  // allowing a session history navigation back to succeed.
  EXPECT_EQ(blob_url, GURL(EvalJs(root, "blob_url;").ExtractString()));

  // Now navigate back in session history. It should successfully go back to
  // the blob: URL. The child frame won't be reusing the exact same origin it
  // used before, but it will commit a new opaque origin which will still have
  // no precursor information.
  {
    TestFrameNavigationObserver observer(child);
    shell()->web_contents()->GetController().GoBack();
    observer.Wait();
  }
  EXPECT_EQ(blob_url, child->current_frame_host()->GetLastCommittedURL());
  EXPECT_TRUE(child->current_origin().opaque());
  EXPECT_NE(blob_origin, child->current_origin());
  EXPECT_EQ(root->current_origin().GetTupleOrPrecursorTupleIfOpaque(),
            child->current_origin().GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_FALSE(
      child->current_origin().GetTupleOrPrecursorTupleIfOpaque().IsValid());
}

// Test to verify that about:blank iframe, which is a child of a sandboxed
// iframe is not considered same origin, but precursor information is preserved
// in its origin.
IN_PROC_BROWSER_TEST_F(CrossProcessFrameTreeBrowserTest,
                       AboutBlankSubframeInSandboxedFrame) {
  // Start off by navigating to a page with sandboxed iframe, which allows
  // script execution.
  GURL main_url(
      embedded_test_server()->GetURL("/sandboxed_main_frame_script.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_EQ(1UL, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  // Navigate the frame to data: URL to cause it to have an opaque origin that
  // is derived from the |main_url| origin.
  GURL data_url("data:text/html,<html><body>foo</body></html>");
  {
    TestFrameNavigationObserver observer(child);
    EXPECT_TRUE(ExecJs(root, JsReplace("frames[0].location = $1", data_url)));
    observer.Wait();
    EXPECT_EQ(data_url, child->current_frame_host()->GetLastCommittedURL());
  }

  // Add an about:blank iframe to the data: frame, which should not inherit the
  // origin, but should preserve the precursor information.
  {
    EXPECT_TRUE(ExecJs(child,
                       "var f = document.createElement('iframe');"
                       "document.body.appendChild(f);"));
  }
  EXPECT_EQ(1UL, child->child_count());
  FrameTreeNode* grandchild = child->child_at(0);

  EXPECT_TRUE(grandchild->current_origin().opaque());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            grandchild->current_frame_host()->GetLastCommittedURL());

  // The origin of the data: document should have precursor information matching
  // the main frame origin.
  EXPECT_EQ(root->current_origin().GetTupleOrPrecursorTupleIfOpaque(),
            child->current_origin().GetTupleOrPrecursorTupleIfOpaque());

  // The same should hold also for the about:blank subframe of the data: frame.
  EXPECT_EQ(root->current_origin().GetTupleOrPrecursorTupleIfOpaque(),
            grandchild->current_origin().GetTupleOrPrecursorTupleIfOpaque());

  // The about:blank document should not be able to access its parent, as they
  // are considered cross origin due to the sandbox flags on the parent.
  EXPECT_FALSE(ExecJs(grandchild, "window.parent.foo = 'bar';"));
  EXPECT_NE(child->current_origin(), grandchild->current_origin());
}

// Ensure that a popup opened from a sandboxed main frame inherits sandbox flags
// from its opener.
IN_PROC_BROWSER_TEST_F(CrossProcessFrameTreeBrowserTest,
                       SandboxFlagsSetForNewWindow) {
  GURL main_url(
      embedded_test_server()->GetURL("/sandboxed_main_frame_script.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Open a new window from the main frame.
  GURL popup_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  Shell* new_shell = OpenPopup(root->current_frame_host(), popup_url, "");
  EXPECT_TRUE(new_shell);
  WebContents* new_contents = new_shell->web_contents();

  // Check that the new window's sandbox flags correctly reflect the opener's
  // flags. Main frame sets allow-popups, allow-pointer-lock and allow-scripts.
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_contents)->GetPrimaryFrameTree().root();
  network::mojom::WebSandboxFlags main_frame_sandbox_flags =
      root->current_frame_host()->active_sandbox_flags();
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
          ~network::mojom::WebSandboxFlags::kPointerLock &
          ~network::mojom::WebSandboxFlags::kPopups &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols,
      main_frame_sandbox_flags);

  EXPECT_EQ(main_frame_sandbox_flags,
            popup_root->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(main_frame_sandbox_flags, popup_root->active_sandbox_flags());
  EXPECT_EQ(main_frame_sandbox_flags,
            popup_root->current_frame_host()->active_sandbox_flags());
}

// Tests that the user activation bits get cleared when a cross-site document is
// installed in the frame.
IN_PROC_BROWSER_TEST_F(CrossProcessFrameTreeBrowserTest,
                       ClearUserActivationForNewDocument) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());

  // Set the user activation bits.
  root->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(root->HasStickyUserActivation());
  EXPECT_TRUE(root->HasTransientUserActivation());

  // Install a new cross-site document to check the clearing of user activation
  // bits.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), cross_site_url));

  EXPECT_FALSE(root->HasStickyUserActivation());
  EXPECT_FALSE(root->HasTransientUserActivation());
}

class BrowserContextGroupSwapFrameTreeBrowserTest : public ContentBrowserTest {
 public:
  BrowserContextGroupSwapFrameTreeBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(&https_server_);
    ASSERT_TRUE(https_server_.Start());
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 public:
  net::EmbeddedTestServer https_server_;
};

// Force a race between when the RenderViewHostImpl's main frame is running
// the unload handlers and when a new navigation occurs that tries to
// reuse a RenderViewHostImpl.
IN_PROC_BROWSER_TEST_F(BrowserContextGroupSwapFrameTreeBrowserTest,
                       NavigateAndGoBack) {
  GURL main_url(https_server()->GetURL("a.test", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  web_contents->GetPrimaryMainFrame()->DoNotDeleteForTesting();
  DisableBFCacheForRFHForTesting(
      web_contents->GetPrimaryFrameTree().root()->current_frame_host());

  // Load a page with COOP set to force the browsing context group swap
  // and clears out old proxies.
  GURL new_main_url(https_server()->GetURL(
      "b.test", "/set-header?Cross-Origin-Opener-Policy: same-origin"));

  EXPECT_TRUE(NavigateToURL(shell(), new_main_url));

  TestNavigationObserver back_load_observer(web_contents);
  web_contents->GetController().GoBack();
  back_load_observer.Wait();
}

// FrameTreeBrowserTest variant where we isolate http://*.is, Iceland's top
// level domain. This is an analogue to isolating extensions, which we can use
// inside content_browsertests, where extensions don't exist. Iceland, like an
// extension process, is a special place with magical powers; we want to protect
// it from outsiders.
class IsolateIcelandFrameTreeBrowserTest : public ContentBrowserTest {
 public:
  IsolateIcelandFrameTreeBrowserTest() = default;

  IsolateIcelandFrameTreeBrowserTest(
      const IsolateIcelandFrameTreeBrowserTest&) = delete;
  IsolateIcelandFrameTreeBrowserTest& operator=(
      const IsolateIcelandFrameTreeBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Blink suppresses navigations to blob URLs of origins different from the
    // frame initiating the navigation. We disable those checks for this test,
    // to test what happens in a compromise scenario.
    command_line->AppendSwitch(switches::kDisableWebSecurity);

    // ProcessSwitchForIsolatedBlob test below requires that one of URLs used in
    // the test (blob:http://b.is/) belongs to an isolated origin.
    command_line->AppendSwitchASCII(switches::kIsolateOrigins, "http://b.is/");
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Regression test for https://crbug.com/644966
IN_PROC_BROWSER_TEST_F(IsolateIcelandFrameTreeBrowserTest,
                       ProcessSwitchForIsolatedBlob) {
  // Set up an iframe.
  WebContents* contents = shell()->web_contents();
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetPrimaryFrameTree().root();
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // The navigation targets an invalid blob url; that's intentional to trigger
  // an error response. The response should commit in a process dedicated to
  // http://b.is or error pages, depending on policy.
  EXPECT_EQ(
      "done",
      EvalJs(
          root,
          "new Promise((resolve) => {"
          "  var iframe_element = document.getElementsByTagName('iframe')[0];"
          "  iframe_element.onload = () => resolve('done');"
          "  iframe_element.src = 'blob:http://b.is/';"
          "});"));
  EXPECT_TRUE(WaitForLoadStop(contents));

  // Make sure we did a process transfer back to "b.is".
  const std::string kExpectedSiteURL =
      AreDefaultSiteInstancesEnabled()
          ? SiteInstanceImpl::GetDefaultSiteURL().spec()
          : "http://a.com/";
  const std::string kExpectedSubframeSiteURL =
      SiteIsolationPolicy::IsErrorPageIsolationEnabled(/*in_main_frame*/ false)
          ? "chrome-error://chromewebdata/"
          : "http://b.is/";
  EXPECT_EQ(base::StringPrintf(" Site A ------------ proxies for B\n"
                               "   +--Site B ------- proxies for A\n"
                               "Where A = %s\n"
                               "      B = %s",
                               kExpectedSiteURL.c_str(),
                               kExpectedSubframeSiteURL.c_str()),
            DepictFrameTree(*root));
}

class FrameTreeCredentiallessIframeBrowserTest : public FrameTreeBrowserTest {
 public:
  FrameTreeCredentiallessIframeBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableBlinkTestFeatures);
  }
};

// Tests the mojo propagation of the 'credentialless' attribute to the browser.
IN_PROC_BROWSER_TEST_F(FrameTreeCredentiallessIframeBrowserTest,
                       AttributeIsPropagatedToBrowser) {
  GURL main_url(embedded_test_server()->GetURL("/hello.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Not setting the attribute => the iframe is not credentialless.
  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('iframe');"
                     "document.body.appendChild(f);"));
  EXPECT_EQ(1U, root->child_count());
  EXPECT_FALSE(root->child_at(0)->Credentialless());
  EXPECT_EQ(false, EvalJs(root->child_at(0)->current_frame_host(),
                          "window.credentialless"));

  // Setting the attribute on the iframe element makes the iframe
  // credentialless.
  EXPECT_TRUE(ExecJs(root,
                     "var d = document.createElement('div');"
                     "d.innerHTML = '<iframe credentialless></iframe>';"
                     "document.body.appendChild(d);"));
  EXPECT_EQ(2U, root->child_count());
  EXPECT_TRUE(root->child_at(1)->Credentialless());
  EXPECT_EQ(true, EvalJs(root->child_at(1)->current_frame_host(),
                         "window.credentialless"));

  // Setting the attribute via javascript works.
  EXPECT_TRUE(ExecJs(root,
                     "var g = document.createElement('iframe');"
                     "g.credentialless = true;"
                     "document.body.appendChild(g);"));
  EXPECT_EQ(3U, root->child_count());
  EXPECT_TRUE(root->child_at(2)->Credentialless());
  EXPECT_EQ(true, EvalJs(root->child_at(2)->current_frame_host(),
                         "window.credentialless"));

  EXPECT_TRUE(ExecJs(root, "g.credentialless = false;"));
  EXPECT_FALSE(root->child_at(2)->Credentialless());
  EXPECT_EQ(true, EvalJs(root->child_at(2)->current_frame_host(),
                         "window.credentialless"));

  EXPECT_TRUE(ExecJs(root, "g.credentialless = true;"));
  EXPECT_TRUE(root->child_at(2)->Credentialless());
  EXPECT_EQ(true, EvalJs(root->child_at(2)->current_frame_host(),
                         "window.credentialless"));
}

// TODO(crbug.com/1407150): Remove this when deprecation trial is complete.
class FrameTreeSessionStorageDeprecationTrialBrowserTest
    : public ContentBrowserTest {
 public:
  FrameTreeSessionStorageDeprecationTrialBrowserTest() {
    feature_list_.InitAndEnableFeature(
        net::features::kThirdPartyStoragePartitioning);
  }

 protected:
  virtual net::EmbeddedTestServer& GetServer() = 0;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GetServer().ServeFilesFromSourceDirectory("content/test/data");
    // EmbeddedTestServer::InitializeAndListen() initializes its `base_url_`
    // which is required below. This cannot invoke Start() however as that kicks
    // off the "EmbeddedTestServer IO Thread" which then races with
    // initialization in ContentBrowserTest::SetUp(), http://crbug.com/674545.
    ASSERT_TRUE(GetServer().InitializeAndListen());

    // Add a host resolver rule to map all outgoing requests to the test server.
    // This allows us to use "real" hostnames in URLs, which we can use to
    // create arbitrary SiteInstances.
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        "MAP * " +
            net::HostPortPair::FromURL(GetServer().base_url()).ToString() +
            ",EXCLUDE localhost");
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUp() override { ContentBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    // Complete the manual Start() after ContentBrowserTest's own
    // initialization, ref. comment on InitializeAndListen() above.
    GetServer().StartAcceptingConnections();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  content::ContentMockCertVerifier mock_cert_verifier_;
};

class FrameTreeSessionStorageDeprecationTrialBrowserSecureTest
    : public FrameTreeSessionStorageDeprecationTrialBrowserTest {
  net::EmbeddedTestServer& GetServer() override {
    static net::EmbeddedTestServer https_server(
        net::EmbeddedTestServer::TYPE_HTTPS);
    return https_server;
  }
};

IN_PROC_BROWSER_TEST_F(FrameTreeSessionStorageDeprecationTrialBrowserSecureTest,
                       RegisterOriginForUnpartitionedSessionStorageAccess) {
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  const blink::StorageKey first_party =
      blink::StorageKey::CreateFirstParty(origin);
  const blink::StorageKey third_party = blink::StorageKey::Create(
      origin, net::SchemefulSite(GURL("https://notexample.com")),
      blink::mojom::AncestorChainBit::kCrossSite);
  const url::Origin opaque_origin = url::Origin();
  const blink::StorageKey opaque_first_party =
      blink::StorageKey::CreateFirstParty(opaque_origin);
  const blink::StorageKey opaque_third_party = blink::StorageKey::Create(
      opaque_origin, net::SchemefulSite(GURL("https://notexample.com")),
      blink::mojom::AncestorChainBit::kCrossSite);
  EXPECT_NE(first_party, third_party);
  EXPECT_NE(opaque_first_party, opaque_third_party);
  FrameTree& frame_tree = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree();

  // Before registering any origins we expect partitioned access for both keys.
  EXPECT_EQ(third_party, frame_tree.GetSessionStorageKey(third_party));
  EXPECT_EQ(opaque_third_party,
            frame_tree.GetSessionStorageKey(opaque_third_party));

  // We then register both origins.
  frame_tree.RegisterOriginForUnpartitionedSessionStorageAccess(origin);
  frame_tree.RegisterOriginForUnpartitionedSessionStorageAccess(opaque_origin);

  // After registration the non-opaque key is unpartitioned but the opaque one
  // is still partitioned.
  EXPECT_EQ(first_party, frame_tree.GetSessionStorageKey(third_party));
  EXPECT_EQ(opaque_third_party,
            frame_tree.GetSessionStorageKey(opaque_third_party));
}

IN_PROC_BROWSER_TEST_F(FrameTreeSessionStorageDeprecationTrialBrowserSecureTest,
                       GetSessionStorageKey) {
  const blink::StorageKey dt_third_party = blink::StorageKey::Create(
      url::Origin::Create(GURL("https://example.com")),
      net::SchemefulSite(GURL("https://notexample.com")),
      blink::mojom::AncestorChainBit::kCrossSite);
  const blink::StorageKey dt_first_party =
      blink::StorageKey::CreateFromStringForTesting("https://example.com");
  const blink::StorageKey random_third_party = blink::StorageKey::Create(
      url::Origin::Create(GURL("https://otherexample.com")),
      net::SchemefulSite(GURL("https://notexample.com")),
      blink::mojom::AncestorChainBit::kCrossSite);
  EXPECT_NE(dt_third_party, dt_first_party);

  // Load a page without the origin trial token.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("https://example.com/empty.html")));
  // We should be able to get a partitioned storage key for example.com.
  EXPECT_EQ(dt_third_party,
            static_cast<WebContentsImpl*>(shell()->web_contents())
                ->GetPrimaryFrameTree()
                .GetSessionStorageKey(dt_third_party));

  // Load a page with the origin trial token.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("https://example.com/session_storage/"
                                          "partition_deprecation_trial.html")));
  // We shouldn't be able to get a partitioned storage key for example.com.
  EXPECT_EQ(dt_first_party,
            static_cast<WebContentsImpl*>(shell()->web_contents())
                ->GetPrimaryFrameTree()
                .GetSessionStorageKey(dt_third_party));
  // Other origins can still get partitioned storage keys.
  EXPECT_EQ(random_third_party,
            static_cast<WebContentsImpl*>(shell()->web_contents())
                ->GetPrimaryFrameTree()
                .GetSessionStorageKey(random_third_party));

  // Load a page without the token after having loaded a page with the token.
  EXPECT_TRUE(
      NavigateToURL(shell(), GURL("https://otherexample.com/empty.html")));
  // We shouldn't be able to get a partitioned storage key for example.com.
  EXPECT_EQ(dt_first_party,
            static_cast<WebContentsImpl*>(shell()->web_contents())
                ->GetPrimaryFrameTree()
                .GetSessionStorageKey(dt_third_party));
  // Other origins can still get partitioned storage keys.
  EXPECT_EQ(random_third_party,
            static_cast<WebContentsImpl*>(shell()->web_contents())
                ->GetPrimaryFrameTree()
                .GetSessionStorageKey(random_third_party));

  // Load a page without the origin trial token.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("https://example.com/empty.html")));
  // We should be able to get a partitioned storage key for example.com.
  EXPECT_EQ(dt_third_party,
            static_cast<WebContentsImpl*>(shell()->web_contents())
                ->GetPrimaryFrameTree()
                .GetSessionStorageKey(dt_third_party));
}

class FrameTreeSessionStorageDeprecationTrialBrowserInsecureTest
    : public FrameTreeSessionStorageDeprecationTrialBrowserTest {
  net::EmbeddedTestServer& GetServer() override {
    static net::EmbeddedTestServer https_server_(
        net::EmbeddedTestServer::TYPE_HTTP);
    return https_server_;
  }
};

IN_PROC_BROWSER_TEST_F(
    FrameTreeSessionStorageDeprecationTrialBrowserInsecureTest,
    GetSessionStorageKeyInsecure) {
  const blink::StorageKey dt_third_party = blink::StorageKey::Create(
      url::Origin::Create(GURL("http://example.com")),
      net::SchemefulSite(GURL("http://notexample.com")),
      blink::mojom::AncestorChainBit::kCrossSite);
  const blink::StorageKey dt_first_party =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");
  const blink::StorageKey random_third_party = blink::StorageKey::Create(
      url::Origin::Create(GURL("http://otherexample.com")),
      net::SchemefulSite(GURL("http://notexample.com")),
      blink::mojom::AncestorChainBit::kCrossSite);
  EXPECT_NE(dt_third_party, dt_first_party);

  // Load a page without the origin trial token.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("http://example.com/empty.html")));
  // We should be able to get a partitioned storage key for example.com.
  EXPECT_EQ(dt_third_party,
            static_cast<WebContentsImpl*>(shell()->web_contents())
                ->GetPrimaryFrameTree()
                .GetSessionStorageKey(dt_third_party));

  // Load a page with the origin trial token.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("http://example.com/session_storage/"
                                          "partition_deprecation_trial.html")));
  // We shouldn't be able to get a partitioned storage key for example.com.
  EXPECT_EQ(dt_first_party,
            static_cast<WebContentsImpl*>(shell()->web_contents())
                ->GetPrimaryFrameTree()
                .GetSessionStorageKey(dt_third_party));
  // Other origins can still get partitioned storage keys.
  EXPECT_EQ(random_third_party,
            static_cast<WebContentsImpl*>(shell()->web_contents())
                ->GetPrimaryFrameTree()
                .GetSessionStorageKey(random_third_party));

  // Load a page without the token after having loaded a page with the token.
  EXPECT_TRUE(
      NavigateToURL(shell(), GURL("http://otherexample.com/empty.html")));
  // We shouldn't be able to get a partitioned storage key for example.com.
  EXPECT_EQ(dt_first_party,
            static_cast<WebContentsImpl*>(shell()->web_contents())
                ->GetPrimaryFrameTree()
                .GetSessionStorageKey(dt_third_party));
  // Other origins can still get partitioned storage keys.
  EXPECT_EQ(random_third_party,
            static_cast<WebContentsImpl*>(shell()->web_contents())
                ->GetPrimaryFrameTree()
                .GetSessionStorageKey(random_third_party));

  // Load a page without the origin trial token.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("http://example.com/empty.html")));
  // We should be able to get a partitioned storage key for example.com.
  EXPECT_EQ(dt_third_party,
            static_cast<WebContentsImpl*>(shell()->web_contents())
                ->GetPrimaryFrameTree()
                .GetSessionStorageKey(dt_third_party));
}
}  // namespace content
