// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/dictation_private/dictation_private_api.h"

#include <string>
#include <string_view>

#include "base/notreached.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/dictation_multiplexer.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "chrome/common/extensions/api/dictation_private.h"

namespace extensions {

namespace dictation_private = api::dictation_private;

namespace {

constexpr std::string_view kInvalidStreamIdError = "Invalid stream ID.";

dictation::StreamProvider::StreamState ConvertStreamState(
    dictation_private::StreamState state) {
  switch (state) {
    case dictation_private::StreamState::kInitializing:
      return dictation::StreamProvider::StreamState::kInitializing;
    case dictation_private::StreamState::kFailed:
      return dictation::StreamProvider::StreamState::kFailed;
    case dictation_private::StreamState::kTranscribing:
      return dictation::StreamProvider::StreamState::kTranscribing;
    case dictation_private::StreamState::kComplete:
      return dictation::StreamProvider::StreamState::kComplete;
    case dictation_private::StreamState::kNone:
      NOTREACHED();
  }
}

}  // namespace

ExtensionFunction::ResponseAction
DictationPrivateUpdateTranscriptionFunction::Run() {
  std::optional<dictation_private::UpdateTranscription::Params> params =
      dictation_private::UpdateTranscription::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  dictation::DictationKeyedService* service =
      dictation::DictationKeyedService::Get(browser_context());
  EXTENSION_FUNCTION_VALIDATE(service);

  dictation::DictationMultiplexer& multiplexer = service->multiplexer();

  dictation::DictationMultiplexer::StreamId stream_id(
      params->details.stream_id);

  bool is_final =
      params->details.type == dictation_private::TranscriptionType::kFinal;
  if (!multiplexer.UpdateTranscription(stream_id, params->details.data,
                                       is_final)) {
    return RespondNow(Error(std::string(kInvalidStreamIdError)));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
DictationPrivateSetStreamStateFunction::Run() {
  std::optional<dictation_private::SetStreamState::Params> params =
      dictation_private::SetStreamState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  dictation::DictationKeyedService* service =
      dictation::DictationKeyedService::Get(browser_context());
  EXTENSION_FUNCTION_VALIDATE(service);

  dictation::DictationMultiplexer& multiplexer = service->multiplexer();

  dictation::DictationMultiplexer::StreamId stream_id(
      params->details.stream_id);

  if (!multiplexer.SetStreamState(stream_id,
                                  ConvertStreamState(params->details.state))) {
    return RespondNow(Error(std::string(kInvalidStreamIdError)));
  }

  return RespondNow(NoArguments());
}

}  // namespace extensions
