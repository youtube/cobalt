// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_MEDIA_CLIENT_H_
#define COBALT_MEDIA_BASE_MEDIA_CLIENT_H_

#include <string>
#include <vector>

#include "cobalt/media/base/key_system_properties.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/video_codecs.h"
#include "googleurl/src/gurl.h"

namespace media {

class MediaClient;

// Setter for the client. If a customized client is needed, it should be set
// early, before the client could possibly be used.
MEDIA_EXPORT void SetMediaClient(MediaClient* media_client);

// Media's embedder API should only be used by media.
#if defined(MEDIA_IMPLEMENTATION) || defined(MEDIA_BLINK_IMPLEMENTATION)
// Getter for the client. Returns NULL if no customized client is needed.
MEDIA_EXPORT MediaClient* GetMediaClient();
#endif

struct MEDIA_EXPORT KeySystemInfoForUMA {
  KeySystemInfoForUMA(const std::string& key_system,
                      const std::string& key_system_name_for_uma);
  ~KeySystemInfoForUMA();

  // Concrete key system name;
  std::string key_system;

  // Display name for UMA reporting. For example, the display name for
  // "org.w3.clearkey" is "ClearKey". When providing this value, make sure to
  // update tools/metrics/histograms/histograms.xml.
  std::string key_system_name_for_uma;
};

// A client interface for embedders (e.g. content/renderer) to provide
// customized service.
class MEDIA_EXPORT MediaClient {
 public:
  MediaClient();
  virtual ~MediaClient();

  // Provides UMA info for key systems that SHOULD be reported to UMA, no matter
  // whether a key system is actually supported by this client or not. Only
  // called once per instance.
  virtual void AddKeySystemsInfoForUMA(
      std::vector<KeySystemInfoForUMA>* key_systems_info_for_uma) = 0;

  // Returns whether client key systems properties should be updated.
  virtual bool IsKeySystemsUpdateNeeded() = 0;

  // Adds properties for supported key systems.
  virtual void AddSupportedKeySystems(
      std::vector<std::unique_ptr<KeySystemProperties>>*
          key_systems_properties) = 0;

  // Records a domain and registry of a url to a Rappor privacy-preserving
  // metric. See: https://www.chromium.org/developers/design-documents/rappor
  virtual void RecordRapporURL(const std::string& metric, const GURL& url) = 0;

  // Returns true if the given combination of video codec, profile and level is
  // supported. The |level| value is codec-specific.
  virtual bool IsSupportedVideoConfig(VideoCodec codec,
                                      VideoCodecProfile profile, int level) = 0;
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_MEDIA_CLIENT_H_
