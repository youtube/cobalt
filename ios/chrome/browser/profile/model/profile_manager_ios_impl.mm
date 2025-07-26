// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/profile_manager_ios_impl.h"

#import <stdint.h>

#import <utility>

#import "base/check.h"
#import "base/check_deref.h"
#import "base/feature_list.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "base/uuid.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/profile/model/off_the_record_profile_ios_impl.h"
#import "ios/chrome/browser/profile/model/profile_ios_impl.h"
#import "ios/chrome/browser/profile_metrics/model/profile_metrics.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/child_account_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/list_family_members_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"

namespace {

int64_t ComputeFilesSize(const base::FilePath& directory,
                         const base::FilePath::StringType& pattern) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int64_t running_size = 0;
  base::FileEnumerator iter(directory, false, base::FileEnumerator::FILES,
                            pattern);
  while (!iter.Next().empty()) {
    running_size += iter.GetInfo().GetSize();
  }
  return running_size;
}

// Simple task to log the size of the profile at `path`.
void RecordProfileSizeTask(const base::FilePath& path) {
  const int64_t kBytesInOneMB = 1024 * 1024;

  int64_t size = ComputeFilesSize(path, FILE_PATH_LITERAL("*"));
  int size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.TotalSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.HistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.TotalHistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Cookies"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.CookiesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Bookmarks"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.BookmarksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Favicons"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.FaviconsSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Top Sites"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.TopSitesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Visited Links"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.VisitedLinksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Web Data"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.WebDataSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Extension*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  base::UmaHistogramCounts10000("Profile.ExtensionSize", size_MB);
}

}  // namespace

// Stores information about a single Profile.
class ProfileManagerIOSImpl::ProfileInfo {
 public:
  explicit ProfileInfo(std::unique_ptr<ProfileIOS> profile)
      : profile_(std::move(profile)) {
    DCHECK(profile_);
  }

  ProfileInfo(ProfileInfo&&) = default;
  ProfileInfo& operator=(ProfileInfo&&) = default;

  ~ProfileInfo() = default;

  ProfileIOS* profile() const { return profile_.get(); }

  bool is_loaded() const { return is_loaded_; }

  void SetIsLoaded();

  void AddCallback(ProfileLoadedCallback callback);

  std::vector<ProfileLoadedCallback> TakeCallbacks() {
    return std::exchange(callbacks_, {});
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<ProfileIOS> profile_;
  std::vector<ProfileLoadedCallback> callbacks_;
  bool is_loaded_ = false;
};

void ProfileManagerIOSImpl::ProfileInfo::SetIsLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_loaded_);
  is_loaded_ = true;
}

void ProfileManagerIOSImpl::ProfileInfo::AddCallback(
    ProfileLoadedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_loaded_);
  if (!callback.is_null()) {
    callbacks_.push_back(std::move(callback));
  }
}

ProfileManagerIOSImpl::ProfileManagerIOSImpl(PrefService* local_state,
                                             const base::FilePath& data_dir)
    : local_state_(local_state),
      profile_data_dir_(data_dir),
      profile_attributes_storage_(local_state) {
  CHECK(local_state_);
  CHECK(!profile_data_dir_.empty());
}

ProfileManagerIOSImpl::~ProfileManagerIOSImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_) {
    observer.OnProfileManagerDestroyed(this);
  }
}

void ProfileManagerIOSImpl::AddObserver(ProfileManagerObserverIOS* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);

  // Notify the observer of any pre-existing Profiles.
  for (auto& [name, profile_info] : profiles_map_) {
    if (IsProfileMarkedForDeletion(name)) {
      continue;
    }

    ProfileIOS* profile = profile_info.profile();
    DCHECK(profile);

    observer->OnProfileCreated(this, profile);
    if (profile_info.is_loaded()) {
      observer->OnProfileLoaded(this, profile);
    }
  }
}

