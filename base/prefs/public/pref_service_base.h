// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the base interface for a preference services that provides
// a way to access the application's current preferences.
//
// This base interface assumes all preferences are local.  See
// SyncablePrefServiceBase for the interface to a preference service
// that stores preferences that can be synced.
//
// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef BASE_PREFS_PUBLIC_PREF_SERVICE_BASE_H_
#define BASE_PREFS_PUBLIC_PREF_SERVICE_BASE_H_

#include "base/values.h"

namespace content {
class BrowserContext;
class NotificationObserver;
}

namespace subtle {
class PrefMemberBase;
}

class FilePath;
class Profile;
class TabContents;

class PrefServiceBase {
 public:
  // Retrieves a PrefServiceBase for the given context.
  static PrefServiceBase* FromBrowserContext(content::BrowserContext* context);

  virtual ~PrefServiceBase() {}

  // Enum used when registering preferences to determine if it should be synced
  // or not. This is only used for profile prefs, not local state prefs.
  // See the Register*Pref methods for profile prefs below.
  enum PrefSyncStatus {
    UNSYNCABLE_PREF,
    SYNCABLE_PREF
  };

  // Interface to a single preference.
  class Preference {
   public:
    virtual ~Preference() {}

    // Returns the name of the Preference (i.e., the key, e.g.,
    // browser.window_placement).
    virtual const std::string name() const = 0;

    // Returns the registered type of the preference.
    virtual base::Value::Type GetType() const = 0;

    // Returns the value of the Preference, falling back to the registered
    // default value if no other has been set.
    virtual const base::Value* GetValue() const = 0;

    // Returns the value recommended by the admin, if any.
    virtual const base::Value* GetRecommendedValue() const = 0;

    // Returns true if the Preference is managed, i.e. set by an admin policy.
    // Since managed prefs have the highest priority, this also indicates
    // whether the pref is actually being controlled by the policy setting.
    virtual bool IsManaged() const = 0;

    // Returns true if the Preference is recommended, i.e. set by an admin
    // policy but the user is allowed to change it.
    virtual bool IsRecommended() const = 0;

    // Returns true if the Preference has a value set by an extension, even if
    // that value is being overridden by a higher-priority source.
    virtual bool HasExtensionSetting() const = 0;

    // Returns true if the Preference has a user setting, even if that value is
    // being overridden by a higher-priority source.
    virtual bool HasUserSetting() const = 0;

    // Returns true if the Preference value is currently being controlled by an
    // extension, and not by any higher-priority source.
    virtual bool IsExtensionControlled() const = 0;

    // Returns true if the Preference value is currently being controlled by a
    // user setting, and not by any higher-priority source.
    virtual bool IsUserControlled() const = 0;

    // Returns true if the Preference is currently using its default value,
    // and has not been set by any higher-priority source (even with the same
    // value).
    virtual bool IsDefaultValue() const = 0;

    // Returns true if the user can change the Preference value, which is the
    // case if no higher-priority source than the user store controls the
    // Preference.
    virtual bool IsUserModifiable() const = 0;

    // Returns true if an extension can change the Preference value, which is
    // the case if no higher-priority source than the extension store controls
    // the Preference.
    virtual bool IsExtensionModifiable() const = 0;
  };

  // Returns true if the preference for the given preference name is available
  // and is managed.
  virtual bool IsManagedPreference(const char* pref_name) const = 0;

  // Returns |true| if a preference with the given name is available and its
  // value can be changed by the user.
  virtual bool IsUserModifiablePreference(const char* pref_name) const = 0;

  // Make the PrefService aware of a pref.
  // TODO(zea): split local state and profile prefs into their own subclasses.
  // ---------- Local state prefs  ----------
  virtual void RegisterBooleanPref(const char* path,
                                   bool default_value) = 0;
  virtual void RegisterIntegerPref(const char* path,
                                   int default_value) = 0;
  virtual void RegisterDoublePref(const char* path,
                                  double default_value) = 0;
  virtual void RegisterStringPref(const char* path,
                                  const std::string& default_value) = 0;
  virtual void RegisterFilePathPref(const char* path,
                                    const FilePath& default_value) = 0;
  virtual void RegisterListPref(const char* path) = 0;
  virtual void RegisterDictionaryPref(const char* path) = 0;
  // These take ownership of the default_value:
  virtual void RegisterListPref(const char* path,
                                base::ListValue* default_value) = 0;
  virtual void RegisterDictionaryPref(
      const char* path, base::DictionaryValue* default_value) = 0;
  // These variants use a default value from the locale dll instead.
  virtual void RegisterLocalizedBooleanPref(
      const char* path, int locale_default_message_id) = 0;
  virtual void RegisterLocalizedIntegerPref(
      const char* path, int locale_default_message_id) = 0;
  virtual void RegisterLocalizedDoublePref(
      const char* path, int locale_default_message_id) = 0;
  virtual void RegisterLocalizedStringPref(
      const char* path, int locale_default_message_id) = 0;
  virtual void RegisterInt64Pref(const char* path,
                                 int64 default_value) = 0;

