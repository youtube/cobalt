// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TEE_ENGINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TEE_ENGINE_H_

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ReadableStream;
class ReadableStreamDefaultController;
class ReadableStreamDefaultReader;
class ScriptState;
class StreamPromiseResolver;

// Implementation of "ReadableStreamDefaultTee()" from the standard.
// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaulttee
class TeeEngine final : public GarbageCollected<TeeEngine> {
 public:
  TeeEngine() = default;
  TeeEngine(const TeeEngine&) = delete;
  TeeEngine& operator=(const TeeEngine&) = delete;

  // Create the streams and start copying data.
  void Start(ScriptState*,
             ReadableStream*,
             bool clone_for_branch2,
             ExceptionState&);

  // Branch1() and Branch2() are null until Start() is called.
  ReadableStream* Branch1() const { return branch_[0].Get(); }
  ReadableStream* Branch2() const { return branch_[1].Get(); }

  void Trace(Visitor* visitor) const {
    visitor->Trace(stream_);
    visitor->Trace(reader_);
    visitor->Trace(reason_[0]);
    visitor->Trace(reason_[1]);
    visitor->Trace(branch_[0]);
    visitor->Trace(branch_[1]);
    visitor->Trace(controller_[0]);
    visitor->Trace(controller_[1]);
    visitor->Trace(cancel_promise_);
  }

 private:
  class PullAlgorithm;
  class CancelAlgorithm;

  // https://streams.spec.whatwg.org/#abstract-opdef-structuredclone
  v8::MaybeLocal<v8::Value> StructuredClone(ScriptState*,
                                            v8::Local<v8::Value> chunk,
                                            ExceptionState&);

  Member<ReadableStream> stream_;
  Member<ReadableStreamDefaultReader> reader_;
  Member<StreamPromiseResolver> cancel_promise_;
  bool reading_ = false;
  bool read_again_ = false;
  bool clone_for_branch2_ = false;

  // The standard contains a number of pairs of variables with one for each
  // stream. These are implemented as arrays here. While they are 1-indexed in
  // the standard, they are 0-indexed here; ie. "canceled_[0]" here corresponds
  // to "canceled1" in the standard.
  bool canceled_[2] = {false, false};
  TraceWrapperV8Reference<v8::Value> reason_[2];
  Member<ReadableStream> branch_[2];
  Member<ReadableStreamDefaultController> controller_[2];
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TEE_ENGINE_H_
