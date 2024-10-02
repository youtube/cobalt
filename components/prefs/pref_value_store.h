// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_VALUE_STORE_H_
#define COMPONENTS_PREFS_PREF_VALUE_STORE_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/prefs_export.h"

class PrefNotifier;
class PrefStore;

// The PrefValueStore manages various sources of values for Preferences
// (e.g., configuration policies, extensions, and user settings). It returns
// the value of a Preference from the source with the highest priority, and
// allows setting user-defined values for preferences that are not managed.
//
// Unless otherwise explicitly noted, all of the methods of this class must
// be called on the UI thread.
class COMPONENTS_PREFS_EXPORT PrefValueStore {
 public:
  using PrefChangedCallback = base::RepeatingCallback<void(const std::string&)>;

  // PrefStores must be listed here in order from highest to lowest priority.
  //   MANAGED contains all managed preferences that are provided by
  //      mandatory policies (e.g. Windows Group Policy or cloud policy).
  //   SUPERVISED_USER contains preferences that are valid for supervised users.
  //   EXTENSION contains preferences set by extensions.
  //   STANDALONE_BROWSER contains system preferences inherited from a separate
  //      Chrome instance. One relevant source is extension prefs in lacros
  //      passed to ash, so these prefs have similar precedence to extension
  //      prefs.
  //   COMMAND_LINE contains preferences set by command-line switches.
  //   USER contains all user-set preferences.
  //   RECOMMENDED contains all preferences that are provided by recommended
  //      policies.
  //   DEFAULT contains all application default preferences.
  enum PrefStoreType {
    // INVALID_STORE is not associated with an actual PrefStore but used as
    // an invalid marker, e.g. as a return value.
    INVALID_STORE = -1,
    MANAGED_STORE = 0,
    SUPERVISED_USER_STORE,
    EXTENSION_STORE,
    STANDALONE_BROWSER_STORE,
    COMMAND_LINE_STORE,
    USER_STORE,
    RECOMMENDED_STORE,
    DEFAULT_STORE,
    PREF_STORE_TYPE_MAX = DEFAULT_STORE
  };

  // In decreasing order of precedence:
  //   |managed_prefs| contains all preferences from mandatory policies.
  //   |supervised_user_prefs| contains all preferences from supervised user
  //        settings, i.e. settings configured for a supervised user by their
  //        custodian.
  //   |extension_prefs| contains preference values set by extensions.
  //   |command_line_prefs| contains preference values set by command-line
  //        switches.
  //   |user_prefs| contains all user-set preference values.
  //   |recommended_prefs| contains all preferences from recommended policies.
  //   |default_prefs| contains application-default preference values. It must
  //        be non-null if any preferences are to be registered.
  //
  // |pref_notifier| facilitates broadcasting preference change notifications
  // to the world.
  PrefValueStore(PrefStore* managed_prefs,
                 PrefStore* supervised_user_prefs,
                 PrefStore* extension_prefs,
                 PrefStore* standalone_browser_prefs,
                 PrefStore* command_line_prefs,
                 PrefStore* user_prefs,
                 PrefStore* recommended_prefs,
                 PrefStore* default_prefs,
                 PrefNotifier* pref_notifier);

  PrefValueStore(const PrefValueStore&) = delete;
  PrefValueStore& operator=(const PrefValueStore&) = delete;

  virtual ~PrefValueStore();

  // Creates a clone of this PrefValueStore with PrefStores overwritten
  // by the parameters passed, if unequal NULL.
  //
  // The new PrefValueStore is passed the |delegate| in its constructor.
  std::unique_ptr<PrefValueStore> CloneAndSpecialize(
      PrefStore* managed_prefs,
      PrefStore* supervised_user_prefs,
      PrefStore* extension_prefs,
      PrefStore* standalone_browser_prefs,
      PrefStore* command_line_prefs,
      PrefStore* user_prefs,
      PrefStore* recommended_prefs,
      PrefStore* default_prefs,
      PrefNotifier* pref_notifier);

  // Gets the value for the given preference name that has the specified value
  // type. Values stored in a PrefStore that have the matching |name| but
  // a non-matching |type| are silently skipped. Returns true if a valid value
  // was found in any of the available PrefStores. Most callers should use
  // Preference::GetValue() instead of calling this method directly.
  bool GetValue(base::StringPiece name,
                base::Value::Type type,
                const base::Value** out_value) const;

  // Gets the recommended value for the given preference name that has the
  // specified value type. A value stored in the recommended PrefStore that has
  // the matching |name| but a non-matching |type| is silently ignored. Returns
  // true if a valid value was found. Most callers should use
  // Preference::GetRecommendedValue() instead of calling this method directly.
  bool GetRecommendedValue(const std::string& name,
                           base::Value::Type type,
                           const base::Value** out_value) const;

  // These methods return true if a preference with the given name is in the
  // indicated pref store, even if that value is currently being overridden by
  // a higher-priority source.
  bool PrefValueInManagedStore(const std::string& name) const;
  bool PrefValueInSupervisedStore(const std::string& name) const;
  bool PrefValueInExtensionStore(const std::string& name) const;
  bool PrefValueInUserStore(const std::string& name) const;
  bool PrefValueInStandaloneBrowserStore(const std::string& name) const;

  // These methods return true if a preference with the given name is actually
  // being controlled by the indicated pref store and not being overridden by
  // a higher-priority source.
  bool PrefValueFromExtensionStore(const std::string& name) const;
  bool PrefValueFromUserStore(const std::string& name) const;
  bool PrefValueFromRecommendedStore(const std::string& name) const;
  bool PrefValueFromDefaultStore(const std::string& name) const;
  bool PrefValueFromStandaloneBrowserStore(const std::string& name) const;

