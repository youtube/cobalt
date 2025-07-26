// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_LINUX_H_
#define UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_LINUX_H_

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/dbus/xdg/request.h"
#include "dbus/bus.h"
#include "dbus/object_proxy.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

namespace dbus_xdg {
class Request;
enum class SystemdUnitStatus;
}  // namespace dbus_xdg

namespace ui {

// Linux-specific implementation of the GlobalShortcutListener class that
// listens for global shortcuts using the org.freedesktop.portal.GlobalShortcuts
// interface.
class GlobalAcceleratorListenerLinux : public GlobalAcceleratorListener {
 public:
  explicit GlobalAcceleratorListenerLinux(scoped_refptr<dbus::Bus> bus);

  GlobalAcceleratorListenerLinux(const GlobalAcceleratorListenerLinux&) =
      delete;
  GlobalAcceleratorListenerLinux& operator=(
      const GlobalAcceleratorListenerLinux&) = delete;

  ~GlobalAcceleratorListenerLinux() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(GlobalAcceleratorListenerLinuxTest,
                           OnCommandsChanged);

  // These are exposed in the header for testing.
  static constexpr char kPortalServiceName[] = "org.freedesktop.portal.Desktop";
  static constexpr char kPortalObjectPath[] = "/org/freedesktop/portal/desktop";
  static constexpr char kGlobalShortcutsInterface[] =
      "org.freedesktop.portal.GlobalShortcuts";
  static constexpr char kSessionInterface[] = "org.freedesktop.portal.Session";

  static constexpr char kMethodCreateSession[] = "CreateSession";
  static constexpr char kMethodListShortcuts[] = "ListShortcuts";
  static constexpr char kMethodBindShortcuts[] = "BindShortcuts";
  static constexpr char kMethodCloseSession[] = "Close";
  static constexpr char kSignalActivated[] = "Activated";

  static constexpr char kSessionTokenPrefix[] = "chromium_";

  struct SessionKey {
    std::string accelerator_group_id;
    std::string profile_id;

    std::string GetTokenKey() const;

    bool operator<(const SessionKey& other) const {
      return std::tie(accelerator_group_id, profile_id) <
             std::tie(other.accelerator_group_id, other.profile_id);
    }
  };

  struct SessionContext {
    SessionContext(Observer* observer, const ui::CommandMap& commands);
    ~SessionContext();

    scoped_refptr<dbus::Bus> bus;
    raw_ptr<dbus::ObjectProxy> session_proxy;
    const raw_ptr<Observer> observer;
    ui::CommandMap commands;
    bool bind_shortcuts_called = false;
    std::unique_ptr<dbus_xdg::Request> request;
  };

  using SessionMap =
      base::flat_map<SessionKey, std::unique_ptr<SessionContext>>;
  using SessionMapPair = std::pair<SessionKey, std::unique_ptr<SessionContext>>;

  // GlobalAcceleratorListener:
  void StartListening() override;
  void StopListening() override;
  bool StartListeningForAccelerator(
      const ui::Accelerator& accelerator) override;
  void StopListeningForAccelerator(const ui::Accelerator& accelerator) override;
  void UnregisterAccelerators(Observer* observer);
  bool IsRegistrationHandledExternally() const override;
  void OnCommandsChanged(const std::string& accelerator_group_id,
                         const std::string& profile_id,
                         const ui::CommandMap& commands,
                         Observer* observer) override;

  void OnCreateSession(
      const SessionKey& session_key,
      base::expected<DbusDictionary, dbus_xdg::ResponseError> results);
  void OnListShortcuts(
      const SessionKey& session_key,
      base::expected<DbusDictionary, dbus_xdg::ResponseError> results);
  void OnBindShortcuts(
      base::expected<DbusDictionary, dbus_xdg::ResponseError> results);

  void RecreateSessionOnClosed(const SessionKey& session_key,
                               dbus::Response* response);

  // Callbacks for DBus signals.
  void OnActivatedSignal(dbus::Signal* signal);

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success);

  void OnSystemdUnitStarted(dbus_xdg::SystemdUnitStatus status);

  void OnServiceStarted(std::optional<bool> service_started);

  void CreateSession(SessionMapPair& pair);

  void BindShortcuts(SessionContext& session_context);

  // DBus components.
  scoped_refptr<dbus::Bus> bus_;
  raw_ptr<dbus::ObjectProxy> global_shortcuts_proxy_ = nullptr;

  // Whether the GlobalShortcuts service is available, or nullopt if the status
  // is not yet known.
  std::optional<bool> service_started_;

  // One session per extension.
  base::flat_map<SessionKey, std::unique_ptr<SessionContext>> session_map_;

  base::WeakPtrFactory<GlobalAcceleratorListenerLinux> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_LINUX_H_
