// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/base/android/media_crypto_context_impl.h"

#include "third_party/chromium/media/base/android/media_drm_bridge.h"

namespace media_m96 {

MediaCryptoContextImpl::MediaCryptoContextImpl(MediaDrmBridge* media_drm_bridge)
    : media_drm_bridge_(media_drm_bridge) {
  DCHECK(media_drm_bridge_);
}

MediaCryptoContextImpl::~MediaCryptoContextImpl() = default;

void MediaCryptoContextImpl::SetMediaCryptoReadyCB(
    MediaCryptoReadyCB media_crypto_ready_cb) {
  media_drm_bridge_->SetMediaCryptoReadyCB(std::move(media_crypto_ready_cb));
}

}  // namespace media_m96
