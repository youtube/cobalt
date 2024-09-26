/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/speech/speech_recognition_controller.h"

#include <memory>

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/speech/speech_grammar_list.h"
#include "third_party/blink/renderer/modules/speech/speech_recognition.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

const char SpeechRecognitionController::kSupplementName[] =
    "SpeechRecognitionController";

SpeechRecognitionController* SpeechRecognitionController::From(
    LocalDOMWindow& window) {
  SpeechRecognitionController* controller =
      Supplement<LocalDOMWindow>::From<SpeechRecognitionController>(window);
  if (!controller) {
    controller = MakeGarbageCollected<SpeechRecognitionController>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, controller);
  }
  return controller;
}

SpeechRecognitionController::SpeechRecognitionController(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window), speech_recognizer_(&window) {}

SpeechRecognitionController::~SpeechRecognitionController() {
  // FIXME: Call m_client->pageDestroyed(); once we have implemented a client.
}

void SpeechRecognitionController::Start(
    mojo::PendingReceiver<mojom::blink::SpeechRecognitionSession>
        session_receiver,
    mojo::PendingRemote<mojom::blink::SpeechRecognitionSessionClient>
        session_client,
    const SpeechGrammarList& grammars,
    const String& lang,
    bool continuous,
    bool interim_results,
    uint32_t max_alternatives) {
  mojom::blink::StartSpeechRecognitionRequestParamsPtr msg_params =
      mojom::blink::StartSpeechRecognitionRequestParams::New();
  for (unsigned i = 0; i < grammars.length(); i++) {
    SpeechGrammar* grammar = grammars.item(i);
    msg_params->grammars.push_back(mojom::blink::SpeechRecognitionGrammar::New(
        grammar->src(), grammar->weight()));
  }
  msg_params->language = lang.IsNull() ? g_empty_string : lang;
  msg_params->max_hypotheses = max_alternatives;
  msg_params->continuous = continuous;
  msg_params->interim_results = interim_results;
  msg_params->client = std::move(session_client);
  msg_params->session_receiver = std::move(session_receiver);

  GetSpeechRecognizer()->Start(std::move(msg_params));
}

void SpeechRecognitionController::Trace(Visitor* visitor) const {
  Supplement::Trace(visitor);
  visitor->Trace(speech_recognizer_);
}

mojom::blink::SpeechRecognizer*
SpeechRecognitionController::GetSpeechRecognizer() {
  if (!speech_recognizer_.is_bound()) {
    GetSupplementable()->GetBrowserInterfaceBroker().GetInterface(
        speech_recognizer_.BindNewPipeAndPassReceiver(
            GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return speech_recognizer_.get();
}

}  // namespace blink
