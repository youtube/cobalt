// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_service.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/cxx17_backports.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/component_updater/component_updater_service_internal.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/test_configurator.h"
#include "components/update_client/test_installer.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using Configurator = update_client::Configurator;
using Result = update_client::CrxInstaller::Result;
using TestConfigurator = update_client::TestConfigurator;
using UpdateClient = update_client::UpdateClient;

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;
using ::testing::Unused;

namespace component_updater {

class MockInstaller : public update_client::CrxInstaller {
 public:
  MockInstaller() = default;
  MOCK_METHOD1(OnUpdateError, void(int error));
  MOCK_METHOD5(Install,
               void(const base::FilePath& unpack_path,
                    const std::string& public_key,
                    std::unique_ptr<InstallParams> install_params,
                    ProgressCallback progress_callback,
                    Callback callback));
  MOCK_METHOD2(GetInstalledFile,
               bool(const std::string& file, base::FilePath* installed_file));
  MOCK_METHOD0(Uninstall, bool());

 private:
  ~MockInstaller() override = default;
};

class MockUpdateClient : public UpdateClient {
 public:
  MockUpdateClient() = default;

  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));
  MOCK_METHOD4(
      Install,
      base::RepeatingClosure(const std::string& id,
                             CrxDataCallback crx_data_callback,
                             CrxStateChangeCallback crx_state_change_callback,
                             Callback callback));
  MOCK_METHOD5(Update,
               void(const std::vector<std::string>& ids,
                    CrxDataCallback crx_data_callback,
                    CrxStateChangeCallback crx_state_change_callback,
                    bool is_foreground,
                    Callback callback));
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
  MOCK_METHOD3(SendUninstallPing,
               void(const CrxComponent& crx_component,
                    int reason,
                    Callback callback));
  MOCK_METHOD2(SendRegistrationPing,
               void(const CrxComponent& crx_component, Callback callback));

 private:
  ~MockUpdateClient() override = default;
};

class MockServiceObserver : public ServiceObserver {
 public:
  MockServiceObserver() = default;
  ~MockServiceObserver() override = default;

  MOCK_METHOD2(OnEvent, void(Events event, const std::string&));
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

class LoopHandler {
 public:
  explicit LoopHandler(int max_cnt, base::OnceClosure quit_closure)
      : max_cnt_(max_cnt), quit_closure_(std::move(quit_closure)) {}

  base::RepeatingClosure OnInstall(const std::string&,
                                   UpdateClient::CrxDataCallback,
                                   UpdateClient::CrxStateChangeCallback,
                                   Callback callback) {
    Handle(std::move(callback));
    return base::DoNothing();
  }

  void OnUpdate(const std::vector<std::string>&,
                UpdateClient::CrxDataCallback,
                UpdateClient::CrxStateChangeCallback,
                bool is_foreground,
                Callback callback) {
    EXPECT_FALSE(is_foreground);
    Handle(std::move(callback));
  }

 private:
  void Handle(Callback callback) {
    ++cnt_;
    if (cnt_ >= max_cnt_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(quit_closure_));
    }
    std::move(callback).Run(update_client::Error::NONE);
  }

  const int max_cnt_ = 0;
  base::OnceClosure quit_closure_;
  int cnt_ = 0;
};

class ComponentUpdaterTest : public testing::Test {
 public:
  ComponentUpdaterTest();
  ComponentUpdaterTest(const ComponentUpdaterTest&) = delete;
  ComponentUpdaterTest& operator=(const ComponentUpdaterTest&) = delete;
  ~ComponentUpdaterTest() override;

  // Makes the full path to a component updater test file.
  const base::FilePath test_file(const char* file);

  MockUpdateClient& update_client() { return *update_client_; }
  ComponentUpdateService& component_updater() { return *component_updater_; }
  scoped_refptr<TestConfigurator> configurator() const { return config_; }
  base::OnceClosure quit_closure() { return runloop_.QuitClosure(); }
  MockUpdateScheduler& scheduler() { return *scheduler_; }

