// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/listener_stream_provider.h"

#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/stream_provider_delegate.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"

namespace dictation {

ListenerStreamProvider::ListenerStreamProvider(
    content::BrowserContext* browser_context,
    StreamProviderDelegate& delegate)
    : delegate_(delegate), browser_context_(browser_context) {}

ListenerStreamProvider::~ListenerStreamProvider() {
  if (stream_id_) {
    GetMultiplexer().UnregisterStreamProvider(stream_id_);
  }
}

void ListenerStreamProvider::BindToTargetAndConnect(
    std::unique_ptr<Target> target) {
  target_ = std::move(target);
  DictationMultiplexer& multiplexer = GetMultiplexer();
  stream_id_ = multiplexer.GenerateStreamId();
  multiplexer.RegisterStreamProvider(stream_id_, this);

  extensions::api::dictation_private::StartStreamDetails details;
  details.stream_id = stream_id_.value();
  // TODO(crbug.com/502587072): Populate page context.
  details.page_context = "";
  // TODO(crbug.com/524620051): Populate editable content.
  details.editable_content = "";

  base::ListValue event_args =
      extensions::api::dictation_private::OnStartStream::Create(details);

  auto event = std::make_unique<extensions::Event>(
      extensions::events::DICTATION_PRIVATE_ON_START_STREAM,
      extensions::api::dictation_private::OnStartStream::kEventName,
      std::move(event_args), browser_context_);

  needs_end_stream_ = true;
  extensions::EventRouter::Get(browser_context_)
      ->BroadcastEvent(std::move(event));
}

void ListenerStreamProvider::Stop() {
  if (!stream_id_) {
    return;
  }

  if (!needs_end_stream_) {
    return;
  }

  extensions::api::dictation_private::EndStreamDetails details;
  details.stream_id = stream_id_.value();

  base::ListValue event_args =
      extensions::api::dictation_private::OnEndStream::Create(details);

  auto event = std::make_unique<extensions::Event>(
      extensions::events::DICTATION_PRIVATE_ON_END_STREAM,
      extensions::api::dictation_private::OnEndStream::kEventName,
      std::move(event_args), browser_context_);

  needs_end_stream_ = false;
  extensions::EventRouter::Get(browser_context_)
      ->BroadcastEvent(std::move(event));
}

void ListenerStreamProvider::OnTranscriptionUpdated(const std::string& data,
                                                    bool is_final) {
  latest_transcription_ = data;
  is_final_ = is_final;

  if (update_callback_for_testing_) {
    update_callback_for_testing_.Run();
  }
}

void ListenerStreamProvider::OnStreamStateChanged(StreamState state) {
  // TODO(crbug.com/502587072): Assert state transitions are correct.
  StreamState old_state = state_;
  state_ = state;

  delegate_->DidUpdateStreamProviderState(*this, old_state);

  if (update_callback_for_testing_) {
    update_callback_for_testing_.Run();
  }
}

void ListenerStreamProvider::SetOnUpdateForTesting(  // IN-TEST
    base::RepeatingClosure callback) {
  update_callback_for_testing_ = std::move(callback);
}

const std::string&
ListenerStreamProvider::GetLatestTranscriptionForTesting()  // IN-TEST
    const {
  return latest_transcription_;
}

bool ListenerStreamProvider::IsTranscriptionFinalForTesting() const {
  return is_final_;
}

ListenerStreamProvider::StreamState ListenerStreamProvider::GetState() const {
  return state_;
}

DictationMultiplexer& ListenerStreamProvider::GetMultiplexer() const {
  DictationKeyedService* service = DictationKeyedService::Get(browser_context_);
  CHECK(service);
  return service->multiplexer();
}

}  // namespace dictation
