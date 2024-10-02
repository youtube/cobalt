// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/test_controller_ash.h"

#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/input_method_test_interface_ash.h"
#include "chrome/browser/ash/crosapi/vpn_service_ash.h"
#include "chrome/browser/ash/crosapi/window_util.h"
#include "chrome/browser/ash/printing/cups_print_job_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/speech/tts_crosapi_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/tabs/tab_scrubber_chromeos.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_driver_client.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/tts_utterance.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_source.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"

#if BUILDFLAG(USE_CUPS)
#include "chrome/browser/ash/printing/cups_print_job.h"
#include "chrome/browser/ash/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_impl.h"
#include "chrome/browser/ash/printing/history/test_print_job_database.h"
#include "chrome/browser/ash/printing/test_cups_print_job_manager.h"
#endif  // BUILDFLAG(USE_CUPS)

namespace crosapi {

namespace {

// Returns whether the dispatcher or target was destroyed.
bool Dispatch(aura::WindowTreeHost* host, ui::Event* event) {
  ui::EventDispatchDetails dispatch_details =
      host->GetEventSource()->SendEventToSink(event);
  return dispatch_details.dispatcher_destroyed ||
         dispatch_details.target_destroyed;
}

// Returns whether the dispatcher or target was destroyed.
bool DispatchMouseEvent(aura::Window* window,
                        ui::EventType type,
                        gfx::Point location) {
  ui::MouseEvent press(type, location, location, ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  return Dispatch(window->GetHost(), &press);
}

// Enables or disables tablet mode and waits for the transition to finish.
void SetTabletModeEnabled(bool enabled) {
  // This does not use ShellTestApi or TabletModeControllerTestApi because those
  // are implemented in test-only files.
  ash::TabletMode::Waiter waiter(enabled);
  ash::Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enabled);
  waiter.Wait();
}

}  // namespace

TestControllerAsh::TestControllerAsh() = default;
TestControllerAsh::~TestControllerAsh() = default;

void TestControllerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::TestController> receiver) {
// This interface is not available on production devices. It's only needed for
// tests that run on Linux-chrome so no reason to expose it.
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  LOG(ERROR) << "Ash does not support TestController on devices";
#else
  receivers_.Add(this, std::move(receiver));
#endif
}

void TestControllerAsh::ClickElement(const std::string& element_name,
                                     ClickElementCallback callback) {
  ui::ElementIdentifier id =
      ui::ElementIdentifier::FromName(element_name.c_str());
  if (!id) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  auto views = views::ElementTrackerViews::GetInstance()
                   ->GetAllMatchingViewsInAnyContext(id);
  if (views.empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // Pick the first view that matches the element name.
  views::Button* button = views::Button::AsButton(views[0]);
  if (!button) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // We directly send mouse events to the view. It's also possible to use
  // EventGenerator to move the mouse and send a click. Unfortunately, that
  // approach has occasional flakiness. This is presumably due to another window
  // appearing on top of the dialog and taking the mouse events but has not been
  // explicitly diagnosed.
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      button, ui::test::InteractionTestUtil::InputType::kMouse);
  std::move(callback).Run(/*success=*/true);
}

void TestControllerAsh::ClickWindow(const std::string& window_id) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  if (!window)
    return;
  const gfx::Point center = window->bounds().CenterPoint();
  bool destroyed = DispatchMouseEvent(window, ui::ET_MOUSE_PRESSED, center);
  if (!destroyed) {
    DispatchMouseEvent(window, ui::ET_MOUSE_RELEASED, center);
  }
}

void TestControllerAsh::ConnectToNetwork(const std::string& service_path) {
  ash::ShillServiceClient::Get()->Connect(
      dbus::ObjectPath(service_path), base::DoNothing(),
      ash::ShillServiceClient::ErrorCallback());
}

void TestControllerAsh::DisconnectFromNetwork(const std::string& service_path) {
  ash::ShillServiceClient::Get()->Disconnect(
      dbus::ObjectPath(service_path), base::DoNothing(),
      ash::ShillServiceClient::ErrorCallback());
}

