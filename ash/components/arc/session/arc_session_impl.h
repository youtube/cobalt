// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_SESSION_IMPL_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_SESSION_IMPL_H_

#include <memory>
#include <ostream>
#include <string>

#include "ash/components/arc/session/arc_client_adapter.h"
#include "ash/components/arc/session/arc_session.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromeos/ash/components/system/scheduler_configuration_manager_base.h"

namespace base {
struct SystemMemoryInfoKB;
}

namespace cryptohome {
class Identification;
}

namespace arc {

namespace mojom {
class ArcBridgeHost;
}  // namespace mojom

constexpr int64_t kMinimumFreeDiskSpaceBytes = 64 << 20;  // 64MB

class ArcSessionImpl : public ArcSession,
                       public ArcClientAdapter::Observer,
                       public ash::SchedulerConfigurationManagerBase::Observer {
 public:
  // The possible states of the session. Expected state changes are as follows.
  //
  // NOT_STARTED
  // -> StartMiniInstance() ->
  // WAITING_FOR_NUM_CORES
  // -> OnConfigurationSet ->
  // STARTING_MINI_INSTANCE
  //   -> OnMiniInstanceStarted() ->
  // RUNNING_MINI_INSTANCE.
  //   -> RequestUpgrade() ->
  // STARTING_FULL_INSTANCE
  //   -> OnUpgraded() ->
  // CONNECTING_MOJO
  //   -> OnMojoConnected() ->
  // RUNNING_FULL_INSTANCE
  //
  // Note that, if RequestUpgrade() is called during STARTING_MINI_INSTANCE
  // state, the state change to STARTING_FULL_INSTANCE is suspended until
  // the state becomes RUNNING_MINI_INSTANCE.
  //
  // Upon |StartMiniInstance()| call, it first waits for # of CPU cores
  // currently available to be reported, and then then asks SessionManager to
  // start mini container (or Concierge to start mini VM if ARCVM is in use)
  // and moves to STARTING_MINI_INSTANCE state.
  //
  // At any state, Stop() can be called. It may not immediately stop the
  // instance, but will eventually stop it. The actual stop will be notified
  // via ArcSession::Observer::OnSessionStopped().
  //
  // When Stop() is called, it makes various behavior based on the current
  // phase.
  //
  // NOT_STARTED:
  //   Do nothing. Immediately transition to the STOPPED state.
  // STARTING_{MINI,FULL}_INSTANCE:
  //   The ARC instance is starting via SessionManager. Stop() just sets the
  //   flag and return. On the main task completion, a callback will run on the
  //   thread, and the flag is checked at the beginning of them. This should
  //   work under the assumption that the main tasks do not block indefinitely.
  //   In its callback, it checks if ARC instance is successfully started or
  //   not. In case of success, a request to stop the ARC instance is sent to
  //   SessionManager. Its completion will be notified via ArcInstanceStopped.
  //   Otherwise, it just turns into STOPPED state.
  // CONNECTING_MOJO:
  //   The main task runs on ThreadPool's thread, but it is a blocking call.
  //   So, Stop() sends a request to cancel the blocking by closing the pipe
  //   whose read side is also polled. Then, in its callback, similar to
  //   STARTING_{MINI,FULL}_INSTANCE, a request to stop the ARC instance is
  //   sent to SessionManager, and ArcInstanceStopped handles remaining
  //   procedure.
  // RUNNING_{MINI,FULL}_INSTANCE:
  //   There is no more callback which runs on normal flow, so Stop() requests
  //   to stop the ARC instance via SessionManager.
  //
  // Another trigger to change the state coming from outside of this class is an
  // event, ArcInstanceStopped(), sent from SessionManager when the ARC instance
  // terminates. ArcInstanceStopped() turns the state into STOPPED immediately.
  //
  // In NOT_STARTED or STOPPED state, the instance can be safely destructed.
  // Specifically, in STOPPED state, there may be inflight operations or
  // pending callbacks. Though, what they do is just do-nothing conceptually
  // and they can be safely ignored.
  //
  // Note: Order of constants below matters. Please make sure to sort them
  // in chronological order.
  enum class State {
    // ARC is not yet started.
    NOT_STARTED,

    // It's waiting for CPU cores information to be available.
    WAITING_FOR_NUM_CORES,

    // The request to start a mini instance has been sent.
    STARTING_MINI_INSTANCE,

    // The instance is set up, but only a handful of processes NOT including
    // arcbridgeservice (i.e. mojo endpoint) are running.
    RUNNING_MINI_INSTANCE,

    // The request to upgrade to a full instance has been sent.
    STARTING_FULL_INSTANCE,

    // The instance has started. Waiting for it to connect to the IPC bridge.
    CONNECTING_MOJO,

    // The instance is fully set up.
    RUNNING_FULL_INSTANCE,

    // ARC is terminated.
    STOPPED,
  };

  // Delegate interface to emulate ArcBridgeHost mojo connection establishment.
  class Delegate {
   public:
    // Used for ConnectMojo completion callback.
    using ConnectMojoCallback =
        base::OnceCallback<void(std::unique_ptr<mojom::ArcBridgeHost>)>;
    using CreateSocketCallback = base::OnceCallback<void(base::ScopedFD)>;

    virtual ~Delegate() = default;

    // Creates arcbridge UNIX domain socket on a worker pool.
    virtual void CreateSocket(CreateSocketCallback callback) = 0;

    // Connects ArcBridgeHost via |socket_fd|, and invokes |callback| with
    // connected ArcBridgeHost instance if succeeded (or nullptr if failed).
    // Returns a FD which cancels the current connection on close(2).
    virtual base::ScopedFD ConnectMojo(base::ScopedFD socket_fd,
                                       ConnectMojoCallback callback) = 0;

    // Gets the available disk space under /home. The result is in bytes.
    using GetFreeDiskSpaceCallback =
        base::OnceCallback<void(absl::optional<int64_t>)>;
    virtual void GetFreeDiskSpace(GetFreeDiskSpaceCallback callback) = 0;

    // Returns the channel for the installation.
    virtual version_info::Channel GetChannel() = 0;

    // Creates and returns a client adapter.
    virtual std::unique_ptr<ArcClientAdapter> CreateClient() = 0;
  };

  using SystemMemoryInfoCallback =
      base::RepeatingCallback<bool(base::SystemMemoryInfoKB*)>;

  ArcSessionImpl(
      std::unique_ptr<Delegate> delegate,
      ash::SchedulerConfigurationManagerBase* scheduler_configuration_manager,
      AdbSideloadingAvailabilityDelegate*
          adb_sideloading_availability_delegate);

  ArcSessionImpl(const ArcSessionImpl&) = delete;
  ArcSessionImpl& operator=(const ArcSessionImpl&) = delete;

  ~ArcSessionImpl() override;

  // Returns default delegate implementation used for the production.
  static std::unique_ptr<Delegate> CreateDelegate(
      ArcBridgeService* arc_bridge_service,
      version_info::Channel channel);

  State GetStateForTesting() { return state_; }
  ArcClientAdapter* GetClientForTesting() { return client_.get(); }

  void SetSystemMemoryInfoCallbackForTesting(SystemMemoryInfoCallback callback);

  // ArcSession overrides:
  void StartMiniInstance() override;
  void RequestUpgrade(UpgradeParams params) override;
  void Stop() override;
  bool IsStopRequested() override;
  void OnShutdown() override;
  void SetUserInfo(const cryptohome::Identification& cryptohome_id,
                   const std::string& hash,
                   const std::string& serial_number) override;
  void SetDemoModeDelegate(
      ArcClientAdapter::DemoModeDelegate* delegate) override;
  void TrimVmMemory(TrimVmMemoryCallback callback, int page_limit) override;
  void SetDefaultDeviceScaleFactor(float scale_factor) override;
  void SetUseVirtioBlkData(bool use_virtio_blk_data) override;

  // ash::SchedulerConfigurationManagerBase::Observer overrides:
  void OnConfigurationSet(bool success, size_t num_cores_disabled) override;

 private:
  // D-Bus callback for StartArcMiniContainer().
  void OnMiniInstanceStarted(bool result);

  // Sends a D-Bus message to upgrade to a full instance.
  void DoUpgrade();

  // Called when arcbridge socket is created.
  void OnSocketCreated(base::ScopedFD fd);

  // D-Bus callback for UpgradeArcContainer(). |socket_fd| should be a socket
  // which should be accept(2)ed to connect ArcBridgeService Mojo channel.
  void OnUpgraded(base::ScopedFD socket_fd, bool result);

  // D-Bus callback for UpgradeArcContainer when the upgrade fails.
  // |low_free_disk_space| signals whether the failure was due to low free disk
  // space.
  void OnUpgradeError(bool low_free_disk_space);

  // Called when Mojo connection is established (or canceled during the
  // connect.)
  void OnMojoConnected(std::unique_ptr<mojom::ArcBridgeHost> arc_bridge_host);

  // Request to stop ARC instance via DBus. Also backs up the ARC
  // bug report if |should_backup_log| is set to true.
  void StopArcInstance(bool on_shutdown, bool should_backup_log);

  // ArcClientAdapter::Observer:
  void ArcInstanceStopped(bool is_system_shutdown) override;

  // Completes the termination procedure. Note that calling this may end up with
  // deleting |this| because the function calls observers' OnSessionStopped().
  void OnStopped(ArcStopReason reason);

  // Called when |state_| moves to STARTING_MINI_INSTANCE.
  void DoStartMiniInstance(size_t num_cores_disabled);

  // Free disk space under /home in bytes.
  void OnFreeDiskSpace(absl::optional<int64_t> space);

  // Whether adb sideloading can be changed
  void OnCanChangeAdbSideloading(bool can_change_adb_sideloading);

  // Checks whether a function runs on the thread where the instance is
  // created.
  THREAD_CHECKER(thread_checker_);

  // Delegate implementation.
  std::unique_ptr<Delegate> delegate_;

  // An adapter to talk to the OS daemon.
  std::unique_ptr<ArcClientAdapter> client_;

  // The state of the session.
  State state_ = State::NOT_STARTED;

  // When Stop() is called, this flag is set.
  bool stop_requested_ = false;

  // Whether the full container has been requested
  bool upgrade_requested_ = false;

  // Whether there's insufficient disk space to start the container.
  bool insufficient_disk_space_ = false;

  // Whether ARCVM uses virtio-blk for /data.
  bool use_virtio_blk_data_ = false;

  // In CONNECTING_MOJO state, this is set to the write side of the pipe
  // to notify cancelling of the procedure.
  base::ScopedFD accept_cancel_pipe_;

  // Parameters to upgrade request.
  UpgradeParams upgrade_params_;

  // Mojo endpoint.
  std::unique_ptr<mojom::ArcBridgeHost> arc_bridge_host_;

  int lcd_density_ = 0;
  const raw_ptr<ash::SchedulerConfigurationManagerBase, ExperimentalAsh>
      scheduler_configuration_manager_;

  // Owned by ArcSessionManager.
  const raw_ptr<AdbSideloadingAvailabilityDelegate, ExperimentalAsh>
      adb_sideloading_availability_delegate_;

  // Callback to read system memory info.
  SystemMemoryInfoCallback system_memory_info_callback_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcSessionImpl> weak_factory_{this};
};

// Stringified output for logging purpose.
std::ostream& operator<<(std::ostream& os, ArcSessionImpl::State state);

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_SESSION_IMPL_H_
