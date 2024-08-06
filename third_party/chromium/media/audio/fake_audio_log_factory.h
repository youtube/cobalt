// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_FAKE_AUDIO_LOG_FACTORY_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_FAKE_AUDIO_LOG_FACTORY_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "third_party/chromium/media/audio/audio_logging.h"
#include "third_party/chromium/media/base/media_export.h"

namespace media_m96 {

// Creates stub AudioLog instances, for testing, which do nothing.
class MEDIA_EXPORT FakeAudioLogFactory : public AudioLogFactory {
 public:
  FakeAudioLogFactory();

  FakeAudioLogFactory(const FakeAudioLogFactory&) = delete;
  FakeAudioLogFactory& operator=(const FakeAudioLogFactory&) = delete;

  ~FakeAudioLogFactory() override;
  std::unique_ptr<AudioLog> CreateAudioLog(AudioComponent component,
                                           int component_id) override;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_FAKE_AUDIO_LOG_FACTORY_H_
