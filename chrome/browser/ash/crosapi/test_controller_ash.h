// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TEST_CONTROLLER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_TEST_CONTROLLER_ASH_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/one_shot_event.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/models/simple_menu_model.h"

namespace crosapi {

// This class is the ash-chrome implementation of the TestController interface.
// This class must only be used from the main thread.
class TestControllerAsh : public mojom::TestController,
                          public CrosapiAsh::TestControllerReceiver {
 public:
  TestControllerAsh();
  TestControllerAsh(const TestControllerAsh&) = delete;
  TestControllerAsh& operator=(const TestControllerAsh&) = delete;
  ~TestControllerAsh() override;

  // CrosapiAsh::TestControllerReceiver:
  void BindReceiver(
      mojo::PendingReceiver<mojom::TestController> receiver) override;

  // crosapi::mojom::TestController:
  void ClickElement(const std::string& element_name,
                    ClickElementCallback callback) override;
  void ClickWindow(const std::string& window_id) override;
  void ConnectToNetwork(const std::string& service_path) override;
  void DisconnectFromNetwork(const std::string& service_path) override;
  void DoesItemExistInShelf(const std::string& item_id,
                            DoesItemExistInShelfCallback callback) override;
  void DoesElementExist(const std::string& element_name,
                        DoesElementExistCallback callback) override;
  void DoesWindowExist(const std::string& window_id,
                       DoesWindowExistCallback callback) override;
  void EnterOverviewMode(EnterOverviewModeCallback callback) override;
  void ExitOverviewMode(ExitOverviewModeCallback callback) override;
  void EnterTabletMode(EnterTabletModeCallback callback) override;
  void ExitTabletMode(ExitTabletModeCallback callback) override;
  void GetShelfItemState(const std::string& app_id,
                         GetShelfItemStateCallback callback) override;
  void GetContextMenuForShelfItem(
      const std::string& item_id,
      GetContextMenuForShelfItemCallback callback) override;
  void GetMinimizeOnBackKeyWindowProperty(
      const std::string& window_id,
      GetMinimizeOnBackKeyWindowPropertyCallback cb) override;
  void GetWindowPositionInScreen(const std::string& window_id,
                                 GetWindowPositionInScreenCallback cb) override;
  void LaunchAppFromAppList(const std::string& app_id) override;
  void AreDesksBeingModified(AreDesksBeingModifiedCallback callback) override;
  void PinOrUnpinItemInShelf(const std::string& item_id,
                             bool pin,
                             PinOrUnpinItemInShelfCallback cb) override;
  void ReinitializeAppService(ReinitializeAppServiceCallback callback) override;
  void SelectContextMenuForShelfItem(
      const std::string& item_id,
      uint32_t index,
      SelectContextMenuForShelfItemCallback cb) override;
  void SelectItemInShelf(const std::string& item_id,
                         SelectItemInShelfCallback cb) override;
  void SendTouchEvent(const std::string& window_id,
                      mojom::TouchEventType type,
                      uint8_t pointer_id,
                      const gfx::PointF& location_in_window,
                      SendTouchEventCallback cb) override;
  void GetOpenAshBrowserWindows(
      GetOpenAshBrowserWindowsCallback callback) override;
  void CloseAllBrowserWindows(CloseAllBrowserWindowsCallback callback) override;
  void RegisterStandaloneBrowserTestController(
      mojo::PendingRemote<mojom::StandaloneBrowserTestController>) override;
  void TriggerTabScrubbing(float x_offset,
                           TriggerTabScrubbingCallback callback) override;
  void SetSelectedSharesheetApp(
      const std::string& app_id,
      SetSelectedSharesheetAppCallback callback) override;
  void GetAshVersion(GetAshVersionCallback callback) override;
  void BindTestShillController(
      mojo::PendingReceiver<crosapi::mojom::TestShillController> receiver,
      BindTestShillControllerCallback callback) override;
  void CreateAndCancelPrintJob(
      const std::string& job_title,
      CreateAndCancelPrintJobCallback callback) override;

  void BindShillClientTestInterface(
      mojo::PendingReceiver<crosapi::mojom::ShillClientTestInterface> receiver,
      BindShillClientTestInterfaceCallback callback) override;

  void GetSanitizedActiveUsername(
      GetSanitizedActiveUsernameCallback callback) override;
  void BindInputMethodTestInterface(
      mojo::PendingReceiver<crosapi::mojom::InputMethodTestInterface> receiver,
      BindInputMethodTestInterfaceCallback callback) override;

  void GetTtsUtteranceQueueSize(
      GetTtsUtteranceQueueSizeCallback callback) override;

  void GetTtsVoices(GetTtsVoicesCallback callback) override;

  void TtsSpeak(crosapi::mojom::TtsUtterancePtr mojo_utterance,
                mojo::PendingRemote<crosapi::mojom::TtsUtteranceClient>
                    utterance_client) override;

  mojo::Remote<mojom::StandaloneBrowserTestController>&
  GetStandaloneBrowserTestController() {
    DCHECK(standalone_browser_test_controller_.is_bound());
    return standalone_browser_test_controller_;
  }

  // Signals when standalone browser test controller becomes bound.
  const base::OneShotEvent& on_standalone_browser_test_controller_bound()
      const {
    return on_standalone_browser_test_controller_bound_;
  }

 private:
  class OverviewWaiter;
  class AshUtteranceEventDelegate;

  // Called when a Tts utterance is finished.
  void OnAshUtteranceFinished(int utterance_id);

  // Called when a waiter has finished waiting for its event.
  void WaiterFinished(OverviewWaiter* waiter);

  // Called when the lacros test controller was disconnected.
  void OnControllerDisconnected();

  // Called when a ShelfItemDelegate returns its context menu and the follow up
  // is to return the results.
  static void OnGetContextMenuForShelfItem(
      GetContextMenuForShelfItemCallback callback,
      std::unique_ptr<ui::SimpleMenuModel> model);
  // Called when a ShelfItemDelegate returns its context menu and the follow up
  // is to select an item.
  static void OnSelectContextMenuForShelfItem(
      SelectContextMenuForShelfItemCallback callback,
      const std::string& item_id,
      size_t index,
      std::unique_ptr<ui::SimpleMenuModel> model);

  // Each call to EnterOverviewMode or ExitOverviewMode spawns a waiter for the
  // corresponding event. The waiters are stored in this struct and deleted once
  // the event triggers.
  std::vector<std::unique_ptr<OverviewWaiter>> overview_waiters_;

  // This class supports any number of connections. This allows multiple
  // crosapi clients.
  mojo::ReceiverSet<mojom::TestController> receivers_;

  // Controller to send commands to the connected lacros crosapi client.
  mojo::Remote<mojom::StandaloneBrowserTestController>
      standalone_browser_test_controller_;

  base::OneShotEvent on_standalone_browser_test_controller_bound_;

  // Ash utterance event delegates by utterance id.
  std::map<int, std::unique_ptr<AshUtteranceEventDelegate>>
      ash_utterance_event_delegates_;
};

class TestShillControllerAsh : public crosapi::mojom::TestShillController {
 public:
  TestShillControllerAsh();
  ~TestShillControllerAsh() override;