void ProfileManagerIOSImpl::RemoveObserver(
    ProfileManagerObserverIOS* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

ProfileIOS* ProfileManagerIOSImpl::GetProfileWithName(std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Do not give access to profiles marked for deletion.
  if (IsProfileMarkedForDeletion(name)) {
    return nullptr;
  }
  // If the profile is already loaded, just return it.
  auto iter = profiles_map_.find(name);
  if (iter != profiles_map_.end()) {
    ProfileInfo& profile_info = iter->second;
    if (profile_info.is_loaded()) {
      DCHECK(profile_info.profile());
      return profile_info.profile();
    }
  }

  return nullptr;
}

std::vector<ProfileIOS*> ProfileManagerIOSImpl::GetLoadedProfiles() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<ProfileIOS*> loaded_profiles;
  for (const auto& [name, profile_info] : profiles_map_) {
    if (IsProfileMarkedForDeletion(name)) {
      continue;
    }

    if (profile_info.is_loaded()) {
      DCHECK(profile_info.profile());
      loaded_profiles.push_back(profile_info.profile());
    }
  }
  return loaded_profiles;
}

bool ProfileManagerIOSImpl::HasProfileWithName(std::string_view name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return profile_attributes_storage_.HasProfileWithName(name);
}

bool ProfileManagerIOSImpl::CanCreateProfileWithName(
    std::string_view name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cannot create a profile with the same name as an existing profile.
  if (HasProfileWithName(name)) {
    return false;
  }

  // Cannot create a profile with the same name as a legacy profile.
  if (local_state_->GetDict(prefs::kLegacyProfileMap).Find(name)) {
    return false;
  }

  // Cannot create a profile that have been marked for deletion, and currently
  // not fully deleted yet.
  if (IsProfileMarkedForDeletion(name)) {
    return false;
  }

  return true;
}

std::string ProfileManagerIOSImpl::ReserveNewProfileName() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string profile_name;
  do {
    const base::Uuid uuid = base::Uuid::GenerateRandomV4();
    profile_name = uuid.AsLowercaseString();
  } while (!CanCreateProfileWithName(profile_name));

  DCHECK(!profile_name.empty());
  DCHECK(!HasProfileWithName(profile_name));
  profile_attributes_storage_.AddProfile(profile_name);

  return profile_name;
}

bool ProfileManagerIOSImpl::CanDeleteProfileWithName(
    std::string_view name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!HasProfileWithName(name)) {
    return false;
  }
  // Cannot delete the personal profile.
  if (name == profile_attributes_storage_.GetPersonalProfileName()) {
    return false;
  }
  return true;
}

bool ProfileManagerIOSImpl::LoadProfileAsync(
    std::string_view name,
    ProfileLoadedCallback initialized_callback,
    ProfileLoadedCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CreateProfileWithMode(name, CreationMode::kAsynchronous,
                               /* load_only_do_not_create */ true,
                               std::move(initialized_callback),
                               std::move(created_callback));
}

bool ProfileManagerIOSImpl::CreateProfileAsync(
    std::string_view name,
    ProfileLoadedCallback initialized_callback,
    ProfileLoadedCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CreateProfileWithMode(name, CreationMode::kAsynchronous,
                               /* load_only_do_not_create */ false,
                               std::move(initialized_callback),
                               std::move(created_callback));
}

ProfileIOS* ProfileManagerIOSImpl::LoadProfile(std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CreateProfileWithMode(name, CreationMode::kSynchronous,
                             /* load_only_do_not_create */ true,
                             /* initialized_callback */ {},
                             /* created_callback */ {})) {
    return nullptr;
  }

  auto iter = profiles_map_.find(name);
  DCHECK(iter != profiles_map_.end());

  DCHECK(iter->second.is_loaded());
  return iter->second.profile();
}

ProfileIOS* ProfileManagerIOSImpl::CreateProfile(std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!CreateProfileWithMode(name, CreationMode::kSynchronous,
                             /* load_only_do_not_create */ false,
                             /* initialized_callback */ {},
                             /* created_callback */ {})) {
    return nullptr;
  }

  auto iter = profiles_map_.find(name);
  DCHECK(iter != profiles_map_.end());

  DCHECK(iter->second.is_loaded());
  return iter->second.profile();
}

void ProfileManagerIOSImpl::UnloadProfile(std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the profile is not loaded, nor loading, return.
  auto iter = profiles_map_.find(name);
  if (iter == profiles_map_.end()) {
    return;
  }

  ProfileInfo info = std::move(iter->second);
  profiles_map_.erase(iter);

  // If profile is loaded, notify all observers that it is unloaded.
  if (info.is_loaded()) {
    ProfileIOS* profile = info.profile();
    for (auto& observer : observers_) {
      observer.OnProfileUnloaded(this, profile);
    }
    return;
  }

  // If the profile is still loading, pretend that the loading failed
  // by calling the ProfileLoadedCallbacks with nullptr.
  for (auto& callback : info.TakeCallbacks()) {
    std::move(callback).Run(nullptr);
  }
}

