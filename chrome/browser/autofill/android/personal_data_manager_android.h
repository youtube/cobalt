// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_

#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"

namespace autofill {

// Android wrapper of the PersonalDataManager which provides access from the
// Java layer. Note that on Android, there's only a single profile, and
// therefore a single instance of this wrapper.
class PersonalDataManagerAndroid : public PersonalDataManagerObserver {
 public:
  PersonalDataManagerAndroid(JNIEnv* env, jobject obj);

  PersonalDataManagerAndroid(const PersonalDataManagerAndroid&) = delete;
  PersonalDataManagerAndroid& operator=(const PersonalDataManagerAndroid&) =
      delete;

  static base::android::ScopedJavaLocalRef<jobject>
  CreateJavaCreditCardFromNative(JNIEnv* env, const CreditCard& card);
  static void PopulateNativeCreditCardFromJava(
      const base::android::JavaRef<jobject>& jcard,
      JNIEnv* env,
      CreditCard* card);

  // Returns true if personal data manager has loaded the initial data.
  jboolean IsDataLoaded(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj) const;

  // These functions act on "web profiles" aka "LOCAL_PROFILE" profiles.
  // -------------------------

  // Returns the GUIDs of all profiles.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileGUIDsForSettings(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Returns the GUIDs of the profiles to suggest to the user. See
  // PersonalDataManager::GetProfilesToSuggest for more details.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileGUIDsToSuggest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Returns the profile with the specified |jguid|, or NULL if there is no
  // profile with the specified |jguid|. Both web and auxiliary profiles may
  // be returned.
  base::android::ScopedJavaLocalRef<jobject> GetProfileByGUID(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Determines whether the logged in user (if any) is eligible to store
  // Autofill address profiles to their account.
  jboolean IsEligibleForAddressAccountStorage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Determines the country for for the newly created address profile.
  base::android::ScopedJavaLocalRef<jstring> GetDefaultCountryCodeForNewAddress(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj) const;

  // Users based in unsupported countries and profiles with a country value set
  // to an unsupported country are not eligible for account storage. This
  // function determines if the `country_code` is eligible.
  bool IsCountryEligibleForAccountStorage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& country_code) const;

  // Adds or modifies a profile.  If |jguid| is an empty string, we are creating
  // a new profile.  Else we are updating an existing profile.  Always returns
  // the GUID for this profile; the GUID it may have just been created.
  base::android::ScopedJavaLocalRef<jstring> SetProfile(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jprofile,
      const base::android::JavaParamRef<jstring>& jguid);
  // Adds or modifies a profile like SetProfile interface if |jprofile| is
  // local. Otherwise it creates a local copy of it.
  base::android::ScopedJavaLocalRef<jstring> SetProfileToLocal(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jprofile,
      const base::android::JavaParamRef<jstring>& jguid);

  // Gets the labels for all known profiles. These labels are useful for
  // distinguishing the profiles from one another.
  //
  // The labels never contain the full name and include at least 2 fields.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileLabelsForSettings(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Gets the labels for the profiles to suggest to the user. These labels are
  // useful for distinguishing the profiles from one another.
  //
  // The labels never contain the email address, or phone numbers. The
  // |include_name_in_label| argument controls whether the name is included.
  // All other fields are included in the label.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileLabelsToSuggest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      jboolean include_name_in_label,
      jboolean include_organization_in_label,
      jboolean include_country_in_label);

