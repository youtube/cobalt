// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/cros_speech_recognition_service.h"
#include "chrome/browser/speech/speech_recognition_service.h"

// static
speech::SpeechRecognitionService*
CrosSpeechRecognitionServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<speech::SpeechRecognitionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrosSpeechRecognitionServiceFactory*
CrosSpeechRecognitionServiceFactory::GetInstanceForTest() {
  return GetInstance();
}

// static
CrosSpeechRecognitionServiceFactory*
CrosSpeechRecognitionServiceFactory::GetInstance() {
  static base::NoDestructor<CrosSpeechRecognitionServiceFactory> instance;
  return instance.get();
}

CrosSpeechRecognitionServiceFactory::CrosSpeechRecognitionServiceFactory()
    : ProfileKeyedServiceFactory(
          "SpeechRecognitionService",
          // Incognito profiles should use their own instance of the browser
          // context.
          ProfileSelections::BuildForRegularAndIncognito()) {}

CrosSpeechRecognitionServiceFactory::~CrosSpeechRecognitionServiceFactory() =
    default;

KeyedService* CrosSpeechRecognitionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new speech::CrosSpeechRecognitionService(context);
}

// static
void CrosSpeechRecognitionServiceFactory::EnsureFactoryBuilt() {
  GetInstance();
}
