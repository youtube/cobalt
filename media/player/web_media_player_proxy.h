// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PLAYER_WEB_MEDIA_PLAYER_PROXY_H_
#define MEDIA_PLAYER_WEB_MEDIA_PLAYER_PROXY_H_

#include <list>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "media/base/decryptor_client.h"
#include "media/base/pipeline.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/skcanvas_video_renderer.h"
#include "media/player/buffered_data_source.h"
#include "ui/gfx/rect.h"

class SkCanvas;

namespace base {
class MessageLoopProxy;
}

namespace media {

class VideoFrame;
class VideoRendererBase;
class WebMediaPlayerImpl;

// Acts as a thread proxy between the various threads used for multimedia and
// the render thread that WebMediaPlayerImpl is running on.
class WebMediaPlayerProxy
    : public base::RefCountedThreadSafe<WebMediaPlayerProxy>,
      public DecryptorClient {
 public:
  WebMediaPlayerProxy(const scoped_refptr<base::MessageLoopProxy>& render_loop,
                      WebMediaPlayerImpl* webmediaplayer);
  BufferedDataSource* data_source() { return data_source_.get(); }
  void set_data_source(scoped_ptr<BufferedDataSource> data_source) {
    data_source_ = data_source.Pass();
  }

  // TODO(scherkus): remove this once VideoRendererBase::PaintCB passes
  // ownership of the VideoFrame http://crbug.com/108435
  void set_frame_provider(VideoRendererBase* frame_provider) {
    frame_provider_ = frame_provider;
  }

  // Methods for WebMediaPlayerImpl -> Filter communication.
  void Paint(SkCanvas* canvas, const gfx::Rect& dest_rect, uint8_t alpha);
  void Detach();
  void GetCurrentFrame(scoped_refptr<VideoFrame>* frame_out);
  void PutCurrentFrame(scoped_refptr<VideoFrame> frame);
  bool HasSingleOrigin();
  bool DidPassCORSAccessCheck() const;

  void AbortDataSource();

  // DecryptorClient implementation.
  virtual void KeyAdded(const std::string& key_system,
                        const std::string& session_id) override;
  virtual void KeyError(const std::string& key_system,
                        const std::string& session_id,
                        Decryptor::KeyError error_code,
                        int system_code) override;
  virtual void KeyMessage(const std::string& key_system,
                          const std::string& session_id,
                          const std::string& message,
                          const std::string& default_url) override;
  virtual void NeedKey(const std::string& key_system,
                       const std::string& session_id,
                       const std::string& type,
                       scoped_array<uint8> init_data,
                       int init_data_size) override;

 private:
  friend class base::RefCountedThreadSafe<WebMediaPlayerProxy>;
  virtual ~WebMediaPlayerProxy();

  // Notify |webmediaplayer_| that a key has been added.
  void KeyAddedTask(const std::string& key_system,
                    const std::string& session_id);

  // Notify |webmediaplayer_| that a key error occurred.
  void KeyErrorTask(const std::string& key_system,
                    const std::string& session_id,
                    Decryptor::KeyError error_code,
                    int system_code);

  // Notify |webmediaplayer_| that a key message has been generated.
  void KeyMessageTask(const std::string& key_system,
                      const std::string& session_id,
                      const std::string& message,
                      const std::string& default_url);

  // Notify |webmediaplayer_| that a key is needed for decryption.
  void NeedKeyTask(const std::string& key_system,
                   const std::string& session_id,
                   const std::string& type,
                   scoped_array<uint8> init_data,
                   int init_data_size);

  // The render message loop where the renderer lives.
  scoped_refptr<base::MessageLoopProxy> render_loop_;
  WebMediaPlayerImpl* webmediaplayer_;

  scoped_ptr<BufferedDataSource> data_source_;
  scoped_refptr<VideoRendererBase> frame_provider_;
  SkCanvasVideoRenderer video_renderer_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMediaPlayerProxy);
};

}  // namespace media

#endif  // MEDIA_PLAYER_WEB_MEDIA_PLAYER_PROXY_H_
