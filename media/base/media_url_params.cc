// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_url_params.h"

namespace media {

MediaUrlParams::MediaUrlParams(const GURL& media_url,
                               const net::SiteForCookies& site_for_cookies,
                               const url::Origin& top_frame_origin,
                               bool allow_credentials,
                               bool is_hls)
    : media_url(media_url),
      site_for_cookies(site_for_cookies),
      top_frame_origin(top_frame_origin),
      allow_credentials(allow_credentials),
      is_hls(is_hls) {}

MediaUrlParams::MediaUrlParams(const MediaUrlParams& other) = default;

MediaUrlParams::~MediaUrlParams() = default;
}  // namespace media
