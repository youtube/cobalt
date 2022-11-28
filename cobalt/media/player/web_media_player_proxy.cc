// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/player/web_media_player_proxy.h"

#include "base/bind.h"
#include "base/logging.h"
#include "cobalt/media/player/web_media_player_impl.h"

namespace cobalt {
namespace media {

WebMediaPlayerProxy::WebMediaPlayerProxy(
    const scoped_refptr<base::SingleThreadTaskRunner>& render_loop,
    WebMediaPlayerImpl* webmediaplayer)
    : render_loop_(render_loop), webmediaplayer_(webmediaplayer) {
  DCHECK(render_loop_);
  DCHECK(webmediaplayer_);
}

WebMediaPlayerProxy::~WebMediaPlayerProxy() { Detach(); }

void WebMediaPlayerProxy::Detach() {
  DCHECK(render_loop_->BelongsToCurrentThread());
  webmediaplayer_ = NULL;
  data_source_.reset();
}

void WebMediaPlayerProxy::AbortDataSource() {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (data_source_) data_source_->Abort();
}

}  // namespace media
}  // namespace cobalt
