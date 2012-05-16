// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This implementation of NetworkChangeNotifier's offline state detection
// depends on D-Bus and NetworkManager, and is known to work on at least
// GNOME version 2.30.  If D-Bus or NetworkManager are unavailable, this
// implementation will always behave as if it is online.

#include "net/base/network_change_notifier_linux.h"

#include <errno.h>
#include <resolv.h>
#include <sys/socket.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/eintr_wrapper.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier_netlink_linux.h"
#include "net/dns/dns_config_watcher.h"

namespace net {

namespace {

const int kInvalidSocket = -1;

const char kNetworkManagerServiceName[] = "org.freedesktop.NetworkManager";
const char kNetworkManagerPath[] = "/org/freedesktop/NetworkManager";
const char kNetworkManagerInterface[] = "org.freedesktop.NetworkManager";

// http://projects.gnome.org/NetworkManager/developers/spec-08.html#type-NM_STATE
enum {
  NM_LEGACY_STATE_UNKNOWN = 0,
  NM_LEGACY_STATE_ASLEEP = 1,
  NM_LEGACY_STATE_CONNECTING = 2,
  NM_LEGACY_STATE_CONNECTED = 3,
  NM_LEGACY_STATE_DISCONNECTED = 4
};

// http://projects.gnome.org/NetworkManager/developers/migrating-to-09/spec.html#type-NM_STATE
enum {
  NM_STATE_UNKNOWN = 0,
  NM_STATE_ASLEEP = 10,
  NM_STATE_DISCONNECTED = 20,
  NM_STATE_DISCONNECTING = 30,
  NM_STATE_CONNECTING = 40,
  NM_STATE_CONNECTED_LOCAL = 50,
  NM_STATE_CONNECTED_SITE = 60,
  NM_STATE_CONNECTED_GLOBAL = 70
};

}  // namespace

// A wrapper around NetworkManager's D-Bus API.
class NetworkManagerApi {
 public:
  NetworkManagerApi(const base::Closure& notification_callback, dbus::Bus* bus)
      : is_offline_(false),
        offline_state_initialized_(true /*manual_reset*/, false),
        notification_callback_(notification_callback),
        helper_thread_id_(base::kInvalidThreadId),
        ALLOW_THIS_IN_INITIALIZER_LIST(ptr_factory_(this)),
        system_bus_(bus) { }

  ~NetworkManagerApi() { }

  // Should be called on a helper thread which must be of type IO.
  void Init();

  // Must be called by the helper thread's CleanUp() method.
  void CleanUp();

  // Implementation of NetworkChangeNotifierLinux::IsCurrentlyOffline().
  // Safe to call from any thread, but will block until Init() has completed.
  bool IsCurrentlyOffline();

 private:
  // Callbacks for D-Bus API.
  void OnInitialResponse(dbus::Response* response) {
    HandleResponse(response);
    offline_state_initialized_.Signal();
  }

  void OnSignaled(dbus::Signal* signal);

  void OnConnected(const std::string&, const std::string&, bool success) {
    if (!success) {
      DLOG(WARNING) << "Failed to set up offline state detection";
      offline_state_initialized_.Signal();
    }
  }

  // Helper for OnInitialResponse.
  void HandleResponse(dbus::Response* response);

  // Converts a NetworkManager state uint to a bool.
  static bool StateIsOffline(uint32 state);

  bool is_offline_;
  base::Lock is_offline_lock_;
  base::WaitableEvent offline_state_initialized_;

  base::Closure notification_callback_;

  base::PlatformThreadId helper_thread_id_;

  base::WeakPtrFactory<NetworkManagerApi> ptr_factory_;

  scoped_refptr<dbus::Bus> system_bus_;

  DISALLOW_COPY_AND_ASSIGN(NetworkManagerApi);
};

void NetworkManagerApi::Init() {
  // D-Bus requires an IO MessageLoop.
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_IO);
  helper_thread_id_ = base::PlatformThread::CurrentId();

  if (!system_bus_) {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    options.connection_type = dbus::Bus::PRIVATE;
    system_bus_ = new dbus::Bus(options);
  }

  // Ignore ServiceUnknown errors to avoid log spam: http://crbug.com/109696.
  dbus::ObjectProxy* proxy = system_bus_->GetObjectProxyWithOptions(
      kNetworkManagerServiceName, dbus::ObjectPath(kNetworkManagerPath),
      dbus::ObjectProxy::IGNORE_SERVICE_UNKNOWN_ERRORS);

