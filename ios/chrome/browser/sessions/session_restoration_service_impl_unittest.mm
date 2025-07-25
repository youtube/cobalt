// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_restoration_service_impl.h"

#import <map>
#import <set>

#import "base/check_op.h"
#import "base/containers/span.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/scoped_multi_source_observation.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "ios/chrome/browser/sessions/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/session_constants.h"
#import "ios/chrome/browser/sessions/session_internal_util.h"
#import "ios/chrome/browser/sessions/session_loading.h"
#import "ios/chrome/browser/sessions/test_session_restoration_observer.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/navigation_util.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "testing/platform_test.h"
#import "ui/base/page_transition_types.h"
#import "ui/base/window_open_disposition.h"
#import "url/gurl.h"

// To get access to web::features::kEnableSessionSerializationOptimizations.
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

namespace {

// Set of FilePath.
using FilePathSet = std::set<base::FilePath>;

// Delay before saving the session to storage during test. This makes the
// test independent from the delay used in production (i.e. does not need
// to know how long to skip ahead with
constexpr base::TimeDelta kSaveDelay = base::Seconds(1);

// Identifier used for the Browser under test.
const char kIdentifier0[] = "browser0";
const char kIdentifier1[] = "browser1";

// List of URLs that are loaded in the session.
constexpr std::string_view kURLs[] = {
    "chrome://version",
    "chrome://flags",
    "chrome://credits",
};

// URL and title used to create an unrealized WebState.
const char kURL[] = "https://example.com";
const char16_t kTitle[] = u"Example Domain";

// Scoped observer template.
template <typename Source, typename Observer>
class ScopedObserver : public Observer {
 public:
  template <typename... Args>
  ScopedObserver(Args&&... args) : Observer(std::forward<Args>(args)...) {}

  // Register self as observing `source`.
  void Observe(Source* source) { observation_.AddObservation(source); }

  // Unregister self from any observed sources.
  void Reset() { observation_.RemoveAllObservations(); }

 private:
  base::ScopedMultiSourceObservation<Source, Observer> observation_{this};
};

// A WebStateObserver that invokes a callback when DidFinishNavigation() happen.
class TestWebStateObserver : public web::WebStateObserver {
 public:
  TestWebStateObserver(const base::RepeatingClosure& closure)
      : closure_(closure) {}

  // web::WebStateObserver implementation.
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* context) override {
    closure_.Run();
  }

 private:
  base::RepeatingClosure closure_;
};

// Scoped variant of observers.
using ScopedTestSessionRestorationObserver =
    ScopedObserver<SessionRestorationService, TestSessionRestorationObserver>;
using ScopedTestWebStateObserver =
    ScopedObserver<web::WebState, TestWebStateObserver>;

// Class that can be used to detect files that are modified.
class FileModificationTracker {
 public:
  FileModificationTracker() = default;

  // Records all existing files below `root` and their timestamp. Used to
  // detect created or updated files later in `ModifiedFiles(...)`.
  void Start(const base::FilePath& path) { snapshot_ = EnumerateFiles(path); }

  // Reports all created or updated files in `path` since call to `Start(...)`.
  FilePathSet ModifiedFiles(const base::FilePath& path) const {
    FilePathSet result;
    for (const auto& [name, time] : EnumerateFiles(path)) {
      auto iterator = snapshot_.find(name);
      if (iterator == snapshot_.end() || iterator->second != time) {
        result.insert(name);
      }
    }
    return result;
  }

  // Reports all deleted files in `path` since call to `Start(...)`.
  FilePathSet DeletedFiles(const base::FilePath& path) const {
    FilePathSet result;
    PathToTimeMap state = EnumerateFiles(path);
    for (const auto& [name, _] : snapshot_) {
      if (!base::Contains(state, name)) {
        result.insert(name);
      }
    }
    return result;
  }

 private:
  using PathToTimeMap = std::map<base::FilePath, base::Time>;

  // Returns a mapping of files to their last modified time below `path`.
  PathToTimeMap EnumerateFiles(const base::FilePath& path) const {
    PathToTimeMap result;

    base::FileEnumerator e(path, true, base::FileEnumerator::FileType::FILES);
    for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
      // Workaround for the fact that base::FileEnumerator::FileInfo drops the
      // sub-second precision when using GetLastModifiedTime() even when the
      // data is available. See https://crbug.com/1491766 for details.
      base::File::Info info;
      info.FromStat(e.GetInfo().stat());

      result.insert(std::make_pair(name, info.last_modified));
    }

