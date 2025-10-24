// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_file_helper.h"

#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/uuid.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrictMock;
using ::testing::Test;

namespace {

#if BUILDFLAG(IS_WIN)
static const char kDownloadPath[] = "c:\\\\path\\to\\download";
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
static const char kDownloadPath[] = "/path/to/download";
#endif  // BUILDFLAG(IS_WIN)

class MockDelegate : public DevToolsFileHelper::Delegate {
 public:
  MOCK_METHOD(void,
              FileSystemAdded,
              (const std::string&, const DevToolsFileHelper::FileSystem*),
              (override));
  MOCK_METHOD(void, FileSystemRemoved, (const std::string&), (override));
  MOCK_METHOD(void,
              FilePathsChanged,
              (const std::vector<std::string>&,
               const std::vector<std::string>&,
               const std::vector<std::string>&),
              (override));
};

class MockStorage : public DevToolsFileHelper::Storage {
 public:
  MOCK_METHOD(DevToolsFileHelper::FileSystem,
              RegisterFileSystem,
              (const base::FilePath&, const std::string&),
              (override));
  MOCK_METHOD(void, UnregisterFileSystem, (const base::FilePath&), (override));

  MOCK_METHOD(std::vector<base::FilePath>,
              GetDraggedFileSystemPaths,
              (const GURL&),
              (override));
};

// testing::InvokeArgument<N> does not work with base::OnceCallback. Use this
// gmock action template to invoke base::OnceCallback. `k` is the k-th argument
// and `T` is the callback's type.
ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_2_TEMPLATE_PARAMS(int, k, typename, T),
                AND_0_VALUE_PARAMS()) {
  std::move(const_cast<T&>(std::get<k>(args))).Run();
}
ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_2_TEMPLATE_PARAMS(int, k, typename, T),
                AND_1_VALUE_PARAMS(p0)) {
  std::move(const_cast<T&>(std::get<k>(args))).Run(p0);
}

}  // namespace

class DevToolsFileHelperTest : public Test {
 protected:
  StrictMock<MockDelegate>& delegate() const { return *delegate_; }
  DevToolsFileHelper* file_helper() const { return file_helper_.get(); }
  TestingProfile* profile() const { return profile_.get(); }
  StrictMock<MockStorage>& storage() const { return *storage_; }

  DevToolsFileHelper::SelectFileCallback FakeSelectFileCallback(
      base::FilePath path) {
    return base::BindLambdaForTesting(
        [path](DevToolsFileHelper::SelectedCallback selected_callback,
               DevToolsFileHelper::CanceledCallback, const base::FilePath&) {
          std::move(selected_callback).Run(path);
        });
  }

  void SetUp() override {
    TestingProfile::Builder builder;
    profile_ = builder.Build();
    storage_ = std::make_unique<StrictMock<MockStorage>>();
    delegate_ = std::make_unique<StrictMock<MockDelegate>>();
    file_helper_ = std::make_unique<DevToolsFileHelper>(
        profile(), delegate_.get(), storage_.get());

    DownloadCoreServiceFactory::GetForBrowserContext(profile())
        ->SetDownloadManagerDelegateForTesting(
            std::make_unique<ChromeDownloadManagerDelegate>(profile()));
    DownloadPrefs::FromBrowserContext(profile())->SetDownloadPath(
        base::FilePath::FromASCII(kDownloadPath));
  }

