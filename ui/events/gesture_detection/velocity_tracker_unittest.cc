// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/notreached.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/gesture_detection/velocity_tracker_state.h"
#include "ui/events/test/motion_event_test_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

using base::TimeTicks;
using ui::test::MockMotionEvent;

namespace ui {
namespace {

const base::TimeDelta kTenMillis = base::Milliseconds(10);
const base::TimeDelta kOneSecond = base::Seconds(1);
const float kEpsilson = .01f;

const char* GetStrategyName(VelocityTracker::Strategy strategy) {
  switch (strategy) {
    case VelocityTracker::LSQ1: return "LSQ1";
    case VelocityTracker::LSQ2: return "LSQ2";
    case VelocityTracker::LSQ2_RESTRICTED: return "LSQ2_RESTRICTED";
    case VelocityTracker::LSQ3: return "LSQ3";
    case VelocityTracker::WLSQ2_DELTA: return "WLSQ2_DELTA";
    case VelocityTracker::WLSQ2_CENTRAL: return "WLSQ2_CENTRAL";
    case VelocityTracker::WLSQ2_RECENT: return "WLSQ2_RECENT";
    case VelocityTracker::INT1: return "INT1";
    case VelocityTracker::INT2: return "INT2";
  }
  NOTREACHED() << "Invalid strategy";
  return "";
}

}  // namespace

class VelocityTrackerTest : public testing::Test {
 public:
  VelocityTrackerTest() {}
  ~VelocityTrackerTest() override {}

 protected:
  static MockMotionEvent Sample(MotionEvent::Action action,
                                const gfx::PointF& p0,
                                TimeTicks t0,
                                const gfx::Vector2dF& v,
                                base::TimeDelta dt) {
    const gfx::PointF p = p0 + ScaleVector2d(v, dt.InSecondsF());
    return MockMotionEvent(action, t0 + dt, p.x(), p.y());
  }

  static void ApplyMovementSequence(VelocityTrackerState* state,
                                    const gfx::PointF& p0,
                                    const gfx::Vector2dF& v,
                                    TimeTicks t0,
                                    base::TimeDelta t,
                                    size_t samples) {
    EXPECT_TRUE(samples);
    if (!samples)
      return;
    const base::TimeDelta dt = t / samples;
    state->AddMovement(Sample(MotionEvent::Action::DOWN, p0, t0, v, dt * 0));
    ApplyMovement(state, p0, v, t0, t, samples);
    state->AddMovement(Sample(MotionEvent::Action::UP, p0, t0, v, t));
  }

