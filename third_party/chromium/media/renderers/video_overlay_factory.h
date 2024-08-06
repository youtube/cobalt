// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_RENDERERS_VIDEO_OVERLAY_FACTORY_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_RENDERERS_VIDEO_OVERLAY_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "third_party/chromium/media/base/media_export.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace media_m96 {

class VideoFrame;

// Creates video overlay frames - native textures that get turned into
// transparent holes in the browser compositor using overlay system.
class MEDIA_EXPORT VideoOverlayFactory {
 public:
  VideoOverlayFactory();

  VideoOverlayFactory(const VideoOverlayFactory&) = delete;
  VideoOverlayFactory& operator=(const VideoOverlayFactory&) = delete;

  ~VideoOverlayFactory();

  scoped_refptr<::media_m96::VideoFrame> CreateFrame(const gfx::Size& size);
  const base::UnguessableToken& overlay_plane_id() const {
    return overlay_plane_id_;
  }

 private:
  // |overlay_plane_id_| identifies the instances of VideoOverlayFactory.
  const base::UnguessableToken overlay_plane_id_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_RENDERERS_VIDEO_OVERLAY_FACTORY_H_