void ProfileManagerIOSImpl::UnloadAllProfiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (!profiles_map_.empty()) {
    const std::string& name = profiles_map_.begin()->first;
    UnloadProfile(name);
  }
}

void ProfileManagerIOSImpl::MarkProfileForDeletion(std::string_view name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(CanDeleteProfileWithName(name));

  // Remove the profile from the ProfileAttributesStorageIOS to prevent
  // people iterating over all profiles from seeing it anymore.
  profile_attributes_storage_.MarkProfileForDeletion(name);

  // If the profile is not loaded, nor loading, return.
  auto iter = profiles_map_.find(name);
  if (iter == profiles_map_.end()) {
    return;
  }

  // If profile is loaded, notify all observers that it is marked for
  // deletion.
  ProfileInfo& info = iter->second;
  if (info.is_loaded()) {
    ProfileIOS* profile = info.profile();
    DCHECK(profile);

    for (auto& observer : observers_) {
      observer.OnProfileMarkedForPermanentDeletion(this, profile);
    }
    return;
  }

  // If the profile is still loading, pretend that the loading failed
  // by calling the ProfileLoadedCallbacks with nullptr.
  for (auto& callback : info.TakeCallbacks()) {
    std::move(callback).Run(nullptr);
  }
}

bool ProfileManagerIOSImpl::IsProfileMarkedForDeletion(
    std::string_view name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return profile_attributes_storage_.IsProfileMarkedForDeletion(name);
}

ProfileAttributesStorageIOS*
ProfileManagerIOSImpl::GetProfileAttributesStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return &profile_attributes_storage_;
}

void ProfileManagerIOSImpl::OnProfileCreationStarted(
    ProfileIOS* profile,
    CreationMode creation_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile);

  for (auto& observer : observers_) {
    observer.OnProfileCreated(this, profile);
  }
}

void ProfileManagerIOSImpl::OnProfileCreationFinished(
    ProfileIOS* profile,
    CreationMode creation_mode,
    bool is_new_profile,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile);
  DCHECK(!profile->IsOffTheRecord());

  // If the Profile is loaded synchronously the method is called as part of the
  // constructor and before the ProfileInfo insertion in the map. The method
  // will be called again after the insertion.
  auto iter = profiles_map_.find(profile->GetProfileName());
  if (iter == profiles_map_.end()) {
    DCHECK(creation_mode == CreationMode::kSynchronous);
    return;
  }

  DCHECK(iter != profiles_map_.end());
  auto callbacks = iter->second.TakeCallbacks();

  // Update the ProfileAttributesStorageIOS before notifying the observers
  // and callbacks of the success or failure of the operation.
  const std::string& name = profile->GetProfileName();
  if (is_new_profile) {
    if (success) {
      profile_attributes_storage_.UpdateAttributesForProfileWithName(
          name, base::BindOnce([](ProfileAttributesIOS& attrs) {
            attrs.ClearIsNewProfile();
          }));
    } else {
      MarkProfileForDeletion(name);
    }
  }

  if (success) {
    DoFinalInit(profile);
    iter->second.SetIsLoaded();
  } else {
    profile = nullptr;
    profiles_map_.erase(iter);
  }

  // Invoke the callbacks, if the load failed, `profile` will be null.
  for (auto& callback : callbacks) {
    std::move(callback).Run(profile);
  }

  // Notify the observers after invoking the callbacks in case of success.
  if (success) {
    DCHECK(profile);
    for (auto& observer : observers_) {
      observer.OnProfileLoaded(this, profile);
    }
  }
}