void TestControllerAsh::DoesItemExistInShelf(
    const std::string& item_id,
    DoesItemExistInShelfCallback callback) {
  bool exists = ash::ShelfModel::Get()->ItemIndexByAppID(item_id) != -1;
  std::move(callback).Run(exists);
}

void TestControllerAsh::DoesElementExist(const std::string& element_name,
                                         DoesElementExistCallback callback) {
  ui::ElementIdentifier id =
      ui::ElementIdentifier::FromName(element_name.c_str());
  if (!id) {
    std::move(callback).Run(/*exists=*/false);
    return;
  }

  bool any_elements_exist = !views::ElementTrackerViews::GetInstance()
                                 ->GetAllMatchingViewsInAnyContext(id)
                                 .empty();
  std::move(callback).Run(/*exists=*/any_elements_exist);
}

void TestControllerAsh::DoesWindowExist(const std::string& window_id,
                                        DoesWindowExistCallback callback) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  // A window exists if it is either visible or minimized.
  bool exists = false;
  if (window) {
    auto* window_state = ash::WindowState::Get(window);
    exists = window->IsVisible() || window_state->IsMinimized();
  }
  std::move(callback).Run(exists);
}

void TestControllerAsh::EnterOverviewMode(EnterOverviewModeCallback callback) {
  overview_waiters_.push_back(std::make_unique<OverviewWaiter>(
      /*wait_for_enter=*/true, std::move(callback), this));
  ash::Shell::Get()->overview_controller()->StartOverview(
      ash::OverviewStartAction::kTests);
}

void TestControllerAsh::ExitOverviewMode(ExitOverviewModeCallback callback) {
  overview_waiters_.push_back(std::make_unique<OverviewWaiter>(
      /*wait_for_enter=*/false, std::move(callback), this));
  ash::Shell::Get()->overview_controller()->EndOverview(
      ash::OverviewEndAction::kTests);
}

void TestControllerAsh::EnterTabletMode(EnterTabletModeCallback callback) {
  SetTabletModeEnabled(true);
  std::move(callback).Run();
}

void TestControllerAsh::ExitTabletMode(ExitTabletModeCallback callback) {
  SetTabletModeEnabled(false);
  std::move(callback).Run();
}

void TestControllerAsh::GetShelfItemState(const std::string& app_id,
                                          GetShelfItemStateCallback callback) {
  ash::RootWindowController* const controller =
      ash::Shell::GetRootWindowControllerWithDisplayId(
          display::Screen::GetScreen()->GetPrimaryDisplay().id());
  ash::ShelfView* const shelf_view =
      controller->shelf()->GetShelfViewForTesting();
  const ash::ShelfAppButton* const app_button =
      shelf_view->GetShelfAppButton(ash::ShelfID(app_id));
  uint32_t state = static_cast<uint32_t>(mojom::ShelfItemState::kNormal);
  if (app_button) {
    if (app_button->state() & ash::ShelfAppButton::STATE_ACTIVE)
      state = static_cast<uint32_t>(mojom::ShelfItemState::kActive);
    else if (app_button->state() & ash::ShelfAppButton::STATE_RUNNING)
      state = static_cast<uint32_t>(mojom::ShelfItemState::kRunning);

    if (app_button->state() & ash::ShelfAppButton::STATE_NOTIFICATION)
      state |= static_cast<uint32_t>(mojom::ShelfItemState::kNotification);
  }

  std::move(callback).Run(state);
}

void TestControllerAsh::GetContextMenuForShelfItem(
    const std::string& item_id,
    GetContextMenuForShelfItemCallback callback) {
  ash::ShelfItemDelegate* delegate =
      ash::ShelfModel::Get()->GetShelfItemDelegate(ash::ShelfID(item_id));
  if (!delegate) {
    std::move(callback).Run({});
    return;
  }
  delegate->GetContextMenu(
      /*display_id=*/0,
      base::BindOnce(&TestControllerAsh::OnGetContextMenuForShelfItem,
                     std::move(callback)));
}

