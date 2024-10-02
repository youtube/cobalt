// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/component_updater_service_internal.h"
#include "components/crx_file/crx_verifier.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/component_unpacker.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/patcher.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ComponentUnpacker = update_client::ComponentUnpacker;
using Configurator = update_client::Configurator;
using CrxUpdateItem = update_client::CrxUpdateItem;
using TestConfigurator = update_client::TestConfigurator;
using UpdateClient = update_client::UpdateClient;

using ::testing::_;
using ::testing::Invoke;

namespace component_updater {

namespace {

// This hash corresponds to jebgalgnebhfojomionfpkfelancnnkf.crx.
const uint8_t kSha256Hash[] = {0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec,
                               0x8e, 0xd5, 0xfa, 0x54, 0xb0, 0xd2, 0xdd, 0xa5,
                               0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47, 0xf6, 0xc4,
                               0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};

constexpr base::FilePath::CharType relative_install_dir[] =
    FILE_PATH_LITERAL("fake");

base::FilePath test_file(const char* file) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("update_client")
      .AppendASCII(file);
}

class MockUpdateClient : public UpdateClient {
 public:
  MockUpdateClient() = default;

  base::RepeatingClosure Install(
      const std::string& id,
      CrxDataCallback crx_data_callback,
      CrxStateChangeCallback crx_state_change_callback,
      Callback callback) override {
    DoInstall(id, std::move(crx_data_callback));
    std::move(callback).Run(update_client::Error::NONE);
    return base::DoNothing();
  }

  void Update(const std::vector<std::string>& ids,
              CrxDataCallback crx_data_callback,
              CrxStateChangeCallback crx_state_change_callback,
              bool is_foreground,
              Callback callback) override {
    DoUpdate(ids, std::move(crx_data_callback));
    std::move(callback).Run(update_client::Error::NONE);
  }

  void SendUninstallPing(const CrxComponent& crx_component,
                         int reason,
                         Callback callback) override {
    DoSendUninstallPing(crx_component, reason);
    std::move(callback).Run(update_client::Error::NONE);
  }

  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));
  MOCK_METHOD2(DoInstall,
               void(const std::string& id,
                    const CrxDataCallback& crx_data_callback));
  MOCK_METHOD2(DoUpdate,
               void(const std::vector<std::string>& ids,
                    const CrxDataCallback& crx_data_callback));
  MOCK_METHOD5(CheckForUpdate,
               void(const std::string& ids,
                    CrxDataCallback crx_data_callback,
                    CrxStateChangeCallback crx_state_change_callback,
                    bool is_foreground,
                    Callback callback));
  MOCK_CONST_METHOD2(GetCrxUpdateState,
                     bool(const std::string& id, CrxUpdateItem* update_item));
  MOCK_CONST_METHOD1(IsUpdating, bool(const std::string& id));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD2(DoSendUninstallPing,
               void(const CrxComponent& crx_component, int reason));

 private:
  ~MockUpdateClient() override = default;
};

class MockInstallerPolicy : public ComponentInstallerPolicy {
 public:
  using ComponentReadyCallback =
      base::OnceCallback<void(const base::Version& version,
                              const base::FilePath& install_dir,
                              base::Value::Dict manifest)>;
  explicit MockInstallerPolicy(
      ComponentReadyCallback component_ready_cb = ComponentReadyCallback())
      : component_ready_cb_(std::move(component_ready_cb)) {}
  ~MockInstallerPolicy() override = default;

  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& dir) const override {
    return true;
  }

  bool SupportsGroupPolicyEnabledComponentUpdates() const override {
    return true;
  }

  bool RequiresNetworkEncryption() const override { return true; }

  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override {
    return update_client::CrxInstaller::Result(0);
  }

  void OnCustomUninstall() override {}

  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override {
    if (component_ready_cb_) {
      std::move(component_ready_cb_)
          .Run(version, install_dir, std::move(manifest));
    }
  }

  base::FilePath GetRelativeInstallDir() const override {
    return base::FilePath(relative_install_dir);
  }

  void GetHash(std::vector<uint8_t>* hash) const override { GetPkHash(hash); }

  std::string GetName() const override { return "fake name"; }

  update_client::InstallerAttributes GetInstallerAttributes() const override {
    update_client::InstallerAttributes installer_attributes;
    installer_attributes["ap"] = "fake-ap";
    installer_attributes["is-enterprise"] = "1";
    return installer_attributes;
  }

 private:
  static void GetPkHash(std::vector<uint8_t>* hash) {
    hash->assign(std::begin(kSha256Hash), std::end(kSha256Hash));
  }

  ComponentReadyCallback component_ready_cb_;
};