bool ProfileManagerIOSImpl::CreateProfileWithMode(
    std::string_view name,
    CreationMode creation_mode,
    bool load_only_do_not_create,
    ProfileLoadedCallback initialized_callback,
    ProfileLoadedCallback created_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool inserted = false;
  bool existing = HasProfileWithName(name);

  // Profile creation is forbiden for profiles that have been marked for
  // deletion. Fail even if the profile is already loaded, to avoid new
  // usages after the deletion.
  if (IsProfileMarkedForDeletion(name)) {
    if (!initialized_callback.is_null()) {
      std::move(initialized_callback).Run(nullptr);
    }
    return false;
  }

  // As the name may have been registered with ProfileAttributesStorageIOS,
  // a profile is considered as a new profile if the storage does not know
  // about it, or if the IsNewProfile() flag is still set. The flag will be
  // cleared the first time the profile is successfully loaded.
  bool is_new_profile = !existing;
  if (existing) {
    const ProfileAttributesIOS attrs =
        profile_attributes_storage_.GetAttributesForProfileWithName(name);
    is_new_profile = attrs.IsNewProfile();
  }

  // Profile creation is forbidden either via `load_only_do_not_create` or
  // if CanCreateProfileWithName(...) return false.
  bool can_create = !load_only_do_not_create;
  if (!existing) {
    can_create &= CanCreateProfileWithName(name);
  }

  auto iter = profiles_map_.find(name);
  if (iter == profiles_map_.end()) {
    if (is_new_profile && !can_create) {
      if (!initialized_callback.is_null()) {
        std::move(initialized_callback).Run(nullptr);
      }
      return false;
    }

    if (!existing) {
      profile_attributes_storage_.AddProfile(name);
      DCHECK(HasProfileWithName(name));
    }

    // If this is the first profile ever loaded, mark it as the personal
    // profile.
    if (profile_attributes_storage_.GetPersonalProfileName().empty()) {
      profile_attributes_storage_.SetPersonalProfileName(name);
    }

    std::tie(iter, inserted) = profiles_map_.insert(std::make_pair(
        std::string(name),
        ProfileInfo(ProfileIOS::CreateProfile(profile_data_dir_.Append(name),
                                              name, creation_mode, this))));

    DCHECK(inserted);
  }

  DCHECK(iter != profiles_map_.end());
  ProfileInfo& profile_info = iter->second;
  DCHECK(profile_info.profile());

  if (!created_callback.is_null()) {
    std::move(created_callback).Run(profile_info.profile());
  }

  if (!initialized_callback.is_null()) {
    if (inserted || !profile_info.is_loaded()) {
      profile_info.AddCallback(std::move(initialized_callback));
    } else {
      std::move(initialized_callback).Run(profile_info.profile());
    }
  }

  // If asked to load synchronously but an asynchronous load was already in
  // progress, pretend the load failed, as we cannot return an uninitialized
  // Profile, nor can we wait for the asynchronous initialisation to complete.
  if (creation_mode == CreationMode::kSynchronous) {
    if (!inserted && !profile_info.is_loaded()) {
      return false;
    }
  }

  // If the Profile was just created, and the creation mode is synchronous then
  // OnProfileCreationFinished() will have been called during the construction
  // of the ProfileInfo. Thus it is necessary to call the method again here.
  if (inserted && creation_mode == CreationMode::kSynchronous) {
    OnProfileCreationFinished(profile_info.profile(),
                              CreationMode::kAsynchronous, is_new_profile,
                              /* success */ true);
  }

  return true;
}

void ProfileManagerIOSImpl::DoFinalInit(ProfileIOS* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DoFinalInitForServices(profile);

  // Log the profile size after a reasonable startup delay.
  DCHECK(!profile->IsOffTheRecord());
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&RecordProfileSizeTask, profile->GetStatePath()),
      base::Seconds(112));

  LogNumberOfProfiles(this);
}

void ProfileManagerIOSImpl::DoFinalInitForServices(ProfileIOS* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IdentityManagerFactory::GetForProfile(profile)->OnNetworkInitialized();

  // Those services needs to be explicitly initialized and can't simply be
  // marked as created with the profile as 1. they depend on initialisation
  // performed in ProfileIOSImpl (thus can't work with TestProfileIOS), and
  // 2. code do not expect them to be null (thus tests cannot be configured
  // to have a null instance).
  ChildAccountServiceFactory::GetForProfile(profile)->Init();
  SupervisedUserServiceFactory::GetForProfile(profile)->Init();
  ListFamilyMembersServiceFactory::GetForProfile(profile)->Init();
}