void TestControllerAsh::GetMinimizeOnBackKeyWindowProperty(
    const std::string& window_id,
    GetMinimizeOnBackKeyWindowPropertyCallback cb) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  if (!window) {
    std::move(cb).Run(mojom::OptionalBoolean::kUnknown);
    return;
  }
  bool* value = window->GetProperty(ash::kMinimizeOnBackKey);
  if (!value) {
    std::move(cb).Run(mojom::OptionalBoolean::kUnknown);
    return;
  }
  std::move(cb).Run(*value ? mojom::OptionalBoolean::kTrue
                           : mojom::OptionalBoolean::kFalse);
}

void TestControllerAsh::GetWindowPositionInScreen(
    const std::string& window_id,
    GetWindowPositionInScreenCallback cb) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  if (!window) {
    std::move(cb).Run(absl::nullopt);
    return;
  }
  std::move(cb).Run(window->GetBoundsInScreen().origin());
}

void TestControllerAsh::LaunchAppFromAppList(const std::string& app_id) {
  ash::Shell::Get()->app_list_controller()->ActivateItem(
      app_id, /*event_flags=*/0, ash::AppListLaunchedFrom::kLaunchedFromGrid);
}

void TestControllerAsh::AreDesksBeingModified(
    AreDesksBeingModifiedCallback callback) {
  std::move(callback).Run(ash::DesksController::Get()->AreDesksBeingModified());
}

void TestControllerAsh::PinOrUnpinItemInShelf(
    const std::string& item_id,
    bool pin,
    PinOrUnpinItemInShelfCallback callback) {
  int item_index = ash::ShelfModel::Get()->ItemIndexByAppID(item_id);
  if (item_index == -1) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (pin) {
    ash::ShelfModel::Get()->PinExistingItemWithID(item_id);
  } else {
    ash::ShelfModel::Get()->UnpinAppWithID(item_id);
  }
  std::move(callback).Run(/*success=*/true);
}

void TestControllerAsh::ReinitializeAppService(
    ReinitializeAppServiceCallback callback) {
  Profile* const profile = ProfileManager::GetPrimaryUserProfile();
  apps::AppServiceProxyFactory::GetForProfile(profile)->ReinitializeForTesting(
      profile);
  std::move(callback).Run();
}

