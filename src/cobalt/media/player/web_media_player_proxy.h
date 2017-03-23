// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_PLAYER_WEB_MEDIA_PLAYER_PROXY_H_
#define COBALT_MEDIA_PLAYER_WEB_MEDIA_PLAYER_PROXY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "cobalt/media/player/buffered_data_source.h"

namespace cobalt {
namespace media {

class WebMediaPlayerImpl;

// Acts as a thread proxy between the various threads used for multimedia and
// the render thread that WebMediaPlayerImpl is running on.
class WebMediaPlayerProxy
    : public base::RefCountedThreadSafe<WebMediaPlayerProxy> {
 public:
  WebMediaPlayerProxy(const scoped_refptr<base::MessageLoopProxy>& render_loop,
                      WebMediaPlayerImpl* webmediaplayer);
  BufferedDataSource* data_source() { return data_source_.get(); }
  void set_data_source(scoped_ptr<BufferedDataSource> data_source) {
    data_source_ = data_source.Pass();
  }

  void Detach();
  bool HasSingleOrigin();
  bool DidPassCORSAccessCheck() const;

  void AbortDataSource();

 private:
  friend class base::RefCountedThreadSafe<WebMediaPlayerProxy>;
  virtual ~WebMediaPlayerProxy();

  // The render message loop where the renderer lives.
  scoped_refptr<base::MessageLoopProxy> render_loop_;
  WebMediaPlayerImpl* webmediaplayer_;

  scoped_ptr<BufferedDataSource> data_source_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMediaPlayerProxy);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_PLAYER_WEB_MEDIA_PLAYER_PROXY_H_