class MockUpdateScheduler : public UpdateScheduler {
 public:
  MOCK_METHOD4(Schedule,
               void(const base::TimeDelta& initial_delay,
                    const base::TimeDelta& delay,
                    const UserTask& user_task,
                    const OnStopTaskCallback& on_stop));
  MOCK_METHOD0(Stop, void());
};

class ComponentInstallerTest : public testing::Test {
 public:
  ComponentInstallerTest();
  ~ComponentInstallerTest() override;

  MockUpdateClient& update_client() { return *update_client_; }
  ComponentUpdateService* component_updater() {
    return component_updater_.get();
  }
  scoped_refptr<TestConfigurator> configurator() const { return config_; }
  base::OnceClosure quit_closure() { return runloop_.QuitClosure(); }
  MockUpdateScheduler& scheduler() { return *scheduler_; }

 protected:
  void RunThreads();
  void Unpack(const base::FilePath& crx_path);
  ComponentUnpacker::Result result() const { return result_; }

  base::test::TaskEnvironment task_environment_;

 private:
  void UnpackComplete(const ComponentUnpacker::Result& result);
  void Schedule(const base::TimeDelta& initial_delay,
                const base::TimeDelta& delay,
                const UpdateScheduler::UserTask& user_task,
                const UpdateScheduler::OnStopTaskCallback& on_stop);

  const scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();
  base::RunLoop runloop_;

  std::unique_ptr<TestingPrefServiceSimple> pref_ =
      std::make_unique<TestingPrefServiceSimple>();

  scoped_refptr<TestConfigurator> config_ =
      base::MakeRefCounted<TestConfigurator>(pref_.get());
  raw_ptr<MockUpdateScheduler> scheduler_ = nullptr;
  scoped_refptr<MockUpdateClient> update_client_ =
      base::MakeRefCounted<MockUpdateClient>();
  std::unique_ptr<ComponentUpdateService> component_updater_;
  ComponentUnpacker::Result result_;
};

ComponentInstallerTest::ComponentInstallerTest() {
  EXPECT_CALL(update_client(), AddObserver(_)).Times(1);
  auto scheduler = std::make_unique<MockUpdateScheduler>();
  scheduler_ = scheduler.get();
  ON_CALL(*scheduler_, Schedule(_, _, _, _))
      .WillByDefault(Invoke(this, &ComponentInstallerTest::Schedule));
  component_updater_ = std::make_unique<CrxUpdateService>(
      config_, std::move(scheduler), update_client_, "");
  RegisterComponentUpdateServicePrefs(pref_->registry());
  update_client::RegisterPrefs(pref_->registry());
}

ComponentInstallerTest::~ComponentInstallerTest() {
  EXPECT_CALL(update_client(), RemoveObserver(_)).Times(1);
  component_updater_.reset();
}

void ComponentInstallerTest::RunThreads() {
  runloop_.Run();
}

void ComponentInstallerTest::Unpack(const base::FilePath& crx_path) {
  auto config = base::MakeRefCounted<TestConfigurator>();
  auto component_unpacker = base::MakeRefCounted<ComponentUnpacker>(
      std::vector<uint8_t>(std::begin(kSha256Hash), std::end(kSha256Hash)),
      crx_path, nullptr, config->GetUnzipperFactory()->Create(),
      config->GetPatcherFactory()->Create(), crx_file::VerifierFormat::CRX3);
  component_unpacker->Unpack(base::BindOnce(
      &ComponentInstallerTest::UnpackComplete, base::Unretained(this)));
  RunThreads();
}

void ComponentInstallerTest::UnpackComplete(
    const ComponentUnpacker::Result& result) {
  result_ = result;

  EXPECT_EQ(update_client::UnpackerError::kNone, result_.error);
  EXPECT_EQ(0, result_.extended_error);

  main_thread_task_runner_->PostTask(FROM_HERE, quit_closure());
}

