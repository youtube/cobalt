// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_ZERO_STATE_DRIVE_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_ZERO_STATE_DRIVE_PROVIDER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/files/file_result.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"

class Profile;

namespace ash {
struct FileSuggestData;
enum class FileSuggestionType;
}  // namespace ash

namespace app_list {
class ZeroStateDriveProvider : public SearchProvider,
                               ash::FileSuggestKeyedService::Observer {
 public:
  explicit ZeroStateDriveProvider(Profile* profile);
  ~ZeroStateDriveProvider() override;

  ZeroStateDriveProvider(const ZeroStateDriveProvider&) = delete;
  ZeroStateDriveProvider& operator=(const ZeroStateDriveProvider&) = delete;

  // SearchProvider:
  void StartZeroState() override;
  void StopZeroState() override;
  ash::AppListSearchResultType ResultType() const override;

 private:
  // Called when file suggestion data are fetched from the service.
  void OnSuggestFileDataFetched(
      const std::optional<std::vector<ash::FileSuggestData>>& suggest_results);

  // Builds the search results from file suggestions then publishes the results.
  void SetSearchResults(
      const std::vector<ash::FileSuggestData>& suggest_results);

  // FileSuggestKeyedService::Observer:
  void OnFileSuggestionUpdated(ash::FileSuggestionType type) override;

  const raw_ptr<Profile> profile_;

  const raw_ptr<ash::FileSuggestKeyedService> file_suggest_service_;

  base::TimeTicks query_start_time_;

  base::ScopedObservation<ash::FileSuggestKeyedService,
                          ash::FileSuggestKeyedService::Observer>
      file_suggest_service_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to guard the query for drive file suggestions.
  base::WeakPtrFactory<ZeroStateDriveProvider> suggestion_query_weak_factory_{
      this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_ZERO_STATE_DRIVE_PROVIDER_H_
