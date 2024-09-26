// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_test_helper.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/fake_speech_recognition_service.h"
#include "chrome/browser/speech/speech_recognition_constants.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"

SpeechRecognitionTestHelper::SpeechRecognitionTestHelper(
    speech::SpeechRecognitionType type)
    : type_(type) {}
SpeechRecognitionTestHelper::~SpeechRecognitionTestHelper() = default;

void SpeechRecognitionTestHelper::SetUp(Profile* profile) {
  if (type_ == speech::SpeechRecognitionType::kNetwork)
    SetUpNetworkRecognition();
  else
    SetUpOnDeviceRecognition(profile);
}

void SpeechRecognitionTestHelper::SetUpNetworkRecognition() {
  fake_speech_recognition_manager_ =
      std::make_unique<content::FakeSpeechRecognitionManager>();
  fake_speech_recognition_manager_->set_should_send_fake_response(false);
  content::SpeechRecognitionManager::SetManagerForTesting(
      fake_speech_recognition_manager_.get());
}

void SpeechRecognitionTestHelper::SetUpOnDeviceRecognition(Profile* profile) {
  // Fake that SODA is installed so SpeechRecognitionPrivate uses
  // OnDeviceSpeechRecognizer.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  CrosSpeechRecognitionServiceFactory::GetInstanceForTest()
      ->SetTestingFactoryAndUse(
          profile,
          base::BindRepeating(&SpeechRecognitionTestHelper::
                                  CreateTestOnDeviceSpeechRecognitionService,
                              base::Unretained(this)));
}

std::unique_ptr<KeyedService>
SpeechRecognitionTestHelper::CreateTestOnDeviceSpeechRecognitionService(
    content::BrowserContext* context) {
  std::unique_ptr<speech::FakeSpeechRecognitionService> fake_service =
      std::make_unique<speech::FakeSpeechRecognitionService>();
  fake_service_ = fake_service.get();
  return std::move(fake_service);
}

void SpeechRecognitionTestHelper::WaitForRecognitionStarted() {
  // Only wait for recognition to start if it hasn't been started yet.
  if (type_ == speech::SpeechRecognitionType::kNetwork &&
      !fake_speech_recognition_manager_->is_recognizing()) {
    fake_speech_recognition_manager_->WaitForRecognitionStarted();
  } else if (type_ == speech::SpeechRecognitionType::kOnDevice &&
             !fake_service_->is_capturing_audio()) {
    fake_service_->WaitForRecognitionStarted();
  }
  base::RunLoop().RunUntilIdle();
}

void SpeechRecognitionTestHelper::WaitForRecognitionStopped() {
  // Only wait for recognition to stop if it hasn't been stopped yet.
  if (type_ == speech::SpeechRecognitionType::kNetwork &&
      fake_speech_recognition_manager_->is_recognizing()) {
    fake_speech_recognition_manager_->WaitForRecognitionEnded();
  }
  base::RunLoop().RunUntilIdle();
}

void SpeechRecognitionTestHelper::SendInterimResultAndWait(
    const std::string& transcript) {
  SendFakeSpeechResultAndWait(transcript, /*is_final=*/false);
}

void SpeechRecognitionTestHelper::SendFinalResultAndWait(
    const std::string& transcript) {
  SendFakeSpeechResultAndWait(transcript, /*is_final=*/true);
}

void SpeechRecognitionTestHelper::SendFakeSpeechResultAndWait(
    const std::string& transcript,
    bool is_final) {
  base::RunLoop loop;
  if (type_ == speech::SpeechRecognitionType::kNetwork) {
    fake_speech_recognition_manager_->SetFakeResult(transcript, is_final);
    fake_speech_recognition_manager_->SendFakeResponse(
        false /* end recognition */, loop.QuitClosure());
    loop.Run();
  } else {
    DCHECK(fake_service_->is_capturing_audio());
    fake_service_->SendSpeechRecognitionResult(
        media::SpeechRecognitionResult(transcript, is_final));
    loop.RunUntilIdle();
  }
}

void SpeechRecognitionTestHelper::SendErrorAndWait() {
  base::RunLoop loop;
  if (type_ == speech::SpeechRecognitionType::kNetwork) {
    fake_speech_recognition_manager_->SendFakeError(loop.QuitClosure());
    loop.Run();
  } else {
    fake_service_->SendSpeechRecognitionError();
    loop.RunUntilIdle();
  }
}

std::vector<base::test::FeatureRef>
SpeechRecognitionTestHelper::GetEnabledFeatures() {
  std::vector<base::test::FeatureRef> features;
  if (type_ == speech::SpeechRecognitionType::kOnDevice)
    features.push_back(ash::features::kOnDeviceSpeechRecognition);
  return features;
}

std::vector<base::test::FeatureRef>
SpeechRecognitionTestHelper::GetDisabledFeatures() {
  std::vector<base::test::FeatureRef> features;
  if (type_ == speech::SpeechRecognitionType::kNetwork)
    features.push_back(ash::features::kOnDeviceSpeechRecognition);
  return features;
}