void ComponentInstallerTest::Schedule(
    const base::TimeDelta& initial_delay,
    const base::TimeDelta& delay,
    const UpdateScheduler::UserTask& user_task,
    const UpdateScheduler::OnStopTaskCallback& on_stop) {
  user_task.Run(base::DoNothing());
}

}  // namespace

absl::optional<base::FilePath> CreateComponentDirectory(
    const base::FilePath& base_dir,
    const std::string& name,
    const std::string& version,
    const std::string& min_env_version) {
  base::FilePath component_dir =
      base_dir.AppendASCII(name).AppendASCII(version);

  if (!base::CreateDirectory(component_dir))
    return absl::nullopt;

  if (!base::WriteFile(component_dir.AppendASCII("manifest.json"),
                       base::StringPrintf(R"({
        "name": "%s",
        "version": "%s",
        "min_env_version": "%s"
    })",
                                          name.c_str(), version.c_str(),
                                          min_env_version.c_str())))
    return absl::nullopt;

  return absl::make_optional(component_dir);
}

// Tests that the component metadata is propagated from the component installer
// and its component policy, through the instance of the CrxComponent, to the
// component updater service.
TEST_F(ComponentInstallerTest, RegisterComponent) {
  class LoopHandler {
   public:
    LoopHandler(int max_cnt, base::OnceClosure quit_closure)
        : max_cnt_(max_cnt), quit_closure_(std::move(quit_closure)) {}

    void OnUpdate(const std::vector<std::string>& ids,
                  const UpdateClient::CrxDataCallback& crx_data_callback) {
      static int cnt = 0;
      ++cnt;
      if (cnt >= max_cnt_)
        std::move(quit_closure_).Run();
    }

   private:
    const int max_cnt_;
    base::OnceClosure quit_closure_;
  };

  base::ScopedPathOverride scoped_path_override(DIR_COMPONENT_USER);

  const std::string id("jebgalgnebhfojomionfpkfelancnnkf");

  // Quit after one update check has been fired.
  LoopHandler loop_handler(1, quit_closure());
  EXPECT_CALL(update_client(), DoUpdate(_, _))
      .WillRepeatedly(Invoke(&loop_handler, &LoopHandler::OnUpdate));

  EXPECT_CALL(update_client(), GetCrxUpdateState(id, _)).Times(1);
  EXPECT_CALL(update_client(), Stop()).Times(1);
  EXPECT_CALL(scheduler(), Schedule(_, _, _, _)).Times(1);
  EXPECT_CALL(scheduler(), Stop()).Times(1);

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<MockInstallerPolicy>());
  installer->Register(component_updater(), base::OnceClosure());

  RunThreads();

  CrxUpdateItem item;
  EXPECT_TRUE(component_updater()->GetComponentDetails(id, &item));
  ASSERT_TRUE(item.component);
  const CrxComponent& component = *item.component;

  update_client::InstallerAttributes expected_attrs;
  expected_attrs["ap"] = "fake-ap";
  expected_attrs["is-enterprise"] = "1";

  EXPECT_EQ(
      std::vector<uint8_t>(std::begin(kSha256Hash), std::end(kSha256Hash)),
      component.pk_hash);
  EXPECT_EQ(base::Version("0.0.0.0"), component.version);
  EXPECT_TRUE(component.fingerprint.empty());
  EXPECT_STREQ("fake name", component.name.c_str());
  EXPECT_EQ(expected_attrs, component.installer_attributes);
  EXPECT_TRUE(component.requires_network_encryption);
}

