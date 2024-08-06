// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_RENDERERS_REMOTE_PLAYBACK_CLIENT_WRAPPER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_RENDERERS_REMOTE_PLAYBACK_CLIENT_WRAPPER_H_

namespace media_m96 {

// Wraps a WebRemotePlaybackClient to expose only the methods used by the
// FlingingRendererClientFactory. This avoids dependencies on the blink layer.
class RemotePlaybackClientWrapper {
 public:
  RemotePlaybackClientWrapper() = default;
  virtual ~RemotePlaybackClientWrapper() = default;

  virtual std::string GetActivePresentationId() = 0;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_RENDERERS_REMOTE_PLAYBACK_CLIENT_WRAPPER_H_
