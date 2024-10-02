// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/counting_policy.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/activity_log/activity_log_task_runner.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension_builder.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/scoped_test_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#endif

namespace extensions {

class CountingPolicyTest : public testing::Test {
 public:
  CountingPolicyTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    test_user_manager_ = std::make_unique<ash::ScopedTestUserManager>();
#endif
    profile_ = std::make_unique<TestingProfile>();
    base::CommandLine::ForCurrentProcess()->
        AppendSwitch(switches::kEnableExtensionActivityLogging);
    base::CommandLine no_program_command_line(base::CommandLine::NO_PROGRAM);
    extension_service_ = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile_.get()))->CreateExtensionService
            (&no_program_command_line, base::FilePath(), false);
    base::RunLoop().RunUntilIdle();
  }

  ~CountingPolicyTest() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    test_user_manager_.reset();
#endif
    base::RunLoop().RunUntilIdle();
    profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Waits for the task queue for the activity log sequence to empty.
  void WaitOnActivityLogSequence() {
    base::RunLoop run_loop;
    GetActivityLogTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  // A wrapper function for CheckReadFilteredData, so that we don't need to
  // enter empty string values for parameters we don't care about.
  void CheckReadData(
      ActivityLogDatabasePolicy* policy,
      const std::string& extension_id,
      int day,
      base::OnceCallback<void(std::unique_ptr<Action::ActionVector>)> checker) {
    CheckReadFilteredData(policy, extension_id, Action::ACTION_ANY, "", "", "",
                          day, std::move(checker));
  }

  // A helper function to call ReadFilteredData on a policy object and wait for
  // the results to be processed.
  void CheckReadFilteredData(
      ActivityLogDatabasePolicy* policy,
      const std::string& extension_id,
      const Action::ActionType type,
      const std::string& api_name,
      const std::string& page_url,
      const std::string& arg_url,
      int day,
      base::OnceCallback<void(std::unique_ptr<Action::ActionVector>)> checker) {
    // Submit a request to the policy to read back some data, and call the
    // checker function when results are available.  This will happen on the
    // database thread.
    policy->ReadFilteredData(
        extension_id, type, api_name, page_url, arg_url, day,
        base::BindOnce(&CountingPolicyTest::CheckWrapper, std::move(checker),
                       base::RunLoop::QuitCurrentWhenIdleClosureDeprecated()));

    // Set up a timeout for receiving results; if we haven't received anything
    // when the timeout triggers then assume that the test is broken.
    base::CancelableOnceClosure timeout(
        base::BindOnce(&CountingPolicyTest::TimeoutCallback));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, timeout.callback(), TestTimeouts::action_timeout());

    // Wait for results; either the checker or the timeout callbacks should
    // cause the main loop to exit.
    base::RunLoop().Run();

    timeout.Cancel();
  }

  // A helper function which verifies that the string_ids and url_ids tables in
  // the database have the specified sizes.
  static void CheckStringTableSizes(CountingPolicy* policy,
                                    int string_size,
                                    int url_size) {
    sql::Database* db = policy->GetDatabaseConnection();
    sql::Statement statement1(db->GetCachedStatement(
        sql::StatementID(SQL_FROM_HERE), "SELECT COUNT(*) FROM string_ids"));
    ASSERT_TRUE(statement1.Step());
    ASSERT_EQ(string_size, statement1.ColumnInt(0));

    sql::Statement statement2(db->GetCachedStatement(
        sql::StatementID(SQL_FROM_HERE), "SELECT COUNT(*) FROM url_ids"));
    ASSERT_TRUE(statement2.Step());
    ASSERT_EQ(url_size, statement2.ColumnInt(0));
  }

  // Checks that the number of queued actions to be written out does not exceed
  // kSizeThresholdForFlush.  Runs on the database thread.
  static void CheckQueueSize(CountingPolicy* policy) {
    // This should be updated if kSizeThresholdForFlush in activity_database.cc
    // changes.
    ASSERT_LE(policy->queued_actions_.size(), 200U);
  }

  static void CheckWrapper(
      base::OnceCallback<void(std::unique_ptr<Action::ActionVector>)> checker,
      base::OnceClosure done,
      std::unique_ptr<Action::ActionVector> results) {
    std::move(checker).Run(std::move(results));
    std::move(done).Run();
  }

  static void TimeoutCallback() {
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
    FAIL() << "Policy test timed out waiting for results";
  }

  static void RetrieveActions_FetchFilteredActions0(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(0, static_cast<int>(i->size()));
  }

  static void RetrieveActions_FetchFilteredActions1(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(1, static_cast<int>(i->size()));
  }

  static void RetrieveActions_FetchFilteredActions2(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(2, static_cast<int>(i->size()));
  }

  static void RetrieveActions_FetchFilteredActions300(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(300, static_cast<int>(i->size()));
  }

  static void Arguments_Stripped(std::unique_ptr<Action::ActionVector> i) {
    scoped_refptr<Action> last = i->front();
    CheckAction(*last, "odlameecjipmbmbejkplpemijjgpljce",
                Action::ACTION_API_CALL, "extension.connect",
                "[\"hello\",\"world\"]", "", "", "", 1);
  }

  static void Arguments_GetSinglesAction(
      std::unique_ptr<Action::ActionVector> actions) {
    ASSERT_EQ(1, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky", Action::ACTION_DOM_ACCESS, "lets", "",
                "http://www.google.com/", "", "", 1);
  }

  static void Arguments_GetTodaysActions(
      std::unique_ptr<Action::ActionVector> actions) {
    ASSERT_EQ(3, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky", Action::ACTION_API_CALL, "brewster",
                "", "", "", "", 2);
    CheckAction(*actions->at(1), "punky", Action::ACTION_DOM_ACCESS, "lets", "",
                "http://www.google.com/", "", "", 1);
    CheckAction(*actions->at(2), "punky", Action::ACTION_API_CALL,
                "extension.sendMessage", "[\"not\",\"stripped\"]", "", "", "",
                1);
  }

  static void Arguments_GetOlderActions(
      std::unique_ptr<Action::ActionVector> actions) {
    ASSERT_EQ(2, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky", Action::ACTION_DOM_ACCESS, "lets", "",
                "http://www.google.com/", "", "", 1);
    CheckAction(*actions->at(1), "punky", Action::ACTION_API_CALL, "brewster",
                "", "", "", "", 1);
  }

  static void Arguments_CheckMergeCount(
      int count,
      std::unique_ptr<Action::ActionVector> actions) {
    if (count > 0) {
      ASSERT_EQ(1u, actions->size());
      CheckAction(*actions->at(0), "punky", Action::ACTION_API_CALL, "brewster",
                  "", "", "", "", count);
    } else {
      ASSERT_EQ(0u, actions->size());
    }
  }

  static void Arguments_CheckMergeCountAndTime(
      int count,
      const base::Time& time,
      std::unique_ptr<Action::ActionVector> actions) {
    if (count > 0) {
      ASSERT_EQ(1u, actions->size());
      CheckAction(*actions->at(0), "punky", Action::ACTION_API_CALL, "brewster",
                  "", "", "", "", count);
      ASSERT_EQ(time, actions->at(0)->time());
    } else {
      ASSERT_EQ(0u, actions->size());
    }
  }

  static void AllURLsRemoved(std::unique_ptr<Action::ActionVector> actions) {
    ASSERT_EQ(2, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky", Action::ACTION_DOM_ACCESS, "lets", "",
                "", "", "", 1);
    CheckAction(*actions->at(1), "punky", Action::ACTION_DOM_ACCESS, "lets", "",
                "", "", "", 1);
  }

  static void SomeURLsRemoved(std::unique_ptr<Action::ActionVector> actions) {
    // These will be in the vector in reverse time order.
    ASSERT_EQ(5, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky", Action::ACTION_DOM_ACCESS, "lets", "",
                "http://www.google.com/", "Google", "http://www.args-url.com/",
                1);
    CheckAction(*actions->at(1), "punky", Action::ACTION_DOM_ACCESS, "lets", "",
                "http://www.google.com/", "Google", "", 1);
    CheckAction(*actions->at(2), "punky", Action::ACTION_DOM_ACCESS, "lets", "",
                "", "", "", 1);
    CheckAction(*actions->at(3), "punky", Action::ACTION_DOM_ACCESS, "lets", "",
                "", "", "http://www.google.com/", 1);
    CheckAction(*actions->at(4), "punky", Action::ACTION_DOM_ACCESS, "lets", "",
                "", "", "", 1);
  }

  static void CheckDuplicates(std::unique_ptr<Action::ActionVector> actions) {
    ASSERT_EQ(2u, actions->size());
    int total_count = 0;
    for (const auto& action : *actions) {
      total_count += action->count();
    }
    ASSERT_EQ(3, total_count);
  }

  static void CheckAction(const Action& action,
                          const std::string& expected_id,
                          const Action::ActionType& expected_type,
                          const std::string& expected_api_name,
                          const std::string& expected_args_str,
                          const std::string& expected_page_url,
                          const std::string& expected_page_title,
                          const std::string& expected_arg_url,
                          int expected_count) {
    ASSERT_EQ(expected_id, action.extension_id());
    ASSERT_EQ(expected_type, action.action_type());
    ASSERT_EQ(expected_api_name, action.api_name());
    ASSERT_EQ(expected_args_str,
              ActivityLogPolicy::Util::Serialize(action.args()));
    ASSERT_EQ(expected_page_url, action.SerializePageUrl());
    ASSERT_EQ(expected_page_title, action.page_title());
    ASSERT_EQ(expected_arg_url, action.SerializeArgUrl());
    ASSERT_EQ(expected_count, action.count());
    ASSERT_NE(-1, action.action_id());
  }

  // A helper function initializes the policy with a number of actions, calls
  // RemoveActions on a policy object and then checks the result of the
  // deletion.
  void CheckRemoveActions(
      ActivityLogDatabasePolicy* policy,
      const std::vector<int64_t>& action_ids,
      base::OnceCallback<void(std::unique_ptr<Action::ActionVector>)> checker) {
    // Use a mock clock to ensure that events are not recorded on the wrong day
    // when the test is run close to local midnight.
    mock_clock_.SetNow(base::Time::Now().LocalMidnight() + base::Hours(12));
    policy->SetClockForTesting(&mock_clock_);

    // Record some actions
    scoped_refptr<Action> action =
        new Action("punky1", mock_clock_.Now() - base::Minutes(40),
                   Action::ACTION_DOM_ACCESS, "lets1");
    action->mutable_args().Append("vamoose1");
    action->set_page_url(GURL("http://www.google1.com"));
    action->set_page_title("Google1");
    action->set_arg_url(GURL("http://www.args-url1.com"));
    policy->ProcessAction(action);
    // Record the same action twice, so there are multiple entries in the
    // database.
    policy->ProcessAction(action);

    action = new Action("punky2", mock_clock_.Now() - base::Minutes(30),
                        Action::ACTION_API_CALL, "lets2");
    action->mutable_args().Append("vamoose2");
    action->set_page_url(GURL("http://www.google2.com"));
    action->set_page_title("Google2");
    action->set_arg_url(GURL("http://www.args-url2.com"));
    policy->ProcessAction(action);
    // Record the same action twice, so there are multiple entries in the
    // database.
    policy->ProcessAction(action);

    // Submit a request to delete actions.
    policy->RemoveActions(action_ids);

    // Check the result of the deletion. The checker function gets all
    // activities in the database.
    CheckReadData(policy, "", -1, std::move(checker));

    // Clean database.
    policy->DeleteDatabase();
  }

  static void AllActionsDeleted(std::unique_ptr<Action::ActionVector> actions) {
    ASSERT_EQ(0, static_cast<int>(actions->size()));
  }

  static void NoActionsDeleted(std::unique_ptr<Action::ActionVector> actions) {
    // These will be in the vector in reverse time order.
    ASSERT_EQ(2, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky2", Action::ACTION_API_CALL, "lets2", "",
                "http://www.google2.com/", "Google2",
                "http://www.args-url2.com/", 2);
    ASSERT_EQ(2, actions->at(0)->action_id());
    CheckAction(*actions->at(1), "punky1", Action::ACTION_DOM_ACCESS, "lets1",
                "", "http://www.google1.com/", "Google1",
                "http://www.args-url1.com/", 2);
    ASSERT_EQ(1, actions->at(1)->action_id());
  }

  static void Action1Deleted(std::unique_ptr<Action::ActionVector> actions) {
    // These will be in the vector in reverse time order.
    ASSERT_EQ(1, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky2", Action::ACTION_API_CALL, "lets2", "",
                "http://www.google2.com/", "Google2",
                "http://www.args-url2.com/", 2);
    ASSERT_EQ(2, actions->at(0)->action_id());
  }

  static void Action2Deleted(std::unique_ptr<Action::ActionVector> actions) {
    // These will be in the vector in reverse time order.
    ASSERT_EQ(1, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky1", Action::ACTION_DOM_ACCESS, "lets1",
                "", "http://www.google1.com/", "Google1",
                "http://www.args-url1.com/", 2);
    ASSERT_EQ(1, actions->at(0)->action_id());
  }

 protected:
  base::SimpleTestClock mock_clock_;
  raw_ptr<ExtensionService> extension_service_;
  std::unique_ptr<TestingProfile> profile_;
  content::BrowserTaskEnvironment task_environment_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  std::unique_ptr<ash::ScopedTestUserManager> test_user_manager_;
