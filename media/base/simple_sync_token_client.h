// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SIMPLE_SYNC_TOKEN_CLIENT_H_
#define MEDIA_BASE_SIMPLE_SYNC_TOKEN_CLIENT_H_

#include "base/macros.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/media_export.h"
#include "media/base/video_frame.h"

namespace media {

class MEDIA_EXPORT SimpleSyncTokenClient : public VideoFrame::SyncTokenClient {
 public:
  explicit SimpleSyncTokenClient(const gpu::SyncToken& sync_token);
  void GenerateSyncToken(gpu::SyncToken* sync_token) final;
  void WaitSyncToken(const gpu::SyncToken& sync_token) final;

 private:
  gpu::SyncToken sync_token_;

  DISALLOW_COPY_AND_ASSIGN(SimpleSyncTokenClient);
};

}  // namespace media

#endif  // MEDIA_BASE_SIMPLE_SYNC_TOKEN_CLIENT_H_
