// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/bottleneck_buffer.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

constexpr base::TimeDelta kOneWayLatency = base::Milliseconds(100);

class BottleneckBufferTest : public testing::Test, public WithTaskEnvironment {
 public:
  BottleneckBufferTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  // Returns a vector<uint8_t> of `n` bytes filled with `fill`.
  static std::vector<uint8_t> MakeBytes(int n, uint8_t fill = 'x') {
    return std::vector<uint8_t>(static_cast<size_t>(n), fill);
  }
};

// --- BdpCapacity ---

TEST_F(BottleneckBufferTest, BdpCapacityFallsBackForUnlimited) {
  // 0 means "no throughput info" — use the default capacity.
  EXPECT_EQ(BottleneckBuffer::BdpCapacity(0, kOneWayLatency),
            BottleneckBuffer::kDefaultCapacity);
  // Zero latency also falls back.
  EXPECT_EQ(BottleneckBuffer::BdpCapacity(1000, base::TimeDelta()),
            BottleneckBuffer::kDefaultCapacity);
}

TEST_F(BottleneckBufferTest, BdpCapacityScalesWithBandwidthDelayProduct) {
  // 100 KB/s × 200 ms RTT (= 2 * 100 ms one-way latency) = 20 KB BDP;
  // BdpCapacity doubles that to 40 KB so the producer can fill the
  // next window while the consumer drains the current one.
  size_t cap = BottleneckBuffer::BdpCapacity(100 * 1024, kOneWayLatency);
  EXPECT_EQ(cap, 40u * 1024);
}

TEST_F(BottleneckBufferTest, BdpCapacityClampsToMinCapacity) {
  // Tiny BDP — should clamp to kMinCapacity.
  size_t cap = BottleneckBuffer::BdpCapacity(10, base::Milliseconds(1));
  EXPECT_EQ(cap, BottleneckBuffer::kMinCapacity);
}

// --- Push / Pull ---

TEST_F(BottleneckBufferTest, PushAcceptsBytesUpToCapacity) {
  BottleneckBuffer buf(kOneWayLatency, /*capacity=*/1024);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  auto bytes = MakeBytes(2000);
  EXPECT_EQ(buf.Push(bytes), 1024);  // partial — capped at capacity.
  EXPECT_TRUE(buf.full());
  EXPECT_EQ(buf.free_space(), 0u);
}

TEST_F(BottleneckBufferTest, PushAllOrNothingAcceptsWholeOrNothing) {
  BottleneckBuffer buf(kOneWayLatency, /*capacity=*/100);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());

  // Fits entirely: accepted whole.
  EXPECT_EQ(buf.Push(MakeBytes(60), BottleneckBuffer::Mode::kDatagram), 60);
  EXPECT_EQ(buf.buffered_bytes(), 60u);

  // 50 more doesn't fit in the remaining 40 bytes: rejected entirely (no
  // partial accept) so the datagram boundary is preserved.
  EXPECT_EQ(buf.Push(MakeBytes(50), BottleneckBuffer::Mode::kDatagram), 0);
  EXPECT_EQ(buf.buffered_bytes(), 60u);

  // The default stream mode, by contrast, takes a partial 40 bytes.
  EXPECT_EQ(buf.Push(MakeBytes(50)), 40);
  EXPECT_TRUE(buf.full());
}

TEST_F(BottleneckBufferTest, PushAllOrNothingTooBigForCapacityReturnsError) {
  BottleneckBuffer buf(kOneWayLatency, /*capacity=*/100);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  // Larger than the whole buffer: can never fit, so it's an error rather
  // than indefinite backpressure.
  EXPECT_EQ(buf.Push(MakeBytes(101), BottleneckBuffer::Mode::kDatagram),
            net::ERR_MSG_TOO_BIG);
  EXPECT_TRUE(buf.empty());
}

TEST_F(BottleneckBufferTest, EmptyPushReturnsZero) {
  BottleneckBuffer buf(kOneWayLatency);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  std::vector<uint8_t> empty;
  EXPECT_EQ(buf.Push(empty), 0);
  EXPECT_TRUE(buf.empty());
  EXPECT_EQ(buf.buffered_bytes(), 0u);
  EXPECT_EQ(buf.free_space(), buf.capacity());
  EXPECT_FALSE(buf.has_ready_data());
}

