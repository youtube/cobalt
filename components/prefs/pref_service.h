// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides a way to access the application's current preferences.

// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef COMPONENTS_PREFS_PREF_SERVICE_H_
#define COMPONENTS_PREFS_PREF_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_value_store.h"
#include "components/prefs/prefs_export.h"

class PrefNotifier;
class PrefNotifierImpl;
class PrefObserver;
class PrefRegistry;
class PrefStore;

namespace base {
class FilePath;
}

namespace prefs {
class ScopedDictionaryPrefUpdate;
}

namespace subtle {
class PrefMemberBase;
class ScopedUserPrefUpdateBase;
}

// Base class for PrefServices. You can use the base class to read and
// interact with preferences, but not to register new preferences; for
// that see e.g. PrefRegistrySimple.
//
// Settings and storage accessed through this class represent
// user-selected preferences and information and MUST not be
// extracted, overwritten or modified except through the defined APIs.
class COMPONENTS_PREFS_EXPORT PrefService {
 public:
  enum PrefInitializationStatus {
    INITIALIZATION_STATUS_WAITING,
    INITIALIZATION_STATUS_SUCCESS,
    INITIALIZATION_STATUS_CREATED_NEW_PREF_STORE,
    INITIALIZATION_STATUS_ERROR
  };

  enum IncludeDefaults {
    INCLUDE_DEFAULTS,
    EXCLUDE_DEFAULTS,
  };

  // A helper class to store all the information associated with a preference.
  class COMPONENTS_PREFS_EXPORT Preference {
   public:
    // The type of the preference is determined by the type with which it is
    // registered. This type needs to be a boolean, integer, double, string,
    // dictionary (a branch), or list.  You shouldn't need to construct this on
    // your own; use the PrefService::Register*Pref methods instead.
    Preference(const PrefService* service,
               std::string name,
               base::Value::Type type);
    ~Preference() {}

    // Returns the name of the Preference (i.e., the key, e.g.,
    // browser.window_placement).
    std::string name() const { return name_; }

    // Returns the registered type of the preference.
    base::Value::Type GetType() const { return type_; }

    // Returns the value of the Preference, falling back to the registered
    // default value if no other has been set.
    const base::Value* GetValue() const;

    // Returns the value recommended by the admin, if any.
    const base::Value* GetRecommendedValue() const;

    // Returns true if the Preference is managed, i.e. set by an admin policy.
    // Since managed prefs have the highest priority, this also indicates
    // whether the pref is actually being controlled by the policy setting.
    bool IsManaged() const;

    // Returns true if the Preference is controlled by the custodian of the
    // supervised user. Since a supervised user is not expected to have an admin
    // policy, this is the controlling pref if set.
    bool IsManagedByCustodian() const;

    // Returns true if the Preference's current value is one recommended by
    // admin policy. Note that this will be false if any other higher-priority
    // source overrides the value (e.g., the user has set a value).
    bool IsRecommended() const;

    // Returns true if the Preference has a value set by an extension, even if
    // that value is being overridden by a higher-priority source.
    bool HasExtensionSetting() const;

    // Returns true if the Preference has a user setting, even if that value is
    // being overridden by a higher-priority source.
    bool HasUserSetting() const;

    // Returns true if the Preference value is currently being controlled by an
    // extension, and not by any higher-priority source.
    bool IsExtensionControlled() const;

    // Returns true if the Preference value is currently being controlled by a
    // user setting, and not by any higher-priority source.
    bool IsUserControlled() const;

    // Returns true if the Preference is currently using its default value,
    // and has not been set by any higher-priority source (even with the same
    // value).
    bool IsDefaultValue() const;

    // Returns true if the user can change the Preference value, which is the
    // case if no higher-priority source than the user store controls the
    // Preference.
    bool IsUserModifiable() const;

    // Returns true if an extension can change the Preference value, which is
    // the case if no higher-priority source than the extension store controls
    // the Preference.
    bool IsExtensionModifiable() const;

    // Return the registration flags for this pref as a bitmask of
    // PrefRegistry::PrefRegistrationFlags.
    uint32_t registration_flags() const { return registration_flags_; }

   private:
    friend class PrefService;

    PrefValueStore* pref_value_store() const {
      return pref_service_->pref_value_store_.get();
    }

    const std::string name_;

    const base::Value::Type type_;

