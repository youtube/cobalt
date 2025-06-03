// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/enterprise/cert_store/arc_cert_installer.h"

#include <string>
#include <vector>

#include "ash/components/arc/mojom/policy.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/components/arc/test/fake_policy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/remote_commands/remote_commands_queue.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArg;

namespace {

MATCHER_P(IsCommandPayloadForName, name, "") {
  constexpr char kAliasFormat[] = "\\\"alias\\\":\\\"%s\\\"";
  return arg.find(base::StringPrintf(kAliasFormat, name.c_str())) !=
         std::string::npos;
}

MATCHER_P(IsCommandWithStatus, status, "") {
  return arg->status() == status;
}

constexpr char kCNFormat[] = "CN=%s";
constexpr char kFakeName1[] = "fake1";
constexpr char kFakeName2[] = "fake2";
constexpr char kFakeName3[] = "fake3";

// Arbitrary label and ID. Note IDs can be any byte blob, not necessarily
// human readable.
constexpr char kLabel[] = "some label";
constexpr char kId[] = "some ID";

class MockRemoteCommandsQueueObserver
    : public StrictMock<policy::RemoteCommandsQueue::Observer> {
 public:
  MOCK_METHOD1(OnJobStarted, void(policy::RemoteCommandJob* command));
  MOCK_METHOD1(OnJobFinished, void(policy::RemoteCommandJob* command));
};

class MockPolicyInstance : public FakePolicyInstance {
 public:
  MOCK_METHOD2(OnCommandReceived,
               void(const std::string& command,
                    OnCommandReceivedCallback callback));
};

void AddCert(const std::string& cn, std::vector<CertDescription>* certs) {
  std::string der_cert;
  net::ScopedCERTCertificate cert;
  std::unique_ptr<crypto::RSAPrivateKey> key(
      crypto::RSAPrivateKey::Create(1024));

  ASSERT_TRUE(net::x509_util::CreateSelfSignedCert(
      key->key(), net::x509_util::DIGEST_SHA256, cn, 1, base::Time::UnixEpoch(),
      base::Time::UnixEpoch(), {}, &der_cert));
  cert = net::x509_util::CreateCERTCertificateFromBytes(
      base::as_bytes(base::make_span(der_cert)));
  ASSERT_TRUE(cert);
  certs->emplace_back(key.release(), cert.release(),
                      keymanagement::mojom::ChapsSlot::kUser, kLabel, kId);
}

}  // namespace

class ArcCertInstallerTest : public testing::Test {
 public:
  ArcCertInstallerTest()
      : arc_service_manager_(std::make_unique<arc::ArcServiceManager>()),
        profile_(std::make_unique<TestingProfile>()) {
    ash::ConciergeClient::InitializeFake(nullptr);
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(arc::FakeArcSession::Create)));
    arc_policy_bridge_ =
        arc::ArcPolicyBridge::GetForBrowserContextForTesting(profile_.get());
    policy_instance_ = std::make_unique<arc::MockPolicyInstance>();
    arc_service_manager_->arc_bridge_service()->policy()->SetInstance(
        policy_instance_.get());
  }

  ArcCertInstallerTest(const ArcCertInstallerTest&) = delete;
  ArcCertInstallerTest& operator=(const ArcCertInstallerTest&) = delete;

  ~ArcCertInstallerTest() override {
    arc_service_manager_->arc_bridge_service()->policy()->CloseInstance(
        policy_instance_.get());
  }

  void SetUp() override {
    auto mock_queue = std::make_unique<policy::RemoteCommandsQueue>();
    queue_ = mock_queue.get();
    installer_ =
        std::make_unique<ArcCertInstaller>(profile(), std::move(mock_queue));
    queue_->AddObserver(&observer_);
  }

  void TearDown() override {
    queue_->RemoveObserver(&observer_);
    installer_.reset();
    queue_ = nullptr;
  }

  void ExpectArcCommandForName(const std::string& name,
                               mojom::CommandResultType status) {
    EXPECT_CALL(*policy_instance_.get(),
                OnCommandReceived(IsCommandPayloadForName(name), _))
        .WillOnce(WithArg<1>(Invoke(
            [status](FakePolicyInstance::OnCommandReceivedCallback callback) {
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback), status));
            })));
  }

  Profile* profile() { return profile_.get(); }
  ArcCertInstaller* installer() { return installer_.get(); }
  MockRemoteCommandsQueueObserver* observer() { return &observer_; }

 private:
  content::BrowserTaskEnvironment task_environment_;

  // ArcServiceManager needs to be created before ArcPolicyBridge (since the
  // Bridge depends on the Manager), and it needs to be destroyed after Profile
  // (because BrowserContextKeyedServices are destroyed together with Profile,
  // and ArcPolicyBridge is such a service).
  const std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  const std::unique_ptr<TestingProfile> profile_;
  raw_ptr<arc::ArcPolicyBridge, ExperimentalAsh> arc_policy_bridge_;
  std::unique_ptr<arc::MockPolicyInstance> policy_instance_;

  raw_ptr<policy::RemoteCommandsQueue, DanglingUntriaged | ExperimentalAsh>
      queue_;
  std::unique_ptr<ArcCertInstaller> installer_;
  MockRemoteCommandsQueueObserver observer_;
};