TEST_F(BottleneckBufferTest, PullReturnsZeroWhenNothingToCopy) {
  BottleneckBuffer buf(kOneWayLatency);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());

  // Empty buffer: nothing buffered, so Pull copies nothing.
  std::vector<uint8_t> dest(100, 0);
  EXPECT_EQ(buf.Pull(base::span(dest)), 0);
  EXPECT_EQ(dest[0], 0);

  // Empty dest with ready data buffered: also a no-op, and the data is
  // left untouched.
  buf.Push(MakeBytes(50));
  FastForwardBy(kOneWayLatency);
  ASSERT_TRUE(buf.has_ready_data());
  std::vector<uint8_t> empty_dest;
  EXPECT_EQ(buf.Pull(base::span(empty_dest)), 0);
  EXPECT_EQ(buf.buffered_bytes(), 50u);
}

TEST_F(BottleneckBufferTest, PullWaitsForLatency) {
  BottleneckBuffer buf(kOneWayLatency);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  auto bytes = MakeBytes(100, 'Z');
  EXPECT_EQ(buf.Push(bytes), 100);

  std::vector<uint8_t> dest(200, 0);
  EXPECT_FALSE(buf.has_ready_data());
  EXPECT_EQ(buf.Pull(base::span(dest)), 0);
  // Pre-latency Pull must not have written to dest.
  EXPECT_EQ(dest[0], 0);

  FastForwardBy(kOneWayLatency);
  EXPECT_TRUE(buf.has_ready_data());
  EXPECT_EQ(buf.Pull(base::span(dest)), 100);
  EXPECT_TRUE(buf.empty());
  // The pulled prefix matches what we pushed; the rest of `dest` was
  // untouched.
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(dest[i], 'Z') << "mismatch at index " << i;
  }
  EXPECT_EQ(dest[100], 0);
}

TEST_F(BottleneckBufferTest, PullAcrossChunksConcatenatesReadyChunks) {
  // Three chunks pushed at the same instant all become ready together.
  // Mode::kStream (the default) drains all three into the same
  // `dest`, concatenated in push order.
  BottleneckBuffer buf(kOneWayLatency);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  EXPECT_EQ(buf.Push(MakeBytes(50, 'A')), 50);
  EXPECT_EQ(buf.Push(MakeBytes(50, 'B')), 50);
  EXPECT_EQ(buf.Push(MakeBytes(50, 'C')), 50);
  FastForwardBy(kOneWayLatency);

  std::vector<uint8_t> dest(200, 0);
  EXPECT_EQ(buf.Pull(base::span(dest)), 150);
  for (int i = 0; i < 50; ++i) {
    EXPECT_EQ(dest[i], 'A') << "A chunk mismatch at index " << i;
  }
  for (int i = 50; i < 100; ++i) {
    EXPECT_EQ(dest[i], 'B') << "B chunk mismatch at index " << i;
  }
  for (int i = 100; i < 150; ++i) {
    EXPECT_EQ(dest[i], 'C') << "C chunk mismatch at index " << i;
  }
  EXPECT_EQ(dest[150], 0);  // Past the concatenated bytes: untouched.
  EXPECT_TRUE(buf.empty());
}

TEST_F(BottleneckBufferTest, PullAcrossChunksStopsAtNotReadyBoundary) {
  // `dest` has room for both chunks, but only chunk A has crossed its
  // latency window. Pull must return just chunk A and leave B in place;
  // it must NOT advance past a not-yet-ready chunk just because `dest`
  // still has capacity.
  BottleneckBuffer buf(kOneWayLatency);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  buf.Push(MakeBytes(50, 'A'));
  FastForwardBy(kOneWayLatency);  // A becomes ready.
  buf.Push(MakeBytes(50, 'B'));   // B's time_available is one latency from now.

  std::vector<uint8_t> dest(200, 0);
  EXPECT_EQ(buf.Pull(base::span(dest)), 50);
  for (int i = 0; i < 50; ++i) {
    EXPECT_EQ(dest[i], 'A') << "A chunk mismatch at index " << i;
  }
  EXPECT_EQ(dest[50], 0);  // B not yet drained.
  EXPECT_FALSE(buf.empty());
  EXPECT_EQ(buf.buffered_bytes(), 50u);

  // After B's latency elapses, a subsequent Pull picks it up.
  std::ranges::fill(dest, 0);
  FastForwardBy(kOneWayLatency);
  EXPECT_EQ(buf.Pull(base::span(dest)), 50);
  for (int i = 0; i < 50; ++i) {
    EXPECT_EQ(dest[i], 'B') << "B chunk mismatch at index " << i;
  }
  EXPECT_TRUE(buf.empty());
}