  void TearDown() override {
    DownloadCoreServiceFactory::GetForBrowserContext(profile())
        ->SetDownloadManagerDelegateForTesting(nullptr);

    file_helper_.reset();
    delegate_.reset();
    storage_.reset();
    profile_.reset();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<StrictMock<MockDelegate>> delegate_;
  std::unique_ptr<StrictMock<MockStorage>> storage_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<DevToolsFileHelper> file_helper_;
};

TEST_F(DevToolsFileHelperTest, SaveToFileBase64) {
  base::ScopedTempFile tf;
  ASSERT_TRUE(tf.Create());
  const std::vector<uint8_t> data{0, 'a', 's', 'm', 1, 0, 0, 0};

  base::RunLoop run_loop;
  file_helper()->Save(
      "https://example.com/test.wasm", "AGFzbQEAAAA=", /* save_as */ true,
      /* is_base64 */ true, FakeSelectFileCallback(tf.path()),
      base::BindLambdaForTesting([&](const std::string&) { run_loop.Quit(); }),
      base::DoNothing());
  run_loop.Run();

  EXPECT_EQ(base::ReadFileToBytes(tf.path()), data);
}

TEST_F(DevToolsFileHelperTest, SaveToFileInvalidBase64) {
  base::ScopedTempFile tf;
  ASSERT_TRUE(tf.Create());

  base::RunLoop run_loop;
  file_helper()->Save(
      "https://example.com/test.wasm", "~~~~",
      /* save_as */ true,
      /* is_base64 */ true, FakeSelectFileCallback(tf.path()),
      base::BindLambdaForTesting([&](const std::string&) { run_loop.Quit(); }),
      base::DoNothing());
  run_loop.Run();

  EXPECT_THAT(base::ReadFileToBytes(tf.path()), Optional(IsEmpty()));
}

TEST_F(DevToolsFileHelperTest, SaveToFileText) {
  base::ScopedTempFile tf;
  ASSERT_TRUE(tf.Create());
  const std::vector<uint8_t> data{'s', 'o', 'm', 'e', ' ', 't', 'e', 'x', 't'};

  base::RunLoop run_loop;
  file_helper()->Save(
      "https://example.com/test.txt", "some text",
      /* save_as */ true,
      /* is_base64 */ false, FakeSelectFileCallback(tf.path()),
      base::BindLambdaForTesting([&](const std::string&) { run_loop.Quit(); }),
      base::DoNothing());
  run_loop.Run();

  EXPECT_EQ(base::ReadFileToBytes(tf.path()), data);
}

TEST_F(DevToolsFileHelperTest, AddFileSystemWithIllegalTypeAutomatic) {
  EXPECT_CALL(delegate(), FileSystemAdded("<illegal type>", IsNull())).Times(1);

  file_helper()->AddFileSystem("automatic", base::DoNothing(),
                               base::DoNothing());

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, AddFileSystemWithIllegalTypeUUID) {
  EXPECT_CALL(delegate(), FileSystemAdded("<illegal type>", IsNull())).Times(1);

  file_helper()->AddFileSystem(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), base::DoNothing(),
      base::DoNothing());

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, AddFileSystemWithSelectionCanceled) {
  base::MockCallback<DevToolsFileHelper::SelectFileCallback> select_file_cb;
  EXPECT_CALL(select_file_cb, Run)
      .WillOnce(InvokeCallbackArgument<1, base::OnceCallback<void(void)>>());
  EXPECT_CALL(delegate(), FileSystemAdded("<selection cancelled>", IsNull()))
      .Times(1);

  file_helper()->AddFileSystem("", select_file_cb.Get(), base::DoNothing());

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, ConnectAutomaticFileSystemWithRelativePath) {
  base::MockCallback<DevToolsFileHelper::ConnectCallback> connect_cb;
  EXPECT_CALL(connect_cb, Run(false)).Times(1);
  EXPECT_CALL(delegate(), FileSystemAdded("<illegal path>", IsNull())).Times(1);

  file_helper()->ConnectAutomaticFileSystem(
      "path/to/folder", base::Uuid::GenerateRandomV4(),
      /* add_if_missing */ false, base::DoNothing(), connect_cb.Get());

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, ConnectAutomaticFileSystemWithNonExistentPath) {
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDir());
  base::FilePath path = td.GetPath().AppendASCII("NonExistent");
  base::MockCallback<DevToolsFileHelper::ConnectCallback> connect_cb;
  EXPECT_CALL(connect_cb, Run(false)).Times(1);
  EXPECT_CALL(delegate(), FileSystemAdded("<illegal path>", IsNull())).Times(1);

  base::RunLoop run_loop;
  ON_CALL(delegate(), FileSystemAdded).WillByDefault([&] { run_loop.Quit(); });
  file_helper()->ConnectAutomaticFileSystem(
      path.AsUTF8Unsafe(), base::Uuid::GenerateRandomV4(),
      /* add_if_missing */ true, base::DoNothing(), connect_cb.Get());
  run_loop.Run();

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, ConnectAutomaticFileSystemButNotAddingMissing) {
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDir());
  base::FilePath path = td.GetPath();
  base::MockCallback<DevToolsFileHelper::ConnectCallback> connect_cb;
  EXPECT_CALL(connect_cb, Run(false)).Times(1);

  file_helper()->ConnectAutomaticFileSystem(
      path.AsUTF8Unsafe(), base::Uuid::GenerateRandomV4(),
      /* add_if_missing */ false, base::DoNothing(), connect_cb.Get());

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, ConnectAutomaticFileSystemInfoBarDenied) {
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDir());
  base::FilePath path = td.GetPath();
  base::MockCallback<DevToolsFileHelper::HandlePermissionsCallback>
      handle_permissions_callback;
  EXPECT_CALL(handle_permissions_callback, Run)
      .WillOnce(
          InvokeCallbackArgument<2, base::OnceCallback<void(bool)>>(false));
  base::MockCallback<DevToolsFileHelper::ConnectCallback> connect_cb;
  EXPECT_CALL(connect_cb, Run(false)).Times(1);
  EXPECT_CALL(delegate(), FileSystemAdded("<permission denied>", IsNull()))
      .Times(1);

  base::RunLoop run_loop;
  ON_CALL(delegate(), FileSystemAdded).WillByDefault([&] { run_loop.Quit(); });
  file_helper()->ConnectAutomaticFileSystem(
      path.AsUTF8Unsafe(), base::Uuid::GenerateRandomV4(),
      /* add_if_missing */ true, handle_permissions_callback.Get(),
      connect_cb.Get());
  run_loop.Run();

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}

