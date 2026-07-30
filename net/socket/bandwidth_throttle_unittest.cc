// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/bandwidth_throttle.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

// 1000 bytes/sec for easy math.
constexpr uint64_t kThroughput = 1000;
// 100ms burst = 100 bytes max burst.
constexpr base::TimeDelta kBurstDuration = base::Milliseconds(100);

class BandwidthThrottleTest : public testing::Test, public WithTaskEnvironment {
 public:
  BandwidthThrottleTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(BandwidthThrottleTest, SmallRequestServedImmediately) {
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);

  // Burst size is 100 bytes. A 50-byte request should be ready without
  // waiting, but completion is always delivered on a separate task so it
  // does not fire before RequestBytes returns.
  bool called = false;
  auto handle = throttle->RequestBytes(
      50, base::BindOnce([](bool* called) { *called = true; }, &called));
  EXPECT_FALSE(called);

  FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(called);
}

TEST_F(BandwidthThrottleTest, BurstExhaustedThenQueued) {
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);

  // Exhaust the burst (100 bytes).
  bool first_called = false;
  auto handle1 = throttle->RequestBytes(
      100, base::BindOnce([](bool* called) { *called = true; }, &first_called));
  // Completion is always async; drain the zero-delay task.
  FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(first_called);

  // Next request should be queued — need to wait for tokens.
  bool second_called = false;
  auto handle2 = throttle->RequestBytes(
      50, base::BindOnce([](bool* called) { *called = true; }, &second_called));
  EXPECT_FALSE(second_called);

  // 50 bytes at 1000 bytes/sec = 50ms.
  FastForwardBy(base::Milliseconds(49));
  EXPECT_FALSE(second_called);

  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(second_called);
}

TEST_F(BandwidthThrottleTest, MultipleRequestsFIFO) {
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);

  // Exhaust burst.
  auto initial = throttle->RequestBytes(100, base::DoNothing());

  // Queue three 50-byte requests.
  std::vector<int> completion_order;
  std::vector<BandwidthThrottle::CancellationHandle> handles;
  for (int i = 0; i < 3; i++) {
    handles.push_back(throttle->RequestBytes(
        50, base::BindOnce(
                [](std::vector<int>* order, int id) { order->push_back(id); },
                &completion_order, i)));
  }

  EXPECT_TRUE(completion_order.empty());

  // First request: 50ms.
  FastForwardBy(base::Milliseconds(50));
  ASSERT_EQ(completion_order.size(), 1u);
  EXPECT_EQ(completion_order[0], 0);

  // Second request: another 50ms.
  FastForwardBy(base::Milliseconds(50));
  ASSERT_EQ(completion_order.size(), 2u);
  EXPECT_EQ(completion_order[1], 1);

  // Third request: another 50ms.
  FastForwardBy(base::Milliseconds(50));
  ASSERT_EQ(completion_order.size(), 3u);
  EXPECT_EQ(completion_order[2], 2);
}

TEST_F(BandwidthThrottleTest, TokensRefillDuringIdle) {
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);

  // Exhaust burst.
  auto handle1 = throttle->RequestBytes(100, base::DoNothing());

  // Wait 100ms — tokens refill to burst_size (100).
  FastForwardBy(base::Milliseconds(100));

  // A 100-byte request should be ready without waiting, modulo the
  // mandatory async hop.
  bool called = false;
  auto handle2 = throttle->RequestBytes(
      100, base::BindOnce([](bool* called) { *called = true; }, &called));
  EXPECT_FALSE(called);
  FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(called);
}

TEST_F(BandwidthThrottleTest, TokensCappedAtBurstSize) {
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);

  // Wait a very long time — tokens should cap at burst_size (100).
  FastForwardBy(base::Seconds(10));

  // First 100 bytes are immediate (burst), next 50 bytes queued.
  auto handle1 = throttle->RequestBytes(100, base::DoNothing());

  bool called = false;
  auto handle2 = throttle->RequestBytes(
      50, base::BindOnce([](bool* called) { *called = true; }, &called));
  EXPECT_FALSE(called);

  FastForwardBy(base::Milliseconds(50));
  EXPECT_TRUE(called);
}

