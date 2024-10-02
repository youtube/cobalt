// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ruleset_publisher_impl.h"

#include <stddef.h>

#include <string>
#include <tuple>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "components/prefs/testing_pref_service.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

using MockClosureTarget =
    ::testing::StrictMock<::testing::MockFunction<void()>>;

class NotifyingMockRenderProcessHost : public content::MockRenderProcessHost {
 public:
  explicit NotifyingMockRenderProcessHost(
      content::BrowserContext* browser_context,
      content::RenderProcessHostCreationObserver* observer)
      : content::MockRenderProcessHost(browser_context) {
    if (observer)
      observer->OnRenderProcessHostCreated(this);
  }
};

std::string ReadFileContentsToString(base::File* file) {
  size_t length = base::checked_cast<size_t>(file->GetLength());
  std::string contents(length, 0);
  file->Read(0, &contents[0], base::checked_cast<int>(length));
  return contents;
}

}  // namespace

class SubresourceFilterRulesetPublisherImplTest : public ::testing::Test {
 public:
  SubresourceFilterRulesetPublisherImplTest()
      : existing_renderer_(&browser_context_, nullptr) {}

  SubresourceFilterRulesetPublisherImplTest(
      const SubresourceFilterRulesetPublisherImplTest&) = delete;
  SubresourceFilterRulesetPublisherImplTest& operator=(
      const SubresourceFilterRulesetPublisherImplTest&) = delete;

 protected:
  void SetUp() override { ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override {}

  content::TestBrowserContext* browser_context() { return &browser_context_; }

  base::FilePath temp_dir() const { return scoped_temp_dir_.GetPath(); }

  base::FilePath scoped_temp_file() const {
    return scoped_temp_dir_.GetPath().AppendASCII("data");
  }

  void AssertSetRulesetFileWithContent(base::File* ruleset_file,
                                       const std::string& expected_contents) {
    ASSERT_TRUE(ruleset_file);
    ASSERT_TRUE(ruleset_file->IsValid());
    ASSERT_EQ(expected_contents, ReadFileContentsToString(ruleset_file));
  }

 private:
  base::ScopedTempDir scoped_temp_dir_;
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  NotifyingMockRenderProcessHost existing_renderer_;
};

class MockRulesetPublisherImpl : public RulesetPublisherImpl {
 public:
  template <typename... Args>
  explicit MockRulesetPublisherImpl(Args&&... args)
      : RulesetPublisherImpl(std::forward<Args>(args)...) {}
  void SendRulesetToRenderProcess(
      base::File* file,
      content::RenderProcessHost* process) override {
    last_file_[process] = file;
    sent_count_++;
  }

  size_t RulesetSent() const { return sent_count_; }

  base::File* RulesetFileForProcess(content::RenderProcessHost* process) {
    auto it = last_file_.find(process);
    if (it == last_file_.end())
      return nullptr;
    return it->second;
  }

 private:
  size_t sent_count_ = 0;
  std::map<content::RenderProcessHost*, base::File*> last_file_;
};

TEST_F(SubresourceFilterRulesetPublisherImplTest, NoRuleset_NoIPCMessages) {
  NotifyingMockRenderProcessHost existing_renderer(browser_context(), nullptr);
  MockRulesetPublisherImpl service(
      nullptr, base::SingleThreadTaskRunner::GetCurrentDefault());
  NotifyingMockRenderProcessHost new_renderer(browser_context(), &service);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, service.RulesetSent());
}

TEST_F(SubresourceFilterRulesetPublisherImplTest,
       PublishedRuleset_IsDistributedToExistingAndNewRenderers) {
  const char kTestFileContents[] = "foobar";
  base::WriteFile(scoped_temp_file(), kTestFileContents);

  RulesetFilePtr file(
      new base::File(scoped_temp_file(),
                     base::File::FLAG_OPEN | base::File::FLAG_READ),
      base::OnTaskRunnerDeleter(
          base::SequencedTaskRunner::GetCurrentDefault()));

  NotifyingMockRenderProcessHost existing_renderer(browser_context(), nullptr);
  MockClosureTarget publish_callback_target;
  MockRulesetPublisherImpl service(
      nullptr, base::SingleThreadTaskRunner::GetCurrentDefault());
  service.SetRulesetPublishedCallbackForTesting(base::BindOnce(
      &MockClosureTarget::Call, base::Unretained(&publish_callback_target)));

  EXPECT_CALL(publish_callback_target, Call()).Times(1);
  service.PublishNewRulesetVersion(std::move(file));
  base::RunLoop().RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&publish_callback_target);

  ASSERT_EQ(2u, service.RulesetSent());
  ASSERT_NO_FATAL_FAILURE(AssertSetRulesetFileWithContent(
      service.RulesetFileForProcess(&existing_renderer), kTestFileContents));

  NotifyingMockRenderProcessHost second_renderer(browser_context(), &service);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(3u, service.RulesetSent());
  ASSERT_NO_FATAL_FAILURE(AssertSetRulesetFileWithContent(
      service.RulesetFileForProcess(&second_renderer), kTestFileContents));
}