    return result;
  }

  PathToTimeMap snapshot_;
};

// Structure storing a WebState and whether the native session is supposed
// to be available. Used by ExpectedStorageFilesForWebStates.
struct WebStateReference {
  const web::WebState* web_state = nullptr;
  bool is_native_session_available = false;
};

// Returns the storage file for `references` in `session_dir`.
FilePathSet ExpectedStorageFilesForWebStates(
    const base::FilePath& session_dir,
    bool expect_session_metadata_storage,
    const std::vector<WebStateReference>& references) {
  FilePathSet result;
  if (expect_session_metadata_storage) {
    result.insert(session_dir.Append(kSessionMetadataFilename));
  }

  for (const WebStateReference& reference : references) {
    const base::FilePath web_state_dir = ios::sessions::WebStateDirectory(
        session_dir, reference.web_state->GetUniqueIdentifier());

    result.insert(web_state_dir.Append(kWebStateStorageFilename));
    result.insert(web_state_dir.Append(kWebStateMetadataStorageFilename));
    if (reference.is_native_session_available) {
      result.insert(web_state_dir.Append(kWebStateSessionFilename));
    }
  }
  return result;
}

// Returns the path of storage file to `browser` in `session_dir`.
FilePathSet ExpectedStorageFilesForBrowser(const base::FilePath& session_dir,
                                           Browser* browser) {
  std::vector<WebStateReference> references;
  WebStateList* web_state_list = browser->GetWebStateList();
  for (int index = 0; index < web_state_list->count(); ++index) {
    references.push_back(WebStateReference{
        .web_state = web_state_list->GetWebStateAt(index),
        .is_native_session_available = true,
    });
  }
  return ExpectedStorageFilesForWebStates(session_dir, true, references);
}

// Set union.
FilePathSet operator+(const FilePathSet& lhs, const FilePathSet& rhs) {
  FilePathSet result;
  result.insert(lhs.begin(), lhs.end());
  result.insert(rhs.begin(), rhs.end());
  return result;
}

// Returns a closure that expects to be call `n` times and that will invoke
// `closure` on the n-th invocation.
base::RepeatingClosure ExpectNCall(base::RepeatingClosure closure, size_t n) {
  __block size_t counter = 0;
  return base::BindRepeating(^{
    DCHECK_LT(counter, n);
    if (++counter == n) {
      closure.Run();
    }
  });
}

}  // namespace