    const uint32_t registration_flags_;

    // Reference to the PrefService in which this pref was created.
    const PrefService* const pref_service_;
  };

  // You may wish to use PrefServiceFactory or one of its subclasses
  // for simplified construction.
  PrefService(std::unique_ptr<PrefNotifierImpl> pref_notifier,
              std::unique_ptr<PrefValueStore> pref_value_store,
              scoped_refptr<PersistentPrefStore> user_prefs,
              scoped_refptr<PrefRegistry> pref_registry,
              base::RepeatingCallback<void(PersistentPrefStore::PrefReadError)>
                  read_error_callback,
              bool async);
  virtual ~PrefService();

  // Lands pending writes to disk. This should only be used if we need to save
  // immediately (basically, during shutdown). |reply_callback| will be posted
  // to the current sequence when changes have been written.
  // |synchronous_done_callback| on the other hand will be invoked right away
  // wherever the writes complete (could even be invoked synchronously if no
  // writes need to occur); this is useful when the current thread cannot pump
  // messages to observe the reply (e.g. nested loops banned on main thread
  // during shutdown). |synchronous_done_callback| must be thread-safe.
  void CommitPendingWrite(
      base::OnceClosure reply_callback = base::OnceClosure(),
      base::OnceClosure synchronous_done_callback = base::OnceClosure());

  // Schedule a write if there is any lossy data pending. Unlike
  // CommitPendingWrite() this does not immediately sync to disk, instead it
  // triggers an eventual write if there is lossy data pending and if there
  // isn't one scheduled already.
  void SchedulePendingLossyWrites();

  // Returns true if the preference for the given preference name is available
  // and is managed.
  bool IsManagedPreference(const std::string& pref_name) const;

  // Returns true if the preference for the given preference name is available
  // and is controlled by the parent/guardian of the child Account.
  bool IsPreferenceManagedByCustodian(const std::string& pref_name) const;

  // Returns |true| if a preference with the given name is available and its
  // value can be changed by the user.
  bool IsUserModifiablePreference(const std::string& pref_name) const;

  // Look up a preference.  Returns NULL if the preference is not
  // registered.
  const PrefService::Preference* FindPreference(const std::string& path) const;

  // If the path is valid and the value at the end of the path matches the type
  // specified, it will return the specified value.  Otherwise, the default
  // value (set when the pref was registered) will be returned.
  bool GetBoolean(const std::string& path) const;
  int GetInteger(const std::string& path) const;
  double GetDouble(const std::string& path) const;
  std::string GetString(const std::string& path) const;
  base::FilePath GetFilePath(const std::string& path) const;

  // Returns the branch if it exists, or the registered default value otherwise.
  // Note that |path| must point to a registered preference. In that case, these
  // functions will never return NULL.
  const base::Value* Get(const std::string& path) const;
  const base::Value::Dict* GetDictionary(const std::string& path) const;
  const base::Value::List* GetList(const std::string& path) const;

  // Removes a user pref and restores the pref to its default value.
  void ClearPref(const std::string& path);

  // If the path is valid (i.e., registered), update the pref value in the user
  // prefs.
  // To set the value of dictionary or list values in the pref tree use
  // Set(), but to modify the value of a dictionary or list use either
  // ListPrefUpdate or DictionaryPrefUpdate from scoped_user_pref_update.h.
  void Set(const std::string& path, const base::Value& value);
  void SetList(const std::string& path, const base::Value::List& value);
  void SetDict(const std::string& path, const base::Value::Dict& value);
  void SetBoolean(const std::string& path, bool value);
  void SetInteger(const std::string& path, int value);
  void SetDouble(const std::string& path, double value);
  void SetString(const std::string& path, const std::string& value);
  void SetFilePath(const std::string& path, const base::FilePath& value);

  // Int64 helper methods that actually store the given value as a string.
  // Note that if obtaining the named value via GetDictionary or GetList, the
  // Value type will be Type::STRING.
  void SetInt64(const std::string& path, int64_t value);
  int64_t GetInt64(const std::string& path) const;

  // As above, but for unsigned values.
  void SetUint64(const std::string& path, uint64_t value);
  uint64_t GetUint64(const std::string& path) const;

