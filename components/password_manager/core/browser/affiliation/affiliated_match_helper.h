// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATED_MATCH_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATED_MATCH_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_interface.h"

namespace password_manager {

class AffiliationService;

// Interacts with the AffiliationService on behalf of the PasswordStore.
// For each GetLogins() request, it provides the PasswordStore with a list of
// additional realms that are affiliation-based matches to the observed realm.
//
// Currently, the supported use-case is obtaining Android applications or Web
// realms affiliated with the web site containing the observed form. This is
// achieved by implementing the "proactive fetching" strategy for interacting
// with the AffiliationService (see affiliation_service.h for details), with
// Android applications and web realms playing the role of facet Y.
class AffiliatedMatchHelper {
 public:
  // Callback to returns the list of affiliated signon_realms (as per defined in
  // PasswordForm) to the caller.
  using AffiliatedRealmsCallback =
      base::OnceCallback<void(const std::vector<std::string>&)>;

  // Callback to return PasswordForms with injected branding information.
  using PasswordFormsOrErrorCallback = base::OnceCallback<void(
      absl::variant<std::vector<std::unique_ptr<PasswordForm>>,
                    PasswordStoreBackendError>)>;

  // The |password_store| must outlive |this|. Both arguments must be non-NULL,
  // except in tests which do not Initialize() the object.
  explicit AffiliatedMatchHelper(AffiliationService* affiliation_service);
  AffiliatedMatchHelper(const AffiliatedMatchHelper&) = delete;
  AffiliatedMatchHelper& operator=(const AffiliatedMatchHelper&) = delete;
  virtual ~AffiliatedMatchHelper();

  // Retrieves realms of Android applications and Web realms affiliated with the
  // realm of the |observed_form| if it is web-based. Otherwise, yields the
  // empty list. The |result_callback| will be invoked in both cases, on the
  // same thread.
  virtual void GetAffiliatedAndroidAndWebRealms(
      const PasswordFormDigest& observed_form,
      AffiliatedRealmsCallback result_callback);

  // Similar to |GetAffiliatedAndroidAndWebRealms()|, but for groups.
  virtual void GetGroup(const PasswordFormDigest& observed_form,
                        AffiliatedRealmsCallback result_callback);

  // Retrieves affiliation and branding information about the Android
  // credentials in |forms|, sets |affiliated_web_realm|, |app_display_name| and
  // |app_icon_url| of forms, and invokes |result_callback|.
  virtual void InjectAffiliationAndBrandingInformation(
      std::vector<std::unique_ptr<PasswordForm>> forms,
      PasswordFormsOrErrorCallback result_callback);

  // Returns whether or not |form| represents a valid Web credential for the
  // purposes of affiliation-based matching.
  static bool IsValidWebCredential(const PasswordFormDigest& form);

  AffiliationService* get_affiliation_service() { return affiliation_service_; }

 private:
  // Reads all autofillable credentials from the password store and starts
  // observing the store for future changes.
  void DoDeferredInitialization();

  // Called back by AffiliationService to supply the list of facets
  // affiliated with |original_facet_uri| so that a
  // |GetAffiliatedAndroidAndWebRealms()| call can be completed.
  void CompleteGetAffiliatedAndroidAndWebRealms(
      const FacetURI& original_facet_uri,
      AffiliatedRealmsCallback result_callback,
      const AffiliatedFacets& results,
      bool success);

  // Called back by AffiliationService to supply the list of facets
  // affiliated with the Android credential in |form|. Injects affiliation and
  // branding information by setting |affiliated_web_realm|, |app_display_name|
  // and |app_icon_url| on |form| if |success| is true and |results| is
  // non-empty. Invokes |barrier_closure|.
  void CompleteInjectAffiliationAndBrandingInformation(
      PasswordForm* form,
      base::OnceClosure barrier_closure,
      const AffiliatedFacets& results,
      bool success);

  // Called back by AffiliationService to supply the list of facets
  // that are in same group with |original_facet_uri| so that a
  // GetGroup() call can be completed.
  void CompleteGetGroup(const FacetURI& original_facet_uri,
                        AffiliatedRealmsCallback result_callback,
                        const std::vector<GroupedFacets>& groups);

  raw_ptr<AffiliationService, DanglingUntriaged> affiliation_service_;

  base::WeakPtrFactory<AffiliatedMatchHelper> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATED_MATCH_HELPER_H_
