// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/glic/widget/glic_window_resize_animation.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace glic {
namespace {

constexpr base::TimeDelta kTestAnimationDuration = base::Milliseconds(300);

void Append(std::string* a, const char* b) {
  *a += b;
}

base::OnceClosure BindAppend(std::string* a, const char* b) {
  return base::BindOnce(&Append, a, b);
}

// Tests for size and position animations on the glic window. The results of
// these animations can be affected by the widget's minimum size (the same as
// the initial size at the time of writing) and by logic that repositions the
// widget to be entirely on screen.
class GlicWindowResizeAnimationTest : public test::InteractiveGlicTest {
 public:
  void SetUpOnMainThread() override {
    test::InteractiveGlicTest::SetUpOnMainThread();
    RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached));
  }

  void ExpectRectBetween(const gfx::Rect& current_rect,
                         const gfx::Rect& initial_rect,
                         const gfx::Rect& final_rect) {
    ExpectValueBetween(current_rect.x(), initial_rect.x(), final_rect.x());
    ExpectValueBetween(current_rect.y(), initial_rect.y(), final_rect.y());
    ExpectValueBetween(current_rect.width(), initial_rect.width(),
                       final_rect.width());
    ExpectValueBetween(current_rect.height(), initial_rect.height(),
                       final_rect.height());
  }

  std::pair<std::unique_ptr<GlicWindowResizeAnimation>,
            std::unique_ptr<gfx::AnimationTestApi>>
  MakeAnimation(const gfx::Rect& target_bounds,
                base::TimeDelta duration,
                base::OnceClosure callback) {
    auto animation = std::make_unique<GlicWindowResizeAnimation>(
        &window_controller(), window_controller().window_animator(),
        target_bounds, duration, std::move(callback));
    auto test_api = std::make_unique<gfx::AnimationTestApi>(animation.get());
    test_api->SetStartTime(animation_creation_time_);
    return {std::move(animation), std::move(test_api)};
  }

  gfx::Rect GetWidgetBounds() {
    return window_controller().GetGlicWidget()->GetWindowBoundsInScreen();
  }

 protected:
  void ExpectValueBetween(int current, int initial, int final) {
    if (initial < final) {
      EXPECT_TRUE(initial <= current);
      EXPECT_TRUE(current <= final);
    } else {
      EXPECT_TRUE(final <= current);
      EXPECT_TRUE(current <= initial);
    }
  }

  base::TimeTicks animation_creation_time() { return animation_creation_time_; }

 private:
  const base::TimeTicks animation_creation_time_ = base::TimeTicks::Now();
};
}  // namespace

IN_PROC_BROWSER_TEST_F(GlicWindowResizeAnimationTest, ExpandsWidgetSize) {
  gfx::Rect test_initial_bounds = GetWidgetBounds();
  gfx::Rect test_new_bounds;
  test_new_bounds.set_origin(test_initial_bounds.origin());
  test_new_bounds.set_size(gfx::Size(400, 600));

  auto [animation, test_api] =
      MakeAnimation(test_new_bounds, kTestAnimationDuration, base::DoNothing());
  test_api->Step(animation_creation_time() + kTestAnimationDuration -
                 base::Milliseconds(100));
  ExpectRectBetween(GetWidgetBounds(), test_initial_bounds, test_new_bounds);

  test_api->Step(animation_creation_time() + kTestAnimationDuration);
  EXPECT_EQ(test_new_bounds.size(), GetWidgetBounds().size());
}

IN_PROC_BROWSER_TEST_F(GlicWindowResizeAnimationTest, ShrinksWidgetSize) {
  // The widget may be starting at its minimum size, so in order to test
  // shrinking, it must grow first.
  gfx::Rect test_initial_bounds(GetWidgetBounds().origin(), {400, 400});
  auto [_, test_api_0] = MakeAnimation(
      test_initial_bounds, kTestAnimationDuration, base::DoNothing());
  test_api_0->Step(animation_creation_time() + kTestAnimationDuration);
  EXPECT_EQ(test_initial_bounds, GetWidgetBounds());

  // Now test getting smaller.
  gfx::Rect test_new_bounds(test_initial_bounds.origin(), {380, 380});
  auto [animation, test_api] =
      MakeAnimation(test_new_bounds, kTestAnimationDuration, base::DoNothing());
  test_api->Step(animation_creation_time() + kTestAnimationDuration -
                 base::Milliseconds(100));
  ExpectRectBetween(GetWidgetBounds(), test_initial_bounds, test_new_bounds);

  test_api->Step(animation_creation_time() + kTestAnimationDuration);
  EXPECT_EQ(test_new_bounds, GetWidgetBounds());
}