void TestControllerAsh::SelectItemInShelf(const std::string& item_id,
                                          SelectItemInShelfCallback callback) {
  ash::ShelfItemDelegate* delegate =
      ash::ShelfModel::Get()->GetShelfItemDelegate(ash::ShelfID(item_id));
  if (!delegate) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  auto mouse_event = std::make_unique<ui::MouseEvent>(
      ui::ET_MOUSE_PRESSED, gfx::PointF(), gfx::PointF(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  delegate->ItemSelected(std::move(mouse_event), display::kInvalidDisplayId,
                         ash::LAUNCH_FROM_SHELF,
                         /*callback=*/base::DoNothing(),
                         /*filter_predicate=*/base::NullCallback());
  std::move(callback).Run(/*success=*/true);
}

void TestControllerAsh::SelectContextMenuForShelfItem(
    const std::string& item_id,
    uint32_t index,
    SelectContextMenuForShelfItemCallback callback) {
  ash::ShelfItemDelegate* delegate =
      ash::ShelfModel::Get()->GetShelfItemDelegate(ash::ShelfID(item_id));
  if (!delegate) {
    std::move(callback).Run(false);
    return;
  }
  delegate->GetContextMenu(
      /*display_id=*/0,
      base::BindOnce(&TestControllerAsh::OnSelectContextMenuForShelfItem,
                     std::move(callback), item_id, index));
}
void TestControllerAsh::SendTouchEvent(const std::string& window_id,
                                       mojom::TouchEventType type,
                                       uint8_t pointer_id,
                                       const gfx::PointF& location_in_window,
                                       SendTouchEventCallback cb) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  if (!window) {
    std::move(cb).Run();
    return;
  }
  // Newer lacros might send an enum we don't know about.
  if (!mojom::IsKnownEnumValue(type)) {
    LOG(WARNING) << "Unknown event type: " << type;
    std::move(cb).Run();
    return;
  }
  ui::EventType event_type;
  switch (type) {
    case mojom::TouchEventType::kUnknown:
      // |type| is not optional, so kUnknown is never expected.
      NOTREACHED();
      return;
    case mojom::TouchEventType::kPressed:
      event_type = ui::ET_TOUCH_PRESSED;
      break;
    case mojom::TouchEventType::kMoved:
      event_type = ui::ET_TOUCH_MOVED;
      break;
    case mojom::TouchEventType::kReleased:
      event_type = ui::ET_TOUCH_RELEASED;
      break;
    case mojom::TouchEventType::kCancelled:
      event_type = ui::ET_TOUCH_CANCELLED;
      break;
  }
  // Compute location relative to display root window.
  gfx::PointF location_in_root(location_in_window);
  aura::Window::ConvertPointToTarget(window, window->GetRootWindow(),
                                     &location_in_root);
  ui::PointerDetails details(ui::EventPointerType::kTouch, pointer_id, 1.0f,
                             1.0f, 0.0f);
  ui::TouchEvent touch_event(event_type, location_in_window, location_in_root,
                             ui::EventTimeForNow(), details);
  Dispatch(window->GetHost(), &touch_event);
  std::move(cb).Run();
}

void TestControllerAsh::RegisterStandaloneBrowserTestController(
    mojo::PendingRemote<mojom::StandaloneBrowserTestController> controller) {
  // At the moment only a single controller is supported.
  // TODO(crbug.com/1174246): Support SxS lacros.
  if (standalone_browser_test_controller_.is_bound()) {
    return;
  }
  standalone_browser_test_controller_.Bind(std::move(controller));
  standalone_browser_test_controller_.set_disconnect_handler(base::BindOnce(
      &TestControllerAsh::OnControllerDisconnected, base::Unretained(this)));

  if (!on_standalone_browser_test_controller_bound_.is_signaled())
    on_standalone_browser_test_controller_bound_.Signal();
}

void TestControllerAsh::WaiterFinished(OverviewWaiter* waiter) {
  for (size_t i = 0; i < overview_waiters_.size(); ++i) {
    if (waiter == overview_waiters_[i].get()) {
      std::unique_ptr<OverviewWaiter> overview_waiter =
          std::move(overview_waiters_[i]);
      overview_waiters_.erase(overview_waiters_.begin() + i);

      // Delete asynchronously to avoid re-entrancy. This is safe because the
      // class will never use |test_controller_| after this callback.
      base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
          FROM_HERE, std::move(overview_waiter));
      break;
    }
  }
}

void TestControllerAsh::OnControllerDisconnected() {
  standalone_browser_test_controller_.reset();
}

void TestControllerAsh::OnGetContextMenuForShelfItem(
    GetContextMenuForShelfItemCallback callback,
    std::unique_ptr<ui::SimpleMenuModel> model) {
  std::vector<std::string> items;
  items.reserve(model->GetItemCount());
  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    items.push_back(base::UTF16ToUTF8(model->GetLabelAt(i)));
  }
  std::move(callback).Run(std::move(items));
}

void TestControllerAsh::OnSelectContextMenuForShelfItem(
    SelectContextMenuForShelfItemCallback callback,
    const std::string& item_id,
    size_t index,
    std::unique_ptr<ui::SimpleMenuModel> model) {
  if (index < model->GetItemCount()) {
    model->ActivatedAt(index, /*event_flags=*/0);
    std::move(callback).Run(/*success=*/true);
    return;
  }
  std::move(callback).Run(/*success=*/false);
}

void TestControllerAsh::GetOpenAshBrowserWindows(
    GetOpenAshBrowserWindowsCallback callback) {
  std::move(callback).Run(BrowserList::GetInstance()->size());
}

void TestControllerAsh::CloseAllBrowserWindows(
    CloseAllBrowserWindowsCallback callback) {
  for (auto* browser : *BrowserList::GetInstance()) {
    browser->window()->Close();
  }

  std::move(callback).Run(/*success*/ true);
}

