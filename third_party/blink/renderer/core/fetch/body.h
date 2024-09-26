// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BODY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BODY_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class BodyStreamBuffer;
class ExceptionState;
class ExecutionContext;
class ReadableStream;
class ScriptState;

// This class represents Body mix-in defined in the fetch spec
// https://fetch.spec.whatwg.org/#body-mixin.
//
// Note: This class has body stream and its predicate whereas in the current
// spec only Response has it and Request has a byte stream defined in the
// Encoding spec. The spec should be fixed shortly to be aligned with this
// implementation.
class CORE_EXPORT Body : public ExecutionContextClient {
 public:
  explicit Body(ExecutionContext*);
  Body(const Body&) = delete;
  Body& operator=(const Body&) = delete;

  ScriptPromise arrayBuffer(ScriptState*, ExceptionState&);
  ScriptPromise blob(ScriptState*, ExceptionState&);
  ScriptPromise formData(ScriptState*, ExceptionState&);
  ScriptPromise json(ScriptState*, ExceptionState&);
  ScriptPromise text(ScriptState*, ExceptionState&);
  ReadableStream* body();
  virtual BodyStreamBuffer* BodyBuffer() = 0;
  virtual const BodyStreamBuffer* BodyBuffer() const = 0;

  // This should only be called from the generated bindings. All other code
  // should use IsBodyUsed() instead.
  bool bodyUsed() const { return IsBodyUsed(); }

  // True if the body has been read from.
  virtual bool IsBodyUsed() const;

  // True if the body is locked.
  bool IsBodyLocked() const;

  bool HasPendingActivity() const;

 private:
  // TODO(e_hakkinen): Fix |MimeType()| to always contain parameters and
  // remove |ContentType()|.
  virtual String ContentType() const = 0;
  virtual String MimeType() const = 0;

  // Body consumption algorithms will reject with a TypeError in a number of
  // error conditions. This method wraps those up into one call which throws
  // an exception if consumption cannot proceed. The caller must check
  // |exception_state| on return.
  void RejectInvalidConsumption(ExceptionState& exception_state) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BODY_H_