TEST_F(BottleneckBufferTest, PullManyChunksAcrossDequeGrowth) {
  // base::circular_deque starts with capacity
  // base::internal::kCircularBufferInitialCapacity (3); pushing more chunks
  // than that forces a reallocation that move-constructs the buffered Chunks.
  // Push well past the initial capacity to exercise Chunk's move constructor.
  BottleneckBuffer buf(kOneWayLatency);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  constexpr int kNumChunks = 6;
  for (int i = 0; i < kNumChunks; ++i) {
    ASSERT_EQ(buf.Push(MakeBytes(10, static_cast<uint8_t>('A' + i))), 10);
  }
  FastForwardBy(kOneWayLatency);

  std::vector<uint8_t> dest(kNumChunks * 10, 0);
  EXPECT_EQ(buf.Pull(base::span(dest)), kNumChunks * 10);
  // Each chunk's bytes survived the deque growth intact and in push order.
  for (int i = 0; i < kNumChunks; ++i) {
    for (int j = 0; j < 10; ++j) {
      EXPECT_EQ(dest[i * 10 + j], static_cast<uint8_t>('A' + i))
          << "chunk " << i << " byte " << j;
    }
  }
  EXPECT_TRUE(buf.empty());
}

TEST_F(BottleneckBufferTest, PullSingleChunkStopsAtChunkBoundary) {
  // Two pushes => two chunks. Mode::kDatagram returns only
  // the first chunk even though `dest` has room for both.
  BottleneckBuffer buf(kOneWayLatency);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  EXPECT_EQ(buf.Push(MakeBytes(50, 'A')), 50);
  EXPECT_EQ(buf.Push(MakeBytes(50, 'B')), 50);
  FastForwardBy(kOneWayLatency);

  std::vector<uint8_t> dest(200, 0);
  EXPECT_EQ(buf.Pull(base::span(dest), BottleneckBuffer::Mode::kDatagram), 50);
  for (int i = 0; i < 50; ++i) {
    EXPECT_EQ(dest[i], 'A') << "first chunk mismatch at index " << i;
  }
  // Bytes past the first chunk were not touched.
  EXPECT_EQ(dest[50], 0);
  // Second chunk still buffered.
  EXPECT_FALSE(buf.empty());
  EXPECT_EQ(buf.buffered_bytes(), 50u);

  std::ranges::fill(dest, 0);
  EXPECT_EQ(buf.Pull(base::span(dest), BottleneckBuffer::Mode::kDatagram), 50);
  for (int i = 0; i < 50; ++i) {
    EXPECT_EQ(dest[i], 'B') << "second chunk mismatch at index " << i;
  }
  EXPECT_TRUE(buf.empty());
}

TEST_F(BottleneckBufferTest, PullSingleChunkErrorsWhenDestTooSmall) {
  BottleneckBuffer buf(kOneWayLatency);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  buf.Push(MakeBytes(50, 'D'));
  FastForwardBy(kOneWayLatency);
  ASSERT_TRUE(buf.has_ready_data());

  // `dest` can't hold the whole 50-byte datagram. Mode::kDatagram refuses
  // to split it and signals the error without consuming anything.
  std::vector<uint8_t> small(30, 0);
  EXPECT_EQ(buf.Pull(base::span(small), BottleneckBuffer::Mode::kDatagram),
            net::ERR_MSG_TOO_BIG);
  EXPECT_EQ(buf.buffered_bytes(), 50u);
  EXPECT_EQ(small[0], 0);  // Nothing copied.

  // A big-enough dest pulls the whole datagram.
  std::vector<uint8_t> big(50, 0);
  EXPECT_EQ(buf.Pull(base::span(big), BottleneckBuffer::Mode::kDatagram), 50);
  for (int i = 0; i < 50; ++i) {
    EXPECT_EQ(big[i], 'D') << "index " << i;
  }
  EXPECT_TRUE(buf.empty());
}

