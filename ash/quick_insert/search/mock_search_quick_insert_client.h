// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_SEARCH_MOCK_SEARCH_QUICK_INSERT_CLIENT_H_
#define ASH_QUICK_INSERT_SEARCH_MOCK_SEARCH_QUICK_INSERT_CLIENT_H_

#include "ash/ash_export.h"
#include "ash/quick_insert/mock_quick_insert_client.h"

namespace ash {

// Mock client used for search tests.
// By default:
// - `StartCrosSearch` will store the supplied callback which can be obtained
//   using `cros_search_callback()`.
// - `GetSharedURLLoaderFactory` and `ShowEditor` will cause the current test to
//   fail.
// These behaviours can be overridden with `WillOnce` and `WillRepeatedly` if
// necessary.
class ASH_EXPORT MockSearchQuickInsertClient : public MockQuickInsertClient {
 public:
  MockSearchQuickInsertClient();
  ~MockSearchQuickInsertClient() override;

  // Set by the default `StartCrosSearch` behaviour. If the behaviour is
  // overridden, this may not be set on a `StartCrosSearch` callback.
  CrosSearchResultsCallback& cros_search_callback();

 private:
  CrosSearchResultsCallback cros_search_callback_;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_SEARCH_MOCK_SEARCH_QUICK_INSERT_CLIENT_H_