  //  ---------- Profile prefs  ----------
  // Profile prefs must specify whether the pref should be synchronized across
  // machines or not (see PrefSyncStatus enum above).
  virtual void RegisterBooleanPref(const char* path,
                                   bool default_value,
                                   PrefSyncStatus sync_status) = 0;
  virtual void RegisterIntegerPref(const char* path,
                                   int default_value,
                                   PrefSyncStatus sync_status) = 0;
  virtual void RegisterDoublePref(const char* path,
                                  double default_value,
                                  PrefSyncStatus sync_status) = 0;
  virtual void RegisterStringPref(const char* path,
                                  const std::string& default_value,
                                  PrefSyncStatus sync_status) = 0;
  virtual void RegisterFilePathPref(const char* path,
                                    const FilePath& default_value,
                                    PrefSyncStatus sync_status) = 0;
  virtual void RegisterListPref(const char* path,
                                PrefSyncStatus sync_status) = 0;
  virtual void RegisterDictionaryPref(const char* path,
                                      PrefSyncStatus sync_status) = 0;
  // These take ownership of the default_value:
  virtual void RegisterListPref(const char* path,
                                base::ListValue* default_value,
                                PrefSyncStatus sync_status) = 0;
  virtual void RegisterDictionaryPref(const char* path,
                                      base::DictionaryValue* default_value,
                                      PrefSyncStatus sync_status) = 0;
  // These variants use a default value from the locale dll instead.
  virtual void RegisterLocalizedBooleanPref(
      const char* path,
      int locale_default_message_id,
      PrefSyncStatus sync_status) = 0;
  virtual void RegisterLocalizedIntegerPref(
      const char* path,
      int locale_default_message_id,
      PrefSyncStatus sync_status) = 0;
  virtual void RegisterLocalizedDoublePref(
      const char* path,
      int locale_default_message_id,
      PrefSyncStatus sync_status) = 0;
  virtual void RegisterLocalizedStringPref(
      const char* path,
      int locale_default_message_id,
      PrefSyncStatus sync_status) = 0;
  virtual void RegisterInt64Pref(const char* path,
                                 int64 default_value,
                                 PrefSyncStatus sync_status) = 0;
  virtual void RegisterUint64Pref(const char* path,
                                  uint64 default_value,
                                  PrefSyncStatus sync_status) = 0;
  // Unregisters a preference.
  virtual void UnregisterPreference(const char* path) = 0;

  // Look up a preference.  Returns NULL if the preference is not
  // registered.
  virtual const Preference* FindPreference(const char* pref_name) const = 0;

  // If the path is valid and the value at the end of the path matches the type
  // specified, it will return the specified value.  Otherwise, the default
  // value (set when the pref was registered) will be returned.
  virtual bool GetBoolean(const char* path) const = 0;
  virtual int GetInteger(const char* path) const = 0;
  virtual double GetDouble(const char* path) const = 0;
  virtual std::string GetString(const char* path) const = 0;
  virtual FilePath GetFilePath(const char* path) const = 0;

  // Returns the branch if it exists, or the registered default value otherwise.
  // Note that |path| must point to a registered preference. In that case, these
  // functions will never return NULL.
  virtual const base::DictionaryValue* GetDictionary(
      const char* path) const = 0;
  virtual const base::ListValue* GetList(const char* path) const = 0;

  // Removes a user pref and restores the pref to its default value.
  virtual void ClearPref(const char* path) = 0;

  // If the path is valid (i.e., registered), update the pref value in the user
  // prefs.
  // To set the value of dictionary or list values in the pref tree use
  // Set(), but to modify the value of a dictionary or list use either
  // ListPrefUpdate or DictionaryPrefUpdate from scoped_user_pref_update.h.
  virtual void Set(const char* path, const base::Value& value) = 0;
  virtual void SetBoolean(const char* path, bool value) = 0;
  virtual void SetInteger(const char* path, int value) = 0;
  virtual void SetDouble(const char* path, double value) = 0;
  virtual void SetString(const char* path, const std::string& value) = 0;
  virtual void SetFilePath(const char* path, const FilePath& value) = 0;

  // Int64 helper methods that actually store the given value as a string.
  // Note that if obtaining the named value via GetDictionary or GetList, the
  // Value type will be TYPE_STRING.
  virtual void SetInt64(const char* path, int64 value) = 0;
  virtual int64 GetInt64(const char* path) const = 0;

  // As above, but for unsigned values.
  virtual void SetUint64(const char* path, uint64 value) = 0;
  virtual uint64 GetUint64(const char* path) const = 0;

 protected:
  // Registration of pref change observers must be done using the
  // PrefChangeRegistrar, which is declared as a friend here to grant it
  // access to the otherwise protected members Add/RemovePrefObserver.
  // PrefMember registers for preferences changes notification directly to
  // avoid the storage overhead of the registrar, so its base class must be
  // declared as a friend, too.
  friend class PrefChangeRegistrar;
  friend class subtle::PrefMemberBase;

  // These are protected so they can only be accessed by the friend
  // classes listed above.
  //
  // If the pref at the given path changes, we call the observer's Observe
  // method with PREF_CHANGED. Note that observers should not call these methods
  // directly but rather use a PrefChangeRegistrar to make sure the observer
  // gets cleaned up properly.
  virtual void AddPrefObserver(const char* path,
                               content::NotificationObserver* obs) = 0;
  virtual void RemovePrefObserver(const char* path,
                                  content::NotificationObserver* obs) = 0;
};

#endif  // BASE_PREFS_PUBLIC_PREF_SERVICE_BASE_H_
