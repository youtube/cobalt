// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_test_util.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/test_underlying_source.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::StrictMock;
using Checkpoint = StrictMock<::testing::MockFunction<void(int)>>;
using Result = BytesConsumer::Result;
using PublicState = BytesConsumer::PublicState;

class MockClient : public GarbageCollected<MockClient>,
                   public BytesConsumer::Client {
 public:
  MockClient() = default;

  MOCK_METHOD0(OnStateChange, void());
  String DebugName() const override { return "MockClient"; }

  void Trace(Visitor* visitor) const override {}
};

TEST(ReadableStreamBytesConsumerTest, Create) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  ExceptionState& exception_state = scope.GetExceptionState();

  auto* stream = ReadableStream::Create(script_state, exception_state);
  ASSERT_TRUE(stream);
  ASSERT_FALSE(exception_state.HadException());

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
}

TEST(ReadableStreamBytesConsumerTest, EmptyStream) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  underlying_source->Close();

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);

  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);

  Checkpoint checkpoint;
  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());
  EXPECT_EQ(Result::kDone, consumer->BeginRead(buffer));
}

TEST(ReadableStreamBytesConsumerTest, ErroredStream) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  underlying_source->Error(ScriptValue(
      script_state->GetIsolate(), v8::Undefined(script_state->GetIsolate())));

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
  EXPECT_EQ(Result::kError, consumer->BeginRead(buffer));
}

TEST(ReadableStreamBytesConsumerTest, TwoPhaseRead) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  {
    auto* chunk1 = DOMUint8Array::Create(0);
    auto* chunk2 =
        DOMUint8Array::Create(std::to_array<uint8_t>({0x43, 0x44, 0x45, 0x46}));
    auto* chunk3 =
        DOMUint8Array::Create(std::to_array<uint8_t>({0x47, 0x48, 0x49, 0x4a}));
    underlying_source->Enqueue(
        ScriptValue(script_state->GetIsolate(),
                    ToV8Traits<DOMUint8Array>::ToV8(script_state, chunk1)));
    underlying_source->Enqueue(
        ScriptValue(script_state->GetIsolate(),
                    ToV8Traits<DOMUint8Array>::ToV8(script_state, chunk2)));
    underlying_source->Enqueue(
        ScriptValue(script_state->GetIsolate(),
                    ToV8Traits<DOMUint8Array>::ToV8(script_state, chunk3)));
    underlying_source->Close();
  }

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));
  EXPECT_CALL(checkpoint, Call(5));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(6));
  EXPECT_CALL(checkpoint, Call(7));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(8));
  EXPECT_CALL(checkpoint, Call(9));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(10));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(Result::kOk, consumer->BeginRead(buffer));
  ASSERT_EQ(0u, buffer.size());
  EXPECT_EQ(Result::kOk, consumer->EndRead(0));
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(5);
  test::RunPendingTasks();
  checkpoint.Call(6);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_THAT(buffer, ElementsAre(0x43, 0x44, 0x45, 0x46));
  EXPECT_EQ(Result::kOk, consumer->EndRead(0));
  EXPECT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_THAT(buffer, ElementsAre(0x43, 0x44, 0x45, 0x46));
  EXPECT_EQ(Result::kOk, consumer->EndRead(1));
  EXPECT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_THAT(buffer, ElementsAre(0x44, 0x45, 0x46));
  EXPECT_EQ(Result::kOk, consumer->EndRead(3));
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(7);
  test::RunPendingTasks();
  checkpoint.Call(8);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_THAT(buffer, ElementsAre(0x47, 0x48, 0x49, 0x4a));
  EXPECT_EQ(Result::kOk, consumer->EndRead(4));
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(9);
  test::RunPendingTasks();
  checkpoint.Call(10);
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());
  EXPECT_EQ(Result::kDone, consumer->BeginRead(buffer));
}

TEST(ReadableStreamBytesConsumerTest, TwoPhaseReadDetachedDuringRead) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);

  auto* chunk =
      DOMUint8Array::Create(std::to_array<uint8_t>({0x43, 0x44, 0x45, 0x46}));
  underlying_source->Enqueue(
      ScriptValue(script_state->GetIsolate(),
                  ToV8Traits<DOMUint8Array>::ToV8(script_state, chunk)));
  underlying_source->Close();

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_THAT(buffer, ElementsAre(0x43, 0x44, 0x45, 0x46));
  chunk->DetachForTesting();
  EXPECT_EQ(Result::kError, consumer->EndRead(4));
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
}

TEST(ReadableStreamBytesConsumerTest, TwoPhaseReadDetachedBetweenReads) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);

  auto* chunk =
      DOMUint8Array::Create(std::to_array<uint8_t>({0x43, 0x44, 0x45, 0x46}));
  underlying_source->Enqueue(
      ScriptValue(script_state->GetIsolate(),
                  ToV8Traits<DOMUint8Array>::ToV8(script_state, chunk)));
  underlying_source->Close();

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_THAT(buffer, ElementsAre(0x43, 0x44, 0x45, 0x46));
  EXPECT_EQ(Result::kOk, consumer->EndRead(1));
  chunk->DetachForTesting();
  EXPECT_EQ(Result::kError, consumer->BeginRead(buffer));
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
}

TEST(ReadableStreamBytesConsumerTest, EnqueueUndefined) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  underlying_source->Enqueue(ScriptValue(
      script_state->GetIsolate(), v8::Undefined(script_state->GetIsolate())));
  underlying_source->Close();

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
  EXPECT_EQ(Result::kError, consumer->BeginRead(buffer));
}

TEST(ReadableStreamBytesConsumerTest, EnqueueNull) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  underlying_source->Enqueue(ScriptValue(script_state->GetIsolate(),
                                         v8::Null(script_state->GetIsolate())));
  underlying_source->Close();

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
  EXPECT_EQ(Result::kError, consumer->BeginRead(buffer));
}

TEST(ReadableStreamBytesConsumerTest, EnqueueString) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  underlying_source->Enqueue(
      ScriptValue(script_state->GetIsolate(),
                  V8String(script_state->GetIsolate(), "hello")));
  underlying_source->Close();

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
  EXPECT_EQ(Result::kError, consumer->BeginRead(buffer));
}

TEST(ReadableStreamBytesConsumerTest, Cancel) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  underlying_source->Enqueue(ScriptValue(script_state->GetIsolate(),
                                         v8::Null(script_state->GetIsolate())));
  underlying_source->Close();

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);

  consumer->Cancel();

  EXPECT_TRUE(underlying_source->IsCancelled());
  EXPECT_TRUE(underlying_source->IsCancelledWithUndefined());
}

}  // namespace

}  // namespace blink
