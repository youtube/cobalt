// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/demuxer.h"

#include "base/logging.h"

namespace media {

DemuxerHost::~DemuxerHost() {}

Demuxer::Demuxer() {}

Demuxer::~Demuxer() {}

void Demuxer::SetPlaybackRate(float playback_rate) {}

void Demuxer::Seek(base::TimeDelta time, const PipelineStatusCB& status_cb) {
  DCHECK(!status_cb.is_null());
  status_cb.Run(PIPELINE_OK);
}

void Demuxer::Stop(const base::Closure& callback) {
  DCHECK(!callback.is_null());
  callback.Run();
}

void Demuxer::OnAudioRendererDisabled() {}

}  // namespace media
