// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_JS_MESSAGING_CONTENT_WEB_FRAMES_MANAGER_H_
#define IOS_WEB_CONTENT_JS_MESSAGING_CONTENT_WEB_FRAMES_MANAGER_H_

#import "ios/web/public/js_messaging/web_frames_manager.h"

#import "base/observer_list.h"
#import "build/blink_buildflags.h"
#import "content/public/browser/global_routing_id.h"
#import "content/public/browser/web_contents_observer.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

namespace web {

class ContentWebState;

// ContentWebFramesManager is a WebFramesManager that is built on top
// of //content. As a WebContentsObserver, it finds out about all frame creation
// and destruction. It uses two id schemes to identify frames: content's id
// scheme for RenderFrameHosts, and a scheme that mimics WebFrameImpl's string
// id scheme.
class ContentWebFramesManager : public WebFramesManager,
                                public content::WebContentsObserver {
 public:
  ContentWebFramesManager(ContentWebState* content_web_state);
  ~ContentWebFramesManager() override;

  // WebFramesManager impl.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::set<WebFrame*> GetAllWebFrames() override;
  WebFrame* GetMainWebFrame() override;
  WebFrame* GetFrameWithId(const std::string& frame_id) override;

  // WebContentsObserver overrides.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void PrimaryPageChanged(content::Page& page) override;

 private:
  // List of pointers to all web frames.
  std::map<std::string, std::unique_ptr<WebFrame>> web_frames_;

  // Map from content's id scheme to web's id scheme.
  std::map<content::GlobalRenderFrameHostId, std::string>
      content_to_web_id_map_;

  // Reference to the current main frame
  content::GlobalRenderFrameHostId main_frame_content_id_;
  base::ObserverList<Observer, /*check_empty=*/false> observers_;

  // The ContentWebState that owns this object.
  raw_ptr<ContentWebState> content_web_state_;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_JS_MESSAGING_CONTENT_WEB_FRAMES_MANAGER_H_
