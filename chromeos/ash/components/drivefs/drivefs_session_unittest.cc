// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_session.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "chromeos/ash/components/drivefs/fake_drivefs.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-test-utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs {
namespace {

constexpr char kExpectedMountDir[] = "drivefs-salt-g-ID";
constexpr char kExpectedMountPath[] = "/media/drivefsroot/mountdir";
constexpr char kExpectedDataDir[] = "/path/to/profile/GCache/v2/salt-g-ID";
constexpr char kExpectedMyFilesDir[] = "/path/to/profile/MyFiles";

static const absl::optional<base::TimeDelta> kEmptyDelay;
static const absl::optional<base::TimeDelta> kDefaultDelay = base::Seconds(5);

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using MountFailure = DriveFsSession::MountObserver::MountFailure;

class DriveFsDiskMounterTest : public testing::Test {
 protected:
  std::string StartMount(DiskMounter* mounter) {
    auto token = base::UnguessableToken::Create();
    std::string source;
    EXPECT_CALL(
        disk_manager_,
        MountPath(testing::StartsWith("drivefs://"), "", kExpectedMountDir,
                  testing::AllOf(
                      testing::Contains(
                          "datadir=/path/to/profile/GCache/v2/salt-g-ID"),
                      testing::Contains("myfiles=/path/to/profile/MyFiles")),
                  _, ash::MountAccessMode::kReadWrite, _))
        .WillOnce(testing::DoAll(testing::SaveArg<0>(&source),
                                 MoveArg<6>(&mount_callback_)));

    mounter->Mount(token, base::FilePath(kExpectedDataDir),
                   base::FilePath(kExpectedMyFilesDir), kExpectedMountDir,
                   base::BindOnce(&DriveFsDiskMounterTest::OnCompleted,
                                  base::Unretained(this)));
    testing::Mock::VerifyAndClear(&disk_manager_);
    return source.substr(strlen("drivefs://"));
  }

  MOCK_METHOD1(OnCompleted, void(base::FilePath));

  base::test::TaskEnvironment task_environment_;
  ash::disks::MockDiskMountManager disk_manager_;
  ash::disks::DiskMountManager::MountPathCallback mount_callback_;
};

TEST_F(DriveFsDiskMounterTest, MountUnmount) {
  auto mounter = DiskMounter::Create(&disk_manager_);
  auto token = StartMount(mounter.get());
  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnCompleted(base::FilePath(kExpectedMountPath)))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  std::move(mount_callback_)
      .Run(ash::MountError::kSuccess,
           {base::StrCat({"drivefs://", token}), kExpectedMountPath,
            ash::MountType::kNetworkStorage});
  run_loop.Run();

  EXPECT_CALL(disk_manager_, UnmountPath(kExpectedMountPath, _));
  mounter.reset();
}

TEST_F(DriveFsDiskMounterTest, DestroyAfterMounted) {
  auto mounter = DiskMounter::Create(&disk_manager_);
  auto token = StartMount(mounter.get());
  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnCompleted(base::FilePath(kExpectedMountPath)))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  std::move(mount_callback_)
      .Run(ash::MountError::kSuccess,
           {base::StrCat({"drivefs://", token}), kExpectedMountPath,
            ash::MountType::kNetworkStorage});
  run_loop.Run();

  EXPECT_CALL(disk_manager_, UnmountPath(kExpectedMountPath, _));
}

TEST_F(DriveFsDiskMounterTest, DestroyBeforeMounted) {
  EXPECT_CALL(disk_manager_, UnmountPath(_, _)).Times(0);
  auto mounter = DiskMounter::Create(&disk_manager_);
  StartMount(mounter.get());
}

TEST_F(DriveFsDiskMounterTest, MountError) {
  EXPECT_CALL(disk_manager_, UnmountPath(_, _)).Times(0);

  auto mounter = DiskMounter::Create(&disk_manager_);
  auto token = StartMount(mounter.get());
  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnCompleted(base::FilePath()))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  std::move(mount_callback_)
      .Run(ash::MountError::kInvalidMountOptions,
           {base::StrCat({"drivefs://", token}), kExpectedMountPath,
            ash::MountType::kNetworkStorage});
  run_loop.Run();
}

class MockDiskMounter : public DiskMounter {
 public:
  MockDiskMounter() { ++gInstanceCounter; }

  MockDiskMounter(const MockDiskMounter&) = delete;
  MockDiskMounter& operator=(const MockDiskMounter&) = delete;

  ~MockDiskMounter() override { --gInstanceCounter; }

  void Mount(const base::UnguessableToken& token,
             const base::FilePath& data_path,
             const base::FilePath& my_files_path,
             const std::string& desired_mount_dir_name,
             base::OnceCallback<void(base::FilePath)> callback) override {
    callback_ = std::move(callback);
    OnMountCalled(token, data_path, desired_mount_dir_name);
  }

