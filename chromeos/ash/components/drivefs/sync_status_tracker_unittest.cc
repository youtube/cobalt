// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/sync_status_tracker.h"

#include <cstdint>
#include <memory>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs {
namespace {

const base::FilePath   //
    root("/"),         //
    a("/a"),           //
    b("/b"),           //
    ab("/a/b"),        //
    ad("/a/d"),        //
    abc("/a/b/c"),     //
    abcd("/a/b/c/d"),  //
    abce("/a/b/c/e"),  //
    abcf("/a/b/c/f"),  //
    abd("/a/b/d"),     //
    abe("/a/b/e"),     //
    af("/a/f"),        //
    afg("/a/f/g");     //

inline SyncState NotFound(const base::FilePath path = base::FilePath()) {
  return {SyncStatus::kNotFound, 0, path};
}
inline SyncState Moved(const base::FilePath path = base::FilePath()) {
  return {SyncStatus::kMoved, 0, path};
}
inline SyncState Completed(const base::FilePath path = base::FilePath()) {
  return {SyncStatus::kCompleted, 1, path};
}
inline SyncState Queued(const base::FilePath path = base::FilePath()) {
  return {SyncStatus::kQueued, 0, path};
}
inline SyncState InProgress(const base::FilePath path = base::FilePath(),
                            const float progress = 0) {
  return {SyncStatus::kInProgress, progress, path};
}
inline SyncState Error(const base::FilePath path = base::FilePath(),
                       const float progress = 0) {
  return {SyncStatus::kError, progress, path};
}

class SyncStatusTrackerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  SyncStatusTracker t;
};

TEST_F(SyncStatusTrackerTest, StatePropagatesToAncestors) {
  t.SetInProgress(1, abc, 0, 100);
  ASSERT_EQ(t.GetSyncState(abc), InProgress(abc));
  ASSERT_EQ(t.GetSyncState(ab), InProgress(ab));
  ASSERT_EQ(t.GetSyncState(a), InProgress(a));
  ASSERT_EQ(t.GetSyncState(root), InProgress(root));
}

TEST_F(SyncStatusTrackerTest, ErrorTakesPrecedenceInAncestors) {
  t.SetInProgress(1, abc, 0, 100);
  t.SetError(2, abd);
  ASSERT_EQ(t.GetSyncState(abc), InProgress(abc));
  ASSERT_EQ(t.GetSyncState(ab), Error(ab));
  ASSERT_EQ(t.GetSyncState(a), Error(a));
  ASSERT_EQ(t.GetSyncState(root), Error(root));
}

TEST_F(SyncStatusTrackerTest, PathsNotInTrackerReturnNotFound) {
  t.SetInProgress(1, abc, 0, 100);
  ASSERT_EQ(t.GetSyncState(abc), InProgress(abc));
  ASSERT_EQ(t.GetSyncState(abd), NotFound(abd));
}

TEST_F(SyncStatusTrackerTest, RemovingAPathRemovesSingleUseAncestors) {
  t.SetInProgress(1, abcf, 10, 100);
  t.SetInProgress(2, abd, 10, 100);
  t.SetInProgress(3, abe, 10, 100);

  t.SetCompleted(1, base::FilePath(abcf));

  ASSERT_EQ(t.GetSyncState(ab), InProgress(ab, 120. / 300.));
  ASSERT_EQ(t.GetSyncState(abc), Completed(abc));
  ASSERT_EQ(t.GetSyncState(abcf), Completed(abcf));

  t.GetChangesAndClean();

  ASSERT_EQ(t.GetSyncState(abcf), NotFound(abcf));
  ASSERT_EQ(t.GetSyncState(abc), NotFound(abc));
}

TEST_F(SyncStatusTrackerTest, FoldersCantBeMarkedCompleted) {
  t.SetInProgress(1, abcd, 0, 100);

  t.SetCompleted(2, base::FilePath(abc));
  t.SetCompleted(3, base::FilePath(ab));
  t.SetCompleted(4, base::FilePath(a));

  ASSERT_EQ(t.GetSyncState(abcd), InProgress(abcd));
}