void TestControllerAsh::TriggerTabScrubbing(
    float x_offset,
    TriggerTabScrubbingCallback callback) {
  crosapi::BrowserManager::Get()->HandleTabScrubbing(x_offset, false);

  // Return whether tab scrubbing logic has started or not in Ash.
  //
  // In practice, it is expected that it does not trigger the scrubbing logic,
  // returning |false|, and signal Lacros to do so.
  bool scrubbing = TabScrubberChromeOS::GetInstance()->IsActivationPending();
  std::move(callback).Run(scrubbing);
}

void TestControllerAsh::SetSelectedSharesheetApp(
    const std::string& app_id,
    SetSelectedSharesheetAppCallback callback) {
  sharesheet::SharesheetService::SetSelectedAppForTesting(
      base::UTF8ToUTF16(app_id));

  std::move(callback).Run();
}

void TestControllerAsh::GetAshVersion(GetAshVersionCallback callback) {
  std::move(callback).Run(version_info::GetVersion().GetString());
}

void TestControllerAsh::BindTestShillController(
    mojo::PendingReceiver<crosapi::mojom::TestShillController> receiver,
    BindTestShillControllerCallback callback) {
  mojo::MakeSelfOwnedReceiver<crosapi::mojom::TestShillController>(
      std::make_unique<crosapi::TestShillControllerAsh>(), std::move(receiver));
  std::move(callback).Run();
}

#if BUILDFLAG(USE_CUPS)
namespace {

// Observer that destroys itself after receiving OnPrintJobFinished event.
class SelfOwnedPrintJobHistoryServiceObserver
    : public ash::PrintJobHistoryService::Observer {
 public:
  SelfOwnedPrintJobHistoryServiceObserver(
      ash::PrintJobHistoryService* print_job_history_service,
      base::OnceClosure on_print_job_finished)
      : on_print_job_finished_(std::move(on_print_job_finished)) {
    observation_.Observe(print_job_history_service);
  }
  ~SelfOwnedPrintJobHistoryServiceObserver() override = default;

 private:
  // PrintJobHistoryService::Observer:
  void OnPrintJobFinished(const ash::printing::proto::PrintJobInfo&) override {
    observation_.Reset();
    std::move(on_print_job_finished_).Run();
    delete this;
  }

  base::ScopedObservation<ash::PrintJobHistoryService,
                          ash::PrintJobHistoryService::Observer>
      observation_{this};
  base::OnceClosure on_print_job_finished_;
};

}  // namespace

#endif  // BUILDFLAG(USE_CUPS)

// This class is set as UtteranceEventDelegate for the Ash TtsUtterance
// created in TestControllerAsh::TtsSpeak(), which is called from lacros browser
// test to simulate speaking an Ash utterance with a Lacros voice. It helps to
// verify that Tts events sent from Tts Speech Engine (in Lacros) are forwarded
// to its utterance event delegate in Ash.
class TestControllerAsh::AshUtteranceEventDelegate
    : public content::UtteranceEventDelegate {
 public:
  AshUtteranceEventDelegate(
      TestControllerAsh* controller,
      mojo::PendingRemote<crosapi::mojom::TtsUtteranceClient> client)
      : controller_(controller), client_(std::move(client)) {}

  AshUtteranceEventDelegate(const AshUtteranceEventDelegate&) = delete;
  AshUtteranceEventDelegate& operator=(const AshUtteranceEventDelegate&) =
      delete;
  ~AshUtteranceEventDelegate() override = default;

  // content::UtteranceEventDelegate:
  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int char_length,
                  const std::string& error_message) override {
    // Forward the TtsEvent back to Lacros, so that Lacros browser test can
    // be notified that TtsEvent has been received by its UtteranceEventDelegate
    // in Ash.
    client_->OnTtsEvent(tts_crosapi_util::ToMojo(event_type), char_index,
                        char_length, error_message);

    if (utterance->IsFinished()) {
      controller_->OnAshUtteranceFinished(utterance->GetId());
      // Note: |this| is deleted at this point.
    }
  }

 private:
  // |controller_| is guaranteed to be valid during the lifetime of this class.
  const raw_ptr<TestControllerAsh> controller_;
  mojo::Remote<crosapi::mojom::TtsUtteranceClient> client_;
};