TEST_F(DevToolsFileHelperTest, ConnectAutomaticFileSystemAlreadyKnown) {
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDir());
  base::FilePath path = td.GetPath();
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  {
    ScopedDictPrefUpdate update(profile()->GetPrefs(),
                                prefs::kDevToolsFileSystemPaths);
    update.Get().Set(path.AsUTF8Unsafe(), uuid.AsLowercaseString());
  }
  EXPECT_THAT(file_helper()->GetFileSystems(), IsEmpty());
  DevToolsFileHelper::FileSystem file_system{
      "automatic", "test", "filesystem:test", path.AsUTF8Unsafe()};
  base::MockCallback<DevToolsFileHelper::HandlePermissionsCallback>
      handle_permissions_callback;
  EXPECT_CALL(handle_permissions_callback, Run).Times(0);
  base::MockCallback<DevToolsFileHelper::ConnectCallback> connect_cb;
  EXPECT_CALL(connect_cb, Run(true)).Times(1);
  EXPECT_CALL(storage(), RegisterFileSystem(path, "automatic"))
      .WillOnce(Return(file_system));
  EXPECT_CALL(delegate(), FileSystemAdded(IsEmpty(), Pointee(file_system)))
      .Times(1);

  base::RunLoop run_loop;
  ON_CALL(delegate(), FileSystemAdded).WillByDefault([&] { run_loop.Quit(); });
  file_helper()->ConnectAutomaticFileSystem(path.AsUTF8Unsafe(), uuid,
                                            /* add_if_missing */ false,
                                            handle_permissions_callback.Get(),
                                            connect_cb.Get());
  run_loop.Run();

  const base::Value::Dict& file_system_paths_value =
      profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths);
  EXPECT_THAT(file_system_paths_value, SizeIs(1));
  EXPECT_THAT(file_system_paths_value.FindString(path.AsUTF8Unsafe()),
              Pointee(uuid.AsLowercaseString()));
}

TEST_F(DevToolsFileHelperTest, ConnectAutomaticFileSystemNewlyAdded) {
  EXPECT_THAT(file_helper()->GetFileSystems(), IsEmpty());
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDir());
  base::FilePath path = td.GetPath();
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  DevToolsFileHelper::FileSystem file_system{
      "automatic", "test", "filesystem:test", path.AsUTF8Unsafe()};
  base::MockCallback<DevToolsFileHelper::HandlePermissionsCallback>
      handle_permissions_callback;
  EXPECT_CALL(handle_permissions_callback, Run)
      .WillOnce(
          InvokeCallbackArgument<2, base::OnceCallback<void(bool)>>(true));
  base::MockCallback<DevToolsFileHelper::ConnectCallback> connect_cb;
  EXPECT_CALL(connect_cb, Run(true)).Times(1);
  EXPECT_CALL(storage(), RegisterFileSystem(path, "automatic"))
      .WillOnce(Return(file_system));
  EXPECT_CALL(delegate(), FileSystemAdded(IsEmpty(), Pointee(file_system)))
      .Times(1);

  base::RunLoop run_loop;
  ON_CALL(delegate(), FileSystemAdded).WillByDefault([&] { run_loop.Quit(); });
  file_helper()->ConnectAutomaticFileSystem(path.AsUTF8Unsafe(), uuid,
                                            /* add_if_missing */ true,
                                            handle_permissions_callback.Get(),
                                            connect_cb.Get());
  run_loop.Run();

  const base::Value::Dict& file_system_paths_value =
      profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths);
  EXPECT_THAT(file_system_paths_value, SizeIs(1));
  EXPECT_THAT(file_system_paths_value.FindString(path.AsUTF8Unsafe()),
              Pointee(uuid.AsLowercaseString()));
}

