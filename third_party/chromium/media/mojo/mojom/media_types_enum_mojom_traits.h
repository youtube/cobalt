// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_MEDIA_TYPES_ENUM_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_MEDIA_TYPES_ENUM_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "third_party/chromium/media/base/renderer_factory_selector.h"
#include "third_party/chromium/media/base/svc_scalability_mode.h"
#include "third_party/chromium/media/base/video_frame_metadata.h"
#include "third_party/chromium/media/base/video_transformation.h"
#include "third_party/chromium/media/mojo/mojom/media_types.mojom-shared.h"

// Most enums have automatically generated traits, in media_types.mojom.h, due
// to their [native] attribute. This file defines traits for enums that are used
// in files that cannot directly include media_types.mojom.h.

namespace mojo {

template <>
struct EnumTraits<media_m96::mojom::CdmSessionClosedReason,
                  ::media_m96::CdmSessionClosedReason> {
  static media_m96::mojom::CdmSessionClosedReason ToMojom(
      ::media_m96::CdmSessionClosedReason input) {
    switch (input) {
      case ::media_m96::CdmSessionClosedReason::kInternalError:
        return media_m96::mojom::CdmSessionClosedReason::kInternalError;
      case ::media_m96::CdmSessionClosedReason::kClose:
        return media_m96::mojom::CdmSessionClosedReason::kClose;
      case ::media_m96::CdmSessionClosedReason::kReleaseAcknowledged:
        return media_m96::mojom::CdmSessionClosedReason::kReleaseAcknowledged;
      case ::media_m96::CdmSessionClosedReason::kHardwareContextReset:
        return media_m96::mojom::CdmSessionClosedReason::kHardwareContextReset;
      case ::media_m96::CdmSessionClosedReason::kResourceEvicted:
        return media_m96::mojom::CdmSessionClosedReason::kResourceEvicted;
    }

    NOTREACHED();
    return static_cast<media_m96::mojom::CdmSessionClosedReason>(input);
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(media_m96::mojom::CdmSessionClosedReason input,
                        ::media_m96::CdmSessionClosedReason* output) {
    switch (input) {
      case media_m96::mojom::CdmSessionClosedReason::kInternalError:
        *output = ::media_m96::CdmSessionClosedReason::kInternalError;
        return true;
      case media_m96::mojom::CdmSessionClosedReason::kClose:
        *output = ::media_m96::CdmSessionClosedReason::kClose;
        return true;
      case media_m96::mojom::CdmSessionClosedReason::kReleaseAcknowledged:
        *output = ::media_m96::CdmSessionClosedReason::kReleaseAcknowledged;
        return true;
      case media_m96::mojom::CdmSessionClosedReason::kHardwareContextReset:
        *output = ::media_m96::CdmSessionClosedReason::kHardwareContextReset;
        return true;
      case media_m96::mojom::CdmSessionClosedReason::kResourceEvicted:
        *output = ::media_m96::CdmSessionClosedReason::kResourceEvicted;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<media_m96::mojom::EncryptionType, ::media_m96::EncryptionType> {
  static media_m96::mojom::EncryptionType ToMojom(::media_m96::EncryptionType input) {
    switch (input) {
      case ::media_m96::EncryptionType::kNone:
        return media_m96::mojom::EncryptionType::kNone;
      case ::media_m96::EncryptionType::kClear:
        return media_m96::mojom::EncryptionType::kClear;
      case ::media_m96::EncryptionType::kEncrypted:
        return media_m96::mojom::EncryptionType::kEncrypted;
      case ::media_m96::EncryptionType::kEncryptedWithClearLead:
        return media_m96::mojom::EncryptionType::kEncryptedWithClearLead;
    }

    NOTREACHED();
    return static_cast<media_m96::mojom::EncryptionType>(input);
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(media_m96::mojom::EncryptionType input,
                        ::media_m96::EncryptionType* output) {
    switch (input) {
      case media_m96::mojom::EncryptionType::kNone:
        *output = ::media_m96::EncryptionType::kNone;
        return true;
      case media_m96::mojom::EncryptionType::kClear:
        *output = ::media_m96::EncryptionType::kClear;
        return true;
      case media_m96::mojom::EncryptionType::kEncrypted:
        *output = ::media_m96::EncryptionType::kEncrypted;
        return true;
      case media_m96::mojom::EncryptionType::kEncryptedWithClearLead:
        *output = ::media_m96::EncryptionType::kEncryptedWithClearLead;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<media_m96::mojom::SVCScalabilityMode, media_m96::SVCScalabilityMode> {
  static media_m96::mojom::SVCScalabilityMode ToMojom(
      media_m96::SVCScalabilityMode input) {
    switch (input) {
      case media_m96::SVCScalabilityMode::kL1T2:
        return media_m96::mojom::SVCScalabilityMode::kL1T2;
      case media_m96::SVCScalabilityMode::kL1T3:
        return media_m96::mojom::SVCScalabilityMode::kL1T3;
      case media_m96::SVCScalabilityMode::kL2T2Key:
        return media_m96::mojom::SVCScalabilityMode::kL2T2Key;
      case media_m96::SVCScalabilityMode::kL2T3Key:
        return media_m96::mojom::SVCScalabilityMode::kL2T3Key;
      case media_m96::SVCScalabilityMode::kL3T2Key:
        return media_m96::mojom::SVCScalabilityMode::kL3T2Key;
      case media_m96::SVCScalabilityMode::kL3T3Key:
        return media_m96::mojom::SVCScalabilityMode::kL3T3Key;
      case media_m96::SVCScalabilityMode::kL2T1:
      case media_m96::SVCScalabilityMode::kL2T2:
      case media_m96::SVCScalabilityMode::kL2T3:
      case media_m96::SVCScalabilityMode::kL3T1:
      case media_m96::SVCScalabilityMode::kL3T2:
      case media_m96::SVCScalabilityMode::kL3T3:
      case media_m96::SVCScalabilityMode::kL2T1h:
      case media_m96::SVCScalabilityMode::kL2T2h:
      case media_m96::SVCScalabilityMode::kL2T3h:
      case media_m96::SVCScalabilityMode::kS2T1:
      case media_m96::SVCScalabilityMode::kS2T2:
      case media_m96::SVCScalabilityMode::kS2T3:
      case media_m96::SVCScalabilityMode::kS2T1h:
      case media_m96::SVCScalabilityMode::kS2T2h:
      case media_m96::SVCScalabilityMode::kS2T3h:
      case media_m96::SVCScalabilityMode::kS3T1:
      case media_m96::SVCScalabilityMode::kS3T2:
      case media_m96::SVCScalabilityMode::kS3T3:
      case media_m96::SVCScalabilityMode::kS3T1h:
      case media_m96::SVCScalabilityMode::kS3T2h:
      case media_m96::SVCScalabilityMode::kS3T3h:
      case media_m96::SVCScalabilityMode::kL2T2KeyShift:
      case media_m96::SVCScalabilityMode::kL2T3KeyShift:
      case media_m96::SVCScalabilityMode::kL3T2KeyShift:
      case media_m96::SVCScalabilityMode::kL3T3KeyShift:
        NOTREACHED();
        return media_m96::mojom::SVCScalabilityMode::kUnsupportedMode;
    }
  }

  static bool FromMojom(media_m96::mojom::SVCScalabilityMode input,
                        media_m96::SVCScalabilityMode* output) {
    switch (input) {
      case media_m96::mojom::SVCScalabilityMode::kUnsupportedMode:
        NOTREACHED();
        return false;
      case media_m96::mojom::SVCScalabilityMode::kL1T2:
        *output = media_m96::SVCScalabilityMode::kL1T2;
        return true;
      case media_m96::mojom::SVCScalabilityMode::kL1T3:
        *output = media_m96::SVCScalabilityMode::kL1T3;
        return true;
      case media_m96::mojom::SVCScalabilityMode::kL2T2Key:
        *output = media_m96::SVCScalabilityMode::kL2T2Key;
        return true;
      case media_m96::mojom::SVCScalabilityMode::kL2T3Key:
        *output = media_m96::SVCScalabilityMode::kL2T3Key;
        return true;
      case media_m96::mojom::SVCScalabilityMode::kL3T2Key:
        *output = media_m96::SVCScalabilityMode::kL3T2Key;
        return true;
      case media_m96::mojom::SVCScalabilityMode::kL3T3Key:
        *output = media_m96::SVCScalabilityMode::kL3T3Key;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<media_m96::mojom::VideoRotation, ::media_m96::VideoRotation> {
  static media_m96::mojom::VideoRotation ToMojom(::media_m96::VideoRotation input) {
    switch (input) {
      case ::media_m96::VideoRotation::VIDEO_ROTATION_0:
        return media_m96::mojom::VideoRotation::kVideoRotation0;
      case ::media_m96::VideoRotation::VIDEO_ROTATION_90:
        return media_m96::mojom::VideoRotation::kVideoRotation90;
      case ::media_m96::VideoRotation::VIDEO_ROTATION_180:
        return media_m96::mojom::VideoRotation::kVideoRotation180;
      case ::media_m96::VideoRotation::VIDEO_ROTATION_270:
        return media_m96::mojom::VideoRotation::kVideoRotation270;
    }

    NOTREACHED();
    return static_cast<media_m96::mojom::VideoRotation>(input);
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(media_m96::mojom::VideoRotation input,
                        media_m96::VideoRotation* output) {
    switch (input) {
      case media_m96::mojom::VideoRotation::kVideoRotation0:
        *output = ::media_m96::VideoRotation::VIDEO_ROTATION_0;
        return true;
      case media_m96::mojom::VideoRotation::kVideoRotation90:
        *output = ::media_m96::VideoRotation::VIDEO_ROTATION_90;
        return true;
      case media_m96::mojom::VideoRotation::kVideoRotation180:
        *output = ::media_m96::VideoRotation::VIDEO_ROTATION_180;
        return true;
      case media_m96::mojom::VideoRotation::kVideoRotation270:
        *output = ::media_m96::VideoRotation::VIDEO_ROTATION_270;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<media_m96::mojom::CopyMode,
                  ::media_m96::VideoFrameMetadata::CopyMode> {
  static media_m96::mojom::CopyMode ToMojom(
      ::media_m96::VideoFrameMetadata::CopyMode input) {
    switch (input) {
      case ::media_m96::VideoFrameMetadata::CopyMode::kCopyToNewTexture:
        return media_m96::mojom::CopyMode::kCopyToNewTexture;
      case ::media_m96::VideoFrameMetadata::CopyMode::kCopyMailboxesOnly:
        return media_m96::mojom::CopyMode::kCopyMailboxesOnly;
    }

    NOTREACHED();
    return static_cast<media_m96::mojom::CopyMode>(input);
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(media_m96::mojom::CopyMode input,
                        media_m96::VideoFrameMetadata::CopyMode* output) {
    switch (input) {
      case media_m96::mojom::CopyMode::kCopyToNewTexture:
        *output = ::media_m96::VideoFrameMetadata::CopyMode::kCopyToNewTexture;
        return true;
      case media_m96::mojom::CopyMode::kCopyMailboxesOnly:
        *output = ::media_m96::VideoFrameMetadata::CopyMode::kCopyMailboxesOnly;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<media_m96::mojom::RendererType, ::media_m96::RendererType> {
  static media_m96::mojom::RendererType ToMojom(::media_m96::RendererType input) {
    switch (input) {
      case ::media_m96::RendererType::kDefault:
        return media_m96::mojom::RendererType::kDefault;
      case ::media_m96::RendererType::kMojo:
        return media_m96::mojom::RendererType::kMojo;
      case ::media_m96::RendererType::kMediaPlayer:
        return media_m96::mojom::RendererType::kMediaPlayer;
      case ::media_m96::RendererType::kCourier:
        return media_m96::mojom::RendererType::kCourier;
      case ::media_m96::RendererType::kFlinging:
        return media_m96::mojom::RendererType::kFlinging;
      case ::media_m96::RendererType::kCast:
        return media_m96::mojom::RendererType::kCast;
      case ::media_m96::RendererType::kMediaFoundation:
        return media_m96::mojom::RendererType::kMediaFoundation;
      case ::media_m96::RendererType::kFuchsia:
        return media_m96::mojom::RendererType::kFuchsia;
      case ::media_m96::RendererType::kRemoting:
        return media_m96::mojom::RendererType::kRemoting;
      case ::media_m96::RendererType::kCastStreaming:
        return media_m96::mojom::RendererType::kCastStreaming;
    }

    NOTREACHED();
    return static_cast<media_m96::mojom::RendererType>(input);
  }

  // Returning false results in deserialization failure and causes the
  // message pipe receiving it to be disconnected.
  static bool FromMojom(media_m96::mojom::RendererType input,
                        ::media_m96::RendererType* output) {
    switch (input) {
      case media_m96::mojom::RendererType::kDefault:
        *output = ::media_m96::RendererType::kDefault;
        return true;
      case media_m96::mojom::RendererType::kMojo:
        *output = ::media_m96::RendererType::kMojo;
        return true;
      case media_m96::mojom::RendererType::kMediaPlayer:
        *output = ::media_m96::RendererType::kMediaPlayer;
        return true;
      case media_m96::mojom::RendererType::kCourier:
        *output = ::media_m96::RendererType::kCourier;
        return true;
      case media_m96::mojom::RendererType::kFlinging:
        *output = ::media_m96::RendererType::kFlinging;
        return true;
      case media_m96::mojom::RendererType::kCast:
        *output = ::media_m96::RendererType::kCast;
        return true;
      case media_m96::mojom::RendererType::kMediaFoundation:
        *output = ::media_m96::RendererType::kMediaFoundation;
        return true;
      case media_m96::mojom::RendererType::kFuchsia:
        *output = ::media_m96::RendererType::kFuchsia;
        return true;
      case media_m96::mojom::RendererType::kRemoting:
        *output = ::media_m96::RendererType::kRemoting;
        return true;
      case media_m96::mojom::RendererType::kCastStreaming:
        *output = ::media_m96::RendererType::kCastStreaming;
        return true;
    }

    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_MEDIA_TYPES_ENUM_MOJOM_TRAITS_H_