 protected:
  void RunThreads();

 private:
  void RunUpdateTask(const UpdateScheduler::UserTask& user_task);
  void Schedule(const base::TimeDelta& initial_delay,
                const base::TimeDelta& delay,
                const UpdateScheduler::UserTask& user_task,
                const UpdateScheduler::OnStopTaskCallback& on_stop);

  base::test::TaskEnvironment task_environment_;
  base::RunLoop runloop_;

  std::unique_ptr<TestingPrefServiceSimple> pref_ =
      std::make_unique<TestingPrefServiceSimple>();
  scoped_refptr<TestConfigurator> config_ =
      base::MakeRefCounted<TestConfigurator>(pref_.get());
  raw_ptr<MockUpdateScheduler> scheduler_;
  scoped_refptr<MockUpdateClient> update_client_ =
      base::MakeRefCounted<MockUpdateClient>();
  std::unique_ptr<ComponentUpdateService> component_updater_;
};

class OnDemandTester {
 public:
  void OnDemand(ComponentUpdateService* cus,
                const std::string& id,
                OnDemandUpdater::Priority priority);
  update_client::Error error() const { return error_; }

 private:
  void OnDemandComplete(update_client::Error error);

  update_client::Error error_ = update_client::Error::NONE;
};

void OnDemandTester::OnDemand(ComponentUpdateService* cus,
                              const std::string& id,
                              OnDemandUpdater::Priority priority) {
  cus->GetOnDemandUpdater().OnDemandUpdate(
      id, priority,
      base::BindOnce(&OnDemandTester::OnDemandComplete,
                     base::Unretained(this)));
}

void OnDemandTester::OnDemandComplete(update_client::Error error) {
  error_ = error;
}

std::unique_ptr<ComponentUpdateService> TestComponentUpdateServiceFactory(
    scoped_refptr<Configurator> config) {
  DCHECK(config);
  return std::make_unique<CrxUpdateService>(
      config, std::make_unique<MockUpdateScheduler>(),
      base::MakeRefCounted<MockUpdateClient>(), "");
}

ComponentUpdaterTest::ComponentUpdaterTest() {
  EXPECT_CALL(update_client(), AddObserver(_)).Times(1);
  auto scheduler = std::make_unique<MockUpdateScheduler>();
  scheduler_ = scheduler.get();
  ON_CALL(*scheduler_, Schedule(_, _, _, _))
      .WillByDefault(Invoke(this, &ComponentUpdaterTest::Schedule));
  component_updater_ = std::make_unique<CrxUpdateService>(
      config_, std::move(scheduler), update_client_, "");
  RegisterComponentUpdateServicePrefs(pref_->registry());
}

ComponentUpdaterTest::~ComponentUpdaterTest() {
  EXPECT_CALL(update_client(), RemoveObserver(_)).Times(1);
  component_updater_.reset();
}

void ComponentUpdaterTest::RunThreads() {
  runloop_.Run();
}

void ComponentUpdaterTest::RunUpdateTask(
    const UpdateScheduler::UserTask& user_task) {
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindRepeating(
                     [](const UpdateScheduler::UserTask& user_task,
                        ComponentUpdaterTest* test) {
                       user_task.Run(base::BindOnce(
                           [](const UpdateScheduler::UserTask& user_task,
                              ComponentUpdaterTest* test) {
                             test->RunUpdateTask(user_task);
                           },
                           user_task, base::Unretained(test)));
                     },
                     user_task, base::Unretained(this)));
}

void ComponentUpdaterTest::Schedule(
    const base::TimeDelta& initial_delay,
    const base::TimeDelta& delay,
    const UpdateScheduler::UserTask& user_task,
    const UpdateScheduler::OnStopTaskCallback& on_stop) {
  RunUpdateTask(user_task);
}

TEST_F(ComponentUpdaterTest, AddObserver) {
  MockServiceObserver observer;
  EXPECT_CALL(update_client(), AddObserver(&observer)).Times(1);
  EXPECT_CALL(update_client(), Stop()).Times(1);
  EXPECT_CALL(scheduler(), Stop()).Times(1);
  component_updater().AddObserver(&observer);
}