void TestControllerAsh::CreateAndCancelPrintJob(
    const std::string& job_title,
    CreateAndCancelPrintJobCallback callback) {
#if BUILDFLAG(USE_CUPS)
  auto* profile = ProfileManager::GetPrimaryUserProfile();

  auto* observer = new SelfOwnedPrintJobHistoryServiceObserver(
      ash::PrintJobHistoryServiceFactory::GetForBrowserContext(profile),
      std::move(callback));
  DCHECK(observer);

  std::unique_ptr<ash::CupsPrintJob> print_job =
      std::make_unique<ash::CupsPrintJob>(
          chromeos::Printer(), /*job_id=*/0, job_title, /*total_page_number=*/1,
          ::printing::PrintJob::Source::kPrintPreview,
          /*source_id=*/"", ash::printing::proto::PrintSettings());

  ash::CupsPrintJobManager* print_job_manager =
      ash::CupsPrintJobManagerFactory::GetForBrowserContext(profile);
  print_job->set_state(ash::CupsPrintJob::State::STATE_NONE);
  print_job_manager->NotifyJobCreated(print_job->GetWeakPtr());
  print_job->set_state(ash::CupsPrintJob::State::STATE_CANCELLED);
  print_job_manager->NotifyJobCanceled(print_job->GetWeakPtr());
#endif  // BUILDFLAG(USE_CUPS)
}

void TestControllerAsh::BindShillClientTestInterface(
    mojo::PendingReceiver<crosapi::mojom::ShillClientTestInterface> receiver,
    BindShillClientTestInterfaceCallback callback) {
  mojo::MakeSelfOwnedReceiver<crosapi::mojom::ShillClientTestInterface>(
      std::make_unique<crosapi::ShillClientTestInterfaceAsh>(),
      std::move(receiver));
  std::move(callback).Run();
}

void TestControllerAsh::GetSanitizedActiveUsername(
    GetSanitizedActiveUsernameCallback callback) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  user_manager::User* user = user_manager->GetActiveUser();
  CHECK(user);

  ::user_data_auth::GetSanitizedUsernameRequest request;

  request.set_username(
      cryptohome::CreateAccountIdentifierFromAccountId(user->GetAccountId())
          .account_id());
  ash::CryptohomeMiscClient::Get()->GetSanitizedUsername(
      request,
      base::BindOnce(
          [](GetSanitizedActiveUsernameCallback callback,
             absl::optional<::user_data_auth::GetSanitizedUsernameReply>
                 result) {
            CHECK(result.has_value());
            std::move(callback).Run(result->sanitized_username());
          },
          std::move(callback)));
}

void TestControllerAsh::BindInputMethodTestInterface(
    mojo::PendingReceiver<crosapi::mojom::InputMethodTestInterface> receiver,
    BindInputMethodTestInterfaceCallback callback) {
  mojo::MakeSelfOwnedReceiver<crosapi::mojom::InputMethodTestInterface>(
      std::make_unique<crosapi::InputMethodTestInterfaceAsh>(),
      std::move(receiver));
  std::move(callback).Run();
}

void TestControllerAsh::GetTtsUtteranceQueueSize(
    GetTtsUtteranceQueueSizeCallback callback) {
  std::move(callback).Run(
      tts_crosapi_util::GetTtsUtteranceQueueSizeForTesting());
}

void TestControllerAsh::GetTtsVoices(GetTtsVoicesCallback callback) {
  std::vector<content::VoiceData> voices;
  tts_crosapi_util::GetAllVoicesForTesting(  // IN-TEST
      ProfileManager::GetActiveUserProfile(), GURL(), &voices);

  std::vector<crosapi::mojom::TtsVoicePtr> mojo_voices;
  for (const auto& voice : voices)
    mojo_voices.push_back(tts_crosapi_util::ToMojo(voice));

  std::move(callback).Run(std::move(mojo_voices));
}