TEST_F(BandwidthThrottleTest, IdleTokensCappedAtBurstSizeAfterPriorRequest) {
  // Regression coverage for the "idle credit" cap in RefillTokens(). The
  // first request primes last_refill_time_ and partially drains the bucket,
  // leaving the queue empty. A long idle period then accumulates far more
  // than burst_size_ worth of tokens, which must be capped because the queue
  // is empty. (TokensCappedAtBurstSize exercises a fresh throttle whose first
  // RefillTokens() only initializes the timestamp, so it never reaches the
  // cap branch.)
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);

  // Prime: drains 50 tokens (100 -> 50) and sets last_refill_time_. The queue
  // is empty again once this completes.
  bool primed = false;
  auto handle1 = throttle->RequestBytes(
      50, base::BindLambdaForTesting([&]() { primed = true; }));
  FastForwardBy(base::TimeDelta());
  ASSERT_TRUE(primed);

  // Idle far longer than burst_duration. With the queue empty this is idle
  // credit: it must cap at burst_size_ (100), not 50 + 10s * 1000 B/s.
  FastForwardBy(base::Seconds(10));

  // Claim the whole (capped) burst.
  auto handle2 = throttle->RequestBytes(100, base::DoNothing());

  // If the idle credit had accumulated uncapped, this 50-byte request would
  // fire immediately. Because it was capped at burst_size_ and handle2 spent
  // it, this request must wait ~50ms for fresh tokens.
  bool capped = false;
  auto handle3 = throttle->RequestBytes(
      50, base::BindLambdaForTesting([&]() { capped = true; }));
  FastForwardBy(base::TimeDelta());
  EXPECT_FALSE(capped);
  FastForwardBy(base::Milliseconds(49));
  EXPECT_FALSE(capped);
  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(capped);
}

TEST_F(BandwidthThrottleTest, DestroyWithPendingRequests) {
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);

  // Exhaust burst and queue a request.
  auto handle1 = throttle->RequestBytes(100, base::DoNothing());

  bool called = false;
  auto handle2 = throttle->RequestBytes(
      50, base::BindOnce([](bool* called) { *called = true; }, &called));
  EXPECT_FALSE(called);

  // Destroying the throttle should not crash. Pending callback is
  // abandoned.
  throttle.reset();
  FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(called);
}

TEST_F(BandwidthThrottleTest, LargeRequestExceedingBurst) {
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);

  // Request 200 bytes. Burst is 100, so the request cannot be served
  // immediately. While the request is queued, tokens are allowed to
  // accumulate beyond burst_size_ so the request can eventually be
  // served atomically. Starting with 100 tokens available, the 100-byte
  // deficit takes 100ms at 1000 bytes/sec.
  bool called = false;
  auto handle = throttle->RequestBytes(
      200, base::BindOnce([](bool* called) { *called = true; }, &called));

  // 200 bytes requested, 100 available. Need 100 more at 1000 bps =
  // 100ms.
  EXPECT_FALSE(called);
  FastForwardBy(base::Milliseconds(99));
  EXPECT_FALSE(called);
  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(called);
}

TEST_F(BandwidthThrottleTest, MultipleLargeRequestsExceedingBurst) {
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);

  std::vector<int> completion_order;
  auto handle1 = throttle->RequestBytes(
      200, base::BindOnce([](std::vector<int>* order) { order->push_back(1); },
                          &completion_order));
  auto handle2 = throttle->RequestBytes(
      200, base::BindOnce([](std::vector<int>* order) { order->push_back(2); },
                          &completion_order));

  // The first 200-byte request starts with 100 burst tokens available,
  // so it needs 100ms to accumulate the remaining 100 tokens.
  EXPECT_TRUE(completion_order.empty());
  FastForwardBy(base::Milliseconds(99));
  EXPECT_TRUE(completion_order.empty());
  FastForwardBy(base::Milliseconds(1));
  ASSERT_EQ(completion_order.size(), 1u);
  EXPECT_EQ(completion_order[0], 1);

  // The first request consumes all 200 available tokens. The second
  // request is also larger than the burst size, so queued tokens must
  // accumulate beyond burst_size_ again. It needs the full 200ms at
  // 1000 bytes/sec.
  FastForwardBy(base::Milliseconds(199));
  ASSERT_EQ(completion_order.size(), 1u);
  FastForwardBy(base::Milliseconds(1));
  ASSERT_EQ(completion_order.size(), 2u);
  EXPECT_EQ(completion_order[1], 2);
}