TEST_F(DevToolsFileHelperTest, ConnectAndDisconnectKnownAutomaticFileSystem) {
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDir());
  base::FilePath path = td.GetPath();
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  {
    ScopedDictPrefUpdate update(profile()->GetPrefs(),
                                prefs::kDevToolsFileSystemPaths);
    update.Get().Set(path.AsUTF8Unsafe(), uuid.AsLowercaseString());
  }
  EXPECT_THAT(file_helper()->GetFileSystems(), IsEmpty());
  DevToolsFileHelper::FileSystem file_system{
      "automatic", "test", "filesystem:test", path.AsUTF8Unsafe()};
  base::MockCallback<DevToolsFileHelper::HandlePermissionsCallback>
      handle_permissions_callback;
  EXPECT_CALL(handle_permissions_callback, Run).Times(0);
  base::MockCallback<DevToolsFileHelper::ConnectCallback> connect_cb;
  EXPECT_CALL(connect_cb, Run(true)).Times(1);
  EXPECT_CALL(storage(), RegisterFileSystem(path, "automatic"))
      .WillOnce(Return(file_system));
  EXPECT_CALL(delegate(), FileSystemAdded(IsEmpty(), Pointee(file_system)))
      .Times(1);

  {
    // Connect the known automatic file system.
    base::RunLoop run_loop;
    ON_CALL(delegate(), FileSystemAdded).WillByDefault([&] {
      run_loop.Quit();
    });
    file_helper()->ConnectAutomaticFileSystem(path.AsUTF8Unsafe(), uuid,
                                              /* add_if_missing */ false,
                                              handle_permissions_callback.Get(),
                                              connect_cb.Get());
    run_loop.Run();

    EXPECT_TRUE(file_helper()->IsFileSystemAdded(path.AsUTF8Unsafe()));
  }

  EXPECT_CALL(storage(), UnregisterFileSystem(path)).Times(1);
  EXPECT_CALL(delegate(), FileSystemRemoved(path.AsUTF8Unsafe())).Times(1);

  {
    // Disconnect the previously connected automatic file system.
    base::RunLoop run_loop;
    ON_CALL(delegate(), FileSystemRemoved).WillByDefault([&] {
      run_loop.Quit();
    });
    file_helper()->DisconnectAutomaticFileSystem(path.AsUTF8Unsafe());
    run_loop.Run();

    EXPECT_FALSE(file_helper()->IsFileSystemAdded(path.AsUTF8Unsafe()));
  }
}

TEST_F(DevToolsFileHelperTest, DisconnectAutomaticFileSystemNotConnected) {
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDir());

  file_helper()->DisconnectAutomaticFileSystem(td.GetPath().AsUTF8Unsafe());
}

TEST_F(DevToolsFileHelperTest, RemoveAutomaticFileSystemNotConnected) {
  base::ScopedTempDir td;
  ASSERT_TRUE(td.CreateUniqueTempDir());
  base::FilePath path = td.GetPath();
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  {
    ScopedDictPrefUpdate update(profile()->GetPrefs(),
                                prefs::kDevToolsFileSystemPaths);
    update.Get().Set(path.AsUTF8Unsafe(), uuid.AsLowercaseString());
  }

  file_helper()->RemoveFileSystem(path.AsUTF8Unsafe());

  EXPECT_THAT(profile()->GetPrefs()->GetDict(prefs::kDevToolsFileSystemPaths),
              IsEmpty());
}