  MOCK_METHOD3(OnMountCalled,
               void(const base::UnguessableToken& token,
                    const base::FilePath& data_path,
                    const std::string& desired_mount_dir_name));

  void CompleteMount(const base::FilePath& mount_path) {
    CHECK(callback_);
    std::move(callback_).Run(mount_path);
  }

  static void AssertNotMounted() { ASSERT_EQ(0, gInstanceCounter); }

 private:
  static int gInstanceCounter;
  base::OnceCallback<void(base::FilePath)> callback_;
};

int MockDiskMounter::gInstanceCounter = 0;

class MockDriveFsConnection : public DriveFsConnection,
                              public mojom::DriveFsInterceptorForTesting {
 public:
  MockDriveFsConnection() = default;

  MockDriveFsConnection(const MockDriveFsConnection&) = delete;
  MockDriveFsConnection& operator=(const MockDriveFsConnection&) = delete;

  ~MockDriveFsConnection() override = default;

  base::UnguessableToken Connect(mojom::DriveFsDelegate* delegate,
                                 base::OnceClosure on_disconnected) override {
    CHECK(!delegate_);
    on_disconnected_ = std::move(on_disconnected);
    delegate_ = delegate;
    OnConnected(delegate);
    return base::UnguessableToken::Create();
  }

  mojom::DriveFs& GetDriveFs() override { return *this; }

  MOCK_METHOD1(OnConnected, void(mojom::DriveFsDelegate* delegate));

  void ForceDisconnect() {
    if (on_disconnected_) {
      std::move(on_disconnected_).Run();
    }
  }

 private:
  mojom::DriveFs* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  raw_ptr<mojom::DriveFsDelegate, DanglingUntriaged | ExperimentalAsh>
      delegate_ = nullptr;
  base::OnceClosure on_disconnected_;
};

class DriveFsSessionForTest : public DriveFsSession {
 public:
  DriveFsSessionForTest(base::OneShotTimer* timer,
                        std::unique_ptr<DiskMounter> disk_mounter,
                        std::unique_ptr<DriveFsConnection> connection,
                        const base::FilePath& data_path,
                        const base::FilePath& my_files_path,
                        const std::string& desired_mount_dir_name,
                        MountObserver* observer)
      : DriveFsSession(timer,
                       std::move(disk_mounter),
                       std::move(connection),
                       data_path,
                       my_files_path,
                       desired_mount_dir_name,
                       observer) {}

  DriveFsSessionForTest(const DriveFsSessionForTest&) = delete;
  DriveFsSessionForTest& operator=(const DriveFsSessionForTest&) = delete;

  ~DriveFsSessionForTest() override = default;

 private:
  void GetAccessToken(const std::string& client_id,
                      const std::string& app_id,
                      const std::vector<std::string>& scopes,
                      GetAccessTokenCallback callback) override {}
  void OnSyncingStatusUpdate(mojom::SyncingStatusPtr status) override {}
  void OnItemProgress(mojom::ProgressEventPtr item_progress) override {}
  void OnFilesChanged(std::vector<mojom::FileChangePtr> changes) override {}
  void OnError(mojom::DriveErrorPtr error) override {}
  void OnTeamDrivesListReady(
      const std::vector<std::string>& team_drive_ids) override {}
  void OnTeamDriveChanged(
      const std::string& team_drive_id,
      mojom::DriveFsDelegate::CreateOrDelete change_type) override {}
  void ConnectToExtension(
      mojom::ExtensionConnectionParamsPtr params,
      mojo::PendingReceiver<mojom::NativeMessagingPort> port,
      mojo::PendingRemote<mojom::NativeMessagingHost> host,
      ConnectToExtensionCallback callback) override {}
  void DisplayConfirmDialog(mojom::DialogReasonPtr error,
                            DisplayConfirmDialogCallback callback) override {}
  void ExecuteHttpRequest(
      mojom::HttpRequestPtr request,
      mojo::PendingRemote<mojom::HttpDelegate> delegate) override {}
  void GetMachineRootID(
      mojom::DriveFsDelegate::GetMachineRootIDCallback callback) override {}
  void PersistMachineRootID(const std::string& id) override {}
  void OnMirrorSyncingStatusUpdate(mojom::SyncingStatusPtr status) override {}
};