TEST_F(BandwidthThrottleTest, ThroughputGetterReturnsConfiguredRate) {
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);
  EXPECT_EQ(throttle->throughput_bytes_per_sec(), kThroughput);
}

TEST_F(BandwidthThrottleTest, MultipleQueuedRequestsAreServedFIFO) {
  // Issue several requests back-to-back while the burst is exhausted.
  // Each must complete in submission order, evenly spaced at
  // |request_size / throughput|. The timer-sharing invariant
  // (RequestBytes does not start a fresh drain timer if one is already
  // running) is verified by inspection of RequestBytes.
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);
  // Exhaust the burst so the first request alone is enough to start the
  // timer.
  auto initial = throttle->RequestBytes(100, base::DoNothing());
  FastForwardBy(base::TimeDelta());

  std::vector<int> order;
  std::vector<BandwidthThrottle::CancellationHandle> handles;
  handles.push_back(throttle->RequestBytes(
      30,
      base::BindOnce([](std::vector<int>* o) { o->push_back(0); }, &order)));
  handles.push_back(throttle->RequestBytes(
      30,
      base::BindOnce([](std::vector<int>* o) { o->push_back(1); }, &order)));
  handles.push_back(throttle->RequestBytes(
      30,
      base::BindOnce([](std::vector<int>* o) { o->push_back(2); }, &order)));

  // 30 bytes at 1000 B/s = 30 ms per request, FIFO.
  FastForwardBy(base::Milliseconds(30));
  ASSERT_EQ(order.size(), 1u);
  FastForwardBy(base::Milliseconds(30));
  ASSERT_EQ(order.size(), 2u);
  FastForwardBy(base::Milliseconds(30));
  ASSERT_EQ(order.size(), 3u);
  EXPECT_EQ(order, (std::vector<int>{0, 1, 2}));
}

// --- Re-entrant RequestBytes from inside a throttle callback ---

TEST_F(BandwidthThrottleTest, ReentrantRequestFromCallbackDoesNotCrash) {
  // The throttle pins `this` in ProcessQueue with a local scoped_refptr
  // so a callback that drops the last external reference and re-enters
  // the throttle does not pull `this` out from under the loop.
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);

  bool first_done = false;
  bool second_done = false;
  BandwidthThrottle::CancellationHandle nested_handle;

  // First callback drops `throttle` (so external refcount hits zero
  // unless ProcessQueue's local scoped_refptr keeps `this` alive), then
  // re-enters the throttle via the same pointer.
  auto handle = throttle->RequestBytes(
      50, base::BindLambdaForTesting([&]() {
        first_done = true;
        BandwidthThrottle* raw = throttle.get();
        throttle.reset();
        nested_handle = raw->RequestBytes(
            50, base::BindLambdaForTesting([&]() { second_done = true; }));
      }));

  // Drain everything. First fires — raw RequestBytes from inside it
  // must not crash and must eventually deliver the second callback.
  FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(first_done);
  EXPECT_TRUE(second_done);
}

// --- Cancellation ---