TEST_F(BottleneckBufferTest, GetReadyBytesAtFrontReflectsLatency) {
  BottleneckBuffer buf(kOneWayLatency);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  buf.Push(MakeBytes(100));
  EXPECT_EQ(buf.GetReadyBytesAtFront(1000), 0u);

  FastForwardBy(kOneWayLatency);
  EXPECT_EQ(buf.GetReadyBytesAtFront(1000), 100u);
  EXPECT_EQ(buf.GetReadyBytesAtFront(40), 40u);  // capped at max_bytes
}

// --- Distinct per-chunk ready_at deadlines ---

// Pushes two chunks at different times; each must become ready exactly
// one latency interval after its own Push, independently of the other.
TEST_F(BottleneckBufferTest, DistinctReadyAtDeadlinesPerChunk) {
  BottleneckBuffer buf(kOneWayLatency);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  buf.Push(MakeBytes(50, 'A'));
  FastForwardBy(kOneWayLatency / 2);  // t = 50ms
  buf.Push(MakeBytes(50, 'B'));       // B arrives 50ms after A.

  // At t=100ms only A is ready (its time_available = 0+100ms).
  FastForwardBy(kOneWayLatency / 2);
  std::vector<uint8_t> dest(200, 0);
  EXPECT_EQ(buf.Pull(base::span(dest), BottleneckBuffer::Mode::kDatagram), 50);
  for (int i = 0; i < 50; ++i) {
    EXPECT_EQ(dest[i], 'A') << "A chunk mismatch at index " << i;
  }
  EXPECT_EQ(dest[50], 0);  // Bytes past the first chunk untouched.

  // B's time_available is 50+100=150ms, so it isn't ready at t=100ms.
  std::ranges::fill(dest, 0);
  EXPECT_EQ(buf.Pull(base::span(dest), BottleneckBuffer::Mode::kDatagram), 0);
  EXPECT_EQ(dest[0], 0);  // Confirmed: nothing copied.

  // After advancing to t=150ms, B becomes ready.
  FastForwardBy(kOneWayLatency / 2);
  EXPECT_EQ(buf.Pull(base::span(dest), BottleneckBuffer::Mode::kDatagram), 50);
  for (int i = 0; i < 50; ++i) {
    EXPECT_EQ(dest[i], 'B') << "B chunk mismatch at index " << i;
  }
  EXPECT_TRUE(buf.empty());
}

// --- Callbacks ---

TEST_F(BottleneckBufferTest, DataReadyCallbackFiresAfterLatency) {
  BottleneckBuffer buf(kOneWayLatency);
  int data_ready_calls = 0;
  buf.SetCallbacks(base::BindLambdaForTesting([&] { ++data_ready_calls; }),
                   /*space_available_cb=*/base::DoNothing());
  buf.Push(MakeBytes(50));
  EXPECT_EQ(data_ready_calls, 0);

  FastForwardBy(kOneWayLatency);
  EXPECT_EQ(data_ready_calls, 1);
}

TEST_F(BottleneckBufferTest, ZeroLatencyDataReadyCallbackFiresAfterTaskHop) {
  // With zero latency, pushed data is immediately ready (its
  // time_available == now). data_ready_cb_ must still fire — but on a
  // posted task hop, not synchronously from Push() — so the consumer's
  // re-entrancy contract holds even on a fast loopback link.
  BottleneckBuffer buf{/*one_way_latency=*/base::TimeDelta()};
  int data_ready_calls = 0;
  buf.SetCallbacks(base::BindLambdaForTesting([&] { ++data_ready_calls; }),
                   base::DoNothing());

  buf.Push(MakeBytes(50));
  EXPECT_EQ(data_ready_calls, 0);  // Posted, not run inline.

  // Drain the zero-delay drain timer + its posted dispatch.
  FastForwardBy(base::TimeDelta());
  EXPECT_EQ(data_ready_calls, 1);
}