  // Check whether a Preference value is modifiable by the user, i.e. whether
  // there is no higher-priority source controlling it.
  bool PrefValueUserModifiable(const std::string& name) const;

  // Check whether a Preference value is modifiable by an extension, i.e.
  // whether there is no higher-priority source controlling it.
  bool PrefValueExtensionModifiable(const std::string& name) const;

  // Check whether a Preference value is modifiable by a standalone browser
  // (lacros), i.e. whether there is no higher-priority source controlling it.
  bool PrefValueStandaloneBrowserModifiable(const std::string& name) const;

  // Update the command line PrefStore with |command_line_prefs|.
  void UpdateCommandLinePrefStore(PrefStore* command_line_prefs);

  bool IsInitializationComplete() const;

 private:
  // Keeps a PrefStore reference on behalf of the PrefValueStore and monitors
  // the PrefStore for changes, forwarding notifications to PrefValueStore. This
  // indirection is here for the sake of disambiguating notifications from the
  // individual PrefStores.
  class PrefStoreKeeper : public PrefStore::Observer {
   public:
    PrefStoreKeeper();

    PrefStoreKeeper(const PrefStoreKeeper&) = delete;
    PrefStoreKeeper& operator=(const PrefStoreKeeper&) = delete;

    ~PrefStoreKeeper() override;

    // Takes ownership of |pref_store|.
    void Initialize(PrefValueStore* store,
                    PrefStore* pref_store,
                    PrefStoreType type);

    PrefStore* store() { return pref_store_.get(); }
    const PrefStore* store() const { return pref_store_.get(); }

   private:
    // PrefStore::Observer implementation.
    void OnPrefValueChanged(const std::string& key) override;
    void OnInitializationCompleted(bool succeeded) override;

    // PrefValueStore this keeper is part of.
    raw_ptr<PrefValueStore> pref_value_store_;

    // The PrefStore managed by this keeper.
    scoped_refptr<PrefStore> pref_store_;

    // Type of the pref store.
    PrefStoreType type_;
  };

  typedef std::map<std::string, base::Value::Type> PrefTypeMap;

  // Returns true if the preference with the given name has a value in the
  // given PrefStoreType, of the same value type as the preference was
  // registered with.
  bool PrefValueInStore(const std::string& name, PrefStoreType store) const;

  // Returns true if a preference has an explicit value in any of the
  // stores in the range specified by |first_checked_store| and
  // |last_checked_store|, even if that value is currently being
  // overridden by a higher-priority store.
  bool PrefValueInStoreRange(const std::string& name,
                             PrefStoreType first_checked_store,
                             PrefStoreType last_checked_store) const;

  // Returns the pref store type identifying the source that controls the
  // Preference identified by |name|. If none of the sources has a value,
  // INVALID_STORE is returned. In practice, the default PrefStore
  // should always have a value for any registered preferencem, so INVALID_STORE
  // indicates an error.
  PrefStoreType ControllingPrefStoreForPref(const std::string& name) const;

  // Get a value from the specified |store|.
  bool GetValueFromStore(base::StringPiece name,
                         PrefStoreType store,
                         const base::Value** out_value) const;

  // Get a value from the specified |store| if its |type| matches.
  bool GetValueFromStoreWithType(base::StringPiece name,
                                 base::Value::Type type,
                                 PrefStoreType store,
                                 const base::Value** out_value) const;

  // Called upon changes in individual pref stores in order to determine whether
  // the user-visible pref value has changed. Triggers the change notification
  // if the effective value of the preference has changed, or if the store
  // controlling the pref has changed.
  void NotifyPrefChanged(const std::string& path, PrefStoreType new_store);

  // Called from the PrefStoreKeeper implementation when a pref value for |key|
  // changed in the pref store for |type|.
  void OnPrefValueChanged(PrefStoreType type, const std::string& key);

  // Handle the event that the store for |type| has completed initialization.
  void OnInitializationCompleted(PrefStoreType type, bool succeeded);

  // Initializes a pref store keeper. Sets up a PrefStoreKeeper that will take
  // ownership of the passed |pref_store|.
  void InitPrefStore(PrefStoreType type, PrefStore* pref_store);

  // Checks whether initialization is completed and tells the notifier if that
  // is the case.
  void CheckInitializationCompleted();

  // Get the PrefStore pointer for the given type. May return NULL if there is
  // no PrefStore for that type.
  PrefStore* GetPrefStore(PrefStoreType type) {
    return pref_stores_[type].store();
  }
  const PrefStore* GetPrefStore(PrefStoreType type) const {
    return pref_stores_[type].store();
  }

  // Keeps the PrefStore references in order of precedence.
  PrefStoreKeeper pref_stores_[PREF_STORE_TYPE_MAX + 1];

  // Used for generating notifications. This is a weak reference,
  // since the notifier is owned by the corresponding PrefService.
  raw_ptr<PrefNotifier> pref_notifier_;

  // A mapping of preference names to their registered types.
  PrefTypeMap pref_types_;

  // True if not all of the PrefStores were initialized successfully.
  bool initialization_failed_;
};

namespace std {

template <>
struct hash<PrefValueStore::PrefStoreType> {
  size_t operator()(PrefValueStore::PrefStoreType type) const {
    return std::hash<
        std::underlying_type<PrefValueStore::PrefStoreType>::type>()(type);
  }
};

}  // namespace std

#endif  // COMPONENTS_PREFS_PREF_VALUE_STORE_H_
