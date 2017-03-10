// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/media_client.h"

#include "base/logging.h"

namespace cobalt {
namespace media {

static MediaClient* g_media_client = NULL;

void SetMediaClient(MediaClient* media_client) {
  g_media_client = media_client;
}

MediaClient* GetMediaClient() { return g_media_client; }

KeySystemInfoForUMA::KeySystemInfoForUMA(
    const std::string& key_system, const std::string& key_system_name_for_uma)
    : key_system(key_system),
      key_system_name_for_uma(key_system_name_for_uma) {}

KeySystemInfoForUMA::~KeySystemInfoForUMA() {}

MediaClient::MediaClient() {}

MediaClient::~MediaClient() {}

}  // namespace media
}  // namespace cobalt