  // crosapi::mojom::TestShillController:
  void OnPacketReceived(const std::string& extension_id,
                        const std::string& configuration_name,
                        const std::vector<uint8_t>& data) override;
  void OnPlatformMessage(const std::string& extension_id,
                         const std::string& configuration_name,
                         uint32_t message) override;
};

class ShillClientTestInterfaceAsh
    : public crosapi::mojom::ShillClientTestInterface {
 public:
  ShillClientTestInterfaceAsh();
  ~ShillClientTestInterfaceAsh() override;

  void AddDevice(const std::string& device_path,
                 const std::string& type,
                 const std::string& name,
                 AddDeviceCallback callback) override;
  void ClearDevices(ClearDevicesCallback callback) override;
  void SetDeviceProperty(const std::string& device_path,
                         const std::string& name,
                         ::base::Value value,
                         bool notify_changed,
                         SetDevicePropertyCallback callback) override;
  void SetSimLocked(const std::string& device_path,
                    bool enabled,
                    SetSimLockedCallback callback) override;

  void AddService(const std::string& service_path,
                  const std::string& guid,
                  const std::string& name,
                  const std::string& type,
                  const std::string& state,
                  bool visible,
                  AddServiceCallback callback) override;
  void ClearServices(ClearServicesCallback callback) override;
  void SetServiceProperty(const std::string& service_path,
                          const std::string& property,
                          base::Value value,
                          SetServicePropertyCallback callback) override;

  void AddProfile(const std::string& profile_path,
                  const std::string& userhash,
                  AddProfileCallback callback) override;
  void AddServiceToProfile(const std::string& profile_path,
                           const std::string& service_path,
                           AddServiceToProfileCallback callback) override;

  void AddIPConfig(const std::string& ip_config_path,
                   ::base::Value properties,
                   AddIPConfigCallback callback) override;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TEST_CONTROLLER_ASH_H_