  static void ApplyMovement(VelocityTrackerState* state,
                            const gfx::PointF& p0,
                            const gfx::Vector2dF& v,
                            TimeTicks t0,
                            base::TimeDelta t,
                            size_t samples) {
    EXPECT_TRUE(samples);
    if (!samples)
      return;
    const base::TimeDelta dt = t / samples;
    for (size_t i = 0; i < samples; ++i)
      state->AddMovement(Sample(MotionEvent::Action::MOVE, p0, t0, v, dt * i));
  }
};

TEST_F(VelocityTrackerTest, Basic) {
  const gfx::PointF p0(0, 0);
  const gfx::Vector2dF v(0, 500);
  const size_t samples = 60;

  for (int i = 0; i <= VelocityTracker::STRATEGY_MAX; ++i) {
    VelocityTracker::Strategy strategy =
        static_cast<VelocityTracker::Strategy>(i);

    SCOPED_TRACE(GetStrategyName(strategy));
    VelocityTrackerState state(strategy);

    // Default state should report zero velocity.
    EXPECT_EQ(0, state.GetXVelocity(0));
    EXPECT_EQ(0, state.GetYVelocity(0));

    // Sample a constant velocity sequence.
    ApplyMovementSequence(&state, p0, v, TimeTicks::Now(), kOneSecond, samples);

    // The computed velocity should match that of the input.
    state.ComputeCurrentVelocity(1000, 20000);
    EXPECT_NEAR(v.x(), state.GetXVelocity(0), kEpsilson * v.x());
    EXPECT_NEAR(v.y(), state.GetYVelocity(0), kEpsilson * v.y());

    // A pointer ID of -1 should report the velocity of the active pointer.
    EXPECT_NEAR(v.x(), state.GetXVelocity(-1), kEpsilson * v.x());
    EXPECT_NEAR(v.y(), state.GetYVelocity(-1), kEpsilson * v.y());

    // Invalid pointer ID's should report zero velocity.
    EXPECT_EQ(0, state.GetXVelocity(1));
    EXPECT_EQ(0, state.GetYVelocity(1));
    EXPECT_EQ(0, state.GetXVelocity(7));
    EXPECT_EQ(0, state.GetYVelocity(7));
  }
}

TEST_F(VelocityTrackerTest, MaxVelocity) {
  const gfx::PointF p0(0, 0);
  const gfx::Vector2dF v(-50000, 50000);
  const size_t samples = 3;
  const base::TimeDelta dt = kTenMillis * 2;

  VelocityTrackerState state(VelocityTracker::Strategy::LSQ2);
  ApplyMovementSequence(&state, p0, v, TimeTicks::Now(), dt, samples);

  // The computed velocity should be restricted to the provided maximum.
  state.ComputeCurrentVelocity(1000, 100);
  EXPECT_NEAR(-100, state.GetXVelocity(0), kEpsilson);
  EXPECT_NEAR(100, state.GetYVelocity(0), kEpsilson);

  state.ComputeCurrentVelocity(1000, 1000);
  EXPECT_NEAR(-1000, state.GetXVelocity(0), kEpsilson);
  EXPECT_NEAR(1000, state.GetYVelocity(0), kEpsilson);
}

TEST_F(VelocityTrackerTest, VaryingVelocity) {
  const gfx::PointF p0(0, 0);
  const gfx::Vector2dF vFast(0, 500);
  const gfx::Vector2dF vSlow = ScaleVector2d(vFast, 0.5f);
  const size_t samples = 12;

  for (int i = 0; i <= VelocityTracker::STRATEGY_MAX; ++i) {
    VelocityTracker::Strategy strategy =
        static_cast<VelocityTracker::Strategy>(i);

    SCOPED_TRACE(GetStrategyName(strategy));
    VelocityTrackerState state(strategy);

    base::TimeTicks t0 = base::TimeTicks::Now();
    base::TimeDelta dt = kTenMillis * 10;
    state.AddMovement(
        Sample(MotionEvent::Action::DOWN, p0, t0, vFast, base::TimeDelta()));

    // Apply some fast movement and compute the velocity.
    gfx::PointF pCurr = p0;
    base::TimeTicks tCurr = t0;
    ApplyMovement(&state, pCurr, vFast, tCurr, dt, samples);
    state.ComputeCurrentVelocity(1000, 20000);
    float vOldY = state.GetYVelocity(0);

    // Apply some slow movement.
    pCurr += ScaleVector2d(vFast, dt.InSecondsF());
    tCurr += dt;
    ApplyMovement(&state, pCurr, vSlow, tCurr, dt, samples);

    // The computed velocity should have decreased.
    state.ComputeCurrentVelocity(1000, 20000);
    float vCurrentY = state.GetYVelocity(0);
    EXPECT_GT(vFast.y(), vCurrentY);
    EXPECT_GT(vOldY, vCurrentY);
    vOldY = vCurrentY;

    // Apply some additional fast movement.
    pCurr += ScaleVector2d(vSlow, dt.InSecondsF());
    tCurr += dt;
    ApplyMovement(&state, pCurr, vFast, tCurr, dt, samples);

    // The computed velocity should have increased.
    state.ComputeCurrentVelocity(1000, 20000);
    vCurrentY = state.GetYVelocity(0);
    EXPECT_LT(vSlow.y(), vCurrentY);
    EXPECT_LT(vOldY, vCurrentY);
  }
}

TEST_F(VelocityTrackerTest, DelayedActionUp) {
  const gfx::PointF p0(0, 0);
  const gfx::Vector2dF v(-50000, 50000);
  const size_t samples = 10;
  const base::TimeTicks t0 = base::TimeTicks::Now();
  const base::TimeDelta dt = kTenMillis * 2;

  VelocityTrackerState state(VelocityTracker::Strategy::LSQ2);
  state.AddMovement(
      Sample(MotionEvent::Action::DOWN, p0, t0, v, base::TimeDelta()));

  // Apply the movement and verify a (non-zero) velocity.
  ApplyMovement(&state, p0, v, t0, dt, samples);
  state.ComputeCurrentVelocity(1000, 1000);
  EXPECT_NEAR(-1000, state.GetXVelocity(0), kEpsilson);
  EXPECT_NEAR(1000, state.GetYVelocity(0), kEpsilson);

  // Apply the delayed Action::UP.
  const gfx::PointF p1 = p0 + ScaleVector2d(v, dt.InSecondsF());
  const base::TimeTicks t1 = t0 + dt + kTenMillis * 10;
  state.AddMovement(
      Sample(MotionEvent::Action::UP, p1, t1, v, base::TimeDelta()));

  // The tracked velocity should have been reset.
  state.ComputeCurrentVelocity(1000, 1000);
  EXPECT_EQ(0.f, state.GetXVelocity(0));
  EXPECT_EQ(0.f, state.GetYVelocity(0));
}

// Tests that a rapid deceleration won't result in a velocity going in the
// opposite direction to the pointers primary movement, with the LSQ_RESTRICTED
// strategy. See crbug.com/417855.
TEST_F(VelocityTrackerTest, NoDirectionReversal) {
  VelocityTrackerState state_unrestricted(VelocityTracker::LSQ2);
  VelocityTrackerState state_restricted(VelocityTracker::LSQ2_RESTRICTED);
  const base::TimeTicks t0 = base::TimeTicks::Now();
  const base::TimeDelta dt = base::Milliseconds(1);
  const size_t samples = 60;

  gfx::PointF p(0, 0);

  MockMotionEvent m1(MotionEvent::Action::DOWN, t0, p.x(), p.y());
  state_unrestricted.AddMovement(m1);
  state_restricted.AddMovement(m1);

  for (size_t i = 0; i < samples; ++i) {
    if (i < 50)
      p.set_y(p.y() + 10);
    MockMotionEvent mi(MotionEvent::Action::MOVE, t0 + dt * i, p.x(), p.y());
    state_unrestricted.AddMovement(mi);
    state_restricted.AddMovement(mi);
  }

  // The computed velocity be zero, as we stopped at the end of the gesture. In
  // particular, it should not be negative, as all movement was in the positive
  // direction.
  state_restricted.ComputeCurrentVelocity(1000, 20000);
  EXPECT_EQ(0, state_restricted.GetXVelocity(0));
  EXPECT_EQ(0, state_restricted.GetYVelocity(0));

  // This does not hold for the unrestricted LSQ2 strategy.
  state_unrestricted.ComputeCurrentVelocity(1000, 20000);
  EXPECT_EQ(0, state_unrestricted.GetXVelocity(0));
  // Y velocity is negative, despite the fact that the finger only moved in the
  // positive y direction.
  EXPECT_GT(0, state_unrestricted.GetYVelocity(0));
}

}  // namespace ui
