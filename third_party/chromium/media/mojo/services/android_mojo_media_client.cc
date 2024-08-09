// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/mojo/services/android_mojo_media_client.h"

#include <utility>

#include <memory>

#include "base/bind.h"
#include "third_party/chromium/media/base/android/android_cdm_factory.h"
#include "third_party/chromium/media/base/audio_decoder.h"
#include "third_party/chromium/media/base/cdm_factory.h"
#include "third_party/chromium/media/filters/android/media_codec_audio_decoder.h"
#include "third_party/chromium/media/mojo/mojom/media_drm_storage.mojom.h"
#include "third_party/chromium/media/mojo/mojom/provision_fetcher.mojom.h"
#include "third_party/chromium/media/mojo/services/android_mojo_util.h"

using media_m96::android_mojo_util::CreateProvisionFetcher;
using media_m96::android_mojo_util::CreateMediaDrmStorage;

namespace media_m96 {

AndroidMojoMediaClient::AndroidMojoMediaClient() {}

AndroidMojoMediaClient::~AndroidMojoMediaClient() {}

// MojoMediaClient overrides.

std::unique_ptr<AudioDecoder> AndroidMojoMediaClient::CreateAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return std::make_unique<MediaCodecAudioDecoder>(task_runner);
}

std::unique_ptr<CdmFactory> AndroidMojoMediaClient::CreateCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  if (!frame_interfaces) {
    NOTREACHED() << "Host interfaces should be provided when using CDM with "
                 << "AndroidMojoMediaClient";
    return nullptr;
  }

  return std::make_unique<AndroidCdmFactory>(
      base::BindRepeating(&CreateProvisionFetcher, frame_interfaces),
      base::BindRepeating(&CreateMediaDrmStorage, frame_interfaces));
}

}  // namespace media_m96