#endif
};

TEST_F(CountingPolicyTest, Construct) {
  ActivityLogDatabasePolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "Test extension")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();
  extension_service_->AddExtension(extension.get());
  scoped_refptr<Action> action = new Action(extension->id(),
                                            base::Time::Now(),
                                            Action::ACTION_API_CALL,
                                            "tabs.testMethod");
  action->set_args(base::Value::List());
  policy->ProcessAction(action);
  policy->Close();
}

TEST_F(CountingPolicyTest, LogWithStrippedArguments) {
  ActivityLogDatabasePolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "Test extension")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();
  extension_service_->AddExtension(extension.get());

  base::Value::List args;
  args.Append("hello");
  args.Append("world");
  scoped_refptr<Action> action = new Action(extension->id(),
                                            base::Time::Now(),
                                            Action::ACTION_API_CALL,
                                            "extension.connect");
  action->set_args(std::move(args));

  policy->ProcessAction(action);
  CheckReadData(policy, extension->id(), 0,
                base::BindOnce(&CountingPolicyTest::Arguments_Stripped));
  policy->Close();
}

TEST_F(CountingPolicyTest, GetTodaysActions) {
  CountingPolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();
  // Disable row expiration for this test by setting a time before any actions
  // we generate.
  policy->set_retention_time(base::Days(14));

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.  Note: Ownership is passed
  // to the policy, but we still keep a pointer locally.  The policy will take
  // care of destruction; this is safe since the policy outlives all our
  // accesses to the mock clock.
  mock_clock_.SetNow(base::Time::Now().LocalMidnight() + base::Hours(12));
  policy->SetClockForTesting(&mock_clock_);

  // Record some actions
  scoped_refptr<Action> action =
      new Action("punky", mock_clock_.Now() - base::Minutes(40),
                 Action::ACTION_API_CALL, "brewster");
  action->mutable_args().Append("woof");
  policy->ProcessAction(action);

  action = new Action("punky", mock_clock_.Now() - base::Minutes(30),
                      Action::ACTION_API_CALL, "brewster");
  action->mutable_args().Append("meow");
  policy->ProcessAction(action);

  action = new Action("punky", mock_clock_.Now() - base::Minutes(20),
                      Action::ACTION_API_CALL, "extension.sendMessage");
  action->mutable_args().Append("not");
  action->mutable_args().Append("stripped");
  policy->ProcessAction(action);

  action =
      new Action("punky", mock_clock_.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args().Append("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  action = new Action("scoobydoo", mock_clock_.Now(), Action::ACTION_DOM_ACCESS,
                      "lets");
  action->mutable_args().Append("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  CheckReadData(
      policy, "punky", 0,
      base::BindOnce(&CountingPolicyTest::Arguments_GetTodaysActions));
  policy->Close();
}

// Check that we can read back less recent actions in the db.
TEST_F(CountingPolicyTest, GetOlderActions) {
  CountingPolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();
  policy->set_retention_time(base::Days(14));

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  mock_clock_.SetNow(base::Time::Now().LocalMidnight() + base::Hours(12));
  policy->SetClockForTesting(&mock_clock_);

  // Record some actions
  scoped_refptr<Action> action =
      new Action("punky", mock_clock_.Now() - base::Days(3) - base::Minutes(40),
                 Action::ACTION_API_CALL, "brewster");
  action->mutable_args().Append("woof");
  policy->ProcessAction(action);

  action = new Action("punky", mock_clock_.Now() - base::Days(3),
                      Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args().Append("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  action =
      new Action("punky", mock_clock_.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args().Append("too new");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  action = new Action("punky", mock_clock_.Now() - base::Days(7),
                      Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args().Append("too old");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  CheckReadData(policy, "punky", 3,
                base::BindOnce(&CountingPolicyTest::Arguments_GetOlderActions));

  policy->Close();
}

TEST_F(CountingPolicyTest, LogAndFetchFilteredActions) {
  ActivityLogDatabasePolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "Test extension")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();
  extension_service_->AddExtension(extension.get());
  GURL gurl("http://www.google.com");

  // Write some API calls
  scoped_refptr<Action> action_api = new Action(extension->id(),
                                                base::Time::Now(),
                                                Action::ACTION_API_CALL,
                                                "tabs.testMethod");
  action_api->set_args(base::Value::List());
  policy->ProcessAction(action_api);

  scoped_refptr<Action> action_dom = new Action(extension->id(),
                                                base::Time::Now(),
                                                Action::ACTION_DOM_ACCESS,
                                                "document.write");
  action_dom->set_args(base::Value::List());
  action_dom->set_page_url(gurl);
  policy->ProcessAction(action_dom);

  CheckReadFilteredData(
      policy, extension->id(), Action::ACTION_API_CALL, "tabs.testMethod", "",
      "", -1,
      base::BindOnce(
          &CountingPolicyTest::RetrieveActions_FetchFilteredActions1));

  // Test for case insensitive matching for api_call.
  CheckReadFilteredData(
      policy, extension->id(), Action::ACTION_API_CALL, "tabs.testmethod", "",
      "", -1,
      base::BindOnce(
          &CountingPolicyTest::RetrieveActions_FetchFilteredActions1));

  // Test for prefix matching for api_call.
  CheckReadFilteredData(
      policy, extension->id(), Action::ACTION_API_CALL, "tabs.testM", "", "",
      -1,
      base::BindOnce(
          &CountingPolicyTest::RetrieveActions_FetchFilteredActions1));

  CheckReadFilteredData(
      policy, "", Action::ACTION_DOM_ACCESS, "", "", "", -1,
      base::BindOnce(
          &CountingPolicyTest::RetrieveActions_FetchFilteredActions1));

  CheckReadFilteredData(
      policy, "", Action::ACTION_DOM_ACCESS, "", "http://www.google.com/", "",
      -1,
      base::BindOnce(
          &CountingPolicyTest::RetrieveActions_FetchFilteredActions1));

  CheckReadFilteredData(
      policy, "", Action::ACTION_DOM_ACCESS, "", "http://www.google.com", "",
      -1,
      base::BindOnce(
          &CountingPolicyTest::RetrieveActions_FetchFilteredActions1));

  CheckReadFilteredData(
      policy, "", Action::ACTION_DOM_ACCESS, "", "http://www.goo", "", -1,
      base::BindOnce(
          &CountingPolicyTest::RetrieveActions_FetchFilteredActions1));

  CheckReadFilteredData(
      policy, extension->id(), Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(
          &CountingPolicyTest::RetrieveActions_FetchFilteredActions2));

  policy->Close();
}

// Check that merging of actions only occurs within the same day, not across
// days, and that old data can be expired from the database.
TEST_F(CountingPolicyTest, MergingAndExpiring) {
  CountingPolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();
  // Initially disable expiration by setting a retention time before any
  // actions we generate.
  policy->set_retention_time(base::Days(14));

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  mock_clock_.SetNow(base::Time::Now().LocalMidnight() + base::Hours(12));
  policy->SetClockForTesting(&mock_clock_);

  // The first two actions should be merged; the last one is on a separate day
  // and should not be.
  scoped_refptr<Action> action =
      new Action("punky", mock_clock_.Now() - base::Days(3) - base::Minutes(40),
                 Action::ACTION_API_CALL, "brewster");
  policy->ProcessAction(action);

  action =
      new Action("punky", mock_clock_.Now() - base::Days(3) - base::Minutes(20),
                 Action::ACTION_API_CALL, "brewster");
  policy->ProcessAction(action);

  action =
      new Action("punky", mock_clock_.Now() - base::Days(2) - base::Minutes(20),
                 Action::ACTION_API_CALL, "brewster");
  policy->ProcessAction(action);

  CheckReadData(
      policy, "punky", 3,
      base::BindOnce(&CountingPolicyTest::Arguments_CheckMergeCount, 2));
  CheckReadData(
      policy, "punky", 2,
      base::BindOnce(&CountingPolicyTest::Arguments_CheckMergeCount, 1));

  // Clean actions before midnight two days ago.  Force expiration to run by
  // clearing last_database_cleaning_time_ and submitting a new action.
  policy->set_retention_time(base::Days(2));
  policy->last_database_cleaning_time_ = base::Time();
  action = new Action("punky", mock_clock_.Now(), Action::ACTION_API_CALL,
                      "brewster");
  policy->ProcessAction(action);

  CheckReadData(
      policy, "punky", 3,
      base::BindOnce(&CountingPolicyTest::Arguments_CheckMergeCount, 0));
  CheckReadData(
      policy, "punky", 2,
      base::BindOnce(&CountingPolicyTest::Arguments_CheckMergeCount, 1));

  policy->Close();
}

// Test cleaning of old data in the string and URL tables.
TEST_F(CountingPolicyTest, StringTableCleaning) {
  CountingPolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();
  // Initially disable expiration by setting a retention time before any
  // actions we generate.
  policy->set_retention_time(base::Days(14));

  mock_clock_.SetNow(base::Time::Now());
  policy->SetClockForTesting(&mock_clock_);

  // Insert an action; this should create entries in both the string table (for
  // the extension and API name) and the URL table (for page_url).
  scoped_refptr<Action> action =
      new Action("punky", mock_clock_.Now() - base::Days(7),
                 Action::ACTION_API_CALL, "brewster");
  action->set_page_url(GURL("http://www.google.com/"));
  policy->ProcessAction(action);

  // Add an action which will not be expired, so that some strings will remain
  // in use.
  action = new Action("punky", mock_clock_.Now(), Action::ACTION_API_CALL,
                      "tabs.create");
  policy->ProcessAction(action);

  // There should now be three strings ("punky", "brewster", "tabs.create") and
  // one URL in the tables.
  policy->Flush();
  policy->ScheduleAndForget(policy,
                            &CountingPolicyTest::CheckStringTableSizes,
                            3,
                            1);
  WaitOnActivityLogSequence();

  // Trigger a cleaning.  The oldest action is expired when we submit a
  // duplicate of the newer action.  After this, there should be two strings
  // and no URLs.
  policy->set_retention_time(base::Days(2));
  policy->last_database_cleaning_time_ = base::Time();
  policy->ProcessAction(action);
  policy->Flush();
  policy->ScheduleAndForget(policy,
                            &CountingPolicyTest::CheckStringTableSizes,
                            2,
                            0);
  WaitOnActivityLogSequence();

  policy->Close();
}

// A stress test for memory- and database-based merging of actions.  Submit
// multiple items, not in chronological order, spanning a few days.  Check that
// items are merged properly and final timestamps are correct.
TEST_F(CountingPolicyTest, MoreMerging) {
  CountingPolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();
  policy->set_retention_time(base::Days(14));

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  mock_clock_.SetNow(base::Time::Now().LocalMidnight() + base::Hours(12));
  policy->SetClockForTesting(&mock_clock_);

  // Create an action 2 days ago, then 1 day ago, then 2 days ago.  Make sure
  // that we end up with two merged records (one for each day), and each has
  // the appropriate timestamp.  These merges should happen in the database
  // since the date keeps changing.
  base::Time time1 = mock_clock_.Now() - base::Days(2) - base::Minutes(40);
  base::Time time2 = mock_clock_.Now() - base::Days(1) - base::Minutes(40);
  base::Time time3 = mock_clock_.Now() - base::Days(2) - base::Minutes(20);

  scoped_refptr<Action> action =
      new Action("punky", time1, Action::ACTION_API_CALL, "brewster");
  policy->ProcessAction(action);

  action = new Action("punky", time2, Action::ACTION_API_CALL, "brewster");
  policy->ProcessAction(action);

  action = new Action("punky", time3, Action::ACTION_API_CALL, "brewster");
  policy->ProcessAction(action);

  CheckReadData(
      policy, "punky", 2,
      base::BindOnce(&CountingPolicyTest::Arguments_CheckMergeCountAndTime, 2,
                     time3));
  CheckReadData(
      policy, "punky", 1,
      base::BindOnce(&CountingPolicyTest::Arguments_CheckMergeCountAndTime, 1,
                     time2));

  // Create three actions today, where the merges should happen in memory.
  // Again these are not chronological; timestamp time5 should win out since it
  // is the latest.
  base::Time time4 = mock_clock_.Now() - base::Minutes(60);
  base::Time time5 = mock_clock_.Now() - base::Minutes(20);
  base::Time time6 = mock_clock_.Now() - base::Minutes(40);

  action = new Action("punky", time4, Action::ACTION_API_CALL, "brewster");
  policy->ProcessAction(action);

  action = new Action("punky", time5, Action::ACTION_API_CALL, "brewster");
  policy->ProcessAction(action);

  action = new Action("punky", time6, Action::ACTION_API_CALL, "brewster");
  policy->ProcessAction(action);

  CheckReadData(
      policy, "punky", 0,
      base::BindOnce(&CountingPolicyTest::Arguments_CheckMergeCountAndTime, 3,
                     time5));
  policy->Close();
}

// Check that actions are flushed to disk before letting too many accumulate in
// memory.
TEST_F(CountingPolicyTest, EarlyFlush) {
  CountingPolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();

  for (int i = 0; i < 500; i++) {
    scoped_refptr<Action> action =
        new Action("punky",
                   base::Time::Now(),
                   Action::ACTION_API_CALL,
                   base::StringPrintf("apicall_%d", i));
    policy->ProcessAction(action);
  }

  policy->ScheduleAndForget(policy, &CountingPolicyTest::CheckQueueSize);
  WaitOnActivityLogSequence();

  policy->Close();
}

TEST_F(CountingPolicyTest, CapReturns) {
  CountingPolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();

  for (int i = 0; i < 305; i++) {
    scoped_refptr<Action> action =
        new Action("punky",
                   base::Time::Now(),
                   Action::ACTION_API_CALL,
                   base::StringPrintf("apicall_%d", i));
    policy->ProcessAction(action);
  }

  policy->Flush();
  WaitOnActivityLogSequence();

  CheckReadFilteredData(
      policy, "punky", Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(
          &CountingPolicyTest::RetrieveActions_FetchFilteredActions300));
  policy->Close();
}

TEST_F(CountingPolicyTest, RemoveAllURLs) {
  ActivityLogDatabasePolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  mock_clock_.SetNow(base::Time::Now().LocalMidnight() + base::Hours(12));
  policy->SetClockForTesting(&mock_clock_);

  // Record some actions
  scoped_refptr<Action> action =
      new Action("punky", mock_clock_.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args().Append("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.args-url.com"));
  policy->ProcessAction(action);

  mock_clock_.Advance(base::Seconds(1));
  action =
      new Action("punky", mock_clock_.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args().Append("vamoose");
  action->set_page_url(GURL("http://www.google2.com"));
  action->set_page_title("Google");
  // Deliberately no arg url set to make sure it stills works if there is no arg
  // url.
  policy->ProcessAction(action);

  // Clean all the URLs.
  std::vector<GURL> no_url_restrictions;
  policy->RemoveURLs(no_url_restrictions);

  CheckReadData(policy, "punky", 0,
                base::BindOnce(&CountingPolicyTest::AllURLsRemoved));
  policy->Close();
}

TEST_F(CountingPolicyTest, RemoveSpecificURLs) {
  ActivityLogDatabasePolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  mock_clock_.SetNow(base::Time::Now().LocalMidnight() + base::Hours(12));
  policy->SetClockForTesting(&mock_clock_);

  // Record some actions
  // This should have the page url and args url cleared.
  scoped_refptr<Action> action =
      new Action("punky", mock_clock_.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args().Append("vamoose");
  action->set_page_url(GURL("http://www.google1.com"));
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.google1.com"));
  policy->ProcessAction(action);

  // This should have the page url cleared but not args url.
  mock_clock_.Advance(base::Seconds(1));
  action =
      new Action("punky", mock_clock_.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args().Append("vamoose");
  action->set_page_url(GURL("http://www.google1.com"));
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  // This should have the page url cleared. The args url is deliberately not
  // set to make sure this doesn't cause any issues.
  mock_clock_.Advance(base::Seconds(1));
  action =
      new Action("punky", mock_clock_.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args().Append("vamoose");
  action->set_page_url(GURL("http://www.google2.com"));
  action->set_page_title("Google");
  policy->ProcessAction(action);

  // This should have the args url cleared but not the page url or page title.
  mock_clock_.Advance(base::Seconds(1));
  action =
      new Action("punky", mock_clock_.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args().Append("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.google1.com"));
  policy->ProcessAction(action);

  // This should have neither cleared.
  mock_clock_.Advance(base::Seconds(1));
  action =
      new Action("punky", mock_clock_.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args().Append("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.args-url.com"));
  action->set_count(5);
  policy->ProcessAction(action);

    // Clean some URLs.
  std::vector<GURL> urls;
  urls.push_back(GURL("http://www.google1.com"));
  urls.push_back(GURL("http://www.google2.com"));
  urls.push_back(GURL("http://www.url_not_in_db.com"));
  policy->RemoveURLs(urls);

  CheckReadData(policy, "punky", 0,
                base::BindOnce(&CountingPolicyTest::SomeURLsRemoved));
  policy->Close();
}

TEST_F(CountingPolicyTest, RemoveExtensionData) {
  CountingPolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  mock_clock_.SetNow(base::Time::Now().LocalMidnight() + base::Hours(12));
  policy->SetClockForTesting(&mock_clock_);

  // Record some actions
  scoped_refptr<Action> action =
      new Action("deleteextensiondata", mock_clock_.Now(),
                 Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args().Append("vamoose");
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);
  policy->ProcessAction(action);
  policy->ProcessAction(action);

  scoped_refptr<Action> action2 = new Action("dontdelete", mock_clock_.Now(),
                                             Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args().Append("vamoose");
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.google.com"));
  policy->ProcessAction(action2);

  policy->Flush();
  policy->RemoveExtensionData("deleteextensiondata");

  CheckReadFilteredData(
      policy, "deleteextensiondata", Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(
          &CountingPolicyTest::RetrieveActions_FetchFilteredActions0));

  CheckReadFilteredData(
      policy, "dontdelete", Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(
          &CountingPolicyTest::RetrieveActions_FetchFilteredActions1));
  policy->Close();
}

TEST_F(CountingPolicyTest, DeleteDatabase) {
  CountingPolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();
  // Disable row expiration for this test by setting a time before any actions
  // we generate.
  policy->set_retention_time(base::Days(14));

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.  Note: Ownership is passed
  // to the policy, but we still keep a pointer locally.  The policy will take
  // care of destruction; this is safe since the policy outlives all our
  // accesses to the mock clock.
  mock_clock_.SetNow(base::Time::Now().LocalMidnight() + base::Hours(12));
  policy->SetClockForTesting(&mock_clock_);

  // Record some actions
  scoped_refptr<Action> action =
      new Action("punky", mock_clock_.Now() - base::Minutes(40),
                 Action::ACTION_API_CALL, "brewster");
  action->mutable_args().Append("woof");
  policy->ProcessAction(action);

  action = new Action("punky", mock_clock_.Now() - base::Minutes(30),
                      Action::ACTION_API_CALL, "brewster");
  action->mutable_args().Append("meow");
  policy->ProcessAction(action);

  action = new Action("punky", mock_clock_.Now() - base::Minutes(20),
                      Action::ACTION_API_CALL, "extension.sendMessage");
  action->mutable_args().Append("not");
  action->mutable_args().Append("stripped");
  policy->ProcessAction(action);

  action =
      new Action("punky", mock_clock_.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args().Append("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  action = new Action("scoobydoo", mock_clock_.Now(), Action::ACTION_DOM_ACCESS,
                      "lets");
  action->mutable_args().Append("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  CheckReadData(
      policy, "punky", 0,
      base::BindOnce(&CountingPolicyTest::Arguments_GetTodaysActions));

  policy->DeleteDatabase();

  CheckReadFilteredData(
      policy, "", Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(
          &CountingPolicyTest::RetrieveActions_FetchFilteredActions0));

  // The following code tests that the caches of url and string tables were
  // cleared by the deletion above.
  // https://code.google.com/p/chromium/issues/detail?id=341674.
  action =
      new Action("punky", mock_clock_.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args().Append("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  CheckReadData(
      policy, "", -1,
      base::BindOnce(&CountingPolicyTest::Arguments_GetSinglesAction));

  policy->DeleteDatabase();

  CheckReadFilteredData(
      policy, "", Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(
          &CountingPolicyTest::RetrieveActions_FetchFilteredActions0));

  policy->Close();
}

// Tests that duplicate rows in the activity log database are handled properly
// when updating counts.
TEST_F(CountingPolicyTest, DuplicateRows) {
  CountingPolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();
  mock_clock_.SetNow(base::Time::Now().LocalMidnight() + base::Hours(12));
  policy->SetClockForTesting(&mock_clock_);

  // Record two actions with distinct URLs.
  scoped_refptr<Action> action = base::MakeRefCounted<Action>(
      "punky", mock_clock_.Now(), Action::ACTION_API_CALL, "brewster");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  action = new Action("punky", mock_clock_.Now(), Action::ACTION_API_CALL,
                      "brewster");
  action->set_page_url(GURL("http://www.google.co.uk"));
  policy->ProcessAction(action);

  // Manipulate the database to clear the URLs, so that we end up with
  // duplicate rows.
  std::vector<GURL> no_url_restrictions;
  policy->RemoveURLs(no_url_restrictions);

  // Record one more action, with no URL.  This should increment the count on
  // one, and exactly one, of the existing rows.
  action = new Action("punky", mock_clock_.Now(), Action::ACTION_API_CALL,
                      "brewster");
  policy->ProcessAction(action);

  CheckReadData(policy, "punky", 0,
                base::BindOnce(&CountingPolicyTest::CheckDuplicates));
  policy->Close();
}

TEST_F(CountingPolicyTest, RemoveActions) {
  ActivityLogDatabasePolicy* policy = new CountingPolicy(profile_.get());
  policy->Init();

  std::vector<int64_t> action_ids;

  CheckRemoveActions(policy, action_ids,
                     base::BindOnce(&CountingPolicyTest::NoActionsDeleted));

  action_ids.push_back(-1);
  action_ids.push_back(-10);
  action_ids.push_back(0);
  action_ids.push_back(5);
  action_ids.push_back(10);
  CheckRemoveActions(policy, action_ids,
                     base::BindOnce(&CountingPolicyTest::NoActionsDeleted));
  action_ids.clear();

  for (int i = 0; i < 50; i++) {
    action_ids.push_back(i + 3);
  }
  CheckRemoveActions(policy, action_ids,
                     base::BindOnce(&CountingPolicyTest::NoActionsDeleted));
  action_ids.clear();

  // CheckRemoveActions pushes two actions to the Activity Log database with IDs
  // 1 and 2.
  action_ids.push_back(1);
  action_ids.push_back(2);
  CheckRemoveActions(policy, action_ids,
                     base::BindOnce(&CountingPolicyTest::AllActionsDeleted));
  action_ids.clear();

  action_ids.push_back(1);
  CheckRemoveActions(policy, action_ids,
                     base::BindOnce(&CountingPolicyTest::Action1Deleted));
  action_ids.clear();

  action_ids.push_back(2);
  CheckRemoveActions(policy, action_ids,
                     base::BindOnce(&CountingPolicyTest::Action2Deleted));
  action_ids.clear();

  policy->Close();
}

}  // namespace extensions
