// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/demuxer_stream_provider.h"

namespace media {

DemuxerStreamProvider::DemuxerStreamProvider() {}

DemuxerStreamProvider::~DemuxerStreamProvider() {}

GURL DemuxerStreamProvider::GetUrl() const {
  NOTREACHED();
  return GURL();
}

DemuxerStreamProvider::Type DemuxerStreamProvider::GetType() const {
  return STREAM;
}

}  // namespace media