// Tests that installation of an empty cert list completes successfully.
TEST_F(ArcCertInstallerTest, NoCertsTest) {
  installer()->InstallArcCerts(
      std::vector<CertDescription>(),
      base::BindOnce([](bool result) { EXPECT_TRUE(result); }));
}

// Tests that installing certs completes successfully if there are two certs
// available.
TEST_F(ArcCertInstallerTest, BasicCertTest) {
  std::vector<CertDescription> certs;

  AddCert(base::StringPrintf(kCNFormat, kFakeName1), &certs);
  AddCert(base::StringPrintf(kCNFormat, kFakeName2), &certs);

  ExpectArcCommandForName(kFakeName1, mojom::CommandResultType::SUCCESS);
  ExpectArcCommandForName(kFakeName2, mojom::CommandResultType::SUCCESS);
  EXPECT_CALL(*observer(), OnJobStarted(IsCommandWithStatus(
                               policy::RemoteCommandJob::Status::RUNNING)))
      .Times(2);
  EXPECT_CALL(*observer(), OnJobFinished(IsCommandWithStatus(
                               policy::RemoteCommandJob::Status::SUCCEEDED)))
      .Times(2);

  base::RunLoop run_loop;
  installer()->InstallArcCerts(std::move(certs),
                               base::BindOnce(
                                   [](base::RunLoop* run_loop, bool result) {
                                     EXPECT_TRUE(result);
                                     run_loop->Quit();
                                   },
                                   &run_loop));
  run_loop.Run();
}

// Tests that consequent calls complete successfully and install each cert once
// (3 times in total for 3 distinct certs).
TEST_F(ArcCertInstallerTest, ConsequentInstallTest) {
  ExpectArcCommandForName(kFakeName1, mojom::CommandResultType::SUCCESS);
  ExpectArcCommandForName(kFakeName2, mojom::CommandResultType::SUCCESS);
  EXPECT_CALL(*observer(), OnJobStarted(IsCommandWithStatus(
                               policy::RemoteCommandJob::Status::RUNNING)))
      .Times(3);
  EXPECT_CALL(*observer(), OnJobFinished(IsCommandWithStatus(
                               policy::RemoteCommandJob::Status::SUCCEEDED)))
      .Times(3);
  {
    std::vector<CertDescription> certs;
    AddCert(base::StringPrintf(kCNFormat, kFakeName1), &certs);
    AddCert(base::StringPrintf(kCNFormat, kFakeName2), &certs);
    base::RunLoop run_loop;
    installer()->InstallArcCerts(std::move(certs),
                                 base::BindOnce(
                                     [](base::RunLoop* run_loop, bool result) {
                                       EXPECT_TRUE(result);
                                       run_loop->Quit();
                                     },
                                     &run_loop));
    run_loop.Run();
  }

  ExpectArcCommandForName(kFakeName3, mojom::CommandResultType::SUCCESS);
  {
    std::vector<CertDescription> certs;
    AddCert(base::StringPrintf(kCNFormat, kFakeName1), &certs);
    AddCert(base::StringPrintf(kCNFormat, kFakeName3), &certs);

    base::RunLoop run_loop;
    installer()->InstallArcCerts(std::move(certs),
                                 base::BindOnce(
                                     [](base::RunLoop* run_loop, bool result) {
                                       EXPECT_TRUE(result);
                                       run_loop->Quit();
                                     },
                                     &run_loop));
    run_loop.Run();
  }
}