TEST_F(ComponentUpdaterTest, RemoveObserver) {
  MockServiceObserver observer;
  EXPECT_CALL(update_client(), RemoveObserver(&observer)).Times(1);
  EXPECT_CALL(update_client(), Stop()).Times(1);
  EXPECT_CALL(scheduler(), Stop()).Times(1);
  component_updater().RemoveObserver(&observer);
}

// Tests that UpdateClient::Update is called by the timer loop when
// components are registered, and the component update starts.
// Also tests that Uninstall is called when a component is unregistered.
TEST_F(ComponentUpdaterTest, RegisterComponent) {
  base::HistogramTester ht;

  scoped_refptr<MockInstaller> installer =
      base::MakeRefCounted<MockInstaller>();
  EXPECT_CALL(*installer, Uninstall()).WillOnce(Return(true));

  using update_client::jebg_hash;
  using update_client::abag_hash;

  const std::string id1 = "abagagagagagagagagagagagagagagag";
  const std::string id2 = "jebgalgnebhfojomionfpkfelancnnkf";
  std::vector<std::string> ids;
  ids.push_back(id1);
  ids.push_back(id2);

  std::vector<uint8_t> hash;
  hash.assign(std::begin(abag_hash), std::end(abag_hash));
  ComponentRegistration component1(id1, {}, hash, base::Version("1.0"), {}, {},
                                   nullptr, installer, false, true);

  hash.assign(std::begin(jebg_hash), std::end(jebg_hash));
  ComponentRegistration component2(id2, {}, hash, base::Version("0.9"), {}, {},
                                   nullptr, installer, false, true);

  // Quit after two update checks have fired.
  LoopHandler loop_handler(2, quit_closure());
  EXPECT_CALL(update_client(), Update(_, _, _, _, _))
      .WillRepeatedly(Invoke(&loop_handler, &LoopHandler::OnUpdate));

  EXPECT_CALL(update_client(), IsUpdating(id1)).Times(1);
  EXPECT_CALL(update_client(), Stop()).Times(1);
  EXPECT_CALL(scheduler(), Schedule(_, _, _, _)).Times(1);
  EXPECT_CALL(scheduler(), Stop()).Times(1);

  EXPECT_TRUE(component_updater().RegisterComponent(component1));
  EXPECT_TRUE(component_updater().RegisterComponent(component2));

  RunThreads();
  EXPECT_TRUE(component_updater().UnregisterComponent(id1));

  ht.ExpectUniqueSample("ComponentUpdater.Calls", 1, 2);
  ht.ExpectUniqueSample("ComponentUpdater.UpdateCompleteResult", 0, 2);
  ht.ExpectTotalCount("ComponentUpdater.UpdateCompleteTime", 2);
}

