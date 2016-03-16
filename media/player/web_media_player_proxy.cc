// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/player/web_media_player_proxy.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#if defined(_DEBUG)
#include "base/string_number_conversions.h"
#endif
#include "media/base/pipeline_status.h"
#include "media/filters/video_renderer_base.h"
#include "media/player/web_media_player_impl.h"

namespace media {

// Limits the maximum outstanding repaints posted on render thread.
// This number of 50 is a guess, it does not take too much memory on the task
// queue but gives up a pretty good latency on repaint.
static const int kMaxOutstandingRepaints = 50;

WebMediaPlayerProxy::WebMediaPlayerProxy(
    const scoped_refptr<base::MessageLoopProxy>& render_loop,
    WebMediaPlayerImpl* webmediaplayer)
    : render_loop_(render_loop),
      webmediaplayer_(webmediaplayer) {
  DCHECK(render_loop_);
  DCHECK(webmediaplayer_);
}

WebMediaPlayerProxy::~WebMediaPlayerProxy() {
  Detach();
}

void WebMediaPlayerProxy::Paint(SkCanvas* canvas,
                                const gfx::Rect& dest_rect,
                                uint8_t alpha) {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (frame_provider_) {
    scoped_refptr<VideoFrame> video_frame;
    frame_provider_->GetCurrentFrame(&video_frame);
    video_renderer_.Paint(video_frame, canvas, dest_rect, alpha);
    frame_provider_->PutCurrentFrame(video_frame);
  }
}

bool WebMediaPlayerProxy::HasSingleOrigin() {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (data_source_)
    return data_source_->HasSingleOrigin();
  return true;
}

bool WebMediaPlayerProxy::DidPassCORSAccessCheck() const {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (data_source_)
    return data_source_->DidPassCORSAccessCheck();
  return false;
}

void WebMediaPlayerProxy::AbortDataSource() {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (data_source_)
    data_source_->Abort();
}

void WebMediaPlayerProxy::Detach() {
  DCHECK(render_loop_->BelongsToCurrentThread());
  webmediaplayer_ = NULL;
  data_source_ = NULL;
  frame_provider_ = NULL;
}

void WebMediaPlayerProxy::GetCurrentFrame(
    scoped_refptr<VideoFrame>* frame_out) {
  if (frame_provider_)
    frame_provider_->GetCurrentFrame(frame_out);
}

void WebMediaPlayerProxy::PutCurrentFrame(scoped_refptr<VideoFrame> frame) {
  if (frame_provider_)
    frame_provider_->PutCurrentFrame(frame);
}

void WebMediaPlayerProxy::KeyAdded(const std::string& key_system,
                                   const std::string& session_id) {
  render_loop_->PostTask(FROM_HERE,
                         base::Bind(&WebMediaPlayerProxy::KeyAddedTask, this,
                                    key_system, session_id));
}

void WebMediaPlayerProxy::KeyError(const std::string& key_system,
                                   const std::string& session_id,
                                   Decryptor::KeyError error_code,
                                   int system_code) {
  render_loop_->PostTask(
      FROM_HERE, base::Bind(&WebMediaPlayerProxy::KeyErrorTask, this,
                            key_system, session_id, error_code, system_code));
}

void WebMediaPlayerProxy::KeyMessage(const std::string& key_system,
                                     const std::string& session_id,
                                     const std::string& message,
                                     const std::string& default_url) {
#if defined(_DEBUG)
  std::string hex = base::HexEncode(message.data(), message.size());
  DLOG(INFO) << "DRM Key Request: " << hex;
#endif

  render_loop_->PostTask(
      FROM_HERE, base::Bind(&WebMediaPlayerProxy::KeyMessageTask, this,
                            key_system, session_id, message, default_url));
}

void WebMediaPlayerProxy::NeedKey(const std::string& key_system,
                                  const std::string& session_id,
                                  const std::string& type,
                                  scoped_array<uint8> init_data,
                                  int init_data_size) {
  render_loop_->PostTask(
      FROM_HERE,
      base::Bind(&WebMediaPlayerProxy::NeedKeyTask, this, key_system,
                 session_id, type, base::Passed(&init_data), init_data_size));
}

void WebMediaPlayerProxy::KeyAddedTask(const std::string& key_system,
                                       const std::string& session_id) {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (webmediaplayer_)
    webmediaplayer_->OnKeyAdded(key_system, session_id);
}

void WebMediaPlayerProxy::KeyErrorTask(const std::string& key_system,
                                       const std::string& session_id,
                                       Decryptor::KeyError error_code,
                                       int system_code) {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (webmediaplayer_)
    webmediaplayer_->OnKeyError(key_system, session_id, error_code,
                                system_code);
}

void WebMediaPlayerProxy::KeyMessageTask(const std::string& key_system,
                                         const std::string& session_id,
                                         const std::string& message,
                                         const std::string& default_url) {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (webmediaplayer_) {
    const GURL default_url_gurl(default_url);
    DLOG_IF(WARNING, !default_url.empty() && !default_url_gurl.is_valid())
        << "Invalid URL in default_url: " << default_url;
    webmediaplayer_->OnKeyMessage(key_system, session_id, message,
                                  default_url_gurl);
  }
}

void WebMediaPlayerProxy::NeedKeyTask(const std::string& key_system,
                                      const std::string& session_id,
                                      const std::string& type,
                                      scoped_array<uint8> init_data,
                                      int init_data_size) {
  DCHECK(render_loop_->BelongsToCurrentThread());
  if (webmediaplayer_)
    webmediaplayer_->OnNeedKey(key_system, session_id, type, init_data.Pass(),
                               init_data_size);
}

}  // namespace media
