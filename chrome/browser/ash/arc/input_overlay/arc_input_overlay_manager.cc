// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_manager.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/input_overlay_resources_util.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/manager/display_manager.h"
#include "ui/wm/core/window_util.h"

namespace arc::input_overlay {
namespace {

// TODO(b/301518561): Add a function in GD to check whether the window is GD
// window.
constexpr char kGameDashboardButton[] = "GameDashboardButton";
constexpr char kGameDashboardMainMenu[] = "GameDashboardMainMenuView";

// Singleton factory for ArcInputOverlayManager.
class ArcInputOverlayManagerFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcInputOverlayManager,
          ArcInputOverlayManagerFactory> {
 public:
  static constexpr const char* kName = "ArcInputOverlayManagerFactory";

  static ArcInputOverlayManagerFactory* GetInstance() {
    return base::Singleton<ArcInputOverlayManagerFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ArcInputOverlayManagerFactory>;
  ArcInputOverlayManagerFactory() = default;
  ~ArcInputOverlayManagerFactory() override = default;
};

// Check if the window is still loading as a ghost window.
bool IsGhostWindowLoading(aura::Window* window) {
  DCHECK(window);
  if (!window->GetProperty(app_restore::kRealArcTaskWindow)) {
    return true;
  }
  // TODO(b/258308970): This is a workaround.
  // `GetProperty(app_restore::kRealArcTaskWindow)` doesn't give an expected
  // value. So check if the window is still loading as a ghost window by
  // checking if there is an overlay.
  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(window);
  return shell_surface_base && shell_surface_base->HasOverlay();
}

void CheckWriteResult(std::string package_name, bool result) {
  if (result) {
    return;
  }
  LOG(ERROR) << "Failed to write proto for " << package_name;
}

}  // namespace

class ArcInputOverlayManager::InputMethodObserver
    : public ui::InputMethodObserver {
 public:
  explicit InputMethodObserver(ArcInputOverlayManager* owner) : owner_(owner) {}
  InputMethodObserver(const InputMethodObserver&) = delete;
  InputMethodObserver& operator=(const InputMethodObserver&) = delete;
  ~InputMethodObserver() override = default;

  // ui::InputMethodObserver overrides:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override {
    owner_->is_text_input_active_ =
        client && client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE &&
        client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NULL;
    owner_->NotifyTextInputState();
  }
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {
    owner_->input_method_ = nullptr;
  }

 private:
  raw_ptr<ArcInputOverlayManager> const owner_;
};

// static
ArcInputOverlayManager* ArcInputOverlayManager::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcInputOverlayManagerFactory::GetForBrowserContext(context);
}

ArcInputOverlayManager::ArcInputOverlayManager(
    content::BrowserContext* browser_context,
    ::arc::ArcBridgeService* arc_bridge_service)
    : input_method_observer_(std::make_unique<InputMethodObserver>(this)) {
  if (aura::Env::HasInstance()) {
    env_observation_.Observe(aura::Env::GetInstance());
  }
  if (ash::Shell::HasInstance()) {
    if (ash::Shell::Get()->tablet_mode_controller()) {
      ash::Shell::Get()->tablet_mode_controller()->AddObserver(this);
    }

    if (ash::Shell::Get()->display_manager()) {
      ash::Shell::Get()->display_manager()->AddObserver(this);
    }

    if (ash::Shell::GetPrimaryRootWindow()) {
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
          ->AddObserver(this);
    }
  }
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       // Should not block shutdown.
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  // For test. The unittest is based on ExoTestBase which must run on
  // Chrome_UIThread. While TestingProfileManager::CreateTestingProfile runs on
  // MainThread.
  if (browser_context) {
    data_controller_ =
        std::make_unique<DataController>(*browser_context, task_runner_);
  }
}

ArcInputOverlayManager::~ArcInputOverlayManager() = default;

// static
void ArcInputOverlayManager::EnsureFactoryBuilt() {
  ArcInputOverlayManagerFactory::GetInstance();
}

void ArcInputOverlayManager::OnWindowInitialized(aura::Window* new_window) {
  // `aura::client::kAppType` property is set in
  // `AppServiceAppWindowShelfController::OnWindowInitialized()`.
  // `AppServiceAppWindowShelfController::OnWindowInitialized()` is called
  // before `ArcInputOverlayManager::OnWindowInitialized()`, so we can filter
  // non-ARC apps here.
  if (!new_window || !ash::IsArcWindow(new_window) ||
      new_window != new_window->GetToplevelWindow() ||
      window_observations_.IsObservingSource(new_window)) {
    return;
  }

  window_observations_.AddObservation(new_window);
}