// Tests that `ComponentInstallerPolicy::ComponentReady` and the completion
// callback of `ComponentInstaller::Register` are called in sequence.
TEST_F(ComponentInstallerTest, InstallerRegister_CheckSequence) {
  class RegisterHandler {
   public:
    virtual ~RegisterHandler() = default;

    virtual void ComponentReady() = 0;
    virtual void RegisterComplete() = 0;
  };

  // Allows defining call expectations on its functions when the functions
  // are invoked by callbacks posted from `ComponentInstaller::Register`.
  class MockRegisterHandler : public RegisterHandler {
   public:
    MockRegisterHandler() {
      ON_CALL(*this, ComponentReady)
          .WillByDefault(Invoke(this, &MockRegisterHandler::CheckSequence));
      ON_CALL(*this, RegisterComplete)
          .WillByDefault(Invoke(this, &MockRegisterHandler::CheckSequence));
    }

    MOCK_METHOD(void, ComponentReady, (), (override));
    MOCK_METHOD(void, RegisterComplete, (), (override));

   private:
    void CheckSequence() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }
    SEQUENCE_CHECKER(sequence_checker_);
  };

  base::ScopedPathOverride scoped_path_override(DIR_COMPONENT_USER);

  // Install a CRX component so that `ComponentInstallerPolicy::ComponentReady`
  // can be invoked later on.
  {
    base::RunLoop run_loop;
    auto installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<MockInstallerPolicy>());
    Unpack(test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));
    ASSERT_EQ(result().error, update_client::UnpackerError::kNone);
    base::FilePath base_dir;
    ASSERT_TRUE(base::PathService::Get(DIR_COMPONENT_USER, &base_dir));
    base_dir = base_dir.Append(relative_install_dir);
    ASSERT_TRUE(base::CreateDirectory(base_dir));
    installer->Install(
        result().unpack_path, update_client::jebg_public_key, nullptr,
        base::DoNothing(),
        base::BindLambdaForTesting(
            [&run_loop](const update_client::CrxInstaller::Result& result) {
              ASSERT_EQ(result.error, 0);
              run_loop.QuitClosure().Run();
            }));
    run_loop.Run();
  }

  base::RunLoop run_loop;
  EXPECT_CALL(update_client(), DoUpdate(_, _)).WillOnce(Invoke([&run_loop]() {
    run_loop.QuitClosure().Run();
  }));

  // Set up expectations for uninteresting calls on the mocks due to component
  // updater waking up after the component is registered.
  EXPECT_CALL(scheduler(), Schedule(_, _, _, _)).Times(1);
  EXPECT_CALL(scheduler(), Stop()).Times(1);
  EXPECT_CALL(update_client(), Stop()).Times(1);

  MockRegisterHandler mock_register_handler;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(mock_register_handler, ComponentReady()).Times(1);
    EXPECT_CALL(mock_register_handler, RegisterComplete()).Times(1);
  }

  auto installer_policy =
      std::make_unique<MockInstallerPolicy>(base::BindLambdaForTesting(
          [&mock_register_handler](const base::Version& version,
                                   const base::FilePath& install_dir,
                                   base::Value::Dict manifest) {
            EXPECT_EQ(version.GetString(), "1.0");
            mock_register_handler.ComponentReady();
          }));
  auto installer =
      base::MakeRefCounted<ComponentInstaller>(std::move(installer_policy));
  installer->Register(component_updater(),
                      base::BindLambdaForTesting([&mock_register_handler]() {
                        mock_register_handler.RegisterComplete();
                      }));
  run_loop.Run();
}

// Tests that the unpack path is removed when the install succeeded.
TEST_F(ComponentInstallerTest, UnpackPathInstallSuccess) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<MockInstallerPolicy>());

  Unpack(test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));

  const auto unpack_path = result().unpack_path;
  EXPECT_TRUE(base::DirectoryExists(unpack_path));
  EXPECT_EQ(update_client::jebg_public_key, result().public_key);

  base::ScopedPathOverride scoped_path_override(DIR_COMPONENT_USER);
  base::FilePath base_dir;
  EXPECT_TRUE(base::PathService::Get(DIR_COMPONENT_USER, &base_dir));
  base_dir = base_dir.Append(relative_install_dir);
  EXPECT_TRUE(base::CreateDirectory(base_dir));
  installer->Install(
      unpack_path, update_client::jebg_public_key, nullptr, base::DoNothing(),
      base::BindOnce([](const update_client::CrxInstaller::Result& result) {
        EXPECT_EQ(0, result.error);
      }));

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::PathExists(unpack_path));
  EXPECT_CALL(update_client(), Stop()).Times(1);
  EXPECT_CALL(scheduler(), Stop()).Times(1);
}