TEST_F(BandwidthThrottleTest, CancelledRequestDoesNotConsumeTokens) {
  // A cancelled request must NOT charge tokens — that's the whole point
  // of cancellation in a shared throttle. Exhaust the burst, queue a
  // large cancelled request, then a small live request; the small one
  // should be served at the same rate it would have been served without
  // the cancelled request in front of it.
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);
  // Exhaust burst.
  auto initial_handle = throttle->RequestBytes(100, base::DoNothing());
  FastForwardBy(base::TimeDelta());

  bool large_called = false;
  auto large_handle = throttle->RequestBytes(
      500, base::BindLambdaForTesting([&]() { large_called = true; }));

  bool small_called = false;
  auto small_handle = throttle->RequestBytes(
      30, base::BindLambdaForTesting([&]() { small_called = true; }));

  // Cancel the large one before any tokens accumulate.
  large_handle.Cancel();

  // 30 bytes at 1000 B/s = 30 ms.
  FastForwardBy(base::Milliseconds(29));
  EXPECT_FALSE(small_called);
  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(small_called);
  EXPECT_FALSE(large_called);
}

TEST_F(BandwidthThrottleTest, CancellationHandleDestructorCancels) {
  // Implicit cancellation via CancellationHandle destruction — the common
  // case when a socket goes away while a request is pending.
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);
  auto initial_handle = throttle->RequestBytes(100, base::DoNothing());
  FastForwardBy(base::TimeDelta());

  bool large_called = false;
  {
    auto large_handle = throttle->RequestBytes(
        500, base::BindLambdaForTesting([&]() { large_called = true; }));
    // `large_handle` goes out of scope here, auto-cancelling.
  }

  bool small_called = false;
  auto small_handle = throttle->RequestBytes(
      30, base::BindLambdaForTesting([&]() { small_called = true; }));

  FastForwardBy(base::Milliseconds(30));
  EXPECT_TRUE(small_called);
  EXPECT_FALSE(large_called);
}

TEST_F(BandwidthThrottleTest, CancelOfCompletedRequestIsNoOp) {
  // Cancel() after the callback has already fired is a harmless no-op.
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);
  bool called = false;
  auto handle = throttle->RequestBytes(
      50, base::BindLambdaForTesting([&]() { called = true; }));
  FastForwardBy(base::TimeDelta());
  ASSERT_TRUE(called);
  // Should not crash and should not retroactively un-fire the callback.
  handle.Cancel();
  handle.Cancel();  // Idempotent.
}

TEST_F(BandwidthThrottleTest, CancellationHandleMoveCancelsPriorOnAssignment) {
  // Move-assignment cancels the previously-held handle so it
  // doesn't leak in the queue.
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);
  auto initial_handle = throttle->RequestBytes(100, base::DoNothing());
  FastForwardBy(base::TimeDelta());

  bool first_called = false;
  bool second_called = false;
  auto handle = throttle->RequestBytes(
      500, base::BindLambdaForTesting([&]() { first_called = true; }));
  // Move-assign a NEW request into `handle`; the previous 500-byte request
  // should be cancelled, freeing tokens for the second.
  handle = throttle->RequestBytes(
      30, base::BindLambdaForTesting([&]() { second_called = true; }));

  FastForwardBy(base::Milliseconds(30));
  EXPECT_TRUE(second_called);
  EXPECT_FALSE(first_called);
}

TEST_F(BandwidthThrottleTest, CancelEagerlyReschedulesDrainTimer) {
  // Regression for the "stale wait" hazard: when the head of the queue
  // is cancelled, the drain timer that was sized for it must be
  // re-evaluated so a small successor isn't starved by the cancelled
  // request's wait. CancellationHandle::Cancel() calls back into the throttle
  // to do exactly that.
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);
  auto initial_handle = throttle->RequestBytes(100, base::DoNothing());
  FastForwardBy(base::TimeDelta());

  // Big cancelled request at the head (10000 bytes / 1000 B/s = 10 s of
  // stale wait if we didn't reschedule on cancel).
  auto large_handle = throttle->RequestBytes(10000, base::DoNothing());
  // Live request behind it.
  bool small_called = false;
  auto small_handle = throttle->RequestBytes(
      30, base::BindLambdaForTesting([&]() { small_called = true; }));

  large_handle.Cancel();

  // 30 bytes at 1000 B/s = 30 ms — not 10 s.
  FastForwardBy(base::Milliseconds(29));
  EXPECT_FALSE(small_called);
  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(small_called);
}