  // Time helper methods that actually store the given value as a string, which
  // represents the number of microseconds elapsed (absolute for TimeDelta and
  // relative to Windows epoch for Time variants). Note that if obtaining the
  // named value via GetDictionary or GetList, the Value type will be
  // Type::STRING.
  void SetTime(const std::string& path, base::Time value);
  base::Time GetTime(const std::string& path) const;
  void SetTimeDelta(const std::string& path, base::TimeDelta value);
  base::TimeDelta GetTimeDelta(const std::string& path) const;

  // Returns the value of the given preference, from the user pref store. If
  // the preference is not set in the user pref store, returns NULL.
  const base::Value* GetUserPrefValue(const std::string& path) const;

  // Changes the default value for a preference.
  //
  // Will cause a pref change notification to be fired if this causes
  // the effective value to change.
  void SetDefaultPrefValue(const std::string& path, base::Value value);

  // Returns the default value of the given preference. |path| must point to a
  // registered preference. In that case, will never return nullptr, so callers
  // do not need to check this.
  const base::Value* GetDefaultPrefValue(const std::string& path) const;

  // Returns true if a value has been set for the specified path.
  // NOTE: this is NOT the same as FindPreference. In particular
  // FindPreference returns whether RegisterXXX has been invoked, where as
  // this checks if a value exists for the path.
  bool HasPrefPath(const std::string& path) const;

  // Issues a callback for every preference value. The preferences must not be
  // mutated during iteration.
  void IteratePreferenceValues(
      base::RepeatingCallback<void(const std::string& key,
                                   const base::Value& value)> callback) const;

  // Returns a dictionary with effective preference values. This is an expensive
  // operation which does a deep copy. Use only if you really need the results
  // in a base::Value (for example, for JSON serialization). Otherwise use
  // IteratePreferenceValues above to avoid the copies.
  //
  // If INCLUDE_DEFAULTS is requested, preferences set to their default values
  // will be included. Otherwise, these will be omitted from the returned
  // dictionary.
  base::Value::Dict GetPreferenceValues(
      IncludeDefaults include_defaults) const;

  bool ReadOnly() const;

  // Returns the initialization state, taking only user prefs into account.
  PrefInitializationStatus GetInitializationStatus() const;

  // Returns the initialization state, taking all pref stores into account.
  PrefInitializationStatus GetAllPrefStoresInitializationStatus() const;

  // Tell our PrefValueStore to update itself to |command_line_store|.
  // Takes ownership of the store.
  virtual void UpdateCommandLinePrefStore(PrefStore* command_line_store);

  // We run the callback once, when initialization completes. The bool
  // parameter will be set to true for successful initialization,
  // false for unsuccessful.
  void AddPrefInitObserver(base::OnceCallback<void(bool)> callback);

  // Returns the PrefRegistry object for this service. You should not
  // use this; the intent is for no registrations to take place after
  // PrefService has been constructed.
  //
  // Instead of using this method, the recommended approach is to
  // register all preferences for a class Xyz up front in a static
  // Xyz::RegisterPrefs function, which gets invoked early in the
  // application's start-up, before a PrefService is created.
  //
  // As an example, prefs registration in Chrome is triggered by the
  // functions chrome::RegisterPrefs (for global preferences) and
  // chrome::RegisterProfilePrefs (for user-specific preferences)
  // implemented in chrome/browser/prefs/browser_prefs.cc.
  PrefRegistry* DeprecatedGetPrefRegistry();

  // Clears mutable values.
  void ClearMutableValues();

  // Invoked when the store is deleted from disk. Allows this PrefService
  // to tangentially cleanup data it may have saved outside the store.
  void OnStoreDeletionFromDisk();

  // Add new pref stores to the existing PrefValueStore. Only adding new
  // stores are allowed. If a corresponding store already exists, calling this
  // will cause DCHECK failures. If the newly added stores already contain
  // values, PrefNotifier associated with this object will be notified with
  // these values. |delegate| can be passed to observe events of the new
  // PrefValueStore.
  // TODO(qinmin): packaging all the input params in a struct, and do the same
  // for the constructor.
  void ChangePrefValueStore(
      PrefStore* managed_prefs,
      PrefStore* supervised_user_prefs,
      PrefStore* extension_prefs,
      PrefStore* recommended_prefs,
      std::unique_ptr<PrefValueStore::Delegate> delegate = nullptr);

