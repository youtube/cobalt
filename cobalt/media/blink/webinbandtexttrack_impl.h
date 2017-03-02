// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BLINK_WEBINBANDTEXTTRACK_IMPL_H_
#define COBALT_MEDIA_BLINK_WEBINBANDTEXTTRACK_IMPL_H_

#include "base/basictypes.h"
#include "third_party/WebKit/public/platform/WebInbandTextTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace media {

class WebInbandTextTrackImpl : public blink::WebInbandTextTrack {
 public:
  WebInbandTextTrackImpl(Kind kind, const blink::WebString& label,
                         const blink::WebString& language,
                         const blink::WebString& id);
  ~WebInbandTextTrackImpl() OVERRIDE;

  void setClient(blink::WebInbandTextTrackClient* client) OVERRIDE;
  blink::WebInbandTextTrackClient* client() OVERRIDE;

  Kind kind() const OVERRIDE;

  blink::WebString label() const OVERRIDE;
  blink::WebString language() const OVERRIDE;
  blink::WebString id() const OVERRIDE;

 private:
  blink::WebInbandTextTrackClient* client_;
  Kind kind_;
  blink::WebString label_;
  blink::WebString language_;
  blink::WebString id_;
  DISALLOW_COPY_AND_ASSIGN(WebInbandTextTrackImpl);
};

}  // namespace media

#endif  // COBALT_MEDIA_BLINK_WEBINBANDTEXTTRACK_IMPL_H_
