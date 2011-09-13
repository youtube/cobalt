// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/demuxer.h"

#include "base/logging.h"

namespace media {

Demuxer::Demuxer() : host_(NULL) {}

Demuxer::~Demuxer() {}

void Demuxer::set_host(FilterHost* host) {
  DCHECK(host);
  DCHECK(!host_);
  host_ = host;
}

void Demuxer::SetPlaybackRate(float playback_rate) {}

void Demuxer::Seek(base::TimeDelta time, const PipelineStatusCB& callback) {
  DCHECK(!callback.is_null());
  callback.Run(PIPELINE_OK);
}

void Demuxer::Stop(FilterCallback* callback) {
  DCHECK(callback);

  callback->Run();
  delete callback;
}

void Demuxer::OnAudioRendererDisabled() {}

}  // namespace media