TEST_F(SyncStatusTrackerTest, Utf8PathsAreSupported) {
  const base::FilePath utf8_path("/a/b/日本");
  t.SetInProgress(1, utf8_path, 0, 100);
  ASSERT_EQ(t.GetSyncState(utf8_path), InProgress(utf8_path));
}

TEST_F(SyncStatusTrackerTest, DeletingNonexistingPathIsNoOp) {
  t.SetInProgress(1, abcd, 0, 100);

  t.SetCompleted(2, base::FilePath("/a/b/c/d/e"));
  t.GetFileCount();

  ASSERT_EQ(t.GetSyncState(abcd), InProgress(abcd));
}

TEST_F(SyncStatusTrackerTest, AddingExistingPathReplacesStatus) {
  t.SetInProgress(1, abcd, 0, 100);
  t.SetError(2, abcd);

  ASSERT_EQ(t.GetSyncState(abcd), Error(abcd));
}

TEST_F(SyncStatusTrackerTest, MalformedPathsAreSupported) {
  base::FilePath malformed_path("////");
  t.SetInProgress(1, malformed_path, 0, 100);

  ASSERT_EQ(t.GetSyncState(malformed_path), InProgress(malformed_path));
}

TEST_F(SyncStatusTrackerTest, RelativePathsAreNotSupported) {
  base::FilePath relative_path1("./..");
  base::FilePath relative_path2("../");

  t.SetInProgress(1, relative_path1, 0, 100);
  t.SetInProgress(2, relative_path2, 0, 100);

  ASSERT_EQ(t.GetSyncState(relative_path1), NotFound(relative_path1));
  ASSERT_EQ(t.GetSyncState(relative_path2), NotFound(relative_path2));
}

TEST_F(SyncStatusTrackerTest, MovingFileDoesNotImmediatelyRemoveOldPath) {
  t.SetInProgress(1, abcd, 10, 100);
  t.SetQueued(2, abce, 0);
  // Rename /a/b/c/d to /a/b/c/f.
  t.SetInProgress(1, abcf, 50, 100);

  // Old path is moved.
  ASSERT_EQ(t.GetSyncState(abcd), Moved(abcd));
  ASSERT_EQ(t.GetSyncState(abce), Queued(abce));
  // New path is tracked.
  ASSERT_EQ(t.GetSyncState(abcf), InProgress(abcf, 0.5));

  ASSERT_EQ(t.GetFileCount(), 2u);
}

TEST_F(SyncStatusTrackerTest,
       MovingFileDoesNotImmediatelyRemoveOldPathAndParents) {
  t.SetInProgress(1, abcd, 10, 100);
  // Rename /a/b/c/d to /a/d.
  t.SetInProgress(1, ad, 20, 100);

  // Old path is marked as "moved" along with any childless parents.
  ASSERT_EQ(t.GetSyncState(abcd), Moved(abcd));
  ASSERT_EQ(t.GetSyncState(abc), Moved(abc));
  ASSERT_EQ(t.GetSyncState(ab), Moved(ab));
  // New path is tracked.
  ASSERT_EQ(t.GetSyncState(ad), InProgress(ad, 0.2));
  ASSERT_EQ(t.GetSyncState(a), InProgress(a, 0.2));

  ASSERT_EQ(t.GetFileCount(), 1u);
}

TEST_F(SyncStatusTrackerTest, FolderAggregateProgress) {
  t.SetInProgress(1, abcd, 10, 100);
  t.SetInProgress(2, abce, 20, 100);
  t.SetInProgress(3, ad, 20, 100);

  ASSERT_EQ(t.GetSyncState(abc), InProgress(abc, 30. / 200.));
  ASSERT_EQ(t.GetSyncState(ab), InProgress(ab, 30. / 200.));
  ASSERT_EQ(t.GetSyncState(a), InProgress(a, 50. / 300.));

  t.SetInProgress(1, abcd, 50, 100);
  t.SetInProgress(3, ad, 10, 200);

  ASSERT_EQ(t.GetSyncState(ab), InProgress(ab, 70. / 200.));
  ASSERT_EQ(t.GetSyncState(a), InProgress(a, 80. / 400.));

  t.SetError(1, abcd);

  ASSERT_EQ(t.GetSyncState(ab), Error(ab, 20. / 200.));
  ASSERT_EQ(t.GetSyncState(a), Error(a, 30. / 400.));
}