void ArcInputOverlayManager::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  // There are two cases when launching an app.
  // 1) Launch from Launcher: Receive {ash::kArcPackageNameKey, package_name}.
  // 2) Restore the app: Receive {ash::kArcPackageNameKey, package_name} and
  // {app_restore::kRealArcTaskWindow, true}. When `ash::kArcPackageNameKey` is
  // changed, the ghost window overlay is not destroyed. The ghost window
  // overlay is destroyed right before property
  // {app_restore::kRealArcTaskWindow} is set.
  if (!window || (key != ash::kArcPackageNameKey &&
                  key != app_restore::kRealArcTaskWindow)) {
    return;
  }

  DCHECK_EQ(window, window->GetToplevelWindow());
  if (input_overlay_enabled_windows_.contains(window) ||
      IsGhostWindowLoading(window) || loading_data_windows_.contains(window)) {
    return;
  }
  std::string* package_name = window->GetProperty(ash::kArcPackageNameKey);
  if (!package_name || package_name->empty()) {
    return;
  }

  // Start to read data.
  auto touch_injector = std::make_unique<TouchInjector>(
      window, *package_name,
      base::BindRepeating(&ArcInputOverlayManager::OnSaveProtoFile,
                          weak_ptr_factory_.GetWeakPtr()));
  loading_data_windows_.insert(window);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ArcInputOverlayManager::ReadDefaultData,
                     std::move(touch_injector)),
      base::BindOnce(&ArcInputOverlayManager::OnFinishReadDefaultData,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcInputOverlayManager::OnWindowDestroying(aura::Window* window) {
  UnregisterAndRemoveObservation(window);
}

void ArcInputOverlayManager::OnWindowAddedToRootWindow(aura::Window* window) {
  if (!window ||
      ash::window_util::GetFocusedWindow()->GetToplevelWindow() != window) {
    return;
  }
  RegisterWindow(window);
}

void ArcInputOverlayManager::OnWindowRemovingFromRootWindow(
    aura::Window* window,
    aura::Window* new_root) {
  if (!window) {
    return;
  }
  // There might be child window surface removing, we don't unregister window
  // until the top_level_window is removed from the root.
  UnRegisterWindow(window);
}

void ArcInputOverlayManager::OnWindowParentChanged(aura::Window* window,
                                                   aura::Window* parent) {
  // Ignore if `parent` is a container.
  if (!parent || parent != parent->GetToplevelWindow()) {
    return;
  }
  UnregisterAndRemoveObservation(window);
}

void ArcInputOverlayManager::Shutdown() {
  UnRegisterWindow(registered_top_level_window_);
  window_observations_.RemoveAllObservations();
  if (ash::Shell::HasInstance()) {
    if (ash::Shell::GetPrimaryRootWindow()) {
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
          ->RemoveObserver(this);
    }

    if (ash::Shell::Get()->tablet_mode_controller()) {
      ash::Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
    }

    if (ash::Shell::Get()->display_manager()) {
      ash::Shell::Get()->display_manager()->RemoveObserver(this);
    }
  }
  if (aura::Env::HasInstance()) {
    env_observation_.Reset();
  }
}

void ArcInputOverlayManager::OnWindowFocused(aura::Window* gained_focus,
                                             aura::Window* lost_focus) {
  if (ash::Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    return;
  }

  aura::Window* lost_focus_top_level_window = nullptr;
  aura::Window* gained_focus_top_level_window = nullptr;

  if (lost_focus) {
    lost_focus_top_level_window = lost_focus->GetToplevelWindow();
  }

  if (gained_focus) {
    gained_focus_top_level_window = gained_focus->GetToplevelWindow();
  }

  // For beta, the window is still considered as focused when the focus is on
  // its transient sibling window or the game dashboard main menu.
  if (IsBeta()) {
    // If `gained_window_by_transient` is nullptr, it means
    // `gained_focus_top_level_window` is nullptr or not a transient window.
    auto* gained_window_by_transient =
        gained_focus_top_level_window
            ? wm::GetTransientParent(gained_focus_top_level_window)
            : nullptr;
    // If `lost_window_by_transient` is nullptr, it means
    // `lost_focus_top_level_window` is nullptr or not a transient window.
    auto* lost_window_by_transient =
        lost_focus_top_level_window
            ? wm::GetTransientParent(lost_focus_top_level_window)
            : nullptr;

    // Check if the focus is on the GD main menu view.
    auto* gained_window_by_main_menu =
        GetGameWindow(gained_focus_top_level_window);

    auto* lost_window_by_main_menu = GetGameWindow(lost_focus_top_level_window);

    auto* real_gained_window =
        gained_window_by_main_menu
            ? gained_window_by_main_menu
            : (gained_window_by_transient ? gained_window_by_transient
                                          : gained_focus_top_level_window);
    auto* real_lost_window =
        lost_window_by_main_menu
            ? lost_window_by_main_menu
            : (lost_window_by_transient ? lost_window_by_transient
                                        : lost_focus_top_level_window);

    if (real_gained_window == real_lost_window) {
      return;
    }

    UnRegisterWindow(real_lost_window);
    RegisterWindow(real_gained_window);
  } else {
    if (lost_focus_top_level_window == gained_focus_top_level_window) {
      return;
    }

    UnRegisterWindow(lost_focus_top_level_window);
    RegisterWindow(gained_focus_top_level_window);
  }
}

void ArcInputOverlayManager::OnTabletModeStarting() {
  UnRegisterWindow(registered_top_level_window_);
}

void ArcInputOverlayManager::OnTabletModeEnded() {
  RegisterFocusedWindow();
}

void ArcInputOverlayManager::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (!registered_top_level_window_) {
    return;
  }

  auto it = input_overlay_enabled_windows_.find(registered_top_level_window_);
  if (it == input_overlay_enabled_windows_.end()) {
    return;
  }

  it->second->UpdatePositionsForRegister();
}