  // Returns the shipping label of the given profile for PaymentRequest. This
  // label does not contain the full name or the email address but will include
  // the country depending on the value of |include_country_in_label|. All other
  // fields are included in the label.
  base::android::ScopedJavaLocalRef<jstring>
  GetShippingAddressLabelForPaymentRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jprofile,
      const base::android::JavaParamRef<jstring>& jguid,
      bool include_country_in_label);

  // These functions act on local credit cards.
  // --------------------

  // Returns the GUIDs of all the credit cards.
  base::android::ScopedJavaLocalRef<jobjectArray> GetCreditCardGUIDsForSettings(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Returns the GUIDs of the credit cards to suggest to the user. See
  // PersonalDataManager::GetCreditCardsToSuggest for more details.
  base::android::ScopedJavaLocalRef<jobjectArray> GetCreditCardGUIDsToSuggest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Returns the credit card with the specified |jguid|, or NULL if there is
  // no credit card with the specified |jguid|.
  base::android::ScopedJavaLocalRef<jobject> GetCreditCardByGUID(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Returns a credit card with the specified |jcard_number|. This is used for
  // determining the card's obfuscated number, issuer icon, and type in one go.
  // This function does not interact with the autofill table on disk, so can be
  // used for cards that are not saved.
  base::android::ScopedJavaLocalRef<jobject> GetCreditCardForNumber(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jcard_number);

  // Adds or modifies a local credit card.  If |jguid| is an empty string, we
  // are creating a new card.  Else we are updating an existing profile.  Always
  // returns the GUID for this profile; the GUID it may have just been created.
  base::android::ScopedJavaLocalRef<jstring> SetCreditCard(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jcard);

  // Updates the billing address of a server credit card |jcard|.
  void UpdateServerCardBillingAddress(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jcard);

  // Returns the issuer network string according to PaymentRequest spec, or an
  // empty string if the given card number is not valid and |jempty_if_invalid|
  // is true.
  base::android::ScopedJavaLocalRef<jstring> GetBasicCardIssuerNetwork(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jcard_number,
      const jboolean jempty_if_invalid);

  // Adds a server credit card. Used only in tests.
  void AddServerCreditCardForTest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jcard);

  // Adds a server credit card and sets the additional fields, for example,
  // card_issuer, nickname. Used only in tests.
  void AddServerCreditCardForTestWithAdditionalFields(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jobject>& jcard,
      const base::android::JavaParamRef<jstring>& jnickname,
      jint jcard_issuer);

  // Removes the profile or credit card represented by |jguid|.
  void RemoveByGUID(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& unused_obj,
                    const base::android::JavaParamRef<jstring>& jguid);

  // Delete all local credit cards.
  void DeleteAllLocalCreditCards(JNIEnv* env);

  // Resets the given unmasked card back to the masked state.
  void ClearUnmaskedCache(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

  // These functions act on the usage stats of local profiles and credit cards.
  // --------------------

  // Records the use and log usage metrics for the profile associated with the
  // |jguid|. Increments the use count of the profile and sets its use date to
  // the current time.
  void RecordAndLogProfileUse(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Sets the use count and number of days since last use of the profile
  // associated to the `jguid`. Both `count` and `days_since_last_used` should
  // be non-negative. `days_since_last_used` represents the numbers of days
  // since the profile was last used.
  void SetProfileUseStatsForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid,
      jint count,
      jint days_since_last_used);

  // Returns the use count of the profile associated to the |jguid|.
  jint GetProfileUseCountForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Returns the use date of the profile associated to the |jguid|. It
  // represents an absolute point in coordinated universal time (UTC)
  // represented as microseconds since the Windows epoch. For more details see
  // the comment header in time.h.
  jlong GetProfileUseDateForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Records the use and log usage metrics for the credit card associated with
  // the |jguid|. Increments the use count of the credit card and sets its use
  // date to the current time.
  void RecordAndLogCreditCardUse(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Sets the use count and number of days since last use of the credit card
  // associated to the`jguid`. Both `count` and `days_since_last_used` should be
  // non-negative. `days_since_last_used` represents the numbers of days since
  // the card was last used.
  void SetCreditCardUseStatsForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid,
      jint count,
      jint days_since_last_used);

  // Returns the use count of the credit card associated to the |jguid|.
  jint GetCreditCardUseCountForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Returns the use date of the credit card associated to the |jguid|. It
  // represents an absolute point in coordinated universal time (UTC)
  // represented as microseconds since the Windows epoch. For more details see
  // the comment header in time.h.
  jlong GetCreditCardUseDateForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      const base::android::JavaParamRef<jstring>& jguid);

  // Returns the current date represented as an absolute point in coordinated
  // universal time (UTC) represented as microseconds since the Unix epoch. For
  // more details see the comment header in time.h
  jlong GetCurrentDateForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Calculates a point in time `days` days ago from the current
  // time. Returns the result as an absolute point in coordinated universal time
  // (UTC) represented as microseconds since the Windows epoch.
  jlong GetDateNDaysAgoForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj,
      jint days);

  // Clears server profiles and cards, to be used in tests only.
  void ClearServerDataForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& unused_obj);

  // Checks whether the Autofill PersonalDataManager has profiles.
  jboolean HasProfiles(JNIEnv* env);

  // Checks whether the Autofill PersonalDataManager has credit cards.
  jboolean HasCreditCards(JNIEnv* env);

  // Checks whether FIDO authentication is available.
  jboolean IsFidoAuthenticationAvailable(JNIEnv* env);

  void SetSyncServiceForTesting(JNIEnv* env);

  // Get Java AutofillImageFetcher.
  base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaImageFetcher(
      JNIEnv* env);

 private:
  ~PersonalDataManagerAndroid() override;

  // Returns the GUIDs of the |profiles| passed as parameter.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileGUIDs(
      JNIEnv* env,
      const std::vector<AutofillProfile*>& profiles);

  // Returns the GUIDs of the |credit_cards| passed as parameter.
  base::android::ScopedJavaLocalRef<jobjectArray> GetCreditCardGUIDs(
      JNIEnv* env,
      const std::vector<CreditCard*>& credit_cards);

  // Gets the labels for the |profiles| passed as parameters. These labels are
  // useful for distinguishing the profiles from one another.
  //
  // The labels never contain the full name and include at least 2 fields.
  //
  // If |address_only| is true, then such fields as phone number, and email
  // address are also omitted, but all other fields are included in the label.
  base::android::ScopedJavaLocalRef<jobjectArray> GetProfileLabels(
      JNIEnv* env,
      bool address_only,
      bool include_name_in_label,
      bool include_organization_in_label,
      bool include_country_in_label,
      std::vector<AutofillProfile*> profiles);

  // Pointer to the java counterpart.
  JavaObjectWeakGlobalRef weak_java_obj_;

  // Pointer to the PersonalDataManager for the main profile.
  raw_ptr<PersonalDataManager> personal_data_manager_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_PERSONAL_DATA_MANAGER_ANDROID_H_
