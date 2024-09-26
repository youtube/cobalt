// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_state/chrome_browser_state_removal_controller.h"

#import <Foundation/Foundation.h>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "components/prefs/pref_service.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/browser_state/constants.h"
#import "ios/chrome/browser/paths/paths_internal.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/system_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

ChromeBrowserStateRemovalController* g_chrome_browser_state_removal_helper =
    nullptr;

NSString* const kPathToBrowserStateToKeepKey = @"PathToBrowserStateToKeep";
NSString* const kHasBrowserStateBeenRemovedKey = @"HasBrowserStateBeenRemoved";

const char kGmailDomain[] = "gmail.com";

// Removes from disk the directories used by the browser states in
// `browser_states_paths`.
void NukeBrowserStates(const std::vector<base::FilePath>& browser_states_path) {
  for (const base::FilePath& browser_state_path : browser_states_path) {
    // Delete both the browser state directory and its corresponding cache.
    base::FilePath cache_path;
    ios::GetUserCacheDirectory(browser_state_path, &cache_path);
    base::DeletePathRecursively(browser_state_path);
    base::DeletePathRecursively(cache_path);
  }
}

// Returns the GAIA Id of the given `browser_state_path` using the `info_cache`.
std::string GetGaiaIdForBrowserState(const std::string& browser_state_path,
                                     BrowserStateInfoCache* info_cache) {
  base::FilePath path = info_cache->GetUserDataDir().Append(browser_state_path);
  size_t index = info_cache->GetIndexOfBrowserStateWithPath(path);
  if (index > info_cache->GetNumberOfBrowserStates())
    return std::string();
  return info_cache->GetGAIAIdOfBrowserStateAtIndex(index);
}

// Returns the email's domain of the identity associated with `gaia_id`.
std::string GetDomainForGaiaId(ChromeBrowserState* browser_state,
                               const std::string& gaia_id) {
  ChromeAccountManagerService* account_manager_service =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browser_state);
  id<SystemIdentity> identity =
      account_manager_service->GetIdentityWithGaiaID(gaia_id);

  NSString* user_email = identity.userEmail;
  if (!user_email.length)
    return std::string();

  return gaia::ExtractDomainName(
      gaia::SanitizeEmail(base::SysNSStringToUTF8(user_email)));
}
}

ChromeBrowserStateRemovalController::ChromeBrowserStateRemovalController()
    : has_changed_last_used_browser_state_(false) {}

ChromeBrowserStateRemovalController*
ChromeBrowserStateRemovalController::GetInstance() {
  if (!g_chrome_browser_state_removal_helper) {
    g_chrome_browser_state_removal_helper =
        new ChromeBrowserStateRemovalController();
  }
  return g_chrome_browser_state_removal_helper;
}

void ChromeBrowserStateRemovalController::RemoveBrowserStatesIfNecessary() {
  ApplicationContext* application_context = GetApplicationContext();
  DCHECK(application_context);
  ios::ChromeBrowserStateManager* manager =
      application_context->GetChromeBrowserStateManager();
  DCHECK(manager);
  BrowserStateInfoCache* info_cache = manager->GetBrowserStateInfoCache();
  DCHECK(info_cache);

  std::string browser_state_to_keep = GetBrowserStatePathToKeep();
  std::string browser_state_last_used = GetLastBrowserStatePathUsed();
  if (browser_state_to_keep.empty()) {
    // If no browser state was marked as to keep, keep the last used one.
    browser_state_to_keep = browser_state_last_used;
  }
  if (browser_state_to_keep.empty()) {
    browser_state_to_keep = kIOSChromeInitialBrowserState;
  }
  if (browser_state_to_keep != browser_state_last_used) {
    std::string gaia_id =
        GetGaiaIdForBrowserState(browser_state_to_keep, info_cache);
    std::string last_used_gaia_id =
        GetGaiaIdForBrowserState(browser_state_last_used, info_cache);
    std::string last_used_domain = GetDomainForGaiaId(
        manager->GetLastUsedBrowserState(), last_used_gaia_id);
    if (gaia_id.empty() && last_used_domain == kGmailDomain) {
      // If browser state to keep is not the last used one, wasn't
      // authenticated, and the last used browser state was a normal account
      // (domain starts with "gmail"), keep the last used browser state instead.
      browser_state_to_keep = browser_state_last_used;
    }
  }

  bool is_removing_browser_states = false;
  std::vector<base::FilePath> browser_states_to_nuke;
  for (size_t index = 0; index < info_cache->GetNumberOfBrowserStates();
       ++index) {
    base::FilePath path = info_cache->GetPathOfBrowserStateAtIndex(index);
    if (path.BaseName().MaybeAsASCII() == browser_state_to_keep) {
      continue;
    }
    is_removing_browser_states = true;
    // Note: if there is more than 2 browser states (which should never be the
    // case), this might show the wrong GAIA Id. However, in this unlikely case,
    // there isn't really more that can be done.
    removed_browser_state_gaia_id_ =
        info_cache->GetGAIAIdOfBrowserStateAtIndex(index);
    browser_states_to_nuke.push_back(path);
    info_cache->RemoveBrowserState(path);
  }

  // Update the last used browser state if the old one was removed.
  if (browser_state_to_keep != browser_state_last_used) {
    has_changed_last_used_browser_state_ = true;
    SetLastBrowserStatePathUsed(browser_state_to_keep);
  }

  if (is_removing_browser_states) {
    SetHasBrowserStateBeenRemoved(true);
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&NukeBrowserStates, browser_states_to_nuke));
  }
}

bool ChromeBrowserStateRemovalController::HasBrowserStateBeenRemoved() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kHasBrowserStateBeenRemovedKey];
}

void ChromeBrowserStateRemovalController::SetHasBrowserStateBeenRemoved(
    bool value) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:value forKey:kHasBrowserStateBeenRemovedKey];
  [defaults synchronize];
}

std::string ChromeBrowserStateRemovalController::GetBrowserStatePathToKeep() {
  return base::SysNSStringToUTF8([[NSUserDefaults standardUserDefaults]
      stringForKey:kPathToBrowserStateToKeepKey]);
}

std::string ChromeBrowserStateRemovalController::GetLastBrowserStatePathUsed() {
  return GetApplicationContext()->GetLocalState()->GetString(
      prefs::kBrowserStateLastUsed);
}

void ChromeBrowserStateRemovalController::SetLastBrowserStatePathUsed(
    const std::string& browser_state_path) {
  GetApplicationContext()->GetLocalState()->SetString(
      prefs::kBrowserStateLastUsed, browser_state_path);
}
