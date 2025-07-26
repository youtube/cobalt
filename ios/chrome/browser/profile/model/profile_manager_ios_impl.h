// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_MANAGER_IOS_IMPL_H_
#define IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_MANAGER_IOS_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "ios/chrome/browser/shared/model/profile/mutable_profile_attributes_storage_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"

class PrefService;

// ProfileManagerIOS implementation.
class ProfileManagerIOSImpl : public ProfileManagerIOS,
                              public ProfileIOS::Delegate {
 public:
  // Constructs the ProfileManagerIOSImpl with a pointer to the local state's
  // PrefService and with the path to the directory containing the Profiles'
  // data.
  ProfileManagerIOSImpl(PrefService* local_state,
                        const base::FilePath& data_dir);

  ProfileManagerIOSImpl(const ProfileManagerIOSImpl&) = delete;
  ProfileManagerIOSImpl& operator=(const ProfileManagerIOSImpl&) = delete;

  ~ProfileManagerIOSImpl() override;

  // ProfileManagerIOS:
  void AddObserver(ProfileManagerObserverIOS* observer) override;
  void RemoveObserver(ProfileManagerObserverIOS* observer) override;
  ProfileIOS* GetProfileWithName(std::string_view name) override;
  std::vector<ProfileIOS*> GetLoadedProfiles() const override;
  bool HasProfileWithName(std::string_view name) const override;
  bool CanCreateProfileWithName(std::string_view name) const override;
  std::string ReserveNewProfileName() override;
  bool CanDeleteProfileWithName(std::string_view name) const override;
  bool LoadProfileAsync(std::string_view name,
                        ProfileLoadedCallback initialized_callback,
                        ProfileLoadedCallback created_callback) override;
  bool CreateProfileAsync(std::string_view name,
                          ProfileLoadedCallback initialized_callback,
                          ProfileLoadedCallback created_callback) override;
  ProfileIOS* LoadProfile(std::string_view name) override;
  ProfileIOS* CreateProfile(std::string_view name) override;
  void UnloadProfile(std::string_view name) override;
  void UnloadAllProfiles() override;
  void MarkProfileForDeletion(std::string_view name) override;
  bool IsProfileMarkedForDeletion(std::string_view name) const override;
  ProfileAttributesStorageIOS* GetProfileAttributesStorage() override;

  // ProfileIOS::Delegate:
  void OnProfileCreationStarted(
      ProfileIOS* profile,
      ProfileIOS::CreationMode creation_mode) override;
  void OnProfileCreationFinished(ProfileIOS* profile,
                                 ProfileIOS::CreationMode creation_mode,
                                 bool is_new_profile,
                                 bool success) override;

 private:
  class ProfileInfo;

  using CreationMode = ProfileIOS::CreationMode;
  using ProfileMap = std::map<std::string, ProfileInfo, std::less<>>;

  // Creates or loads the Profile known by `name` using the `creation_mode`.
  // The callbacks have the same meaning as the method CreateProfileAsync().
  // Returns whether a Profile with that name already exists or it can be
  // created. If `load_only_do_not_create` is true, then the method will fail
  // if the profile does not exists on disk yet.
  bool CreateProfileWithMode(std::string_view name,
                             CreationMode creation_mode,
                             bool load_only_do_not_create,
                             ProfileLoadedCallback initialized_callback,
                             ProfileLoadedCallback created_callback);

  // Final initialization of the profile.
  void DoFinalInit(ProfileIOS* profile);
  void DoFinalInitForServices(ProfileIOS* profile);

  SEQUENCE_CHECKER(sequence_checker_);

  // The PrefService storing the local state.
  raw_ptr<PrefService> local_state_;

  // The path to the directory where the Profiles' data are stored.
  const base::FilePath profile_data_dir_;

  // Holds the Profile instances that this instance has created.
  ProfileMap profiles_map_;

  // The owned ProfileAttributesStorageIOS instance.
  MutableProfileAttributesStorageIOS profile_attributes_storage_;

  // The list of registered observers.
  base::ObserverList<ProfileManagerObserverIOS, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_MANAGER_IOS_IMPL_H_