class DriveFsSessionTest : public ::testing::Test,
                           public DriveFsSession::MountObserver {
 public:
  DriveFsSessionTest() {}

  DriveFsSessionTest(const DriveFsSessionTest&) = delete;
  DriveFsSessionTest& operator=(const DriveFsSessionTest&) = delete;

 protected:
  MOCK_METHOD1(OnMounted, void(const base::FilePath& path));
  MOCK_METHOD1(OnUnmounted, void(absl::optional<base::TimeDelta> delay));
  MOCK_METHOD2(OnMountFailed,
               void(MountFailure failure,
                    absl::optional<base::TimeDelta> delay));

  void StartMounting() {
    DCHECK(!holder_);
    DCHECK(!session_);
    auto mounter = std::make_unique<MockDiskMounter>();
    auto connection = std::make_unique<MockDriveFsConnection>();
    holder_ = std::make_unique<PointerHolder>();
    holder_->mounter = mounter.get();
    holder_->connection = connection.get();

    base::FilePath data_path(kExpectedDataDir);

    EXPECT_CALL(*holder_->connection, OnConnected(_));
    EXPECT_CALL(*holder_->mounter,
                OnMountCalled(_, data_path, kExpectedMountDir));
    session_ = std::make_unique<DriveFsSessionForTest>(
        &timer_, std::move(mounter), std::move(connection), data_path,
        base::FilePath(kExpectedMyFilesDir), kExpectedMountDir, this);
    holder_->delegate = session_.get();
    ASSERT_FALSE(session_->is_mounted());
  }

  void CompleteDiskMount() {
    holder_->mounter->CompleteMount(base::FilePath(kExpectedMountPath));
  }

  void ConfirmDriveFsMounted() { holder_->delegate->OnMounted(); }

  void FinishMounting() {
    CompleteDiskMount();
    ASSERT_FALSE(session_->is_mounted());
    EXPECT_CALL(*this, OnMounted(base::FilePath(kExpectedMountPath)));
    ConfirmDriveFsMounted();
    ASSERT_TRUE(session_->is_mounted());
  }

  void DoUnmount() {
    session_.reset();
    MockDiskMounter::AssertNotMounted();
    holder_.reset();
  }

  base::test::TaskEnvironment task_environment_;

  struct PointerHolder {
    raw_ptr<MockDiskMounter, DanglingUntriaged | ExperimentalAsh> mounter =
        nullptr;
    raw_ptr<MockDriveFsConnection, DanglingUntriaged | ExperimentalAsh>
        connection = nullptr;
    raw_ptr<mojom::DriveFsDelegate, DanglingUntriaged | ExperimentalAsh>
        delegate = nullptr;
  };
  base::MockOneShotTimer timer_;
  std::unique_ptr<PointerHolder> holder_;
  std::unique_ptr<DriveFsSession> session_;
};

}  // namespace

TEST_F(DriveFsSessionTest, OnMounted_DisksThenDriveFs) {
  EXPECT_CALL(*this, OnMountFailed(_, _)).Times(0);
  EXPECT_CALL(*this, OnUnmounted(_)).Times(0);
  ASSERT_NO_FATAL_FAILURE(StartMounting());

  CompleteDiskMount();
  ASSERT_FALSE(session_->is_mounted());
  EXPECT_CALL(*this, OnMounted(base::FilePath(kExpectedMountPath)));
  ConfirmDriveFsMounted();
  ASSERT_TRUE(session_->is_mounted());

  EXPECT_EQ(base::FilePath(kExpectedMountPath), session_->mount_path());
  ASSERT_NO_FATAL_FAILURE(DoUnmount());
}

TEST_F(DriveFsSessionTest, OnMounted_DriveFsThenDisks) {
  EXPECT_CALL(*this, OnMountFailed(_, _)).Times(0);
  EXPECT_CALL(*this, OnUnmounted(_)).Times(0);
  ASSERT_NO_FATAL_FAILURE(StartMounting());

  ConfirmDriveFsMounted();
  ASSERT_FALSE(session_->is_mounted());
  EXPECT_CALL(*this, OnMounted(base::FilePath(kExpectedMountPath)));
  CompleteDiskMount();
  ASSERT_TRUE(session_->is_mounted());

  EXPECT_EQ(base::FilePath(kExpectedMountPath), session_->mount_path());
  ASSERT_NO_FATAL_FAILURE(DoUnmount());
}

TEST_F(DriveFsSessionTest, OnMountFailed_InDriveFs) {
  EXPECT_CALL(*this, OnMounted(_)).Times(0);
  EXPECT_CALL(*this, OnUnmounted(_)).Times(0);

  ASSERT_NO_FATAL_FAILURE(StartMounting());
  CompleteDiskMount();

  EXPECT_CALL(*this, OnMountFailed(MountFailure::kUnknown, kEmptyDelay));
  holder_->delegate->OnMountFailed(kEmptyDelay);
  ASSERT_FALSE(session_->is_mounted());
}

