// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_WRITER_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_WRITER_FACTORY_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class AI;
class AIWriter;
class AIWriterCreateOptions;

// This class is responsible for creating AIWriter instances.
class AIWriterFactory final : public ScriptWrappable,
                              public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AIWriterFactory(AI* ai);
  void Trace(Visitor* visitor) const override;

  // ai_writer_factory.idl implementation.
  ScriptPromise<AIWriter> create(ScriptState* script_state,
                                 const AIWriterCreateOptions* options,
                                 ExceptionState& exception_state);

 private:
  Member<AI> ai_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_WRITER_FACTORY_H_
