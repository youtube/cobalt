// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_REQUEST_EXTRA_DATA_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_REQUEST_EXTRA_DATA_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_frame_request_blocker.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

namespace network {
struct ResourceRequest;
}

namespace blink {

class BLINK_PLATFORM_EXPORT WebURLRequestExtraData
    : public base::RefCounted<WebURLRequestExtraData> {
 public:
  WebURLRequestExtraData();
  WebURLRequestExtraData(const WebURLRequestExtraData&) = delete;
  WebURLRequestExtraData& operator=(const WebURLRequestExtraData&) = delete;

  void set_is_outermost_main_frame(bool is_outermost_main_frame) {
    is_outermost_main_frame_ = is_outermost_main_frame;
  }
  ui::PageTransition transition_type() const { return transition_type_; }
  void set_transition_type(ui::PageTransition transition_type) {
    transition_type_ = transition_type;
  }

  // Set the top origin. Only applicable for frames.
  void set_top_frame_origin(const url::Origin& origin) {
    top_frame_origin_ = origin;
  }

  // The origin of the topmost frame. Only applicable for frames.
  const url::Origin& top_frame_origin() { return top_frame_origin_; }

  // The request is for a prefetch-only client (i.e. running NoStatePrefetch)
  // and should use LOAD_PREFETCH network flags.
  bool is_for_no_state_prefetch() const { return is_for_no_state_prefetch_; }
  void set_is_for_no_state_prefetch(bool prefetch) {
    is_for_no_state_prefetch_ = prefetch;
  }

  // true if the request originated from within a service worker e.g. due to
  // a fetch() in the service worker script.
  void set_originated_from_service_worker(bool originated_from_service_worker) {
    originated_from_service_worker_ = originated_from_service_worker;
  }

  // |custom_user_agent| is used to communicate an overriding custom user agent
  // to |RenderViewImpl::willSendRequest()|; set to a null string to indicate no
  // override and an empty string to indicate that there should be no user
  // agent.
  const WebString& custom_user_agent() const { return custom_user_agent_; }
  void set_custom_user_agent(const WebString& custom_user_agent) {
    custom_user_agent_ = custom_user_agent;
  }

  WebVector<std::unique_ptr<URLLoaderThrottle>> TakeURLLoaderThrottles() {
    return std::move(url_loader_throttles_);
  }
  void set_url_loader_throttles(
      WebVector<std::unique_ptr<URLLoaderThrottle>> throttles) {
    url_loader_throttles_ = std::move(throttles);
  }
  void set_frame_request_blocker(
      scoped_refptr<WebFrameRequestBlocker> frame_request_blocker) {
    frame_request_blocker_ = frame_request_blocker;
  }
  scoped_refptr<WebFrameRequestBlocker> frame_request_blocker() {
    return frame_request_blocker_;
  }
  bool allow_cross_origin_auth_prompt() const {
    return allow_cross_origin_auth_prompt_;
  }
  void set_allow_cross_origin_auth_prompt(bool allow_cross_origin_auth_prompt) {
    allow_cross_origin_auth_prompt_ = allow_cross_origin_auth_prompt;
  }

  void CopyToResourceRequest(network::ResourceRequest* request) const;

 protected:
  friend class base::RefCounted<WebURLRequestExtraData>;
  virtual ~WebURLRequestExtraData();

 private:
  bool is_outermost_main_frame_ = false;
  ui::PageTransition transition_type_ = ui::PAGE_TRANSITION_LINK;
  bool is_for_no_state_prefetch_ = false;
  bool originated_from_service_worker_ = false;
  WebString custom_user_agent_;
  WebVector<std::unique_ptr<URLLoaderThrottle>> url_loader_throttles_;
  scoped_refptr<WebFrameRequestBlocker> frame_request_blocker_;
  bool allow_cross_origin_auth_prompt_ = false;

  // The origin of the top most frame. Only applicable for frames.
  url::Origin top_frame_origin_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_URL_REQUEST_EXTRA_DATA_H_