TEST_F(BottleneckBufferTest, RebindCallbackFiresIfDataAlreadyReady) {
  // Avoids a lost-wakeup hazard for consumers that re-install callbacks
  // after data has already become ready. SetCallbacks() opportunistically
  // posts data_ready_cb_ if data is already ready at registration time.
  BottleneckBuffer buf(kOneWayLatency);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  buf.Push(MakeBytes(50));
  FastForwardBy(kOneWayLatency);
  ASSERT_TRUE(buf.has_ready_data());

  // Re-install the callback — the producer-side state is already
  // "ready", but a consumer that swaps in a new callback here would
  // otherwise miss the drain timer's earlier signal.
  int data_ready_calls = 0;
  buf.SetCallbacks(base::BindLambdaForTesting([&] { ++data_ready_calls; }),
                   base::DoNothing());
  // The callback is posted, not run sync.
  EXPECT_EQ(data_ready_calls, 0);
  FastForwardBy(base::TimeDelta());
  EXPECT_EQ(data_ready_calls, 1);
}

TEST_F(BottleneckBufferTest,
       LateBoundCallbackDoesNotFireSpaceAvailableForEmptyBuffer) {
  // A freshly-constructed empty buffer is trivially "not full", and firing
  // space_available there would be a false positive.
  BottleneckBuffer buf(kOneWayLatency, /*capacity=*/100);
  int space_available_calls = 0;
  buf.SetCallbacks(base::DoNothing(), base::BindLambdaForTesting(
                                          [&] { ++space_available_calls; }));
  FastForwardBy(base::TimeDelta());
  EXPECT_EQ(space_available_calls, 0);
}

TEST_F(BottleneckBufferTest, SpaceAvailableCallbackFiresWhenDraining) {
  BottleneckBuffer buf(kOneWayLatency, /*capacity=*/100);
  int space_available_calls = 0;
  buf.SetCallbacks(base::DoNothing(), base::BindLambdaForTesting(
                                          [&] { ++space_available_calls; }));
  // Fills capacity.
  buf.Push(MakeBytes(100));
  ASSERT_TRUE(buf.full());

  FastForwardBy(kOneWayLatency);
  std::vector<uint8_t> dest(50);
  EXPECT_EQ(buf.Pull(base::span(dest)), 50);
  // The callback is posted, not run sync.
  EXPECT_FALSE(buf.full());
  FastForwardBy(base::TimeDelta());
  EXPECT_EQ(space_available_calls, 1);
}

// --- Reset() and future callbacks ---
TEST_F(BottleneckBufferTest, ResetInvalidatesPostedSpaceAvailable) {
  BottleneckBuffer buf(kOneWayLatency, /*capacity=*/100);
  int space_available_calls = 0;
  buf.SetCallbacks(base::DoNothing(), base::BindLambdaForTesting(
                                          [&] { ++space_available_calls; }));
  // Fill to capacity, wait out latency, then Pull — Pull posts the
  // space-available dispatch to be run async.
  buf.Push(MakeBytes(100));
  FastForwardBy(kOneWayLatency);
  std::vector<uint8_t> dest(50);
  EXPECT_EQ(buf.Pull(base::span(dest)), 50);
  EXPECT_FALSE(buf.full());
  EXPECT_EQ(space_available_calls, 0);

  // Reset before the dispatch fires.
  buf.Reset();
  FastForwardBy(base::TimeDelta());
  EXPECT_EQ(space_available_calls, 0);
}

TEST_F(BottleneckBufferTest, ResetInvalidatesPostedDataReady) {
  // Mirrors ResetInvalidatesPostedSpaceAvailable, but for the data-ready
  // path: a DispatchDataReady task posted before Reset() must be dropped
  // because Reset() bumps the generation counter.
  BottleneckBuffer buf(kOneWayLatency);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  buf.Push(MakeBytes(50));
  FastForwardBy(kOneWayLatency);
  ASSERT_TRUE(buf.has_ready_data());

  // Re-installing posts DispatchDataReady immediately because data is
  // already ready at registration time.
  int data_ready_calls = 0;
  buf.SetCallbacks(base::BindLambdaForTesting([&] { ++data_ready_calls; }),
                   base::DoNothing());
  EXPECT_EQ(data_ready_calls, 0);  // Posted, not run inline.

  // Reset before the dispatch runs — the in-flight task is tagged with the
  // old generation and must be ignored.
  buf.Reset();
  FastForwardBy(base::TimeDelta());
  EXPECT_EQ(data_ready_calls, 0);
}

