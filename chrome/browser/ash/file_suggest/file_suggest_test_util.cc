// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/file_suggest_test_util.h"

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"

namespace ash {

void WaitForFileSuggestionUpdate(
    const testing::NiceMock<MockFileSuggestKeyedServiceObserver>& mock,
    ash::FileSuggestionType expected_type) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock, OnFileSuggestionUpdated)
      .WillRepeatedly([&](ash::FileSuggestionType type) {
        if (type == expected_type) {
          run_loop.Quit();
        }
      });
  run_loop.Run();
}

void WaitUntilFileSuggestServiceReady(FileSuggestKeyedService* service) {
  if (!service->IsReadyForTest()) {
    testing::NiceMock<MockFileSuggestKeyedServiceObserver> mock;
    base::ScopedObservation<ash::FileSuggestKeyedService,
                            ash::FileSuggestKeyedService::Observer>
        service_observer{&mock};
    service_observer.Observe(service);
    // Not sure which suggestion type is ready first. Therefore, wait for both.
    WaitForFileSuggestionUpdate(mock, FileSuggestionType::kDriveFile);
    if (service->IsReadyForTest()) {
      return;
    }

    WaitForFileSuggestionUpdate(mock, FileSuggestionType::kLocalFile);
    EXPECT_TRUE(service->IsReadyForTest());
  }
}

}  // namespace ash