// Tests that starting the second cert installation before finishing the
// first one fails.
TEST_F(ArcCertInstallerTest, FailureIncompleteInstallationTest) {
  ExpectArcCommandForName(kFakeName1, mojom::CommandResultType::SUCCESS);
  EXPECT_CALL(*observer(), OnJobStarted(IsCommandWithStatus(
                               policy::RemoteCommandJob::Status::RUNNING)));
  EXPECT_CALL(*observer(), OnJobFinished(IsCommandWithStatus(
                               policy::RemoteCommandJob::Status::SUCCEEDED)));

  {
    std::vector<CertDescription> certs;
    AddCert(base::StringPrintf(kCNFormat, kFakeName1), &certs);

    installer()->InstallArcCerts(std::move(certs),
                                 base::BindOnce([](bool result) {
                                   // The first installation has not finished
                                   // before the second started.
                                   EXPECT_FALSE(result);
                                 }));
  }

  {
    std::vector<CertDescription> certs;
    AddCert(base::StringPrintf(kCNFormat, kFakeName1), &certs);

    base::RunLoop run_loop;
    installer()->InstallArcCerts(std::move(certs),
                                 base::BindOnce(
                                     [](base::RunLoop* run_loop, bool result) {
                                       EXPECT_TRUE(result);
                                       run_loop->Quit();
                                     },
                                     &run_loop));
    run_loop.Run();
  }
}

// Tests the failed certificate installation.
TEST_F(ArcCertInstallerTest, FailedRequiredSmartCardTest) {
  ExpectArcCommandForName(kFakeName1, mojom::CommandResultType::FAILURE);
  EXPECT_CALL(*observer(), OnJobStarted(IsCommandWithStatus(
                               policy::RemoteCommandJob::Status::RUNNING)));
  EXPECT_CALL(*observer(), OnJobFinished(IsCommandWithStatus(
                               policy::RemoteCommandJob::Status::FAILED)));

  std::vector<CertDescription> certs;
  AddCert(base::StringPrintf(kCNFormat, kFakeName1), &certs);

  base::RunLoop run_loop;
  installer()->InstallArcCerts(std::move(certs),
                               base::BindOnce(
                                   [](base::RunLoop* run_loop, bool result) {
                                     EXPECT_FALSE(result);
                                     run_loop->Quit();
                                   },
                                   &run_loop));
  run_loop.Run();
}

// Tests that the failed installation does not fail the consequent operation
// if the cert is no longer required.
TEST_F(ArcCertInstallerTest, FailiedNotRequiredSmartCardTest) {
  EXPECT_CALL(*observer(), OnJobStarted(IsCommandWithStatus(
                               policy::RemoteCommandJob::Status::RUNNING)))
      .Times(2);
  {
    std::vector<CertDescription> certs;
    AddCert(base::StringPrintf(kCNFormat, kFakeName1), &certs);

    installer()->InstallArcCerts(std::move(certs),
                                 base::BindOnce([](bool result) {
                                   // The first installation has not finished
                                   // before the second started.
                                   EXPECT_FALSE(result);
                                 }));
  }

  ExpectArcCommandForName(kFakeName1, mojom::CommandResultType::FAILURE);
  ExpectArcCommandForName(kFakeName2, mojom::CommandResultType::SUCCESS);
  EXPECT_CALL(*observer(), OnJobFinished(IsCommandWithStatus(
                               policy::RemoteCommandJob::Status::SUCCEEDED)));
  EXPECT_CALL(*observer(), OnJobFinished(IsCommandWithStatus(
                               policy::RemoteCommandJob::Status::FAILED)));

  {
    std::vector<CertDescription> certs;
    AddCert(base::StringPrintf(kCNFormat, kFakeName2), &certs);

    base::RunLoop run_loop;
    installer()->InstallArcCerts(std::move(certs),
                                 base::BindOnce(
                                     [](base::RunLoop* run_loop, bool result) {
                                       EXPECT_TRUE(result);
                                       run_loop->Quit();
                                     },
                                     &run_loop));
    run_loop.Run();
  }
}

}  // namespace arc