void ArcInputOverlayManager::RemoveWindowObservation(aura::Window* window) {
  if (window_observations_.IsObservingSource(window)) {
    window_observations_.RemoveObservation(window);
  }
}

void ArcInputOverlayManager::UnregisterAndRemoveObservation(
    aura::Window* window) {
  UnRegisterWindow(window);
  input_overlay_enabled_windows_.erase(window);
  loading_data_windows_.erase(window);
  RemoveWindowObservation(window);
}

// static
std::unique_ptr<TouchInjector> ArcInputOverlayManager::ReadDefaultData(
    std::unique_ptr<TouchInjector> touch_injector) {
  DCHECK(touch_injector);

  const std::string& package_name = touch_injector->package_name();
  auto resource_id = GetInputOverlayResourceId(package_name);
  if (!resource_id) {
    return touch_injector;
  }

  auto json_file = ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
      resource_id.value());
  if (json_file.empty()) {
    LOG(WARNING) << "No content for: " << package_name;
    return touch_injector;
  }
  auto result = base::JSONReader::ReadAndReturnValueWithError(json_file);
  DCHECK(result.has_value())
      << "Could not load input overlay data file: " << result.error().message;
  if (!result.has_value() || !result->is_dict()) {
    return touch_injector;
  }

  touch_injector->ParseActions(result->GetDict());
  return touch_injector;
}

void ArcInputOverlayManager::OnFinishReadDefaultData(
    std::unique_ptr<TouchInjector> touch_injector) {
  DCHECK(touch_injector);

  if (!IsBeta() && touch_injector->actions().empty()) {
    ResetForPendingTouchInjector(std::move(touch_injector));
    return;
  }

  // Null for unit test.
  if (!data_controller_) {
    OnProtoDataAvailable(std::move(touch_injector), /*proto=*/nullptr);
    return;
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DataController::ReadProtoFromFile,
                     data_controller_->GetFilePathFromPackageName(
                         touch_injector->package_name())),
      base::BindOnce(&ArcInputOverlayManager::OnProtoDataAvailable,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(touch_injector)));
}

void ArcInputOverlayManager::OnDidCheckGioApplicable(
    std::unique_ptr<TouchInjector> touch_injector,
    bool is_gio_applicable) {
  // For Optimized-for-Chromebook (O4C) games (which shouldn't apply GIO), GIO
  // still applies if the associated `TouchInjector` is not empty. For example,
  // users may have the mapping customization before games become O4C games and
  // users may want to keep the previous mapping.
  if (!is_gio_applicable && touch_injector->actions().empty()) {
    ResetForPendingTouchInjector(std::move(touch_injector));
  } else {
    OnLoadingFinished(std::move(touch_injector));
  }
}

