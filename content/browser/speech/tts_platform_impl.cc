// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/tts_platform_impl.h"

#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

#include <string>

#include "build/chromeos_buildflags.h"

namespace content {

// static
TtsPlatform* TtsPlatform::GetInstance() {
#if !defined(NO_CONTENT_CLIENT)
  TtsPlatform* result = GetContentClient()->browser()->GetTtsPlatform();
  if (result)
    return result;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // On Chrome OS, the platform TTS definition is provided by the content
  // client.
  //
  // If this code is reached in production it implies that somebody is
  // trying to do TTS on a platform where the content client implementation
  // is not provided, that's probably not intended. It's not important
  // if this is hit in something like a content-only unit test.
  NOTREACHED();
  return nullptr;
#else
  return TtsPlatformImpl::GetInstance();
#endif
}

void TtsPlatformImpl::LoadBuiltInTtsEngine(BrowserContext* browser_context) {}

void TtsPlatformImpl::WillSpeakUtteranceWithVoice(TtsUtterance* utterance,
                                                  const VoiceData& voice_data) {
}

std::string TtsPlatformImpl::GetError() {
  return error_;
}

void TtsPlatformImpl::ClearError() {
  error_ = std::string();
}

void TtsPlatformImpl::SetError(const std::string& error) {
  error_ = error;
}

void TtsPlatformImpl::Shutdown() {}

void TtsPlatformImpl::FinalizeVoiceOrdering(std::vector<VoiceData>& voices) {}

ExternalPlatformDelegate* TtsPlatformImpl::GetExternalPlatformDelegate() {
  return nullptr;
}

}  // namespace content
