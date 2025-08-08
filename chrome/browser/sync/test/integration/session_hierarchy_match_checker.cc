// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/session_hierarchy_match_checker.h"

SessionHierarchyMatchChecker::SessionHierarchyMatchChecker(
    const fake_server::SessionsHierarchy& sessions_hierarchy,
    syncer::SyncServiceImpl* service,
    fake_server::FakeServer* fake_server)
    : SingleClientStatusChangeChecker(service),
      sessions_hierarchy_(sessions_hierarchy),
      verifier_(fake_server) {}

bool SessionHierarchyMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for matching sessions hierarchy to be reflected in fake "
         "server. ";
  testing::AssertionResult result =
      verifier_.VerifySessions(sessions_hierarchy_);
  *os << result.message();
  return result;
}