void ArcInputOverlayManager::OnProtoDataAvailable(
    std::unique_ptr<TouchInjector> touch_injector,
    std::unique_ptr<AppDataProto> proto) {
  DCHECK(touch_injector);
  if (proto) {
    touch_injector->OnProtoDataAvailable(*proto);
  } else {
    touch_injector->NotifyFirstTimeLaunch();
  }

  if (!IsBeta()) {
    DCHECK(!touch_injector->actions().empty());
    OnLoadingFinished(std::move(touch_injector));
    return;
  }

  // Check if GIO is applicable from mojom instance.
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    LOG(ERROR) << "Failed to get ArcServiceManager";
    MayKeepTouchInjectorAfterError(std::move(touch_injector));
    return;
  }
  auto* compatibility_mode =
      arc_service_manager->arc_bridge_service()->compatibility_mode();
  if (!compatibility_mode || !compatibility_mode->IsConnected()) {
    LOG(ERROR) << "No supported Android connection.";
    MayKeepTouchInjectorAfterError(std::move(touch_injector));
    return;
  }
  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(compatibility_mode, IsGioApplicable);
  if (!instance) {
    LOG(ERROR) << "IsGioApplicable method for ARC is not available";
    MayKeepTouchInjectorAfterError(std::move(touch_injector));
    return;
  }

  std::string package_name = touch_injector->package_name();
  VLOG(2) << "Check if GIO applicable on package: " << package_name;
  instance->IsGioApplicable(
      package_name,
      base::BindOnce(&ArcInputOverlayManager::OnDidCheckGioApplicable,
                     Unretained(this), std::move(touch_injector)));
}

void ArcInputOverlayManager::OnSaveProtoFile(
    std::unique_ptr<AppDataProto> proto,
    std::string package_name) {
  if (!data_controller_) {
    return;
  }
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &DataController::WriteProtoToFile, std::move(proto),
          data_controller_->GetFilePathFromPackageName(package_name)),
      base::BindOnce(&CheckWriteResult, package_name));
}

void ArcInputOverlayManager::NotifyTextInputState() {
  auto it = input_overlay_enabled_windows_.find(registered_top_level_window_);
  if (it != input_overlay_enabled_windows_.end()) {
    it->second->NotifyTextInputState(is_text_input_active_);
  }
}

void ArcInputOverlayManager::AddObserverToInputMethod() {
  if (!registered_top_level_window_) {
    return;
  }
  DCHECK(registered_top_level_window_->GetHost());
  DCHECK(!input_method_);
  input_method_ = registered_top_level_window_->GetHost()->GetInputMethod();
  if (input_method_) {
    input_method_->AddObserver(input_method_observer_.get());
  }
}

void ArcInputOverlayManager::RemoveObserverFromInputMethod() {
  if (!input_method_) {
    return;
  }
  input_method_->RemoveObserver(input_method_observer_.get());
  input_method_ = nullptr;
}

void ArcInputOverlayManager::RegisterWindow(aura::Window* window) {
  // Only register the focused window that is not registered.
  if (!window || window != window->GetToplevelWindow() ||
      registered_top_level_window_ == window) {
    return;
  }

  // It should always unregister the window first, then register another window.
  DCHECK(!registered_top_level_window_);

  // For Beta version, it may focus on its transient sibling window.
  if (!IsBeta()) {
    DCHECK_EQ(ash::window_util::GetFocusedWindow()->GetToplevelWindow(),
              window);
  }

  auto it = input_overlay_enabled_windows_.find(window);
  if (it == input_overlay_enabled_windows_.end()) {
    return;
  }
  it->second->RegisterEventRewriter();
  registered_top_level_window_ = window;
  AddObserverToInputMethod();
  AddDisplayOverlayController(it->second.get());
  // If the window is on the extended window, it turns out only primary root
  // window catches the key event. So it needs to forward the key event from
  // primary root window to extended root window event source.
  if (registered_top_level_window_->GetRootWindow() !=
      ash::Shell::GetPrimaryRootWindow()) {
    key_event_source_rewriter_ =
        std::make_unique<KeyEventSourceRewriter>(registered_top_level_window_);
  }
}

void ArcInputOverlayManager::UnRegisterWindow(aura::Window* window) {
  if (!registered_top_level_window_ || registered_top_level_window_ != window) {
    return;
  }
  auto it = input_overlay_enabled_windows_.find(registered_top_level_window_);
  DCHECK(it != input_overlay_enabled_windows_.end());
  if (it == input_overlay_enabled_windows_.end()) {
    return;
  }
  if (key_event_source_rewriter_) {
    key_event_source_rewriter_.reset();
  }
  it->second->UnRegisterEventRewriter();
  RemoveDisplayOverlayController();
  RemoveObserverFromInputMethod();
  it->second->NotifyTextInputState(false);
  registered_top_level_window_ = nullptr;
}