  // Get the initial state asynchronously.
  dbus::MethodCall method_call(DBUS_INTERFACE_PROPERTIES, "Get");
  dbus::MessageWriter builder(&method_call);
  builder.AppendString(kNetworkManagerInterface);
  builder.AppendString("State");
  proxy->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::Bind(&NetworkManagerApi::OnInitialResponse,
                 ptr_factory_.GetWeakPtr()));

  // And sign up for notifications.
  proxy->ConnectToSignal(
      kNetworkManagerInterface,
      "StateChanged",
      base::Bind(&NetworkManagerApi::OnSignaled, ptr_factory_.GetWeakPtr()),
      base::Bind(&NetworkManagerApi::OnConnected, ptr_factory_.GetWeakPtr()));
}

void NetworkManagerApi::CleanUp() {
  DCHECK_EQ(helper_thread_id_, base::PlatformThread::CurrentId());
  ptr_factory_.InvalidateWeakPtrs();
}

void NetworkManagerApi::HandleResponse(dbus::Response* response) {
  DCHECK_EQ(helper_thread_id_, base::PlatformThread::CurrentId());
  if (!response) {
    DVLOG(1) << "No response received for initial state request";
    return;
  }
  dbus::MessageReader reader(response);
  uint32 state = 0;
  if (!reader.PopVariantOfUint32(&state)) {
    DLOG(WARNING) << "Unexpected response for NetworkManager State request: "
                  << response->ToString();
    return;
  }
  {
    base::AutoLock lock(is_offline_lock_);
    is_offline_ = StateIsOffline(state);
  }
}

void NetworkManagerApi::OnSignaled(dbus::Signal* signal) {
  DCHECK_EQ(helper_thread_id_, base::PlatformThread::CurrentId());
  dbus::MessageReader reader(signal);
  uint32 state = 0;
  if (!reader.PopUint32(&state)) {
    DLOG(WARNING) << "Unexpected signal for NetworkManager StateChanged: "
                  << signal->ToString();
    return;
  }
  bool new_is_offline = StateIsOffline(state);
  {
    base::AutoLock lock(is_offline_lock_);
    if (is_offline_ != new_is_offline)
      is_offline_ = new_is_offline;
    else
      return;
  }
  notification_callback_.Run();
}

bool NetworkManagerApi::StateIsOffline(uint32 state) {
  switch (state) {
    case NM_LEGACY_STATE_CONNECTED:
    case NM_STATE_CONNECTED_SITE:
    case NM_STATE_CONNECTED_GLOBAL:
      // Definitely connected
      return false;
    case NM_LEGACY_STATE_DISCONNECTED:
    case NM_STATE_DISCONNECTED:
      // Definitely disconnected
      return true;
    case NM_STATE_CONNECTED_LOCAL:
      // Local networking only; I'm treating this as offline (keybuk)
      return true;
    case NM_LEGACY_STATE_CONNECTING:
    case NM_STATE_DISCONNECTING:
    case NM_STATE_CONNECTING:
      // In-flight change to connection status currently underway
      return true;
    case NM_LEGACY_STATE_ASLEEP:
    case NM_STATE_ASLEEP:
      // Networking disabled or no devices on system
      return true;
    default:
      // Unknown status
      return false;
  }
}

bool NetworkManagerApi::IsCurrentlyOffline() {
  // http://crbug.com/125097
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  offline_state_initialized_.Wait();
  base::AutoLock lock(is_offline_lock_);
  return is_offline_;
}

class NetworkChangeNotifierLinux::Thread
    : public base::Thread, public MessageLoopForIO::Watcher {
 public:
  explicit Thread(dbus::Bus* bus);
  virtual ~Thread();

  // MessageLoopForIO::Watcher:
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int /* fd */) OVERRIDE;

  // Plumbing for NetworkChangeNotifier::IsCurrentlyOffline.
  // Safe to call from any thread.
  bool IsCurrentlyOffline() {
    return network_manager_api_.IsCurrentlyOffline();
  }

 protected:
  // base::Thread
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

 private:
  // Starts listening for netlink messages.  Also handles the messages if there
  // are any available on the netlink socket.
  void ListenForNotifications();

  // Attempts to read from the netlink socket into |buf| of length |len|.
  // Returns the bytes read on synchronous success and ERR_IO_PENDING if the
  // recv() would block.  Otherwise, it returns a net error code.
  int ReadNotificationMessage(char* buf, size_t len);

  // The netlink socket descriptor.
  int netlink_fd_;
  MessageLoopForIO::FileDescriptorWatcher netlink_watcher_;

  // Used to detect online/offline state changes.
  NetworkManagerApi network_manager_api_;

  internal::DnsConfigWatcher dns_watcher_;

  DISALLOW_COPY_AND_ASSIGN(Thread);
};

