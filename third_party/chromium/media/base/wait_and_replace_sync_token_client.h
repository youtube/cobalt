// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_WAIT_AND_REPLACE_SYNC_TOKEN_CLIENT_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_WAIT_AND_REPLACE_SYNC_TOKEN_CLIENT_H_

#include "base/macros.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/chromium/media/base/media_export.h"
#include "third_party/chromium/media/base/video_frame.h"

namespace gpu {
class InterfaceBase;
}

namespace media_m96 {

class MEDIA_EXPORT WaitAndReplaceSyncTokenClient
    : public VideoFrame::SyncTokenClient {
 public:
  explicit WaitAndReplaceSyncTokenClient(gpu::InterfaceBase* ib);
  void GenerateSyncToken(gpu::SyncToken* sync_token) final;
  void WaitSyncToken(const gpu::SyncToken& sync_token) final;

 private:
  gpu::InterfaceBase* ib_;

  DISALLOW_COPY_AND_ASSIGN(WaitAndReplaceSyncTokenClient);
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_WAIT_AND_REPLACE_SYNC_TOKEN_CLIENT_H_
