// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ash/extensions/speech/speech_recognition_private_api.h"

#include "chrome/browser/ash/extensions/speech/speech_recognition_private_manager.h"
#include "chrome/browser/speech/speech_recognition_constants.h"
#include "chrome/common/extensions/api/speech_recognition_private.h"
#include "content/public/browser/browser_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

ExtensionFunction::ResponseAction SpeechRecognitionPrivateStartFunction::Run() {
  // Extract arguments.
  absl::optional<api::speech_recognition_private::Start::Params> params =
      api::speech_recognition_private::Start::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const api::speech_recognition_private::StartOptions* options =
      &params->options;
  DCHECK(options);
  absl::optional<int> client_id;
  absl::optional<std::string> locale;
  absl::optional<bool> interim_results;
  if (options->client_id)
    client_id = *options->client_id;
  if (options->locale)
    locale = *options->locale;
  if (options->interim_results)
    interim_results = *options->interim_results;

  // Get the manager for this context and ask it to handle this API call.
  SpeechRecognitionPrivateManager* manager =
      SpeechRecognitionPrivateManager::Get(browser_context());
  const std::string key = manager->CreateKey(extension_id(), client_id);
  manager->HandleStart(
      key, locale, interim_results,
      base::BindOnce(&SpeechRecognitionPrivateStartFunction::OnStart, this));
  return RespondLater();
}

void SpeechRecognitionPrivateStartFunction::OnStart(
    speech::SpeechRecognitionType type,
    absl::optional<std::string> error) {
  if (error.has_value()) {
    Respond(Error(error.value()));
    return;
  }

  Respond(WithArguments(api::speech_recognition_private::ToString(
      speech::SpeechRecognitionTypeToApiType(type))));
}

ExtensionFunction::ResponseAction SpeechRecognitionPrivateStopFunction::Run() {
  // Extract arguments.
  absl::optional<api::speech_recognition_private::Stop::Params> params =
      api::speech_recognition_private::Stop::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const api::speech_recognition_private::StopOptions* options =
      &params->options;
  DCHECK(options);
  absl::optional<int> client_id;
  if (options->client_id)
    client_id = *options->client_id;

  // Get the manager for this context and ask it to handle this API call.
  SpeechRecognitionPrivateManager* manager =
      SpeechRecognitionPrivateManager::Get(browser_context());
  const std::string key = manager->CreateKey(extension_id(), client_id);
  manager->HandleStop(
      key, base::BindOnce(&SpeechRecognitionPrivateStopFunction::OnStop, this));
  return RespondLater();
}

void SpeechRecognitionPrivateStopFunction::OnStop(
    absl::optional<std::string> error) {
  if (error.has_value()) {
    Respond(Error(error.value()));
    return;
  }

  Respond(NoArguments());
}

}  // namespace extensions