// Tests that the unpack path is removed when the install failed.
TEST_F(ComponentInstallerTest, UnpackPathInstallError) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<MockInstallerPolicy>());

  Unpack(test_file("jebgalgnebhfojomionfpkfelancnnkf.crx"));

  const auto unpack_path = result().unpack_path;
  EXPECT_TRUE(base::DirectoryExists(unpack_path));

  // Test the precondition that DIR_COMPONENT_USER is not registered with
  // the path service.
  base::FilePath base_dir;
  EXPECT_FALSE(base::PathService::Get(DIR_COMPONENT_USER, &base_dir));

  // Calling |Install| fails since DIR_COMPONENT_USER does not exist.
  installer->Install(
      unpack_path, update_client::jebg_public_key, nullptr, base::DoNothing(),
      base::BindOnce([](const update_client::CrxInstaller::Result& result) {
        EXPECT_EQ(static_cast<int>(
                      update_client::InstallError::NO_DIR_COMPONENT_USER),
                  result.error);
      }));

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::PathExists(unpack_path));
  EXPECT_CALL(update_client(), Stop()).Times(1);
  EXPECT_CALL(scheduler(), Stop()).Times(1);
}

TEST_F(ComponentInstallerTest, SelectComponentVersion) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<MockInstallerPolicy>());

  base::FilePath base_dir;
  base::ScopedPathOverride scoped_path_override(DIR_COMPONENT_USER);
  ASSERT_TRUE(base::PathService::Get(DIR_COMPONENT_USER, &base_dir));
  base_dir = base_dir.AppendASCII("select_component_version_test");

  for (const auto* n : {"1", "2", "3", "4", "5", "6", "7"}) {
    CreateComponentDirectory(base_dir, "test_component",
                             base::StrCat({n, ".0.0.0"}), "0.0.1");
  }

  base_dir = base_dir.AppendASCII("test_component");

  absl::optional<base::Version> selected_component;

  auto registration_info =
      base::MakeRefCounted<ComponentInstaller::RegistrationInfo>();
  selected_component = installer->SelectComponentVersion(
      base::Version("1.0.0.0"), base_dir, registration_info);
  ASSERT_TRUE(selected_component &&
              selected_component == base::Version("1.0.0.0"));
  ASSERT_EQ(registration_info->version, base::Version("1.0.0.0"));

  // Case where no valid bundled or registered version.
  registration_info =
      base::MakeRefCounted<ComponentInstaller::RegistrationInfo>();
  selected_component = installer->SelectComponentVersion(
      base::Version("0.0.0.0"), base_dir, registration_info);
  ASSERT_TRUE(selected_component &&
              *selected_component == base::Version("7.0.0.0"));
  ASSERT_EQ(registration_info->version, base::Version("7.0.0.0"));

  registration_info->version = base::Version("3.0.0.0");
  selected_component = installer->SelectComponentVersion(
      base::Version("5.0.0.0"), base_dir, registration_info);
  ASSERT_TRUE(selected_component &&
              *selected_component == base::Version("5.0.0.0"));
  ASSERT_EQ(registration_info->version, base::Version("5.0.0.0"));

  registration_info->version = base::Version("4.0.0.0");
  selected_component = installer->SelectComponentVersion(
      base::Version("0.0.0.0"), base_dir, registration_info);
  ASSERT_TRUE(selected_component &&
              *selected_component == base::Version("7.0.0.0"));
  ASSERT_EQ(registration_info->version, base::Version("7.0.0.0"));

  registration_info->version = base::Version("12.0.0.0");
  selected_component = installer->SelectComponentVersion(
      base::Version("1.0.0.0"), base_dir, registration_info);
  ASSERT_FALSE(selected_component);
  ASSERT_EQ(registration_info->version, base::Version("12.0.0.0"));

  registration_info->version = base::Version("6.0.0.0");
  selected_component = installer->SelectComponentVersion(
      base::Version("1.0.0.0"), base_dir, registration_info);
  ASSERT_TRUE(selected_component &&
              *selected_component == base::Version("7.0.0.0"));
  ASSERT_EQ(registration_info->version, base::Version("7.0.0.0"));
}

}  // namespace component_updater