TEST_F(SubresourceFilterRulesetPublisherImplTest,
       PublishesRulesetInOnePostTask) {
  // Regression test for crbug.com/817308. Test verifies that ruleset is
  // published on browser startup via exactly one PostTask.

  const base::FilePath base_dir =
      temp_dir().AppendASCII("Rules").AppendASCII("Indexed");

  // Create a testing ruleset.
  testing::TestRulesetPair ruleset;
  ASSERT_NO_FATAL_FAILURE(
      testing::TestRulesetCreator().CreateRulesetToDisallowURLsWithPathSuffix(
          "foo", &ruleset));

  // Create local state and save the ruleset version to emulate invariant that
  // the version of the last indexed ruleset is stored in the local state.
  TestingPrefServiceSimple prefs;
  IndexedRulesetVersion::RegisterPrefs(prefs.registry());
  IndexedRulesetVersion current_version(
      "1.2.3.4", IndexedRulesetVersion::CurrentFormatVersion());
  current_version.SaveToPrefs(&prefs);

  // Create ruleset data on a disk.
  const base::FilePath version_dir_path =
      IndexedRulesetLocator::GetSubdirectoryPathForVersion(base_dir,
                                                           current_version);
  ASSERT_EQ(RulesetService::IndexAndWriteRulesetResult::SUCCESS,
            RulesetService::WriteRuleset(version_dir_path,
                                         /* license_path =*/base::FilePath(),
                                         ruleset.indexed.contents.data(),
                                         ruleset.indexed.contents.size()));

  // Create a ruleset service and its harness.
  scoped_refptr<base::TestSimpleTaskRunner> blocking_task_runner =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();
  scoped_refptr<base::TestSimpleTaskRunner> background_task_runner =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();
  NotifyingMockRenderProcessHost renderer_host(browser_context(), nullptr);
  base::RunLoop callback_waiter;
  auto content_service =
      std::make_unique<MockRulesetPublisherImpl>(nullptr, blocking_task_runner);
  content_service->SetRulesetPublishedCallbackForTesting(
      callback_waiter.QuitClosure());
  auto* mock_publisher = content_service.get();

  // |RulesetService| constructor should read the last indexed ruleset version
  // and post ruleset setup on |blocking_task_runner|.
  ASSERT_EQ(0u, blocking_task_runner->NumPendingTasks());
  auto service =
      std::make_unique<RulesetService>(&prefs, background_task_runner, base_dir,
                                       nullptr, std::move(content_service));

  // The key test assertion is that ruleset data is published via exactly one
  // post task on |blocking_task_runner|. It is important to run pending tasks
  // only once here.
  ASSERT_EQ(1u, blocking_task_runner->NumPendingTasks());
  blocking_task_runner->RunPendingTasks();
  callback_waiter.Run();

  // Check that the ruleset data is delivered to the renderer.
  ASSERT_EQ(2u, mock_publisher->RulesetSent());
  const std::string expected_data(ruleset.indexed.contents.begin(),
                                  ruleset.indexed.contents.end());
  ASSERT_NO_FATAL_FAILURE(AssertSetRulesetFileWithContent(
      mock_publisher->RulesetFileForProcess(&renderer_host), expected_data));

  //
  // |RulesetPublisherImpl| destruction requires additional tricks. Its member
  // |VerifiedRulesetDealer::Handle| posts task upon destruction on
  // |blocking_task_runner|.
  service.reset();
  // Need to wait for |VerifiedRulesetDealer| destruction on the
  // |blocking_task_runner|.
  blocking_task_runner->RunPendingTasks();
}

}  // namespace subresource_filter
