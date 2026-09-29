// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/demuxer.h"

namespace media {

DemuxerHost::~DemuxerHost() = default;

Demuxer::Demuxer() = default;

Demuxer::~Demuxer() = default;

// Most Demuxer implementations don't need to disable canChangeType.
// Do nothing by default.
void Demuxer::DisableCanChangeType() {}

#if BUILDFLAG(IS_IOS_TVOS) && BUILDFLAG(USE_STARBOARD_MEDIA)
void Demuxer::SetEncryptedMediaInitDataCB(EncryptedMediaInitDataCB cb) {}
#endif  // BUILDFLAG(IS_IOS_TVOS) && BUILDFLAG(USE_STARBOARD_MEDIA)

}  // namespace media
