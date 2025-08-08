// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_ACCELERATED_IMAGE_INFO_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_ACCELERATED_IMAGE_INFO_H_

#include "base/functional/callback.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

// This struct represents all the information needed to create an
// AcceleratedStaticImageBitmap in the receiving process.
// See third_party/blink/public/mojom/messaging/static_bitmap_image.mojom
// for details.
struct BLINK_COMMON_EXPORT AcceleratedImageInfo {
  gpu::MailboxHolder mailbox_holder;
  uint32_t usage;
  SkImageInfo image_info;
  bool is_origin_top_left;
  bool supports_display_compositing;
  bool is_overlay_candidate;
  base::OnceCallback<void(const gpu::SyncToken& sync_token)> release_callback;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_ACCELERATED_IMAGE_INFO_H_