NetworkChangeNotifierLinux::Thread::Thread(dbus::Bus* bus)
    : base::Thread("NetworkChangeNotifier"),
      netlink_fd_(kInvalidSocket),
      network_manager_api_(
          base::Bind(&NetworkChangeNotifier
                     ::NotifyObserversOfOnlineStateChange),
          bus) {
}

NetworkChangeNotifierLinux::Thread::~Thread() {
  DCHECK(!Thread::IsRunning());
}

void NetworkChangeNotifierLinux::Thread::Init() {
  netlink_fd_ = InitializeNetlinkSocket();
  if (netlink_fd_ < 0) {
    netlink_fd_ = kInvalidSocket;
    return;
  }
  ListenForNotifications();

  network_manager_api_.Init();

  dns_watcher_.Init();
}

void NetworkChangeNotifierLinux::Thread::CleanUp() {
  if (netlink_fd_ != kInvalidSocket) {
    if (HANDLE_EINTR(close(netlink_fd_)) != 0)
      PLOG(ERROR) << "Failed to close socket";
    netlink_fd_ = kInvalidSocket;
    netlink_watcher_.StopWatchingFileDescriptor();
  }
  network_manager_api_.CleanUp();

  dns_watcher_.CleanUp();
}

void NetworkChangeNotifierLinux::Thread::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(fd, netlink_fd_);
  ListenForNotifications();
}

void NetworkChangeNotifierLinux::Thread::OnFileCanWriteWithoutBlocking(
    int /* fd */) {
  NOTREACHED();
}

void NetworkChangeNotifierLinux::Thread::ListenForNotifications() {
  char buf[4096];
  int rv = ReadNotificationMessage(buf, arraysize(buf));
  while (rv > 0) {
    if (HandleNetlinkMessage(buf, rv)) {
      VLOG(1) << "Detected IP address changes.";
      NotifyObserversOfIPAddressChange();
    }
    rv = ReadNotificationMessage(buf, arraysize(buf));
  }

  if (rv == ERR_IO_PENDING) {
    rv = MessageLoopForIO::current()->WatchFileDescriptor(netlink_fd_, false,
        MessageLoopForIO::WATCH_READ, &netlink_watcher_, this);
    LOG_IF(ERROR, !rv) << "Failed to watch netlink socket: " << netlink_fd_;
  }
}

int NetworkChangeNotifierLinux::Thread::ReadNotificationMessage(
    char* buf,
    size_t len) {
  DCHECK_NE(len, 0u);
  DCHECK(buf);
  memset(buf, 0, len);
  int rv = recv(netlink_fd_, buf, len, 0);
  if (rv > 0)
    return rv;

  DCHECK_NE(rv, 0);
  if (errno != EAGAIN && errno != EWOULDBLOCK) {
    PLOG(DFATAL) << "recv";
    return ERR_FAILED;
  }

  return ERR_IO_PENDING;
}

NetworkChangeNotifierLinux* NetworkChangeNotifierLinux::Create() {
  return new NetworkChangeNotifierLinux(NULL);
}

NetworkChangeNotifierLinux* NetworkChangeNotifierLinux::CreateForTest(
    dbus::Bus* bus) {
  return new NetworkChangeNotifierLinux(bus);
}

NetworkChangeNotifierLinux::NetworkChangeNotifierLinux(dbus::Bus* bus)
    : notifier_thread_(new Thread(bus)) {
  // We create this notifier thread because the notification implementation
  // needs a MessageLoopForIO, and there's no guarantee that
  // MessageLoop::current() meets that criterion.
  base::Thread::Options thread_options(MessageLoop::TYPE_IO, 0);
  notifier_thread_->StartWithOptions(thread_options);
}

NetworkChangeNotifierLinux::~NetworkChangeNotifierLinux() {
  // Stopping from here allows us to sanity- check that the notifier
  // thread shut down properly.
  notifier_thread_->Stop();
}

bool NetworkChangeNotifierLinux::IsCurrentlyOffline() const {
  return notifier_thread_->IsCurrentlyOffline();
}

}  // namespace net
