// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_byob_reader.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_read_result.h"
#include "third_party/blink/renderer/core/streams/read_into_request.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_controller.h"
#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

namespace blink {

class ReadableStreamBYOBReader::BYOBReaderReadIntoRequest final
    : public ReadIntoRequest {
 public:
  explicit BYOBReaderReadIntoRequest(StreamPromiseResolver* resolver)
      : resolver_(resolver) {}

  void ChunkSteps(ScriptState* script_state,
                  DOMArrayBufferView* chunk) const override {
    auto* read_result = ReadableStreamReadResult::Create();
    read_result->setValue(
        ScriptValue(script_state->GetIsolate(),
                    ToV8Traits<DOMArrayBufferView>::ToV8(script_state, chunk)
                        .ToLocalChecked()));
    read_result->setDone(false);
    resolver_->Resolve(script_state,
                       ToV8(read_result, script_state->GetContext()->Global(),
                            script_state->GetIsolate()));
  }

  void CloseSteps(ScriptState* script_state,
                  DOMArrayBufferView* chunk) const override {
    auto* read_result = ReadableStreamReadResult::Create();
    read_result->setValue(ScriptValue(
        script_state->GetIsolate(),
        chunk ? ToV8Traits<DOMArrayBufferView>::ToV8(script_state, chunk)
                    .ToLocalChecked()
              : static_cast<v8::Local<v8::Value>>(
                    v8::Undefined(script_state->GetIsolate()))));
    read_result->setDone(true);
    resolver_->Resolve(script_state,
                       ToV8(read_result, script_state->GetContext()->Global(),
                            script_state->GetIsolate()));
  }

  void ErrorSteps(ScriptState* script_state,
                  v8::Local<v8::Value> e) const override {
    resolver_->Reject(script_state, e);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(resolver_);
    ReadIntoRequest::Trace(visitor);
  }

 private:
  Member<StreamPromiseResolver> resolver_;
};

ReadableStreamBYOBReader* ReadableStreamBYOBReader::Create(
    ScriptState* script_state,
    ReadableStream* stream,
    ExceptionState& exception_state) {
  auto* reader = MakeGarbageCollected<ReadableStreamBYOBReader>(
      script_state, stream, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return reader;
}

ReadableStreamBYOBReader::ReadableStreamBYOBReader(
    ScriptState* script_state,
    ReadableStream* stream,
    ExceptionState& exception_state) {
  SetUpBYOBReader(script_state, this, stream, exception_state);
}

ReadableStreamBYOBReader::~ReadableStreamBYOBReader() = default;

ScriptPromise ReadableStreamBYOBReader::read(ScriptState* script_state,
                                             NotShared<DOMArrayBufferView> view,
                                             ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#byob-reader-read
  // 1. If view.[[ByteLength]] is 0, return a promise rejected with a TypeError
  // exception.
  if (view->byteLength() == 0) {
    exception_state.ThrowTypeError(
        "This readable stream reader cannot be used to read as the view has "
        "byte length equal to 0");
    return ScriptPromise();
  }

  // 2. If view.[[ViewedArrayBuffer]].[[ArrayBufferByteLength]] is 0, return a
  // promise rejected with a TypeError exception.
  if (view->buffer()->ByteLength() == 0) {
    exception_state.ThrowTypeError(
        "This readable stream reader cannot be used to read as the viewed "
        "array buffer has 0 byte length");
    return ScriptPromise();
  }

  // 3. If ! IsDetachedBuffer(view.[[ViewedArrayBuffer]]) is true, return a
  // promise rejected with a TypeError exception.
  if (view->buffer()->IsDetached()) {
    exception_state.ThrowTypeError(
        "This readable stream reader cannot be used to read as the viewed "
        "array buffer is detached");
    return ScriptPromise();
  }

  // 4. If this.[[stream]] is undefined, return a promise rejected with a
  // TypeError exception.
  if (!owner_readable_stream_) {
    exception_state.ThrowTypeError(
        "This readable stream reader has been released and cannot be used to "
        "read from its previous owner stream");
    return ScriptPromise();
  }

  // 5. Let promise be a new promise.
  auto* promise = MakeGarbageCollected<StreamPromiseResolver>(script_state);

  // 6. Let readIntoRequest be a new read-into request with the following items:
  //    chunk steps, given chunk
  //      1. Resolve promise with «[ "value" → chunk, "done" → false ]».
  //    close steps, given chunk
  //      1. Resolve promise with «[ "value" → chunk, "done" → true ]».
  //    error steps, given e
  //      1. Reject promise with e.
  auto* read_into_request =
      MakeGarbageCollected<BYOBReaderReadIntoRequest>(promise);

  // 7. Perform ! ReadableStreamBYOBReaderRead(this, view, readIntoRequest).
  Read(script_state, this, view, read_into_request, exception_state);
  // 8. Return promise.
  return promise->GetScriptPromise(script_state);
}

void ReadableStreamBYOBReader::Read(ScriptState* script_state,
                                    ReadableStreamBYOBReader* reader,
                                    NotShared<DOMArrayBufferView> view,
                                    ReadIntoRequest* read_into_request,
                                    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-stream-byob-reader-read
  // 1. Let stream be reader.[[stream]].
  ReadableStream* stream = reader->owner_readable_stream_;

  // 2. Assert: stream is not undefined.
  DCHECK(stream);

  // 3. Set stream.[[disturbed]] to true.
  stream->is_disturbed_ = true;

  // 4. If stream.[[state]] is "errored", perform readIntoRequest's error steps
  // given stream.[[storedError]].
  if (stream->state_ == ReadableStream::kErrored) {
    read_into_request->ErrorSteps(
        script_state, stream->GetStoredError(script_state->GetIsolate()));
  } else {
    // 5. Otherwise, perform !
    // ReadableByteStreamControllerPullInto(stream.[[controller]], view,
    // readIntoRequest).
    ReadableStreamController* controller = stream->readable_stream_controller_;
    ReadableByteStreamController::PullInto(
        script_state, To<ReadableByteStreamController>(controller), view,
        read_into_request, exception_state);
  }
}

void ReadableStreamBYOBReader::ErrorReadIntoRequests(
    ScriptState* script_state,
    ReadableStreamBYOBReader* reader,
    v8::Local<v8::Value> e) {
  // https://streams.spec.whatwg.org/#abstract-opdef-readablestreambyobreadererrorreadintorequests
  // 1. Let readIntoRequests be reader.[[readIntoRequests]].
  // 2. Set reader.[[readIntoRequests]] to a new empty list.
  // 3. For each readIntoRequest of readIntoRequests,
  for (ReadIntoRequest* request : reader->read_into_requests_) {
    //   a. Perform readIntoRequest’s error steps, given e.
    request->ErrorSteps(script_state, e);
  }
  reader->read_into_requests_.clear();
}

void ReadableStreamBYOBReader::Release(ScriptState* script_state,
                                       ReadableStreamBYOBReader* reader) {
  // https://streams.spec.whatwg.org/#abstract-opdef-readablestreambyobreaderrelease
  // 1. Perform ! ReadableStreamReaderGenericRelease(reader).
  ReadableStreamGenericReader::GenericRelease(script_state, reader);

  // 2. Let e be a new TypeError exception.
  v8::Local<v8::Value> e = V8ThrowException::CreateTypeError(
      script_state->GetIsolate(), "Releasing BYOB reader");

  // 3. Perform ! ReadableStreamBYOBReaderErrorReadIntoRequests(reader, e).
  ErrorReadIntoRequests(script_state, reader, e);
}

void ReadableStreamBYOBReader::releaseLock(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#byob-reader-release-lock
  // 1. If this.[[stream]] is undefined, return.
  if (!owner_readable_stream_) {
    return;
  }

  // 2. Perform ! ReadableStreamBYOBReaderRelease(this).
  Release(script_state, this);
}

void ReadableStreamBYOBReader::SetUpBYOBReader(
    ScriptState* script_state,
    ReadableStreamBYOBReader* reader,
    ReadableStream* stream,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#set-up-readable-stream-byob-reader
  // If ! IsReadableStreamLocked(stream) is true, throw a TypeError exception.
  if (ReadableStream::IsLocked(stream)) {
    exception_state.ThrowTypeError(
        "ReadableStreamBYOBReader constructor can only accept readable streams "
        "that are not yet locked to a reader");
    return;
  }

  // If stream.[[controller]] does not implement ReadableByteStreamController,
  // throw a TypeError exception.
  if (!stream->readable_stream_controller_->IsByteStreamController()) {
    exception_state.ThrowTypeError(
        "Cannot use a BYOB reader with a non-byte stream");
    return;
  }

  // Perform ! ReadableStreamReaderGenericInitialize(reader, stream).
  ReadableStreamGenericReader::GenericInitialize(script_state, reader, stream);

  // Set reader.[[readIntoRequests]] to a new empty list.
  DCHECK_EQ(reader->read_into_requests_.size(), 0u);
}

void ReadableStreamBYOBReader::Trace(Visitor* visitor) const {
  visitor->Trace(read_into_requests_);
  ReadableStreamGenericReader::Trace(visitor);
}

}  // namespace blink
