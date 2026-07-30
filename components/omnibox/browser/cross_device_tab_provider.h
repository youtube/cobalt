// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_CROSS_DEVICE_TAB_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_CROSS_DEVICE_TAB_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteInput;
class AutocompleteProviderClient;
struct OmniboxLog;

namespace sessions {
struct SessionTab;
}  // namespace sessions

// Autocomplete provider for tabs from other devices.
class CrossDeviceTabProvider : public AutocompleteProvider {
 public:
  explicit CrossDeviceTabProvider(AutocompleteProviderClient* client);

  CrossDeviceTabProvider(const CrossDeviceTabProvider&) = delete;
  CrossDeviceTabProvider& operator=(const CrossDeviceTabProvider&) = delete;

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

  base::Time most_recent_tab_timestamp() const {
    return most_recent_tab_timestamp_;
  }

  // Records interaction metrics (`ShowAge`, `FocusToOpenTime`, `ClickAge`) for
  // the `CrossDeviceTab` match and action if they are present in `log`.
  static void RecordInteractionMetrics(const OmniboxLog& log);

  // Exposed publicly for testing purposes only.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(CrossDeviceTabProviderEligibility)
  enum class Eligibility {
    kMatchCreated = 0,
    kNoSyncService = 1,
    kNoOpenTabsDelegate = 2,
    kNoForeignSessions = 3,
    kNoTabs = 4,
    kTabTooOld = 5,
    kInvalidUrl = 6,
    kLocalSessionNotRecent = 7,
    kMaxValue = kLocalSessionNotRecent,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:CrossDeviceTabProviderEligibility)

 protected:
  ~CrossDeviceTabProvider() override;

 private:
  using QueryResult = base::expected<const sessions::SessionTab*, Eligibility>;

  QueryResult GetMostRecentTab();
  void LogEligibility(Eligibility eligibility);

  // Covers the user journey where the user switches to another (this) device
  // shortly after navigating on a remote device (e.g. using both devices
  // simultaneously).
  bool IsVeryRecentRemoteTimestamp(base::Time timestamp) const;

  // Covers the delayed continuation scenario: the user stopped using all
  // devices for some time, then started using a device different to the one
  // being used previously (and they may be interested in resuming their
  // browsing journey).
  bool IsModeratelyRecentRemoteTimestampWithRecentLocalSessionStart(
      base::Time timestamp) const;

  // Returns true if the remote `timestamp` satisfies any of the activation
  // criteria required to surface the cross-device tab suggestion.
  //
  // The combined logic is:
  // (
  //    if tab is newer than 5 minutes (simultaneous use)
  //    OR
  //    (
  //      if the tab is newer than 720 minutes (12 hours) (delayed continuation
  //      age limit)
  //      AND
  //      profile uptime is less than 5 minutes (delayed continuation
  //      uptime limit)
  //    )
  // )
  //
  // (Note: actual limits are configurable via features).
  bool IsRecentEnoughRemoteTimestampToSuggestRemoteTab(
      base::Time timestamp) const;

  const raw_ptr<AutocompleteProviderClient> client_;

  // The timestamp of the tab (when it was last active on the remote device),
  // updated as part of `Start()`. Null if it produced no matches (i.e.
  // `matches_` is empty).
  base::Time most_recent_tab_timestamp_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_CROSS_DEVICE_TAB_PROVIDER_H_
