// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NEW_TAB_HANDLE_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NEW_TAB_HANDLE_H_

#include <memory>

#include "content/browser/preloading/prerender/prerender_attributes.h"
#include "content/common/frame.mojom-forward.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

class BrowserContext;
class PrerenderCancellationReason;
class PrerenderHost;
class PrerenderHostRegistry;
class WebContentsImpl;

// PrerenderNewTabHandle creates a new WebContentsImpl instance, starts
// prerendering on that, and keeps the instance until the prerendered page is
// activated for navigation or cancelled for some reason. By starting
// prerendering in a new WebContentsImpl instance, this can serve a prerendered
// page to navigation in a new tab different from a tab of the page that
// triggered prerendering.
//
// PrerenderHostRegistry instantiates and owns PrerenderNewTabHandle in response
// to prerendering requests via Speculation Rules API. When navigation is about
// to start for a new tab, the navigation asks PrerenderHostRegistry if there is
// a PrerenderNewTabHandle instance that has a pre-created WebContentsImpl
// suitable for this navigation. If it's found, the navigation takes over the
// ownership of the pre-created WebContentsImpl instance and destroys the
// handle.
class PrerenderNewTabHandle {
 public:
  PrerenderNewTabHandle(const PrerenderAttributes& attributes,
                        BrowserContext& browser_context);
  ~PrerenderNewTabHandle();

  // Starts prerendering in `web_contents_`. Returns the root FrameTreeNode id
  // of the prerendered page, which can be used as the id of PrerenderHost, on
  // success. Returns RenderFrameHost::kNoFrameTreeNodeId on failure.
  int StartPrerendering();

  // Cancels prerendering started in `web_contents_`.
  void CancelPrerendering(const PrerenderCancellationReason& reason);

  // Passes the ownership of `web_contents_` to the caller if it's available for
  // new tab navigation with given params.
  std::unique_ptr<WebContentsImpl> TakeWebContentsIfAvailable(
      const mojom::CreateNewWindowParams& create_new_window_params,
      const WebContents::CreateParams& web_contents_create_params);

  // Returns PrerenderHost that `web_contents_` is hosting.
  PrerenderHost* GetPrerenderHostForTesting();

  // Returns PrerenderTriggerType.
  PrerenderTriggerType trigger_type() const { return attributes_.trigger_type; }

 private:
  PrerenderHostRegistry& GetPrerenderHostRegistry();

  const PrerenderAttributes attributes_;

  // Used for creating WebContentsImpl that contains a prerendered page for a
  // new tab navigation.
  WebContents::CreateParams web_contents_create_params_;

  // Contains a prerendered page for a new tab navigation. This is valid until
  // TakeWebContentsIfAvailable() is called.
  std::unique_ptr<WebContentsImpl> web_contents_;

  // This is a tentative instance of WebContentsDelegate for `web_contents_`
  // until prerender activation. The delegate of `web_contents_` will be swapped
  // with a proper one on prerender page activation.
  //
  // Note: WebContentsDelegate of the initiator WebContentsImpl is not available
  // for `web_contents_`. This is because WebContentsDelegate can be specific to
  // a tab, and `web_contents_` will be placed in a different tab from the
  // initiator's tab.
  class WebContentsDelegateImpl;
  std::unique_ptr<WebContentsDelegateImpl> web_contents_delegate_;

  int prerender_host_id_ = RenderFrameHost::kNoFrameTreeNodeId;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NEW_TAB_HANDLE_H_
