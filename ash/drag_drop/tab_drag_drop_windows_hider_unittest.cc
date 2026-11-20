// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/tab_drag_drop_windows_hider.h"

#include "ash/test/ash_test_base.h"

namespace ash {

class TabDragDropWindowsHiderTest : public AshTestBase {
 public:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    dummy_window_ = CreateToplevelTestWindow();
  }

  void TearDown() override {
    dummy_window_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<aura::Window> dummy_window_;
};

class DeleteOnHideObserver : public aura::WindowObserver {
 public:
  explicit DeleteOnHideObserver(std::unique_ptr<aura::Window> window)
      : window_(std::move(window)) {
    window_->AddObserver(this);
  }

  DeleteOnHideObserver(const DeleteOnHideObserver&) = delete;
  DeleteOnHideObserver& operator=(const DeleteOnHideObserver&) = delete;

  ~DeleteOnHideObserver() override {
    if (window_) {
      window_->RemoveObserver(this);
    }
  }

  aura::Window* GetWindow() { return window_.get(); }

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (window == window_.get() && !visible) {
      window_->RemoveObserver(this);
      window_.reset();
    }
  }

 private:
  std::unique_ptr<aura::Window> window_;
};

// Test for crbug.com/1330038 .
TEST_F(TabDragDropWindowsHiderTest, WindowVisibilityChangedDuringDrag) {
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();

  // Create a sub window and hide it.
  std::unique_ptr<aura::Window> sub_window = CreateWindowWithAppType();
  dummy_window_->AddChild(sub_window.get());
  sub_window->Hide();
  auto hider = std::make_unique<TabDragDropWindowsHider>(source_window.get());
  int size = hider->GetWindowVisibilityMapSizeForTesting();

  // Show the sub window. Make sure the window observer list size remains the
  // same.
  sub_window->Show();
  EXPECT_EQ(size, hider->GetWindowVisibilityMapSizeForTesting());
  sub_window.reset();
}

// Tests that if a window is synchronously destroyed when the hider attempts to
// hide it (due to a re-entrant visibility change), the hider does not keep a
// dangling pointer to the destroyed window.
TEST_F(TabDragDropWindowsHiderTest, ReentrantDestroyDuringForcedHide) {
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();
  std::unique_ptr<aura::Window> tracked_window = CreateToplevelTestWindow();

  tracked_window->Show();
  source_window->Show();

  auto hider = std::make_unique<TabDragDropWindowsHider>(source_window.get());

  DeleteOnHideObserver observer(std::move(tracked_window));
  observer.GetWindow()->Show();

  // Destroy the hider. If it holds a dangling pointer, this will trigger a UAF
  // or crash (especially under ASAN).
  hider.reset();
}

// Tests that if a window is synchronously destroyed during the hider's
// constructor (when it force-hides all windows in the MRU list), the hider
// does not crash or keep a dangling pointer.
TEST_F(TabDragDropWindowsHiderTest, ReentrantDestroyDuringConstructorHide) {
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();
  std::unique_ptr<aura::Window> tracked_window = CreateToplevelTestWindow();

  tracked_window->Show();
  source_window->Show();

  DeleteOnHideObserver observer(std::move(tracked_window));

  // The constructor will hide `tracked_window`, triggering DeleteOnHideObserver
  // to destroy it synchronously.
  auto hider = std::make_unique<TabDragDropWindowsHider>(source_window.get());

  EXPECT_EQ(1, hider->GetWindowVisibilityMapSizeForTesting());
}

}  // namespace ash