void TestControllerAsh::TtsSpeak(
    crosapi::mojom::TtsUtterancePtr mojo_utterance,
    mojo::PendingRemote<crosapi::mojom::TtsUtteranceClient> utterance_client) {
  std::unique_ptr<content::TtsUtterance> ash_utterance =
      tts_crosapi_util::CreateUtteranceFromMojo(
          mojo_utterance, /*should_always_be_spoken=*/false);
  auto event_delegate = std::make_unique<AshUtteranceEventDelegate>(
      this, std::move(utterance_client));
  ash_utterance->SetEventDelegate(event_delegate.get());
  ash_utterance_event_delegates_.emplace(ash_utterance->GetId(),
                                         std ::move(event_delegate));
  tts_crosapi_util::SpeakForTesting(std::move(ash_utterance));
}

void TestControllerAsh::OnAshUtteranceFinished(int utterance_id) {
  // Delete the utterace event delegate object when the utterance is finished.
  ash_utterance_event_delegates_.erase(utterance_id);
}

// This class waits for overview mode to either enter or exit and fires a
// callback. This class will fire the callback at most once.
class TestControllerAsh::OverviewWaiter : public ash::OverviewObserver {
 public:
  OverviewWaiter(bool wait_for_enter,
                 base::OnceClosure closure,
                 TestControllerAsh* test_controller)
      : wait_for_enter_(wait_for_enter),
        closure_(std::move(closure)),
        test_controller_(test_controller) {
    ash::Shell::Get()->overview_controller()->AddObserver(this);
  }
  OverviewWaiter(const OverviewWaiter&) = delete;
  OverviewWaiter& operator=(const OverviewWaiter&) = delete;
  ~OverviewWaiter() override {
    ash::Shell::Get()->overview_controller()->RemoveObserver(this);
  }

  // OverviewObserver:
  void OnOverviewModeStartingAnimationComplete(bool canceled) override {
    if (wait_for_enter_) {
      if (closure_) {
        std::move(closure_).Run();
        DCHECK(test_controller_);
        TestControllerAsh* controller = test_controller_;
        test_controller_ = nullptr;
        controller->WaiterFinished(this);
      }
    }
  }

  void OnOverviewModeEndingAnimationComplete(bool canceled) override {
    if (!wait_for_enter_) {
      if (closure_) {
        std::move(closure_).Run();
        DCHECK(test_controller_);
        TestControllerAsh* controller = test_controller_;
        test_controller_ = nullptr;
        controller->WaiterFinished(this);
      }
    }
  }

 private:
  // If true, waits for enter. Otherwise waits for exit.
  const bool wait_for_enter_;
  base::OnceClosure closure_;

  // The test controller owns this object so is never invalid.
  raw_ptr<TestControllerAsh, ExperimentalAsh> test_controller_;
};

TestShillControllerAsh::TestShillControllerAsh() {
  ash::ShillProfileClient::Get()->GetTestInterface()->AddProfile(
      "/network/test", ash::ProfileHelper::GetUserIdHashFromProfile(
                           ProfileManager::GetPrimaryUserProfile()));
}

TestShillControllerAsh::~TestShillControllerAsh() = default;

void TestShillControllerAsh::OnPacketReceived(
    const std::string& extension_id,
    const std::string& configuration_name,
    const std::vector<uint8_t>& data) {
  const std::string key = crosapi::VpnServiceForExtensionAsh::GetKey(
      extension_id, configuration_name);
  const std::string shill_key = shill::kObjectPathBase + key;
  // On linux ShillThirdPartyVpnDriverClient is initialized as Fake and
  // therefore exposes a testing interface.
  auto* client = ash::ShillThirdPartyVpnDriverClient::Get()->GetTestInterface();
  CHECK(client);
  client->OnPacketReceived(shill_key,
                           std::vector<char>(data.begin(), data.end()));
}