// The fixture used to test SessionRestorationServiceImpl.
//
// It uses a TaskEnvironment mocking the time to allow to easily control when
// SessionRestorationServiceImpl's task to save data to storage will execute.
class SessionRestorationServiceImplTest : public PlatformTest {
 public:
  SessionRestorationServiceImplTest() {
    // Enable the feature. Needs to happen before the threads are created.
    scoped_feature_list_.InitAndEnableFeature(
        web::features::kEnableSessionSerializationOptimizations);

    // Use the ChromeWebClient as the test tries to load chrome:// URLs.
    scoped_web_client_ = std::make_unique<web::ScopedTestingWebClient>(
        std::make_unique<ChromeWebClient>());

    // Configure a WebTaskEnvironment with mocked time to be able to
    // fast-forward time and skip the delay before saving the data.
    web_task_environment_ = std::make_unique<web::WebTaskEnvironment>(
        web::WebTaskEnvironment::Options::DEFAULT,
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);

    // Create a test ChromeBrowserState and an object to track the files
    // that are created by the session restoration service operations.
    browser_state_ = TestChromeBrowserState::Builder().Build();
    file_tracker_.Start(browser_state_->GetStatePath());

    // Create the service, force enabling the pinned tab support (since
    // the code using the `enable_pinned_web_states` is tested by the
    // deserialization code and does not need to be tested again here).
    service_ = std::make_unique<SessionRestorationServiceImpl>(
        kSaveDelay, /* enable_pinned_web_states */ true,
        browser_state_->GetStatePath(),
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  ~SessionRestorationServiceImplTest() override { service_->Shutdown(); }

  // Returns the ChromeBrowserState used for tests.
  ChromeBrowserState* browser_state() { return browser_state_.get(); }

  // Returns the service under test.
  SessionRestorationService* service() { return service_.get(); }

  // Returns the path to the storage for Browser with `identifier`.
  base::FilePath SessionPathFromIdentifier(std::string_view identifier) {
    return browser_state_->GetStatePath()
        .Append(kSessionRestorationDirname)
        .Append(identifier);
  }

  // Inserts WebStates into `browser` each one loading a new URL from `urls`
  // and wait until all the WebStates are done with the navigation.
  void InsertTabsWithUrls(Browser& browser,
                          base::span<const std::string_view> urls) {
    base::RunLoop run_loop;
    ScopedTestWebStateObserver web_state_observer(
        ExpectNCall(run_loop.QuitClosure(), std::size(urls)));

    WebStateList* web_state_list = browser.GetWebStateList();
    for (std::string_view url : urls) {
      std::unique_ptr<web::WebState> web_state =
          web::WebState::Create(web::WebState::CreateParams(browser_state()));

      web_state_observer.Observe(web_state.get());

      // The view of the WebState needs to be created before the navigation
      // is really executed.
      std::ignore = web_state->GetView();
      web_state->GetNavigationManager()->LoadURLWithParams(
          web::NavigationManager::WebLoadParams(GURL(url)));

      web_state_list->InsertWebState(
          WebStateList::kInvalidIndex, std::move(web_state),
          WebStateList::INSERT_ACTIVATE, WebStateOpener());
    }

    // Wait for the navigation to commit.
    run_loop.Run();
  }

  // Wait until all task posted on the background sequence are complete.
  void WaitForBackgroundTaskComplete() {
    // Since the current TaskRunner is injected in the service to use for
    // background processing, posting a task to and waiting for it to be
    // invoked will ensure that all pending background processing is now
    // complete.
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Wait until the save delay expired and then for all background task
  // to complete.
  void WaitForSessionSaveComplete() {
    // Fast forward the time to allow any timer to expire (and thus the
    // delayed save to be scheduled).
    web_task_environment_->FastForwardBy(kSaveDelay);

    WaitForBackgroundTaskComplete();
  }

  // Take a snapshot of the existing files.
  void SnapshotFiles() { file_tracker_.Start(browser_state_->GetStatePath()); }

  // Returns the list of modified files.
  FilePathSet ModifiedFiles() const {
    return file_tracker_.ModifiedFiles(browser_state_->GetStatePath());
  }

  // Returns the list of deleted files.
  FilePathSet DeletedFiles() const {
    return file_tracker_.DeletedFiles(browser_state_->GetStatePath());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  FileModificationTracker file_tracker_;

  std::unique_ptr<web::ScopedTestingWebClient> scoped_web_client_;
  std::unique_ptr<web::WebTaskEnvironment> web_task_environment_;

  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<SessionRestorationServiceImpl> service_;
};

// Tests that adding and removing observer works correctly.
TEST_F(SessionRestorationServiceImplTest, ObserverRegistration) {
  ScopedTestSessionRestorationObserver observer;
  ASSERT_FALSE(observer.IsInObserverList());

  // Check that registering/unregistering the observer works.
  observer.Observe(service());
  EXPECT_TRUE(observer.IsInObserverList());

  observer.Reset();
  EXPECT_FALSE(observer.IsInObserverList());
}

// Tests that SetSessionID does not load the session.
TEST_F(SessionRestorationServiceImplTest, SetSessionID) {
  ScopedTestSessionRestorationObserver observer;
  observer.Observe(service());

  // Check that calling SetSessionID() does not load the session.
  TestBrowser browser = TestBrowser(browser_state());
  service()->SetSessionID(&browser, kIdentifier0);
  EXPECT_FALSE(observer.restore_started());

  // Check that calling Disconnect() when there are no changes on the
  // Browser does not cause the session to be saved.
  service()->Disconnect(&browser);
  WaitForSessionSaveComplete();

  // Check that no session file was written to disk.
  EXPECT_EQ(ModifiedFiles(), FilePathSet{});
}

// Tests that LoadSession correctly loads the session from disk.
TEST_F(SessionRestorationServiceImplTest, LoadSession) {
  ScopedTestSessionRestorationObserver observer;
  observer.Observe(service());

  // Check that when a Browser is modified, the changes are reflected to the
  // storage after a delay.
  {
    TestBrowser browser = TestBrowser(browser_state());
    service()->SetSessionID(&browser, kIdentifier0);
    EXPECT_FALSE(observer.restore_started());

    // Insert a few WebState in the Browser's WebStateList.
    InsertTabsWithUrls(browser, base::make_span(kURLs));

    // Check that the session was written to disk.
    WaitForSessionSaveComplete();
    EXPECT_EQ(ModifiedFiles(),
              ExpectedStorageFilesForBrowser(
                  SessionPathFromIdentifier(kIdentifier0), &browser));

    // Disconnect the Browser before destroying it. The service should no
    // longer track it and any modification should not be reflected.
    service()->Disconnect(&browser);

    // Check that closing the all the tabs after disconnecting the Browser
    // does not delete the sesion.
    SnapshotFiles();
    browser.GetWebStateList()->CloseAllWebStates(WebStateList::CLOSE_NO_FLAGS);

    WaitForSessionSaveComplete();
    EXPECT_EQ(ModifiedFiles(), FilePathSet{});
    EXPECT_EQ(DeletedFiles(), FilePathSet{});
  }

  // Check that the session can be reloaded and that it contains the same
  // state as when it was saved.
  {
    TestBrowser browser = TestBrowser(browser_state());
    service()->SetSessionID(&browser, kIdentifier0);
    EXPECT_FALSE(observer.restore_started());

    // Perform session restore, and check that the expected WebState have
    // been recreated with the correct navigation history.
    service()->LoadSession(&browser);

    EXPECT_TRUE(observer.restore_started());
    EXPECT_EQ(observer.restored_web_states_count(),
              static_cast<int>(std::size(kURLs)));

    WebStateList* web_state_list = browser.GetWebStateList();
    EXPECT_EQ(web_state_list->count(), static_cast<int>(std::size(kURLs)));
    EXPECT_EQ(web_state_list->active_index(), web_state_list->count() - 1);
    for (int index = 0; index < web_state_list->count(); ++index) {
      web::WebState* web_state = web_state_list->GetWebStateAt(index);
      EXPECT_EQ(web_state->GetLastCommittedURL(), GURL(kURLs[index]));
    }

    // Disconnect the Browser before destroying it.
    service()->Disconnect(&browser);
  }
}

// Tests that LoadSession succeed even if the session is empty.
TEST_F(SessionRestorationServiceImplTest, LoadSession_EmptySession) {
  ScopedTestSessionRestorationObserver observer;
  observer.Observe(service());

  // Write an empty session.
  ios::proto::WebStateListStorage storage;
  storage.set_active_index(-1);

  const base::FilePath metadata_path =
      SessionPathFromIdentifier(kIdentifier0).Append(kSessionMetadataFilename);
  EXPECT_TRUE(ios::sessions::WriteProto(metadata_path, storage));

  // Check that the session can be loaded even if non-existent and that the
  // Browser is unmodified (but the observers notified).
  {
    TestBrowser browser = TestBrowser(browser_state());
    service()->SetSessionID(&browser, kIdentifier0);
    EXPECT_FALSE(observer.restore_started());

    // Check that loading the sessions succeed.
    service()->LoadSession(&browser);

    EXPECT_TRUE(observer.restore_started());
    EXPECT_EQ(observer.restored_web_states_count(), 0);

    // Disconnect the Browser before destroying it.
    service()->Disconnect(&browser);
  }
}

// Tests that LoadSession succeed even if the session does not exist.
TEST_F(SessionRestorationServiceImplTest, LoadSession_MissingSession) {
  ScopedTestSessionRestorationObserver observer;
  observer.Observe(service());

  // Check that the session can be loaded even if non-existent and that the
  // Browser is unmodified (but the observers notified).
  {
    TestBrowser browser = TestBrowser(browser_state());
    service()->SetSessionID(&browser, kIdentifier0);
    EXPECT_FALSE(observer.restore_started());

    // Check that loading the sessions succeed, even if there is no session.
    service()->LoadSession(&browser);

    EXPECT_TRUE(observer.restore_started());
    EXPECT_EQ(observer.restored_web_states_count(), 0);

    // Disconnect the Browser before destroying it.
    service()->Disconnect(&browser);
  }
}

// Tests that the service only saves the session of modified Browser.
TEST_F(SessionRestorationServiceImplTest, SaveSessionOfModifiedBrowser) {
  // Register multiple Browser and modify one of them. Check that
  // only data for the modified Browser is written to disk.
  TestBrowser browser0 = TestBrowser(browser_state());
  TestBrowser browser1 = TestBrowser(browser_state());
  service()->SetSessionID(&browser0, kIdentifier0);
  service()->SetSessionID(&browser1, kIdentifier1);

  // Insert a few WebState in browser1's WebStateList.
  InsertTabsWithUrls(browser1, base::make_span(kURLs));

  // Check that only browser1's session was written to disk.
  WaitForSessionSaveComplete();
  EXPECT_EQ(ModifiedFiles(),
            ExpectedStorageFilesForBrowser(
                SessionPathFromIdentifier(kIdentifier1), &browser1));

  // Disconnect the Browser before destroying it.
  service()->Disconnect(&browser1);
  service()->Disconnect(&browser0);
}

// Tests that the service only save content that has changed.
TEST_F(SessionRestorationServiceImplTest, SaveSessionChangesOnlyRequiredFiles) {
  // Create a Browser and add a few WebStates to it.
  TestBrowser browser = TestBrowser(browser_state());
  service()->SetSessionID(&browser, kIdentifier0);
  InsertTabsWithUrls(browser, base::make_span(kURLs));

  // Check that the session was written to disk.
  WaitForSessionSaveComplete();
  EXPECT_EQ(ModifiedFiles(),
            ExpectedStorageFilesForBrowser(
                SessionPathFromIdentifier(kIdentifier0), &browser));

  // Record the list of existing files and their timestamp.
  SnapshotFiles();

  // Change the active WebState, and mark the new WebState as visited. This
  // should result in saving the session metadata (due to the change in the
  // WebStateList) and of one WebState (as its last active timestamp changed).
  ASSERT_NE(browser.GetWebStateList()->active_index(), 0);
  browser.GetWebStateList()->ActivateWebStateAt(0);
  browser.GetWebStateList()->GetActiveWebState()->WasShown();

  // Check that session metadata storage file and the active WebState storage
  // files are eventually saved.
  WaitForSessionSaveComplete();
  EXPECT_EQ(ModifiedFiles(),
            ExpectedStorageFilesForWebStates(
                SessionPathFromIdentifier(kIdentifier0),
                /* expect_session_metadata_storage */ true,
                {WebStateReference{
                    .web_state = browser.GetWebStateList()->GetActiveWebState(),
                    .is_native_session_available = true,
                }}));

  // Disconnect the Browser before destroying it.
  service()->Disconnect(&browser);
}

// Tests that the service correctly support moving "unrealized" tabs between
// Browsers and that this results in a copy of the moved WebState's storage.
TEST_F(SessionRestorationServiceImplTest, AdoptUnrealizedWebStateOnMove) {
  // In order to have unrealized WebState in the Browser, create a Browser,
  // add some WebState, wait for the session to be serialized. The session
  // can then be loaed to get unrealized WebStates.
  {
    TestBrowser browser = TestBrowser(browser_state());
    service()->SetSessionID(&browser, kIdentifier0);

    // Insert a few WebState in the Browser's WebStateList.
    InsertTabsWithUrls(browser, base::make_span(kURLs));

    // Check that the session was written to disk.
    WaitForSessionSaveComplete();
    EXPECT_TRUE(base::DirectoryExists(SessionPathFromIdentifier(kIdentifier0)));

    // Disconnect the Browser before destroying it.
    service()->Disconnect(&browser);

    // Check that closing the all the tabs after disconnecting the Browser
    // does not delete the sesion.
    WaitForSessionSaveComplete();
    EXPECT_EQ(ModifiedFiles(),
              ExpectedStorageFilesForBrowser(
                  SessionPathFromIdentifier(kIdentifier0), &browser));
  }

  // Load the session created before, and then move the tabs from the first
  // browser to the second one. Check that the session files have been copied.
  TestBrowser browser0 = TestBrowser(browser_state());
  TestBrowser browser1 = TestBrowser(browser_state());
  service()->SetSessionID(&browser0, kIdentifier0);
  service()->SetSessionID(&browser1, kIdentifier1);

  // Load the session in `browser0` and check that the expected number of tabs
  // are present and that the inactive tabs are not realized.
  service()->LoadSession(&browser0);
  service()->LoadSession(&browser1);

  // Record the list of existing files and their timestamp.
  SnapshotFiles();

  WebStateList* list0 = browser0.GetWebStateList();
  WebStateList* list1 = browser1.GetWebStateList();
  ASSERT_EQ(list0->count(), static_cast<int>(std::size(kURLs)));
  ASSERT_EQ(list1->count(), 0);

  for (int index = 0; index < list0->count(); ++index) {
    web::WebState* web_state = list0->GetWebStateAt(index);
    EXPECT_EQ(web_state->IsRealized(), index == list0->active_index());
  }

  // Move all tabs from browser0 to browser1 and check that this results in
  // the copy of the WebState's storage from browser0 to browser1 session
  // directory.
  //
  // Start by moving the inactive tabs, then move all the active tabs. The
  // move is done in reverse order to simplify the iteration (this allows
  // skipping the active_index during the iteration without going out of
  // range).
  const int old_count = list0->count();
  const int old_active_index = list0->active_index();
  ASSERT_EQ(list1->active_index(), WebStateList::kInvalidIndex);
  for (int index = 0; index < old_count; ++index) {
    const int reverse_index = old_count - 1 - index;
    if (reverse_index == old_active_index) {
      continue;
    }

    list1->InsertWebState(0, list0->DetachWebStateAt(reverse_index),
                          WebStateList::INSERT_FORCE_INDEX, WebStateOpener());
    ASSERT_EQ(list1->active_index(), WebStateList::kInvalidIndex);
  }

  ASSERT_EQ(list0->count(), 1);
  std::unique_ptr<web::WebState> web_state = list0->DetachWebStateAt(0);
  list1->InsertWebState(
      old_active_index, std::move(web_state),
      WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE,
      WebStateOpener());

  ASSERT_EQ(list0->count(), 0);
  ASSERT_EQ(list1->count(), static_cast<int>(std::size(kURLs)));

  // Check that no files were deleted, the metadata for both session updated
  // and the WebState's storage copied from one Browser storage to the other.
  WaitForSessionSaveComplete();

  FilePathSet expected_browser0 = ExpectedStorageFilesForWebStates(
      SessionPathFromIdentifier(kIdentifier0),
      /* expect_session_metadata_storage */ true, {});
  FilePathSet expected_browser1 = ExpectedStorageFilesForBrowser(
      SessionPathFromIdentifier(kIdentifier1), &browser1);

  EXPECT_EQ(ModifiedFiles(), expected_browser0 + expected_browser1);

  // Disconnect the Browser before destroying them.
  service()->Disconnect(&browser1);
  service()->Disconnect(&browser0);
}

// Tests that the service save pending changes on disconnect.
TEST_F(SessionRestorationServiceImplTest, SavePendingChangesOnDisconnect) {
  // Create a Browser and add a few WebStates to it.
  TestBrowser browser = TestBrowser(browser_state());
  service()->SetSessionID(&browser, kIdentifier0);
  InsertTabsWithUrls(browser, base::make_span(kURLs));

  // Inserting the tabs may take more time than the save delay. Always
  // wait for the state to be saved so that the test is deterministic.
  WaitForSessionSaveComplete();
  EXPECT_EQ(ModifiedFiles(),
            ExpectedStorageFilesForBrowser(
                SessionPathFromIdentifier(kIdentifier0), &browser));

  // Record the list of existing files and their timestamp.
  SnapshotFiles();

  // Modify the Browser which will schedule a session after a delay.
  {
    WebStateList* web_state_list = browser.GetWebStateList();
    const int active_index = web_state_list->active_index();
    ASSERT_NE(active_index, 0);

    web_state_list->MoveWebStateAt(active_index, 0);
  }

  // Record the time and check that no file have been saved yet.
  const base::Time disconnect_time = base::Time::Now();
  EXPECT_EQ(ModifiedFiles(), FilePathSet{});

  // Disconnect the Browser. This should save the session immediately.
  service()->Disconnect(&browser);

  // Not using `WaitForSessionSaveComplete()` because we explicitly do
  // not want to wait for the kSaveDelay timeout.
  WaitForBackgroundTaskComplete();

  // Check that even though the save delay has not expired, the data still
  // has been written to disk (because it was scheduled when the Browser
  // was disconnected as it contained pending changes).
  EXPECT_LT(base::Time::Now() - disconnect_time, kSaveDelay);
  EXPECT_EQ(ModifiedFiles(),
            ExpectedStorageFilesForWebStates(
                SessionPathFromIdentifier(kIdentifier0),
                /* expect_session_metadata_storage */ true, {}));
}

// Tests that the service delete obsolete files when loading the session.
TEST_F(SessionRestorationServiceImplTest, DeleteObsoleteFilesOnLoadSession) {
  // Used to store the list of files that correspond to the session used
  // for the closed WebState.
  FilePathSet expected_deleted_files;

  {
    // Create a Browser and add a few WebStates to it.
    TestBrowser browser = TestBrowser(browser_state());
    service()->SetSessionID(&browser, kIdentifier0);
    InsertTabsWithUrls(browser, base::make_span(kURLs));

    // Inserting the tabs may take more time than the save delay. Always
    // wait for the state to be saved so that the test is deterministic.
    WaitForSessionSaveComplete();
    EXPECT_EQ(ModifiedFiles(),
              ExpectedStorageFilesForBrowser(
                  SessionPathFromIdentifier(kIdentifier0), &browser));

    // Record the list of existing files and their timestamp.
    SnapshotFiles();

    // Detach one WebState from the Browser (not the active one).
    ASSERT_NE(browser.GetWebStateList()->active_index(), 0);
    std::unique_ptr<web::WebState> detached_web_state =
        browser.GetWebStateList()->DetachWebStateAt(0);

    // Wait for the sesssion to be saved, and check that no file were deleted,
    // but that the session metadata was updated.
    WaitForSessionSaveComplete();
    EXPECT_EQ(DeletedFiles(), FilePathSet{});
    EXPECT_EQ(ModifiedFiles(),
              ExpectedStorageFilesForWebStates(
                  SessionPathFromIdentifier(kIdentifier0),
                  /* expect_session_metatadat_storage */ true, {}));

    // Record the files used to represent the state of the detached WebState.
    // Those files will be deleted when the session is loaded.
    expected_deleted_files = ExpectedStorageFilesForWebStates(
        SessionPathFromIdentifier(kIdentifier0),
        /* expect_session_metadata_storage */ false,
        {WebStateReference{.web_state = detached_web_state.get(),
                           .is_native_session_available = true}});

    // Disconnect the Browser before destroying it.
    service()->Disconnect(&browser);
  }

  // Record the list of existing files and their timestamp.
  SnapshotFiles();

  // Create a Browser and load the session in it.
  TestBrowser browser = TestBrowser(browser_state());
  service()->SetSessionID(&browser, kIdentifier0);
  service()->LoadSession(&browser);

  // Check that the expected content was loaded and no files deleted yet (the
  // deletion is scheduled on the background sequence).
  EXPECT_EQ(browser.GetWebStateList()->count(),
            static_cast<int>(std::size(kURLs)) - 1);
  EXPECT_EQ(DeletedFiles(), FilePathSet{});

  // Wait for background processing to complete and check that the obsolete
  // session files have been deleted.
  WaitForSessionSaveComplete();
  EXPECT_EQ(DeletedFiles(), expected_deleted_files);

  // Disconnect the Browser before destroying it.
  service()->Disconnect(&browser);
}

// Tests that histograms are correctly recorded.
TEST_F(SessionRestorationServiceImplTest, RecordHistograms) {
  {
    // Create a Browser and add a few WebStates to it and wait for all
    // pending scheduled tasks to complete.
    TestBrowser browser = TestBrowser(browser_state());
    service()->SetSessionID(&browser, kIdentifier0);
    InsertTabsWithUrls(browser, base::make_span(kURLs));
    WaitForSessionSaveComplete();

    // Check that session is saved and histogram is recorded when making
    // some changes to the Browser's WebStateList (changing the active
    // index).
    base::HistogramTester histogram_tester;
    ASSERT_NE(browser.GetWebStateList()->active_index(), 0);
    browser.GetWebStateList()->ActivateWebStateAt(0);
    WaitForSessionSaveComplete();

    // Check that the time spent to record the session was logged.
    histogram_tester.ExpectTotalCount(
        "Session.WebStates.SavingTimeOnMainThread", 1);

    // Disconnect the Browser before destroying it.
    service()->Disconnect(&browser);
  }

  // Create a Browser.
  TestBrowser browser = TestBrowser(browser_state());
  service()->SetSessionID(&browser, kIdentifier0);

  // Load the session and check that the time spent loading was logged.
  base::HistogramTester histogram_tester;
  service()->LoadSession(&browser);

  // Check that the expected content was loaded.
  EXPECT_EQ(browser.GetWebStateList()->count(),
            static_cast<int>(std::size(kURLs)));
  histogram_tester.ExpectTotalCount("Session.WebStates.LoadingTimeOnMainThread",
                                    1);

  // Disconnect the Browser before destroying it.
  service()->Disconnect(&browser);
}

// Tests that creating an unrealized WebState succeed and that the data
// is correctly saved to the disk.
TEST_F(SessionRestorationServiceImplTest, CreateUnrealizedWebState) {
  // Create a Browser.
  TestBrowser browser = TestBrowser(browser_state());
  service()->SetSessionID(&browser, kIdentifier0);

  // Create an unrealized WebState.
  std::unique_ptr<web::WebState> web_state =
      service()->CreateUnrealizedWebState(
          &browser,
          web::CreateWebStateStorage(
              web::NavigationManager::WebLoadParams(GURL(kURL)), kTitle, false,
              web::UserAgentType::MOBILE, base::Time::Now()));
  ASSERT_TRUE(web_state);

  // Record the list of expected files while the pointer to the newly created
  // WebState is still valid.
  const FilePathSet expected_files = ExpectedStorageFilesForWebStates(
      SessionPathFromIdentifier(kIdentifier0),
      /* expect_session_metadata_storage */ true,
      {WebStateReference{
          .web_state = web_state.get(),
          .is_native_session_available = false,
      }});

  // Insert the WebState into the Browser's WebStateList and then wait for
  // the session to be saved to storage.
  browser.GetWebStateList()->InsertWebState(
      WebStateList::kInvalidIndex, std::move(web_state),
      WebStateList::InsertionFlags::INSERT_ACTIVATE, WebStateOpener());
  WaitForSessionSaveComplete();

  // Check that the data for the WebState has been saved to disk.
  EXPECT_EQ(ModifiedFiles(), expected_files);

  // Disconnect the Browser before destroying it.
  service()->Disconnect(&browser);
}

// Tests that calling SaveSessions() can be done at any point in time.
TEST_F(SessionRestorationServiceImplTest, SaveSessionsCallableAtAnyTime) {
  // Check that calling SaveSessions() when no Browser is observed is a no-op.
  service()->SaveSessions();

  WaitForBackgroundTaskComplete();
  EXPECT_EQ(ModifiedFiles(), FilePathSet{});

  // Check that calling SaveSessions() when Browser are registered with no
  // changes is a no-op
  TestBrowser browser0 = TestBrowser(browser_state());
  TestBrowser browser1 = TestBrowser(browser_state());
  service()->SetSessionID(&browser0, kIdentifier0);
  service()->SetSessionID(&browser1, kIdentifier1);

  service()->SaveSessions();

  WaitForBackgroundTaskComplete();
  EXPECT_EQ(ModifiedFiles(), FilePathSet{});

  // Insert a few WebStage in one of the Browser and wait for the changes
  // to automatically be saved (this is because loading the pages will
  // take time and may cause automatically saving the session).
  {
    InsertTabsWithUrls(browser0, base::make_span(kURLs));
    WaitForSessionSaveComplete();

    EXPECT_EQ(ModifiedFiles(),
              ExpectedStorageFilesForBrowser(
                  SessionPathFromIdentifier(kIdentifier0), &browser0));

    SnapshotFiles();
  }

  // Check that making a modification and then calling SaveSessions() will
  // result in a save immediately, even without waiting for the save delay.
  ASSERT_NE(browser0.GetWebStateList()->active_index(), 0);
  browser0.GetWebStateList()->ActivateWebStateAt(0);

  WaitForBackgroundTaskComplete();
  EXPECT_EQ(ModifiedFiles(), FilePathSet{});

  service()->SaveSessions();
  WaitForBackgroundTaskComplete();
  EXPECT_EQ(ModifiedFiles(),
            ExpectedStorageFilesForWebStates(
                SessionPathFromIdentifier(kIdentifier0),
                /* expect_session_metadata_storage */ true, {}));

  SnapshotFiles();

  // Check that calling SaveSessions() when all Browser have been disconnected
  // is a no-op;
  service()->Disconnect(&browser0);
  service()->Disconnect(&browser1);

  service()->SaveSessions();

  WaitForBackgroundTaskComplete();
  EXPECT_EQ(ModifiedFiles(), FilePathSet{});
}