TEST_F(BottleneckBufferTest, ResetAllowsFutureCallbacks) {
  BottleneckBuffer buf(kOneWayLatency);
  int data_ready_calls = 0;
  int space_available_calls = 0;
  buf.SetCallbacks(
      base::BindLambdaForTesting([&] { ++data_ready_calls; }),
      base::BindLambdaForTesting([&] { ++space_available_calls; }));
  buf.Push(MakeBytes(50));
  FastForwardBy(kOneWayLatency);
  ASSERT_EQ(data_ready_calls, 1);

  buf.Reset();

  // After Reset() the generation counter has been bumped. A fresh Push
  // still triggers data_ready_cb_ on the new generation.
  buf.Push(MakeBytes(50));
  FastForwardBy(kOneWayLatency);
  EXPECT_EQ(data_ready_calls, 2);
}

// --- Destruction during callbacks ---

// Destroying the BottleneckBuffer from within one of its callbacks must
// not crash. The buffer posts its callbacks as separate tasks, so the
// consumer's destruction happens after the emitting buffer method has
// returned.
TEST_F(BottleneckBufferTest, DestroyFromDataReadyCallbackDoesNotCrash) {
  auto buffer = std::make_unique<BottleneckBuffer>(kOneWayLatency);
  BottleneckBuffer* buffer_raw = buffer.get();
  int data_ready_calls = 0;
  buffer_raw->SetCallbacks(base::BindLambdaForTesting([&] {
                             ++data_ready_calls;
                             buffer.reset();
                           }),
                           base::DoNothing());
  buffer_raw->Push(MakeBytes(50));
  FastForwardBy(kOneWayLatency);
  EXPECT_EQ(data_ready_calls, 1);
  EXPECT_FALSE(buffer);
}

TEST_F(BottleneckBufferTest, DestroyFromSpaceAvailableCallbackDoesNotCrash) {
  auto buffer = std::make_unique<BottleneckBuffer>(kOneWayLatency,
                                                   /*capacity=*/100);
  BottleneckBuffer* buffer_raw = buffer.get();
  int space_available_calls = 0;
  buffer_raw->SetCallbacks(base::DoNothing(), base::BindLambdaForTesting([&] {
                             ++space_available_calls;
                             buffer.reset();
                           }));
  buffer_raw->Push(MakeBytes(100));
  FastForwardBy(kOneWayLatency);
  std::vector<uint8_t> dest(100);
  EXPECT_EQ(buffer_raw->Pull(base::span(dest)), 100);
  ASSERT_TRUE(buffer);
  EXPECT_EQ(space_available_calls, 0);  // Posted, not run inline.
  FastForwardBy(base::TimeDelta());
  EXPECT_EQ(space_available_calls, 1);
  EXPECT_FALSE(buffer);
}

// --- Reentrant Push from inside data_ready_cb_ ---

// The data_ready_cb_ may legitimately re-Push into the same buffer
// (e.g., a consumer that triggers more producer work on each ready
// signal). Push() does not call any consumer callbacks synchronously,
// so the buffer's invariants stay consistent. This test also pins the
// "invitation to drain" contract documented on DataReadyCallback: a
// fresh Push() while ready data is still buffered re-fires the
// callback, even though no fresh empty→ready edge occurred. See the
// inline timeline for the exact firing sequence.
TEST_F(BottleneckBufferTest, PushFromDataReadyCallbackDoesNotCrash) {
  BottleneckBuffer buf(kOneWayLatency);
  int data_ready_calls = 0;
  bool nested_push_done = false;
  buf.SetCallbacks(base::BindLambdaForTesting([&] {
                     ++data_ready_calls;
                     if (!nested_push_done) {
                       nested_push_done = true;
                       buf.Push(MakeBytes(50, 'Q'));
                     }
                   }),
                   base::DoNothing());

  buf.Push(MakeBytes(50, 'P'));

  // chunk_p crosses its latency. data_ready_cb_ fires twice at this
  // instant: once for chunk_p becoming ready, then again because the
  // nested Push('Q') schedules a fresh zero-delay drain that finds
  // chunk_p still in front and ready (the "invitation to drain" model
  // re-fires rather than coalescing).
  FastForwardBy(kOneWayLatency);
  EXPECT_EQ(data_ready_calls, 2);
  EXPECT_TRUE(nested_push_done);
  EXPECT_EQ(buf.buffered_bytes(), 100u);  // P + Q both buffered.

  // chunk_q's deadline arrives, but data_ready_cb_ does not fire
  // again: once the consumer has been notified, rearming for
  // subsequent chunks is Pull's responsibility.
  FastForwardBy(kOneWayLatency);
  EXPECT_EQ(data_ready_calls, 2);
  EXPECT_EQ(buf.buffered_bytes(), 100u);
}

