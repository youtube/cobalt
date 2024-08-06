// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_WINDOWS_OUTPUT_WITH_RELEASE_MAILBOX_CB_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_WINDOWS_OUTPUT_WITH_RELEASE_MAILBOX_CB_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "third_party/chromium/media/base/video_frame.h"

namespace media_m96 {

// This is soon to be deprecated in favor of VideoFrame destruction CBs.
// Please do not use this.

namespace deprecated {
// Similar to VideoFrame::ReleaseMailboxCB for now.
using ReleaseMailboxCB = base::OnceCallback<void(const gpu::SyncToken&)>;
using OutputWithReleaseMailboxCB =
    base::RepeatingCallback<void(ReleaseMailboxCB, scoped_refptr<VideoFrame>)>;
}  // namespace deprecated

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_WINDOWS_OUTPUT_WITH_RELEASE_MAILBOX_CB_H_