TEST_F(DriveFsSessionTest, OnMountFailed_InDisks) {
  EXPECT_CALL(*this, OnMounted(_)).Times(0);
  EXPECT_CALL(*this, OnUnmounted(_)).Times(0);

  ASSERT_NO_FATAL_FAILURE(StartMounting());

  EXPECT_CALL(*this, OnMountFailed(MountFailure::kInvocation, kEmptyDelay));
  holder_->mounter->CompleteMount({});
  ASSERT_FALSE(session_->is_mounted());
}

TEST_F(DriveFsSessionTest, OnMountFailed_DriveFsNeedsRestart) {
  EXPECT_CALL(*this, OnMounted(_)).Times(0);
  EXPECT_CALL(*this, OnUnmounted(_)).Times(0);

  ASSERT_NO_FATAL_FAILURE(StartMounting());
  CompleteDiskMount();

  EXPECT_CALL(*this, OnMountFailed(MountFailure::kNeedsRestart, kDefaultDelay));
  holder_->delegate->OnMountFailed(kDefaultDelay);
  ASSERT_FALSE(session_->is_mounted());
}

TEST_F(DriveFsSessionTest, OnMountFailed_UnmountInObserver) {
  EXPECT_CALL(*this, OnMounted(_)).Times(0);
  EXPECT_CALL(*this, OnUnmounted(_)).Times(0);

  ASSERT_NO_FATAL_FAILURE(StartMounting());
  EXPECT_CALL(*this, OnMountFailed(MountFailure::kInvocation, kEmptyDelay))
      .WillOnce(testing::InvokeWithoutArgs([&]() { session_.reset(); }));
  holder_->mounter->CompleteMount({});
  ASSERT_FALSE(session_);
}

TEST_F(DriveFsSessionTest, DestroyBeforeMojoConnection) {
  EXPECT_CALL(*this, OnMounted(_)).Times(0);
  EXPECT_CALL(*this, OnUnmounted(_)).Times(0);

  ASSERT_NO_FATAL_FAILURE(StartMounting());
  CompleteDiskMount();

  session_.reset();
}

TEST_F(DriveFsSessionTest, DestroyBeforeMountEvent) {
  EXPECT_CALL(*this, OnMounted(_)).Times(0);
  EXPECT_CALL(*this, OnUnmounted(_)).Times(0);

  ASSERT_NO_FATAL_FAILURE(StartMounting());
  ConfirmDriveFsMounted();

  session_.reset();
}

TEST_F(DriveFsSessionTest, UnmountByRemote) {
  EXPECT_CALL(*this, OnMountFailed(_, _)).Times(0);

  ASSERT_NO_FATAL_FAILURE(StartMounting());
  ASSERT_NO_FATAL_FAILURE(FinishMounting());

  EXPECT_CALL(*this, OnUnmounted(kDefaultDelay));
  holder_->delegate->OnUnmounted(kDefaultDelay);
}

TEST_F(DriveFsSessionTest, BreakConnectionAfterMount) {
  EXPECT_CALL(*this, OnMountFailed(_, _)).Times(0);

  ASSERT_NO_FATAL_FAILURE(StartMounting());
  ASSERT_NO_FATAL_FAILURE(FinishMounting());

  EXPECT_CALL(*this, OnUnmounted(kEmptyDelay));
  holder_->connection->ForceDisconnect();
}

TEST_F(DriveFsSessionTest, BreakConnectionBeforeMount) {
  EXPECT_CALL(*this, OnMounted(_)).Times(0);
  EXPECT_CALL(*this, OnUnmounted(_)).Times(0);

  ASSERT_NO_FATAL_FAILURE(StartMounting());
  CompleteDiskMount();

  EXPECT_CALL(*this, OnMountFailed(MountFailure::kIpcDisconnect, kEmptyDelay));
  holder_->connection->ForceDisconnect();
}

TEST_F(DriveFsSessionTest, BreakConnectionOnUnmount) {
  EXPECT_CALL(*this, OnMountFailed(_, _)).Times(0);
  ASSERT_NO_FATAL_FAILURE(StartMounting());
  ASSERT_NO_FATAL_FAILURE(FinishMounting());

  EXPECT_CALL(*this, OnUnmounted(kDefaultDelay))
      .WillOnce(testing::InvokeWithoutArgs(
          [&]() { holder_->connection->ForceDisconnect(); }));
  holder_->delegate->OnUnmounted(kDefaultDelay);
  ASSERT_NO_FATAL_FAILURE(DoUnmount());
}

TEST_F(DriveFsSessionTest, MountTimeout) {
  EXPECT_CALL(*this, OnMounted(_)).Times(0);
  EXPECT_CALL(*this, OnUnmounted(_)).Times(0);

  ASSERT_NO_FATAL_FAILURE(StartMounting());
  CompleteDiskMount();

  EXPECT_CALL(*this, OnMountFailed(MountFailure::kTimeout, kEmptyDelay));
  timer_.Fire();
}

}  // namespace drivefs
