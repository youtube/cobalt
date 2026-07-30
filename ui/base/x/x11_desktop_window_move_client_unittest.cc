// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_desktop_window_move_client.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

namespace {

class TestDelegate : public X11DesktopWindowMoveClient::Delegate {
 public:
  TestDelegate() = default;
  ~TestDelegate() override = default;

  void SetBoundsOnMove(const gfx::Rect& requested_bounds) override {
    set_bounds_on_move_called_ = true;
  }

  scoped_refptr<X11Cursor> GetLastCursor() override {
    return nullptr;
  }

  gfx::Size GetSize() override {
    get_size_called_ = true;
    if (destroy_on_get_size_) {
      // Synchronously destroy the client (which models the destruction flow in
      // production when X11Window is destroyed and in turn deletes its move client).
      move_client_holder_->reset();
    }
    return gfx::Size(100, 100);
  }

  base::WeakPtr<X11DesktopWindowMoveClient::Delegate> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void Init(std::unique_ptr<X11DesktopWindowMoveClient>* holder,
            bool destroy_on_get_size) {
    move_client_holder_ = holder;
    destroy_on_get_size_ = destroy_on_get_size;
  }

  bool get_size_called() const { return get_size_called_; }
  bool set_bounds_on_move_called() const { return set_bounds_on_move_called_; }

 private:
  raw_ptr<std::unique_ptr<X11DesktopWindowMoveClient>> move_client_holder_ = nullptr;
  bool destroy_on_get_size_ = false;
  bool get_size_called_ = false;
  bool set_bounds_on_move_called_ = false;

  base::WeakPtrFactory<TestDelegate> weak_ptr_factory_{this};
};

}  // namespace

TEST(X11DesktopWindowMoveClientTest, SafeOnMouseMovementWithDestruction) {
  TestDelegate delegate;
  auto move_client = std::make_unique<X11DesktopWindowMoveClient>(&delegate);
  delegate.Init(&move_client, /*destroy_on_get_size=*/true);

  // Trigger OnMouseMovement. Inside GetSize(), the delegate will destroy
  // `move_client` synchronously.
  move_client->OnMouseMovement(gfx::Point(10, 10), 0, base::TimeTicks());

  // Confirm that the move client was indeed destroyed.
  EXPECT_FALSE(move_client);
  EXPECT_TRUE(delegate.get_size_called());
  // SetBoundsOnMove should NOT have been called, as the delegate and the client
  // are destroyed/invalidated.
  EXPECT_FALSE(delegate.set_bounds_on_move_called());
}

TEST(X11DesktopWindowMoveClientTest, OnMouseMovementWithoutDestruction) {
  TestDelegate delegate;
  auto move_client = std::make_unique<X11DesktopWindowMoveClient>(&delegate);
  delegate.Init(&move_client, /*destroy_on_get_size=*/false);

  move_client->OnMouseMovement(gfx::Point(10, 10), 0, base::TimeTicks());

  EXPECT_TRUE(move_client);
  EXPECT_TRUE(delegate.get_size_called());
  EXPECT_TRUE(delegate.set_bounds_on_move_called());
}

}  // namespace ui
