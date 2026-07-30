// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGEST_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGEST_TEST_UTIL_H_

#include "chrome/browser/ash/file_suggest/mock_file_suggest_keyed_service_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

// Waits until `mock` is notified of the file suggestion update.
void WaitForFileSuggestionUpdate(
    const testing::NiceMock<MockFileSuggestKeyedServiceObserver>& mock,
    ash::FileSuggestionType expected_type);

// Waits until `service` is ready.
void WaitUntilFileSuggestServiceReady(ash::FileSuggestKeyedService* service);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGEST_TEST_UTIL_H_