void ArcInputOverlayManager::RegisterFocusedWindow() {
  auto* focused_window = ash::window_util::GetFocusedWindow();
  // Don't register window if it is in tablet mode.
  if (ash::Shell::Get()->tablet_mode_controller()->InTabletMode() ||
      !focused_window) {
    return;
  }

  auto* top_level_window = focused_window->GetToplevelWindow();
  if (input_overlay_enabled_windows_.find(top_level_window) ==
          input_overlay_enabled_windows_.end() &&
      IsBeta()) {
    RegisterWindow(GetGameWindow(top_level_window));
  } else {
    RegisterWindow(top_level_window);
  }
}

void ArcInputOverlayManager::AddDisplayOverlayController(
    TouchInjector* touch_injector) {
  DCHECK(registered_top_level_window_);
  DCHECK(touch_injector);
  if (!registered_top_level_window_ || !touch_injector) {
    return;
  }
  DCHECK(!display_overlay_controller_);

  display_overlay_controller_ = std::make_unique<DisplayOverlayController>(
      touch_injector, touch_injector->first_launch());
}

void ArcInputOverlayManager::RemoveDisplayOverlayController() {
  if (!registered_top_level_window_) {
    return;
  }

  // There is only one `display_overlay_controller_` active at a time. When
  // window is destroyed, the attached sibling window is destroyed first, which
  // triggers the window focus change. And then it also triggers the window
  // unregister and gets `display_overlay_controller_` reset before here.
  if (!IsBeta()) {
    DCHECK(display_overlay_controller_);
  }

  if (display_overlay_controller_) {
    display_overlay_controller_.reset();
  }
}

void ArcInputOverlayManager::ResetForPendingTouchInjector(
    std::unique_ptr<TouchInjector> touch_injector) {
  auto* window = touch_injector->window();

  // If `window` is destroyed, it will be removed from `loading_data_window_` by
  // OnWindowDestroying(). So it is safe to call Window class functions after
  // checking `loading_data_window_`.
  if ((IsGameDashboardFlagOn() || IsBeta()) &&
      loading_data_windows_.contains(window) && !window->is_destroying()) {
    // GIO status is known here and GIO is not available.
    window->SetProperty(ash::kArcGameControlsFlagsKey,
                        ash::ArcGameControlsFlag::kKnown);
  }
  loading_data_windows_.erase(window);
  touch_injector.reset();
  RemoveWindowObservation(window);
}

void ArcInputOverlayManager::OnLoadingFinished(
    std::unique_ptr<TouchInjector> touch_injector) {
  auto* window = touch_injector->window();
  DCHECK(window);
  // Check if `window` is destroyed or destroying when calling this function.
  if (!loading_data_windows_.contains(window) || window->is_destroying()) {
    ResetForPendingTouchInjector(std::move(touch_injector));
    return;
  }

  touch_injector->UpdateFlags();

  // Record the menu state when there is at least one action.
  if (!touch_injector->actions().empty()) {
    touch_injector->RecordMenuStateOnLaunch();
  }

  input_overlay_enabled_windows_.emplace(window, std::move(touch_injector));
  loading_data_windows_.erase(window);
  RegisterFocusedWindow();
}

void ArcInputOverlayManager::MayKeepTouchInjectorAfterError(
    std::unique_ptr<TouchInjector> touch_injector) {
  if (touch_injector->actions().empty()) {
    ResetForPendingTouchInjector(std::move(touch_injector));
  } else {
    OnLoadingFinished(std::move(touch_injector));
  }
}

aura::Window* ArcInputOverlayManager::GetGameWindow(aura::Window* window) {
  // TODO(b/301518561): Add a function in GD to check whether the window is GD
  // window.
  if (!window || (window->GetName() != kGameDashboardMainMenu &&
                  window->GetName() != kGameDashboardButton)) {
    return nullptr;
  }

  auto* widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  if (window->GetName() == kGameDashboardMainMenu) {
    DCHECK(widget->parent());
    DCHECK(widget->parent()->GetNativeWindow());
    return wm::GetTransientParent(widget->parent()->GetNativeWindow());
  }

  DCHECK_EQ(window->GetName(), kGameDashboardButton);
  return wm::GetTransientParent(widget->GetNativeWindow());
}

}  // namespace arc::input_overlay