// --- Getters ---

TEST_F(BottleneckBufferTest, BufferedBytesAndCapacityGetters) {
  BottleneckBuffer buf(kOneWayLatency, /*capacity=*/512);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  EXPECT_EQ(buf.capacity(), 512u);
  EXPECT_EQ(buf.buffered_bytes(), 0u);
  buf.Push(MakeBytes(100));
  EXPECT_EQ(buf.buffered_bytes(), 100u);
  EXPECT_EQ(buf.free_space(), 412u);
}

TEST_F(BottleneckBufferTest, GetReadyBytesAtFrontRejectsZeroMax) {
  BottleneckBuffer buf(kOneWayLatency);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  buf.Push(MakeBytes(50));
  FastForwardBy(kOneWayLatency);
  EXPECT_EQ(buf.GetReadyBytesAtFront(0), 0u);
  // A max larger than the ready bytes returns the full ready amount.
  EXPECT_EQ(buf.GetReadyBytesAtFront(std::numeric_limits<size_t>::max()), 50u);
}

// --- Reset ---

TEST_F(BottleneckBufferTest, ResetClearsState) {
  BottleneckBuffer buf(kOneWayLatency);
  buf.SetCallbacks(base::DoNothing(), base::DoNothing());
  buf.Push(MakeBytes(100));
  FastForwardBy(kOneWayLatency);
  EXPECT_TRUE(buf.has_ready_data());

  buf.Reset();
  EXPECT_TRUE(buf.empty());
  EXPECT_EQ(buf.buffered_bytes(), 0u);
  EXPECT_FALSE(buf.has_ready_data());
}

// --- Death tests ---
//
// Use bare TEST() rather than a TEST_F() fixture, and deliberately do NOT
// install a TaskEnvironment, so the death-test child forks without a thread
// pool running. Forking a process with a live TaskEnvironment thread pool is
// racy and was observed to flake on the bots. None of these CHECKs need a
// task runner: the constructor CHECKs fire during construction, and the
// SetCallbacks() null CHECKs fire before the buffer touches any sequenced
// state or posts a task.

TEST(BottleneckBufferDeathTest, ZeroCapacityCHECKs) {
  EXPECT_CHECK_DEATH(BottleneckBuffer(kOneWayLatency, /*capacity=*/0));
}

TEST(BottleneckBufferDeathTest, NegativeLatencyCHECKs) {
  EXPECT_CHECK_DEATH(BottleneckBuffer(base::Milliseconds(-1)));
}

TEST(BottleneckBufferDeathTest, NullDataReadyCallbackCHECKs) {
  // Callbacks must be callable; suppress a signal with base::DoNothing()
  // rather than a null callback.
  BottleneckBuffer buf(base::Milliseconds(100));
  EXPECT_CHECK_DEATH(buf.SetCallbacks(BottleneckBuffer::DataReadyCallback(),
                                      base::DoNothing()));
}

TEST(BottleneckBufferDeathTest, NullSpaceAvailableCallbackCHECKs) {
  BottleneckBuffer buf(base::Milliseconds(100));
  EXPECT_CHECK_DEATH(buf.SetCallbacks(
      base::DoNothing(), BottleneckBuffer::SpaceAvailableCallback()));
}

}  // namespace
}  // namespace net