  // A low level function for registering an observer for every single
  // preference changed notification. The caller must ensure that the observer
  // remains valid as long as it is registered. Pointer ownership is not
  // transferred.
  //
  // Almost all calling code should use a PrefChangeRegistrar instead.
  //
  // AVOID ADDING THESE. These are low-level observer notifications that are
  // called for every pref change. This can lead to inefficiency, and the lack
  // of a "registrar" model makes it easy to forget to undregister. It is
  // really designed for integrating other notification systems, not for normal
  // observation.
  void AddPrefObserverAllPrefs(PrefObserver* obs);
  void RemovePrefObserverAllPrefs(PrefObserver* obs);

 protected:
  // The PrefNotifier handles registering and notifying preference observers.
  // It is created and owned by this PrefService. Subclasses may access it for
  // unit testing.
  const std::unique_ptr<PrefNotifierImpl> pref_notifier_;

  // The PrefValueStore provides prioritized preference values. It is owned by
  // this PrefService. Subclasses may access it for unit testing.
  std::unique_ptr<PrefValueStore> pref_value_store_;

  // Pref Stores and profile that we passed to the PrefValueStore.
  const scoped_refptr<PersistentPrefStore> user_pref_store_;

  // Callback to call when a read error occurs. Always invoked on the sequence
  // this PrefService was created own.
  const base::RepeatingCallback<void(PersistentPrefStore::PrefReadError)>
      read_error_callback_;

 private:
  // Hash map expected to be fastest here since it minimises expensive
  // string comparisons. Order is unimportant, and deletions are rare.
  // Confirmed on Android where this speeded Chrome startup by roughly 50ms
  // vs. std::map, and by roughly 180ms vs. std::set of Preference pointers.
  typedef std::unordered_map<std::string, Preference> PreferenceMap;

  // Give access to ReportUserPrefChanged() and GetMutableUserPref().
  friend class subtle::ScopedUserPrefUpdateBase;
  friend class PrefServiceTest_WriteablePrefStoreFlags_Test;
  friend class prefs::ScopedDictionaryPrefUpdate;

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
  // If the pref at the given path changes, we call the observer's
  // OnPreferenceChanged method. Note that observers should not call
  // these methods directly but rather use a PrefChangeRegistrar to
  // make sure the observer gets cleaned up properly.
  //
  // Virtual for testing.
  virtual void AddPrefObserver(const std::string& path, PrefObserver* obs);
  virtual void RemovePrefObserver(const std::string& path, PrefObserver* obs);

  // Sends notification of a changed preference. This needs to be called by
  // a ScopedUserPrefUpdate or ScopedDictionaryPrefUpdate if a DictionaryValue
  // or ListValue is changed.
  void ReportUserPrefChanged(const std::string& key);
  void ReportUserPrefChanged(
      const std::string& key,
      std::set<std::vector<std::string>> path_components);

  // Sets the value for this pref path in the user pref store and informs the
  // PrefNotifier of the change.
  void SetUserPrefValue(const std::string& path,
                        base::Value new_value);

  // Load preferences from storage, attempting to diagnose and handle errors.
  // This should only be called from the constructor.
  void InitFromStorage(bool async);

  // Used to set the value of dictionary or list values in the user pref store.
  // This will create a dictionary or list if one does not exist in the user
  // pref store. This method returns NULL only if you're requesting an
  // unregistered pref or a non-dict/non-list pref.
  // |type| may only be Values::Type::DICTIONARY or Values::Type::LIST and
  // |path| must point to a registered preference of type |type|.
  // Ownership of the returned value remains at the user pref store.
  base::Value* GetMutableUserPref(const std::string& path,
                                  base::Value::Type type);

  // GetPreferenceValue is the equivalent of FindPreference(path)->GetValue(),
  // it has been added for performance. It is faster because it does
  // not need to find or create a Preference object to get the
  // value (GetValue() calls back though the preference service to
  // actually get the value.).
  const base::Value* GetPreferenceValue(const std::string& path) const;
  const base::Value* GetPreferenceValueChecked(const std::string& path) const;

  const scoped_refptr<PrefRegistry> pref_registry_;

  // Local cache of registered Preference objects. The pref_registry_
  // is authoritative with respect to what the types and default values
  // of registered preferences are.
  mutable PreferenceMap prefs_map_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PrefService);
};

#endif  // COMPONENTS_PREFS_PREF_SERVICE_H_
