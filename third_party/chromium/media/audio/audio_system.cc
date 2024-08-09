// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/audio/audio_system.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "third_party/chromium/media/audio/audio_device_description.h"

namespace media_m96 {

// static
AudioSystem::OnDeviceDescriptionsCallback
AudioSystem::WrapCallbackWithDeviceNameLocalization(
    OnDeviceDescriptionsCallback callback) {
  return base::BindOnce(
      [](OnDeviceDescriptionsCallback cb,
         media_m96::AudioDeviceDescriptions descriptions) {
        media_m96::AudioDeviceDescription::LocalizeDeviceDescriptions(
            &descriptions);
        std::move(cb).Run(std::move(descriptions));
      },
      std::move(callback));
}

}  // namespace media_m96