// Tests that on-demand updates invoke UpdateClient::Install.
TEST_F(ComponentUpdaterTest, OnDemandUpdate) {
  base::HistogramTester ht;

  // Don't run periodic update task.
  ON_CALL(scheduler(), Schedule(_, _, _, _)).WillByDefault(Return());

  auto& cus = component_updater();

  // Tests calling OnDemand for an unregistered component. This call results in
  // an error, which is recorded by the OnDemandTester instance. Since the
  // component was not registered, the call is ignored for UMA metrics.
  OnDemandTester ondemand_tester_component_not_registered;
  ondemand_tester_component_not_registered.OnDemand(
      &cus, "ihfokbkgjpifnbbojhneepfflplebdkc",
      OnDemandUpdater::Priority::FOREGROUND);

  // Register two components, then call |OnDemand| for each component, with
  // foreground and background priorities. Expect calls to |Schedule| because
  // components have registered, calls to |Install| and |Update| corresponding
  // to each |OnDemand| invocation, and calls to |Stop| when the mocks are
  // torn down.
  LoopHandler loop_handler(2, quit_closure());
  EXPECT_CALL(scheduler(), Schedule(_, _, _, _)).Times(1);
  EXPECT_CALL(update_client(), Install(_, _, _, _))
      .WillOnce(Invoke(&loop_handler, &LoopHandler::OnInstall));
  EXPECT_CALL(update_client(), Update(_, _, _, _, _))
      .WillOnce(Invoke(&loop_handler, &LoopHandler::OnUpdate));
  EXPECT_CALL(update_client(), Stop()).Times(1);
  EXPECT_CALL(scheduler(), Stop()).Times(1);

  {
    using update_client::jebg_hash;
    std::vector<uint8_t> hash;
    hash.assign(std::begin(jebg_hash), std::end(jebg_hash));
    EXPECT_TRUE(cus.RegisterComponent(ComponentRegistration(
        "jebgalgnebhfojomionfpkfelancnnkf", {}, hash, base::Version("0.9"), {},
        {}, nullptr, base::MakeRefCounted<MockInstaller>(), false, true)));
  }
  {
    using update_client::abag_hash;
    std::vector<uint8_t> hash;
    hash.assign(std::begin(abag_hash), std::end(abag_hash));
    EXPECT_TRUE(cus.RegisterComponent(ComponentRegistration(
        "abagagagagagagagagagagagagagagag", {}, hash, base::Version("0.9"), {},
        {}, nullptr, base::MakeRefCounted<MockInstaller>(), false, true)));
  }

  OnDemandTester ondemand_tester;
  ondemand_tester.OnDemand(&cus, "jebgalgnebhfojomionfpkfelancnnkf",
                           OnDemandUpdater::Priority::FOREGROUND);
  ondemand_tester.OnDemand(&cus, "abagagagagagagagagagagagagagagag",
                           OnDemandUpdater::Priority::BACKGROUND);
  RunThreads();

  EXPECT_EQ(update_client::Error::INVALID_ARGUMENT,
            ondemand_tester_component_not_registered.error());
  EXPECT_EQ(update_client::Error::NONE, ondemand_tester.error());

  ht.ExpectUniqueSample("ComponentUpdater.Calls", 0, 2);
  ht.ExpectUniqueSample("ComponentUpdater.UpdateCompleteResult", 0, 2);
  ht.ExpectTotalCount("ComponentUpdater.UpdateCompleteTime", 2);
}

// Tests that throttling an update invokes UpdateClient::Install.
TEST_F(ComponentUpdaterTest, MaybeThrottle) {
  base::HistogramTester ht;

  // Don't run periodic update task.
  ON_CALL(scheduler(), Schedule(_, _, _, _)).WillByDefault(Return());

  using update_client::jebg_hash;
  std::vector<uint8_t> hash;
  hash.assign(std::begin(jebg_hash), std::end(jebg_hash));

  LoopHandler loop_handler(1, quit_closure());
  EXPECT_CALL(update_client(), Install(_, _, _, _))
      .WillOnce(Invoke(&loop_handler, &LoopHandler::OnInstall));
  EXPECT_CALL(update_client(), Stop()).Times(1);
  EXPECT_CALL(scheduler(), Schedule(_, _, _, _)).Times(1);
  EXPECT_CALL(scheduler(), Stop()).Times(1);

  EXPECT_TRUE(component_updater().RegisterComponent(ComponentRegistration(
      "jebgalgnebhfojomionfpkfelancnnkf", {}, hash, base::Version("0.9"), {},
      {}, nullptr, base::MakeRefCounted<MockInstaller>(), false, true)));
  component_updater().MaybeThrottle("jebgalgnebhfojomionfpkfelancnnkf",
                                    base::DoNothing());

  RunThreads();

  ht.ExpectUniqueSample("ComponentUpdater.Calls", 0, 1);
  ht.ExpectUniqueSample("ComponentUpdater.UpdateCompleteResult", 0, 1);
  ht.ExpectTotalCount("ComponentUpdater.UpdateCompleteTime", 1);
}

}  // namespace component_updater