TEST_F(SyncStatusTrackerTest, OnlyDirtyNodesAreReturned) {
  t.SetInProgress(1, abcd, 10, 100);
  t.SetInProgress(2, abce, 20, 100);
  t.SetInProgress(3, ad, 20, 100);

  ASSERT_THAT(t.GetChangesAndClean(),
              testing::UnorderedElementsAre(InProgress(root, 50. / 300.),  //
                                            InProgress(a, 50. / 300.),     //
                                            InProgress(ab, 30. / 200.),    //
                                            InProgress(abc, 30. / 200.),   //
                                            InProgress(abcd, 10. / 100.),  //
                                            InProgress(abce, 20. / 100.),  //
                                            InProgress(ad, 20. / 100.)));  //

  t.SetError(1, abcd);
  t.SetQueued(4, afg, 100);

  ASSERT_THAT(t.GetChangesAndClean(),
              testing::UnorderedElementsAre(Error(root, 40. / 400.),  //
                                            Error(a, 40. / 400.),     //
                                            Error(ab, 20. / 200.),    //
                                            Error(abc, 20. / 200.),   //
                                            Error(abcd, 0. / 100.),   //
                                            Queued(af),               //
                                            Queued(afg)));            //

  t.SetCompleted(2, abce);

  ASSERT_THAT(t.GetChangesAndClean(),
              testing::UnorderedElementsAre(Error(root, 120. / 400.),  //
                                            Error(a, 120. / 400.),     //
                                            Error(ab, 100. / 200.),    //
                                            Error(abc, 100. / 200.),   //
                                            Completed(abce)));         //

  // Move /a/b/c/d to /b.
  t.SetInProgress(1, b, 20, 100);

  ASSERT_THAT(t.GetChangesAndClean(),
              testing::UnorderedElementsAre(InProgress(root, 140. / 400.),  //
                                            InProgress(a, 120. / 300.),     //
                                            Completed(ab),                  //
                                            Completed(abc),                 //
                                            Moved(abcd),                    //
                                            InProgress(b, 20. / 100.)));    //
}

TEST_F(SyncStatusTrackerTest, StaleNodesGetDeleted) {
  t.SetQueued(1, abcd, 100);

  ASSERT_EQ(t.GetSyncState(abcd), Queued(abcd));

  t.GetChangesAndClean();
  ASSERT_EQ(t.GetSyncState(abcd), Queued(abcd));

  // Paths not updated in the last 10s should be purged, so let's test at both
  // edges (9s and 11s).
  task_environment.FastForwardBy(base::Seconds(9));
  auto changes = t.GetChangesAndClean();
  ASSERT_EQ(t.GetSyncState(abcd), Queued(abcd));
  ASSERT_TRUE(changes.empty());

  task_environment.FastForwardBy(base::Seconds(2));
  changes = t.GetChangesAndClean();
  ASSERT_EQ(t.GetSyncState(abcd), NotFound(abcd));
  ASSERT_EQ(changes.size(), 5u);
  ASSERT_EQ(changes.back(), Moved(abcd));
}

TEST_F(SyncStatusTrackerTest, MoveOverridesDoNotSegFault) {
  t.SetQueued(1, a, 0);
  t.SetQueued(2, b, 0);

  // Override /b with id #3.
  t.SetQueued(3, b, 0);

  // Rename /b to /a.
  t.SetQueued(2, a, 0);

  ASSERT_EQ(t.GetSyncState(a), Queued(a));
  ASSERT_EQ(t.GetSyncState(b), Queued(b));

  auto changes = t.GetChangesAndClean();

  // Make all nodes stale.
  task_environment.FastForwardBy(base::Seconds(11));

  // This should not lead to a SegFault (addresses use after free bug where
  // id_to_leaf_ members could point to a node that had already been erased -
  // b/276784178).
  changes = t.GetChangesAndClean();
}

}  // namespace
}  // namespace drivefs
