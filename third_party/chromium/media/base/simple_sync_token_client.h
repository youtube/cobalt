// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_SIMPLE_SYNC_TOKEN_CLIENT_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_SIMPLE_SYNC_TOKEN_CLIENT_H_

#include "base/macros.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/chromium/media/base/media_export.h"
#include "third_party/chromium/media/base/video_frame.h"

namespace media_m96 {

class MEDIA_EXPORT SimpleSyncTokenClient : public VideoFrame::SyncTokenClient {
 public:
  explicit SimpleSyncTokenClient(const gpu::SyncToken& sync_token);
  void GenerateSyncToken(gpu::SyncToken* sync_token) final;
  void WaitSyncToken(const gpu::SyncToken& sync_token) final;

 private:
  gpu::SyncToken sync_token_;

  DISALLOW_COPY_AND_ASSIGN(SimpleSyncTokenClient);
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_SIMPLE_SYNC_TOKEN_CLIENT_H_
