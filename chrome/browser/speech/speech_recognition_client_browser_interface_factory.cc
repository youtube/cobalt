// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_client_browser_interface_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"

// static
speech::SpeechRecognitionClientBrowserInterface*
SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<speech::SpeechRecognitionClientBrowserInterface*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SpeechRecognitionClientBrowserInterfaceFactory*
SpeechRecognitionClientBrowserInterfaceFactory::GetInstance() {
  static base::NoDestructor<SpeechRecognitionClientBrowserInterfaceFactory>
      instance;
  return instance.get();
}

SpeechRecognitionClientBrowserInterfaceFactory::
    SpeechRecognitionClientBrowserInterfaceFactory()
    : ProfileKeyedServiceFactory(
          "SpeechRecognitionClientBrowserInterface",
          // Incognito profiles should use their own instance of the browser
          // context.
          ProfileSelections::BuildForRegularAndIncognito()) {
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  DependsOn(::captions::LiveCaptionControllerFactory::GetInstance());
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
}

SpeechRecognitionClientBrowserInterfaceFactory::
    ~SpeechRecognitionClientBrowserInterfaceFactory() = default;

KeyedService*
SpeechRecognitionClientBrowserInterfaceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new speech::SpeechRecognitionClientBrowserInterface(context);
}

// static
void SpeechRecognitionClientBrowserInterfaceFactory::EnsureFactoryBuilt() {
  SpeechRecognitionClientBrowserInterfaceFactory::GetInstance();
}
