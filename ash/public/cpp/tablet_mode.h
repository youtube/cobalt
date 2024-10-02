// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TABLET_MODE_H_
#define ASH_PUBLIC_CPP_TABLET_MODE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/run_loop.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// An interface implemented by Ash that allows Chrome to be informed of changes
// to tablet mode state.
class ASH_PUBLIC_EXPORT TabletMode {
 public:
  // Helper class to wait until the tablet mode transition is complete.
  class Waiter : public TabletModeObserver {
   public:
    explicit Waiter(bool enable);

    Waiter(const Waiter&) = delete;
    Waiter& operator=(const Waiter&) = delete;

    ~Waiter() override;

    void Wait();

    // TabletModeObserver:
    void OnTabletModeStarted() override;
    void OnTabletModeEnded() override;

   private:
    bool enable_;
    base::RunLoop run_loop_;
  };

  // Returns true if the device's board is tablet mode capable.
  static bool IsBoardTypeMarkedAsTabletCapable();

  // Returns the singleton instance.
  static TabletMode* Get();

  virtual void AddObserver(TabletModeObserver* observer) = 0;
  virtual void RemoveObserver(TabletModeObserver* observer) = 0;

  // Returns true if the system is in tablet mode.
  virtual bool InTabletMode() const = 0;

  // Returns true if TabletMode singleton exists and is in the tablet mode.
  static bool IsInTabletMode();

  // Force the tablet mode state for integration tests. The meaning of |enabled|
  // are as follows:
  //   true: UI in the tablet mode
  //   false: UI in the clamshell mode
  //   nullopt: reset the forcing, UI in the default behavior (i.e. checking the
  //   physical state).
  // Returns true if it actually initiates the change of the tablet mode state.
  virtual bool ForceUiTabletModeState(absl::optional<bool> enabled) = 0;

  // Enable/disable the tablet mode. Used only by test cases.
  virtual void SetEnabledForTest(bool enabled) = 0;

 protected:
  TabletMode();
  virtual ~TabletMode();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TABLET_MODE_H_