// TODO(419581863): Disabled on Linux because some Linux testing environments
// use a screen size of 0x0 which causes the window position to be clamped. This
// doesn't reflect a real condition that the code under test needs to handle so
// the test is disabled.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_MovesAndChangesWidgetSize DISABLED_MovesAndChangesWidgetSize
#else
#define MAYBE_MovesAndChangesWidgetSize MovesAndChangesWidgetSize
#endif
IN_PROC_BROWSER_TEST_F(GlicWindowResizeAnimationTest,
                       MAYBE_MovesAndChangesWidgetSize) {
  gfx::Rect test_initial_bounds = GetWidgetBounds();
  gfx::Rect test_new_bounds(
      gfx::Point(test_initial_bounds.x() - 10, test_initial_bounds.y() + 10),
      gfx::Size(400, 400));

  auto [animation, test_api] =
      MakeAnimation(test_new_bounds, kTestAnimationDuration, base::DoNothing());
  test_api->Step(animation_creation_time() + kTestAnimationDuration -
                 base::Milliseconds(100));
  ExpectRectBetween(GetWidgetBounds(), test_initial_bounds, test_new_bounds);

  test_api->Step(animation_creation_time() + kTestAnimationDuration +
                 base::Milliseconds(100));
  EXPECT_EQ(test_new_bounds, GetWidgetBounds());
}

IN_PROC_BROWSER_TEST_F(GlicWindowResizeAnimationTest, UpdateTargetPosition) {
  gfx::Rect initial_bounds = GetWidgetBounds();
  gfx::Rect target_bounds_1(initial_bounds.origin(), {400, 400});

  // Start changing size and position
  auto [animation, test_api] =
      MakeAnimation(target_bounds_1, kTestAnimationDuration, base::DoNothing());
  test_api->Step(animation_creation_time() + kTestAnimationDuration -
                 base::Milliseconds(100));
  ExpectRectBetween(GetWidgetBounds(), initial_bounds, target_bounds_1);

  // Update position, advance to the end
  gfx::Point new_target_point(50, 50);
  gfx::Rect new_target(new_target_point, target_bounds_1.size());
  animation->UpdateTargetBounds(new_target, base::DoNothing());
  test_api->Step(animation_creation_time() + kTestAnimationDuration);

  // Widget should now have the latest target origin and size.
  EXPECT_EQ(gfx::Rect(new_target_point, target_bounds_1.size()),
            GetWidgetBounds());
}

IN_PROC_BROWSER_TEST_F(GlicWindowResizeAnimationTest, UpdateTargetSize) {
  gfx::Rect initial_bounds = GetWidgetBounds();
  gfx::Rect target_bounds_1(100, 100, 400, 400);

  // Start changing size and position
  auto [animation, test_api] =
      MakeAnimation(target_bounds_1, kTestAnimationDuration, base::DoNothing());
  test_api->Step(animation_creation_time() + kTestAnimationDuration -
                 base::Milliseconds(100));
  ExpectRectBetween(GetWidgetBounds(), initial_bounds, target_bounds_1);

  // Update size, advance to the end
  gfx::Size new_target_size(500, 500);
  gfx::Rect new_target(target_bounds_1.origin(), new_target_size);
  animation->UpdateTargetBounds(new_target, base::DoNothing());
  test_api->Step(animation_creation_time() + kTestAnimationDuration);

  // Widget should now have the latest target origin and size.
  EXPECT_EQ(gfx::Rect(target_bounds_1.origin(), new_target_size),
            GetWidgetBounds());
}

// TODO(419581863): Disabled on Linux because some Linux testing environments
// use a screen size of 0x0 which causes the window position to be clamped. This
// doesn't reflect a real condition that the code under test needs to handle so
// the test is disabled.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_AllCallbacksRunInOrder DISABLED_AllCallbacksRunInOrder
#else
#define MAYBE_AllCallbacksRunInOrder AllCallbacksRunInOrder
#endif
IN_PROC_BROWSER_TEST_F(GlicWindowResizeAnimationTest,
                       MAYBE_AllCallbacksRunInOrder) {
  gfx::Rect initial_bounds = GetWidgetBounds();
  gfx::Rect target_bounds_1(initial_bounds.origin(), {400, 400});

  std::string call_log;

  // Start animating.
  auto [animation, test_api] = MakeAnimation(
      target_bounds_1, kTestAnimationDuration, BindAppend(&call_log, "1"));
  test_api->Step(animation_creation_time() + kTestAnimationDuration -
                 base::Milliseconds(100));
  ExpectRectBetween(GetWidgetBounds(), initial_bounds, target_bounds_1);

  // Make some updates, being careful to avoid reaching the right edge of the
  // screen so the x position isn't clamped.
  gfx::Rect target_bounds_2(target_bounds_1.origin() + gfx::Vector2d(-500, 10),
                            gfx::Size(500, 500));
  animation->UpdateTargetBounds(target_bounds_2, BindAppend(&call_log, " 2"));
  gfx::Rect target_bounds_3(target_bounds_2.origin(), gfx::Size(600, 600));
  animation->UpdateTargetBounds(target_bounds_3, BindAppend(&call_log, " 3"));
  gfx::Rect target_bounds_4(target_bounds_3.origin() - gfx::Vector2d(10, 10),
                            target_bounds_3.size());
  animation->UpdateTargetBounds(target_bounds_4, BindAppend(&call_log, " 4"));

  // Advance to the end. Widget should be at its final bounds.
  test_api->Step(animation_creation_time() + kTestAnimationDuration);
  EXPECT_EQ(target_bounds_4, GetWidgetBounds());

  // Callbacks should run in the order in which they were added.
  test_api.reset();
  animation.reset();
  // TODO: Find another way to wait for the tasks to finish.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("1 2 3 4", call_log);
}

}  // namespace glic