void TestShillControllerAsh::OnPlatformMessage(
    const std::string& extension_id,
    const std::string& configuration_name,
    uint32_t message) {
  const std::string key = crosapi::VpnServiceForExtensionAsh::GetKey(
      extension_id, configuration_name);
  const std::string shill_key = shill::kObjectPathBase + key;
  // On linux ShillThirdPartyVpnDriverClient is initialized as Fake and
  // therefore exposes a testing interface.
  auto* client = ash::ShillThirdPartyVpnDriverClient::Get()->GetTestInterface();
  CHECK(client);
  client->OnPlatformMessage(shill_key, message);
}

////////////
// ShillClientTestInterfaceAsh

ShillClientTestInterfaceAsh::ShillClientTestInterfaceAsh() = default;
ShillClientTestInterfaceAsh::~ShillClientTestInterfaceAsh() = default;

void ShillClientTestInterfaceAsh::AddDevice(const std::string& device_path,
                                            const std::string& type,
                                            const std::string& name,
                                            AddDeviceCallback callback) {
  auto* device_test = ash::ShillDeviceClient::Get()->GetTestInterface();
  device_test->AddDevice(device_path, type, name);
  std::move(callback).Run();
}

void ShillClientTestInterfaceAsh::ClearDevices(ClearDevicesCallback callback) {
  auto* device_test = ash::ShillDeviceClient::Get()->GetTestInterface();
  device_test->ClearDevices();
  std::move(callback).Run();
}

void ShillClientTestInterfaceAsh::SetDeviceProperty(
    const std::string& device_path,
    const std::string& name,
    ::base::Value value,
    bool notify_changed,
    SetDevicePropertyCallback callback) {
  auto* device_test = ash::ShillDeviceClient::Get()->GetTestInterface();
  device_test->SetDeviceProperty(device_path, name, value, notify_changed);
  std::move(callback).Run();
}

void ShillClientTestInterfaceAsh::SetSimLocked(const std::string& device_path,
                                               bool enabled,
                                               SetSimLockedCallback callback) {
  auto* device_test = ash::ShillDeviceClient::Get()->GetTestInterface();
  device_test->SetSimLocked(device_path, enabled);
  std::move(callback).Run();
}

void ShillClientTestInterfaceAsh::AddService(
    const std::string& service_path,
    const std::string& guid,
    const std::string& name,
    const std::string& type,
    const std::string& state,
    bool visible,
    SetDevicePropertyCallback callback) {
  auto* service_test = ash::ShillServiceClient::Get()->GetTestInterface();
  service_test->AddService(service_path, guid, name, type, state, visible);
  std::move(callback).Run();
}

void ShillClientTestInterfaceAsh::ClearServices(
    ClearServicesCallback callback) {
  auto* service_test = ash::ShillServiceClient::Get()->GetTestInterface();
  service_test->ClearServices();
  std::move(callback).Run();
}

void ShillClientTestInterfaceAsh::SetServiceProperty(
    const std::string& service_path,
    const std::string& property,
    base::Value value,
    SetServicePropertyCallback callback) {
  auto* service_test = ash::ShillServiceClient::Get()->GetTestInterface();
  service_test->SetServiceProperty(service_path, property, value);
  std::move(callback).Run();
}

void ShillClientTestInterfaceAsh::AddProfile(const std::string& profile_path,
                                             const std::string& userhash,
                                             AddProfileCallback callback) {
  auto* profile_test = ash::ShillProfileClient::Get()->GetTestInterface();
  profile_test->AddProfile(profile_path, userhash);
  std::move(callback).Run();
}

void ShillClientTestInterfaceAsh::AddServiceToProfile(
    const std::string& profile_path,
    const std::string& service_path,
    AddServiceToProfileCallback callback) {
  auto* profile_test = ash::ShillProfileClient::Get()->GetTestInterface();
  profile_test->AddService(profile_path, service_path);
  std::move(callback).Run();
}

void ShillClientTestInterfaceAsh::AddIPConfig(const std::string& ip_config_path,
                                              ::base::Value properties,
                                              AddIPConfigCallback callback) {
  auto* ip_config_test = ash::ShillIPConfigClient::Get()->GetTestInterface();
  ip_config_test->AddIPConfig(ip_config_path, std::move(properties).TakeDict());
  std::move(callback).Run();
}

}  // namespace crosapi
