// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_BYTE_STREAM_TEE_ENGINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_BYTE_STREAM_TEE_ENGINE_H_

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ReadableByteStreamController;
class ReadableStream;
class ReadableStreamGenericReader;
class ScriptState;
class StreamPromiseResolver;

// Implementation of "ReadableByteStreamTee()" from the standard.
// https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee
class ByteStreamTeeEngine final : public GarbageCollected<ByteStreamTeeEngine> {
 public:
  ByteStreamTeeEngine() = default;
  ByteStreamTeeEngine(const ByteStreamTeeEngine&) = delete;
  ByteStreamTeeEngine& operator=(const ByteStreamTeeEngine&) = delete;

  // Create the streams and start copying data.
  void Start(ScriptState*, ReadableStream*, ExceptionState&);

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

  class ByteTeeReadRequest;
  class ByteTeeReadIntoRequest;

  void ForwardReaderError(ScriptState*,
                          ReadableStreamGenericReader* this_reader);

  void PullWithDefaultReader(ScriptState*, ExceptionState&);

  void PullWithBYOBReader(ScriptState*,
                          NotShared<DOMArrayBufferView> view,
                          bool for_branch_2,
                          ExceptionState&);

  // https://streams.spec.whatwg.org/#abstract-opdef-cloneasuint8array
  DOMUint8Array* CloneAsUint8Array(DOMArrayBufferView* chunk);

  Member<ReadableStream> stream_;
  Member<ReadableStreamGenericReader> reader_;
  Member<StreamPromiseResolver> cancel_promise_;
  bool reading_ = false;

  // The standard contains a number of pairs of variables with one for each
  // stream. These are implemented as arrays here. While they are 1-indexed in
  // the standard, they are 0-indexed here; ie. "canceled_[0]" here corresponds
  // to "canceled1" in the standard.
  bool canceled_[2] = {false, false};
  bool read_again_for_branch_[2] = {false, false};
  TraceWrapperV8Reference<v8::Value> reason_[2];
  Member<ReadableStream> branch_[2];
  Member<ReadableByteStreamController> controller_[2];
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_BYTE_STREAM_TEE_ENGINE_H_
