// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/frame_test_utils.h"

#include "base/strings/strcat.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/cookies/canonical_cookie.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

void ArrangeFramesAndNavigate(WebContents* web_contents,
                              net::EmbeddedTestServer* server,
                              const std::string& frame_tree) {
  ASSERT_TRUE(NavigateToURL(
      web_contents,
      server->GetURL(
          frame_tree.substr(0, frame_tree.find("(")),
          base::StrCat({"/cross_site_iframe_factory.html?", frame_tree}))));
}

RenderFrameHostImpl* SelectDescendentFrame(WebContents* web_contents,
                                           const std::vector<int>& indices) {
  RenderFrameHostImpl* selected_frame =
      static_cast<RenderFrameHostImpl*>(web_contents->GetPrimaryMainFrame());
  for (int index : indices) {
    selected_frame = selected_frame->child_at(index)->current_frame_host();
  }
  return selected_frame;
}

}  // namespace

std::string ArrangeFramesAndGetContentFromLeaf(
    WebContents* web_contents,
    net::EmbeddedTestServer* server,
    const std::string& frame_tree_pattern,
    const std::vector<int>& leaf_path,
    const GURL& leaf_url) {
  ArrangeFramesAndNavigate(
      web_contents, server,
      base::StringPrintf(frame_tree_pattern.c_str(), leaf_url.spec().c_str()));
  return EvalJs(SelectDescendentFrame(web_contents, leaf_path),
                "document.body.textContent")
      .ExtractString();
}

std::vector<net::CanonicalCookie> ArrangeFramesAndGetCanonicalCookiesForLeaf(
    WebContents* web_contents,
    net::EmbeddedTestServer* server,
    const std::string& frame_tree_pattern,
    const GURL& leaf_url) {
  return ArrangeFramesAndGetCanonicalCookiesForLeaf(
      web_contents, server, frame_tree_pattern, leaf_url,
      url::Origin::Create(leaf_url).GetURL());
}

std::vector<net::CanonicalCookie> ArrangeFramesAndGetCanonicalCookiesForLeaf(
    WebContents* web_contents,
    net::EmbeddedTestServer* server,
    const std::string& frame_tree_pattern,
    const GURL& leaf_url,
    const GURL& cookie_url) {
  ArrangeFramesAndNavigate(
      web_contents, server,
      base::StringPrintf(frame_tree_pattern.c_str(), leaf_url.spec().c_str()));
  return GetCanonicalCookies(web_contents->GetBrowserContext(), cookie_url);
}

}  // namespace content