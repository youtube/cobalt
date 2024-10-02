// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_FAMILY_INFO_FETCHER_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_FAMILY_INFO_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace signin {
struct AccessTokenInfo;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

// Fetches information about the family of the signed-in user. It can get
// information about the family itself (e.g. a name), as well as a list of
// family members and their properties.
class FamilyInfoFetcher {
 public:
  enum class ErrorCode : int {
    kSuccess = 0,
    kTokenError,    // Failed to get OAuth2 token.
    kNetworkError,  // Network failure.
    kServiceError   // Service returned an error or malformed reply.
  };

  // Must be reflected in kFamilyMemberRoleStrings.
  // LINT.IfChange(family_member_roles)
  enum FamilyMemberRole { HEAD_OF_HOUSEHOLD = 0, PARENT, MEMBER, CHILD };
  // LINT.ThenChange(//chrome/browser/supervised_user/child_accounts/family_info_fetcher.cc:family_member_roles)

  struct FamilyMember {
    FamilyMember();
    FamilyMember(const std::string& obfuscated_gaia_id,
                 FamilyMemberRole role,
                 const std::string& display_name,
                 const std::string& email,
                 const std::string& profile_url,
                 const std::string& profile_image_url);
    FamilyMember(const FamilyMember& other);
    ~FamilyMember();
    std::string obfuscated_gaia_id;
    FamilyMemberRole role;
    // All of the following may be empty.
    std::string display_name;
    std::string email;
    std::string profile_url;
    std::string profile_image_url;
  };

  class Consumer {
   public:
    virtual void OnGetFamilyMembersSuccess(
        const std::vector<FamilyMember>& members) {}
    virtual void OnFailure(ErrorCode error) {}
    virtual ~Consumer() = default;
  };

  // Instantiates a fetcher, but doesn't start a fetch - use the StartGet*
  // methods below. |consumer| must outlive us.
  FamilyInfoFetcher(
      Consumer* consumer,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  FamilyInfoFetcher(const FamilyInfoFetcher&) = delete;
  FamilyInfoFetcher& operator=(const FamilyInfoFetcher&) = delete;

  ~FamilyInfoFetcher();

  // Public so tests can use them.
  static std::string RoleToString(FamilyMemberRole role);
  static bool StringToRole(const std::string& str, FamilyMemberRole* role);

  // Start a fetch for the family members.
  void StartGetFamilyMembers();

  // Public so tests can use it.
  void OnSimpleLoaderCompleteInternal(int net_error,
                                      int response_code,
                                      const std::string& response_body);

 private:
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info);

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  static bool ParseMembers(const base::Value::List& list,
                           std::vector<FamilyMember>* members);
  static bool ParseMember(const base::Value::Dict& dict, FamilyMember* member);
  static void ParseProfile(const base::Value::Dict& dict, FamilyMember* member);

  void StartFetching();
  void StartFetchingAccessToken();
  void FamilyMembersFetched(const std::string& response);

  raw_ptr<Consumer> consumer_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  std::string access_token_;
  bool access_token_expired_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  base::TimeTicks simple_url_loader_start_time_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_FAMILY_INFO_FETCHER_H_