TEST_F(BandwidthThrottleTest, SiblingCancellationFromCallbackDoesNotCrash) {
  // A fired ThrottleCallback may legitimately cancel a sibling pending
  // request. The throttle must survive that without re-arming a doomed
  // drain timer while ProcessQueue is still iterating, and the queue
  // state must remain consistent so subsequent live requests fire in
  // their expected order.
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);
  // Exhaust burst so all subsequent requests queue.
  auto initial_handle = throttle->RequestBytes(100, base::DoNothing());
  FastForwardBy(base::TimeDelta());

  BandwidthThrottle::CancellationHandle handle_b;
  bool a_done = false;
  bool b_done = false;
  bool c_done = false;

  // A's callback cancels B mid-loop. C should still be served.
  auto handle_a = throttle->RequestBytes(30, base::BindLambdaForTesting([&]() {
                                           a_done = true;
                                           handle_b.Cancel();
                                         }));
  handle_b = throttle->RequestBytes(
      30, base::BindLambdaForTesting([&]() { b_done = true; }));
  auto handle_c = throttle->RequestBytes(
      30, base::BindLambdaForTesting([&]() { c_done = true; }));

  // A serves at 30 ms, then B is skipped (cancelled), then C serves at
  // 60 ms. Without sibling-cancellation safety the cancel mid-callback
  // would crash or wedge the queue.
  FastForwardBy(base::Milliseconds(60));
  EXPECT_TRUE(a_done);
  EXPECT_FALSE(b_done);
  EXPECT_TRUE(c_done);
}

// --- Negative / null input is CHECKed ---
//
// These use bare TEST() rather than a TEST_F() fixture, and deliberately do
// NOT install a TaskEnvironment, so the death-test child forks without a
// thread pool running. Forking a process that has a live TaskEnvironment
// thread pool is racy and was observed to flake on the bots. None of these
// CHECKs need a task runner: the constructor CHECKs fire during construction,
// and the request-side CHECKs (bytes/callback validation) fire at the top of
// RequestBytes(), before it touches the token bucket, timer, or any posted
// task.

TEST(BandwidthThrottleDeathTest, ZeroBurstDurationCHECKs) {
  EXPECT_CHECK_DEATH(
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, base::TimeDelta()));
}

TEST(BandwidthThrottleDeathTest, NegativeBurstDurationCHECKs) {
  EXPECT_CHECK_DEATH(base::MakeRefCounted<BandwidthThrottle>(
      kThroughput, base::Milliseconds(-1)));
}

TEST(BandwidthThrottleDeathTest, ZeroThroughputCHECKs) {
  // Callers that want an unconstrained link must not construct a
  // throttle at all — passing 0 here would silently produce a 1 B/s
  // link in earlier revisions.
  EXPECT_CHECK_DEATH(
      base::MakeRefCounted<BandwidthThrottle>(0u, kBurstDuration));
}

TEST(BandwidthThrottleDeathTest, ZeroBytesRequestCHECKs) {
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);
  // The CHECK fires before the CancellationHandle is constructed, so binding it
  // is unnecessary; assign to a discard variable to silence [[nodiscard]].
  EXPECT_CHECK_DEATH(
      { auto unused = throttle->RequestBytes(0, base::DoNothing()); });
}

TEST(BandwidthThrottleDeathTest, NegativeBytesRequestCHECKs) {
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);
  EXPECT_CHECK_DEATH(
      { auto unused = throttle->RequestBytes(-50, base::DoNothing()); });
}

TEST(BandwidthThrottleDeathTest, NullCallbackCHECKs) {
  auto throttle =
      base::MakeRefCounted<BandwidthThrottle>(kThroughput, kBurstDuration);
  EXPECT_CHECK_DEATH({
    auto unused =
        throttle->RequestBytes(50, BandwidthThrottle::ThrottleCallback());
  });
}

}  // namespace
}  // namespace net
