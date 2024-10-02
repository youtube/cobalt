// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/document_module_script_fetcher.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/script/pending_script.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

DocumentModuleScriptFetcher::DocumentModuleScriptFetcher(
    base::PassKey<ModuleScriptLoader> pass_key)
    : ModuleScriptFetcher(pass_key) {}

void DocumentModuleScriptFetcher::Fetch(
    FetchParameters& fetch_params,
    ModuleType expected_module_type,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    ModuleGraphLevel level,
    ModuleScriptFetcher::Client* client) {
  DCHECK_EQ(fetch_params.GetScriptType(), mojom::blink::ScriptType::kModule);
  DCHECK(fetch_client_settings_object_fetcher);
  DCHECK(!client_);
  client_ = client;
  expected_module_type_ = expected_module_type;
  // Streaming can currently only be triggered from the main thread. This
  // currently happens only for dynamic imports in worker modules.
  ScriptResource::StreamingAllowed streaming_allowed =
                        IsMainThread() ? ScriptResource::kAllowStreaming
                                       : ScriptResource::kNoStreaming;
  ScriptResource::Fetch(fetch_params, fetch_client_settings_object_fetcher,
                        this, streaming_allowed);
}

void DocumentModuleScriptFetcher::NotifyFinished(Resource* resource) {
  ClearResource();

  auto* script_resource = To<ScriptResource>(resource);

  {
    HeapVector<Member<ConsoleMessage>> error_messages;
    if (!WasModuleLoadSuccessful(script_resource, expected_module_type_,
                                 &error_messages)) {
      client_->NotifyFetchFinishedError(error_messages);
      return;
    }
  }
  // Check if we can use the script streamer.
  ScriptStreamer* streamer;
  ScriptStreamer::NotStreamingReason not_streamed_reason;
  std::tie(streamer, not_streamed_reason) = ResourceScriptStreamer::TakeFrom(
      script_resource, mojom::blink::ScriptType::kModule);

  ScriptStreamer::RecordStreamingHistogram(ScriptSchedulingType::kAsync,
                                           streamer, not_streamed_reason);

  TRACE_EVENT_WITH_FLOW1(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                         "DocumentModuleScriptFetcher::NotifyFinished", this,
                         TRACE_EVENT_FLAG_FLOW_IN, "not_streamed_reason",
                         not_streamed_reason);
  // TODO(crbug.com/1061857): Pass ScriptStreamer to the client here.
  const KURL& url = script_resource->GetResponse().ResponseUrl();
  // Create an external module script where base_url == source_url.
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-base-url
  client_->NotifyFetchFinishedSuccess(ModuleScriptCreationParams(
      /*source_url=*/url, /*base_url=*/url,
      ScriptSourceLocationType::kExternalFile, expected_module_type_,
      script_resource->SourceText(), script_resource->CacheHandler(), streamer,
      not_streamed_reason));
}

void DocumentModuleScriptFetcher::Trace(Visitor* visitor) const {
  ModuleScriptFetcher::Trace(visitor);
  visitor->Trace(client_);
  ResourceClient::Trace(visitor);
}

}  // namespace blink
