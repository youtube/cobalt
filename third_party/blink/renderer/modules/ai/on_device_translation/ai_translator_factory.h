// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_TRANSLATOR_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_TRANSLATOR_FACTORY_H_

#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_translator_create_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_translator.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_translator_capabilities.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// Implementation of the AITranslatorFactory interface.
// Explainer: https://github.com/WICG/translation-api
class AITranslatorFactory final : public ScriptWrappable,
                                  public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AITranslatorFactory(ExecutionContext* execution_context);

  ScriptPromise<AITranslator> create(ScriptState* script_state,
                                     AITranslatorCreateOptions* options,
                                     ExceptionState& exception_state);

  ScriptPromise<AITranslatorCapabilities> capabilities(
      ScriptState* script_state,
      ExceptionState& exception_state);

  HeapMojoRemote<mojom::blink::TranslationManager>&
  GetTranslationManagerRemote();

  void Trace(Visitor* visitor) const override;

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<mojom::blink::TranslationManager> translation_manager_remote_{
      nullptr};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_TRANSLATOR_FACTORY_H_
