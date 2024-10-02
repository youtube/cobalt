// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/cros_speech_recognition_recognizer_impl.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "chrome/services/speech/soda/cros_soda_client.h"
#include "google_apis/google_api_keys.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace speech {

namespace {
constexpr char kNoClientError[] = "No cros soda client.";

chromeos::machine_learning::mojom::SodaRecognitionMode
GetSodaSpeechRecognitionMode(
    media::mojom::SpeechRecognitionMode recognition_mode) {
  switch (recognition_mode) {
    case media::mojom::SpeechRecognitionMode::kIme:
      return chromeos::machine_learning::mojom::SodaRecognitionMode::kIme;
    case media::mojom::SpeechRecognitionMode::kCaption:
      return chromeos::machine_learning::mojom::SodaRecognitionMode::kCaption;
    case media::mojom::SpeechRecognitionMode::kUnknown:
      // Chrome OS SODA doesn't support unknown recognition type. Default to
      // caption.
      NOTREACHED();
      return chromeos::machine_learning::mojom::SodaRecognitionMode::kCaption;
  }
}
}  // namespace

void CrosSpeechRecognitionRecognizerImpl::Create(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> remote,
    media::mojom::SpeechRecognitionOptionsPtr options,
    const base::FilePath& binary_path,
    const base::flat_map<std::string, base::FilePath>& config_paths,
    const std::string& primary_language_name) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<CrosSpeechRecognitionRecognizerImpl>(
          std::move(remote), std::move(options), binary_path, config_paths,
          primary_language_name),
      std::move(receiver));
}
CrosSpeechRecognitionRecognizerImpl::~CrosSpeechRecognitionRecognizerImpl() =
    default;

CrosSpeechRecognitionRecognizerImpl::CrosSpeechRecognitionRecognizerImpl(
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> remote,
    media::mojom::SpeechRecognitionOptionsPtr options,
    const base::FilePath& binary_path,
    const base::flat_map<std::string, base::FilePath>& config_paths,
    const std::string& primary_language_name)
    : SpeechRecognitionRecognizerImpl(std::move(remote),
                                      std::move(options),
                                      binary_path,
                                      config_paths,
                                      primary_language_name),
      binary_path_(binary_path) {
  cros_soda_client_ = std::make_unique<soda::CrosSodaClient>();
}

void CrosSpeechRecognitionRecognizerImpl::
    SendAudioToSpeechRecognitionServiceInternal(
        media::mojom::AudioDataS16Ptr buffer) {
  // Soda is on, let's send the audio to it.
  int channel_count = buffer->channel_count;
  int sample_rate = buffer->sample_rate;
  size_t buffer_size = 0;
  // Verify and calculate the buffer size.
  if (!base::CheckMul(buffer->data.size(), sizeof(buffer->data[0]))
           .AssignIfValid(&buffer_size)) {
    LOG(DFATAL) << "Size check invalid.";
    return;
  }
  if (cros_soda_client_ == nullptr) {
    LOG(DFATAL) << "No soda client, stopping.";
    mojo::ReportBadMessage(kNoClientError);
    return;
  }

  if (!cros_soda_client_->IsInitialized() ||
      cros_soda_client_->DidAudioPropertyChange(sample_rate, channel_count)) {
    auto config = chromeos::machine_learning::mojom::SodaConfig::New();
    config->channel_count = channel_count;
    config->sample_rate = sample_rate;
    config->api_key = google_apis::GetSodaAPIKey();
    config->language_dlc_path = config_paths()[primary_language_name()].value();
    config->library_dlc_path = binary_path_.value();
    config->recognition_mode =
        GetSodaSpeechRecognitionMode(options_->recognition_mode);
    config->enable_formatting =
        options_->enable_formatting
            ? chromeos::machine_learning::mojom::OptionalBool::kTrue
            : chromeos::machine_learning::mojom::OptionalBool::kFalse;
    cros_soda_client_->Reset(std::move(config), recognition_event_callback(),
                             speech_recognition_stopped_callback());
  }
  cros_soda_client_->AddAudio(reinterpret_cast<char*>(buffer->data.data()),
                              buffer_size);
}

void CrosSpeechRecognitionRecognizerImpl::MarkDone() {
  if (cros_soda_client_ == nullptr) {
    LOG(DFATAL) << "No soda client, stopping.";
    mojo::ReportBadMessage(kNoClientError);
    return;
  }

  if (!cros_soda_client_->IsInitialized()) {
    // Speech recognition was stopped before it could initialize. Return early
    // in this case.
    return;
  }

  cros_soda_client_->MarkDone();
}

}  // namespace speech
