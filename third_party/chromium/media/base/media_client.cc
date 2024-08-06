// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/base/media_client.h"


namespace media_m96 {

static MediaClient* g_media_client = nullptr;

void SetMediaClient(MediaClient* media_client) {
  g_media_client = media_client;
}

MediaClient* GetMediaClient() {
  return g_media_client;
}

MediaClient::MediaClient() = default;

MediaClient::~MediaClient() = default;

}  // namespace media_m96
