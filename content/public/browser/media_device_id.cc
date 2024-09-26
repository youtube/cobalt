// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/public/browser/media_device_id.h"

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "media/audio/audio_device_description.h"

namespace content {

std::string GetHMACForMediaDeviceID(const std::string& salt,
                                    const url::Origin& security_origin,
                                    const std::string& raw_unique_id) {
  return MediaStreamManager::GetHMACForMediaDeviceID(salt, security_origin,
                                                     raw_unique_id);
}

bool DoesMediaDeviceIDMatchHMAC(const std::string& salt,
                                const url::Origin& security_origin,
                                const std::string& device_guid,
                                const std::string& raw_unique_id) {
  return MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
      salt, security_origin, device_guid, raw_unique_id);
}

void GetMediaDeviceIDForHMAC(
    blink::mojom::MediaStreamType stream_type,
    std::string salt,
    url::Origin security_origin,
    std::string hmac_device_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceCallback<void(const absl::optional<std::string>&)> callback) {
  MediaStreamManager::GetMediaDeviceIDForHMAC(
      stream_type, std::move(salt), std::move(security_origin),
      std::move(hmac_device_id), task_runner, std::move(callback));
}

bool IsValidDeviceId(const std::string& device_id) {
  constexpr int hash_size = 64;  // 32 bytes * 2 char/byte hex encoding
  if (media::AudioDeviceDescription::IsDefaultDevice(device_id) ||
      device_id == media::AudioDeviceDescription::kCommunicationsDeviceId)
    return true;

  if (device_id.length() != hash_size)
    return false;

  return base::ranges::all_of(device_id, [](const char& c) {
    return base::IsAsciiLower(c) || base::IsAsciiDigit(c);
  });
}

}  // namespace content
