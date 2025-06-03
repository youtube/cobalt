// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_util.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/auto_reset.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot_aura.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Helper functions ------------------------------------------------------------

void SnapshotCallback(base::RunLoop* run_loop,
                      gfx::Image* ret_image,
                      gfx::Image image) {
  *ret_image = image;
  run_loop->Quit();
}

// Returns the menu item view with `label` from the specified view.
views::MenuItemView* FindMenuItemWithLabelFromView(
    views::View* search_root,
    const std::u16string& label) {
  // Check whether `search_root` is the target menu item view.
  if (views::MenuItemView* const menu_item_view =
          views::AsViewClass<views::MenuItemView>(search_root);
      menu_item_view && menu_item_view->title() == label) {
    return menu_item_view;
  }

  // Keep searching in children views.
  for (views::View* const child : search_root->children()) {
    if (views::MenuItemView* const found =
            FindMenuItemWithLabelFromView(child, label)) {
      return found;
    }
  }

  return nullptr;
}

// Returns the menu item view with `label` from the specified window.
views::MenuItemView* FindMenuItemWithLabelFromWindow(
    aura::Window* search_root,
    const std::u16string& label) {
  // If `search_root` is a window for widget, search the target menu item view
  // in the view tree.
  if (views::Widget* const root_widget =
          views::Widget::GetWidgetForNativeWindow(search_root)) {
    return FindMenuItemWithLabelFromView(root_widget->GetRootView(), label);
  }

  for (aura::Window* const child : search_root->children()) {
    if (auto* found = FindMenuItemWithLabelFromWindow(child, label)) {
      return found;
    }
  }

  return nullptr;
}

// MenuItemViewWithLabelWaiter -------------------------------------------------

class MenuItemViewWithLabelWaiter : public aura::WindowObserver {
 public:
  explicit MenuItemViewWithLabelWaiter(const std::u16string& label)
      : label_(label),
        menu_container_(Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                            kShellWindowId_MenuContainer)) {
    CHECK(menu_container_) << "The menu container is expected to exist.";
  }

  // Waits until the target menu item view is found. Returns the pointer to the
  // target view.
  views::MenuItemView* Wait() {
    // Return early if the target view already exists.
    if ((cached_target_view_ =
             FindMenuItemWithLabelFromWindow(menu_container_, label_))) {
      return cached_target_view_;
    }

    base::AutoReset<std::unique_ptr<base::RunLoop>> auto_reset(
        &run_loop_, std::make_unique<base::RunLoop>());

    // Start the observation on `menu_container_`. A new menu should add
    // itself under `menu_container_`.
    base::ScopedObservation<aura::Window, aura::WindowObserver> observation{
        this};
    observation.Observe(menu_container_);

    run_loop_->Run();

    CHECK(cached_target_view_);
    return cached_target_view_;
  }

 private:
  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* window) override {
    // Menu items are built after the window is added. Therefore, check the
    // target menu item in an asynchronous task.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&MenuItemViewWithLabelWaiter::CacheTargetView,
                                  weak_factory_.GetWeakPtr()));
  }

  void OnWindowDestroying(aura::Window* window) override {
    CHECK(!run_loop_)
        << "The menu container was destroyed while we were waiting for "
           "the target menu item.";
  }

  void CacheTargetView() {
    if ((cached_target_view_ =
             FindMenuItemWithLabelFromWindow(menu_container_, label_))) {
      run_loop_->Quit();
    }
  }

  const std::u16string label_;
  const raw_ptr<aura::Window> menu_container_;
  std::unique_ptr<base::RunLoop> run_loop_;
  raw_ptr<views::MenuItemView> cached_target_view_ = nullptr;
  base::WeakPtrFactory<MenuItemViewWithLabelWaiter> weak_factory_{this};
};

}  // namespace

bool TakePrimaryDisplayScreenshotAndSave(const base::FilePath& file_path) {
  // Return false if the file extension is not "png".
  if (file_path.Extension() != ".png")
    return false;

  // Return false if `file_path`'s directory does not exist.
  const base::FilePath directory_name = file_path.DirName();
  if (!base::PathExists(directory_name))
    return false;

  base::RunLoop run_loop;
  gfx::Image image;
  ui::GrabWindowSnapshotAsyncAura(
      Shell::Get()->GetPrimaryRootWindow(),
      Shell::Get()->GetPrimaryRootWindow()->bounds(),
      base::BindOnce(&SnapshotCallback, &run_loop, &image));
  run_loop.Run();
  auto data = image.As1xPNGBytes();
  DCHECK_GT(data->size(), 0u);
  return base::WriteFile(file_path, *data);
}

void GiveItSomeTimeForDebugging(base::TimeDelta time_duration) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), time_duration);
  run_loop.Run();
}

bool IsSystemTrayForRootWindowVisible(size_t root_window_index) {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  RootWindowController* controller =
      RootWindowController::ForWindow(root_windows[root_window_index]);
  return controller->GetStatusAreaWidget()->unified_system_tray()->GetVisible();
}

gfx::ImageSkia CreateSolidColorTestImage(const gfx::Size& image_size,
                                         SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(image_size.width(), image_size.height());
  bitmap.eraseColor(color);
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  return image;
}

void DecorateWindow(aura::Window* window,
                    const std::u16string& title,
                    SkColor color) {
  auto* widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  widget->client_view()->AddChildView(
      views::Builder<views::View>()
          .SetBackground(views::CreateRoundedRectBackground(color, 4.f))
          .Build());

  // Add a title and an app icon so that the header is fully stocked.
  window->SetTitle(title);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(SK_ColorCYAN);
  window->SetProperty(aura::client::kAppIconKey,
                      gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
}

views::MenuItemView* WaitForMenuItemWithLabel(const std::u16string& label) {
  return MenuItemViewWithLabelWaiter(label).Wait();
}

}  // namespace ash
