// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/one_time_permissions_tracker.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/visibility.h"
#include "url/gurl.h"

constexpr auto kBackgroundExpirationDuration = base::Minutes(5);
constexpr auto kMediaExpirationDuration = kBackgroundExpirationDuration;

OneTimePermissionsTracker::OneTimePermissionsTracker() = default;
OneTimePermissionsTracker::~OneTimePermissionsTracker() = default;

OneTimePermissionsTracker::OriginTrackEntry::OriginTrackEntry() = default;

OneTimePermissionsTracker::OriginTrackEntry::~OriginTrackEntry() = default;

void OneTimePermissionsTracker::Shutdown() {
  for (auto& observer : observer_list_) {
    observer.OnShutdown();
  }
  observer_list_.Clear();
}

void OneTimePermissionsTracker::AddObserver(
    OneTimePermissionsTrackerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void OneTimePermissionsTracker::RemoveObserver(
    OneTimePermissionsTrackerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void OneTimePermissionsTracker::WebContentsBackgrounded(
    const url::Origin& origin) {
  if (!ShouldIgnoreOrigin(origin)) {
    origin_tracker_[origin].background_tab_counter++;

    if (AreAllTabsToOriginBackgroundedOrDiscarded(origin)) {
      // When all undiscarded tabs which point to the origin are in the
      // background, the timer should be reset.
      origin_tracker_[origin].background_expiration_timer->Start(
          FROM_HERE, kBackgroundExpirationDuration,
          base::BindOnce(
              &OneTimePermissionsTracker::NotifyBackgroundTimerExpired,
              weak_factory_.GetWeakPtr(), origin));

      HandleUserMediaState(origin, ContentSettingsType::MEDIASTREAM_CAMERA);
      HandleUserMediaState(origin, ContentSettingsType::MEDIASTREAM_MIC);
    } else {
      origin_tracker_[origin].background_expiration_timer->AbandonAndStop();
    }
  }
}

void OneTimePermissionsTracker::WebContentsUnbackgrounded(
    const url::Origin& origin) {
  if (!ShouldIgnoreOrigin(origin)) {
    origin_tracker_[origin].background_tab_counter--;
    // Since the tab has been unbackgrounded, the timer should be reset
    origin_tracker_[origin].background_expiration_timer->AbandonAndStop();
  }
}

void OneTimePermissionsTracker::WebContentsLoadedOrigin(
    const url::Origin& origin) {
  if (!ShouldIgnoreOrigin(origin)) {
    origin_tracker_[origin].undiscarded_tab_counter++;
    origin_tracker_[origin].background_expiration_timer->AbandonAndStop();
  }
}

void OneTimePermissionsTracker::WebContentsUnloadedOrigin(
    const url::Origin& origin) {
  if (!ShouldIgnoreOrigin(origin)) {
    origin_tracker_[origin].undiscarded_tab_counter--;
    DCHECK(!(origin_tracker_[origin].undiscarded_tab_counter < 0));
    if (origin_tracker_[origin].undiscarded_tab_counter == 0) {
      for (auto& observer : observer_list_) {
        observer.OnLastPageFromOriginClosed(origin);
      }
    }
  }
}

void OneTimePermissionsTracker::StartContentSpecificExpirationTimer(
    const url::Origin& origin,
    ContentSettingsType content_setting,
    NotifyFunction notify_callback) {
  origin_tracker_[origin].used_content_settings_set.insert(content_setting);
  origin_tracker_[origin]
      .content_setting_specific_expiration_timer_map[content_setting]
      ->Start(
          FROM_HERE, kMediaExpirationDuration,
          base::BindOnce(notify_callback, weak_factory_.GetWeakPtr(), origin));
}

void OneTimePermissionsTracker::HandleUserMediaState(
    const url::Origin& origin,
    ContentSettingsType content_setting) {
  NotifyFunction notify_callback;
  switch (content_setting) {
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      notify_callback = &OneTimePermissionsTracker::NotifyCapturingVideoExpired;
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      notify_callback = &OneTimePermissionsTracker::NotifyCapturingAudioExpired;
      break;
    default:
      NOTREACHED();
      return;
  }

  if (origin_tracker_[origin].used_content_settings_set.find(content_setting) !=
      origin_tracker_[origin].used_content_settings_set.end()) {
    if (origin_tracker_[origin]
            .content_setting_specific_expiration_timer_map.find(
                content_setting) ==
        origin_tracker_[origin]
            .content_setting_specific_expiration_timer_map.end()) {
      origin_tracker_[origin]
          .content_setting_specific_expiration_timer_map[content_setting] =
          std::make_unique<base::OneShotTimer>();
    }

    if (origin_tracker_[origin]
                .content_setting_specific_counter_map[content_setting] == 0 &&
        AreAllTabsToOriginBackgroundedOrDiscarded(origin)) {
      StartContentSpecificExpirationTimer(origin, content_setting,
                                          notify_callback);
    } else {
      origin_tracker_[origin]
          .content_setting_specific_expiration_timer_map[content_setting]
          ->AbandonAndStop();
    }
  }
}

void OneTimePermissionsTracker::CapturingVideoChanged(const url::Origin& origin,
                                                      bool is_capturing_video) {
  if (!ShouldIgnoreOrigin(origin)) {
    if (is_capturing_video &&
        origin_tracker_[origin].used_content_settings_set.find(
            ContentSettingsType::MEDIASTREAM_CAMERA) ==
            origin_tracker_[origin].used_content_settings_set.end()) {
      origin_tracker_[origin].used_content_settings_set.insert(
          ContentSettingsType::MEDIASTREAM_CAMERA);
    }

    origin_tracker_[origin].content_setting_specific_counter_map
        [ContentSettingsType::MEDIASTREAM_CAMERA] +=
        is_capturing_video ? 1 : -1;
    HandleUserMediaState(origin, ContentSettingsType::MEDIASTREAM_CAMERA);
  }
}

void OneTimePermissionsTracker::CapturingAudioChanged(const url::Origin& origin,
                                                      bool is_capturing_audio) {
  if (!ShouldIgnoreOrigin(origin)) {
    if (is_capturing_audio &&
        origin_tracker_[origin].used_content_settings_set.find(
            ContentSettingsType::MEDIASTREAM_MIC) ==
            origin_tracker_[origin].used_content_settings_set.end()) {
      origin_tracker_[origin].used_content_settings_set.insert(
          ContentSettingsType::MEDIASTREAM_MIC);
    }

    origin_tracker_[origin].content_setting_specific_counter_map
        [ContentSettingsType::MEDIASTREAM_MIC] += is_capturing_audio ? 1 : -1;
    HandleUserMediaState(origin, ContentSettingsType::MEDIASTREAM_MIC);
  }
}

void OneTimePermissionsTracker::FireRunningTimersForTesting() {
  // The loops in this method require manual 'forward-looking' iteration because
  // the methods executing upon timer expiration might erase elements from the
  // map that is being iterated over.
  for (auto i = origin_tracker_.begin(), e = origin_tracker_.end(); i != e;) {
    auto origin_entry = i++;
    if (origin_entry->second.background_expiration_timer->IsRunning()) {
      origin_entry->second.background_expiration_timer->FireNow();
    }
  }

  for (auto i_outer = origin_tracker_.begin(), e_outer = origin_tracker_.end();
       i_outer != e_outer;) {
    auto origin_entry = i_outer++;
    for (auto
             i_inner =
                 origin_entry->second
                     .content_setting_specific_expiration_timer_map.begin(),
             e_inner = origin_entry->second
                           .content_setting_specific_expiration_timer_map.end();
         i_inner != e_inner;) {
      auto timer_entry = i_inner++;
      if (timer_entry->second->IsRunning()) {
        timer_entry->second->FireNow();
      }
    }
  }
}

bool OneTimePermissionsTracker::ShouldIgnoreOrigin(const url::Origin& origin) {
  // There are cases where chrome://newtab/ and chrome://new-tab-page/ are
  // used synonymously causing inconsistencies in the map. So we just ignore
  // them.
  return origin.opaque() ||
         origin == url::Origin::Create(GURL("chrome://newtab/")) ||
         origin == url::Origin::Create(GURL("chrome://new-tab-page/"));
}

void OneTimePermissionsTracker::NotifyBackgroundTimerExpired(
    const url::Origin& origin) {
  for (auto& observer : observer_list_) {
    observer.OnAllTabsInBackgroundTimerExpired(origin);
  }
  origin_tracker_[origin].background_expiration_timer->AbandonAndStop();
}

void OneTimePermissionsTracker::NotifyCapturingVideoExpired(
    const url::Origin& origin) {
  for (auto& observer : observer_list_) {
    observer.OnCapturingVideoExpired(origin);
  }
}

void OneTimePermissionsTracker::NotifyCapturingAudioExpired(
    const url::Origin& origin) {
  for (auto& observer : observer_list_) {
    observer.OnCapturingAudioExpired(origin);
  }
}

bool OneTimePermissionsTracker::AreAllTabsToOriginBackgroundedOrDiscarded(
    const url::Origin& origin) {
  return origin_tracker_[origin].background_tab_counter ==
         origin_tracker_[origin].undiscarded_tab_counter;
}
