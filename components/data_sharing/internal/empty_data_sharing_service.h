// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_EMPTY_DATA_SHARING_SERVICE_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_EMPTY_DATA_SHARING_SERVICE_H_

#include "components/data_sharing/public/data_sharing_service.h"
#include "components/sync/model/data_type_sync_bridge.h"

namespace data_sharing {

// An empty implementation of DataSharingService that can be used when the
// data sharing feature is disabled.
class EmptyDataSharingService : public DataSharingService {
 public:
  EmptyDataSharingService();
  ~EmptyDataSharingService() override;

  // Disallow copy/assign.
  EmptyDataSharingService(const EmptyDataSharingService&) = delete;
  EmptyDataSharingService& operator=(const EmptyDataSharingService&) = delete;

  // DataSharingService implementation.
  bool IsEmptyService() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  DataSharingNetworkLoader* GetDataSharingNetworkLoader() override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetCollaborationGroupControllerDelegate() override;
  bool IsGroupDataModelLoaded() override;
  std::optional<GroupData> ReadGroup(const GroupId& group_id) override;
  std::set<GroupData> ReadAllGroups() override;
  std::optional<GroupMemberPartialData> GetPossiblyRemovedGroupMember(
      const GroupId& group_id,
      const GaiaId& member_gaia_id) override;
  std::optional<GroupData> GetPossiblyRemovedGroup(
      const GroupId& group_id) override;
  void ReadGroupDeprecated(
      const GroupId& group_id,
      base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback)
      override;
  void ReadNewGroup(const GroupToken& token,
                    base::OnceCallback<void(const GroupDataOrFailureOutcome&)>
                        callback) override;
  void CreateGroup(const std::string& group_name,
                   base::OnceCallback<void(const GroupDataOrFailureOutcome&)>
                       callback) override;
  void DeleteGroup(
      const GroupId& group_id,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) override;
  void InviteMember(
      const GroupId& group_id,
      const std::string& invitee_email,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) override;
  void AddMember(
      const GroupId& group_id,
      const std::string& access_token,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) override;
  void RemoveMember(
      const GroupId& group_id,
      const std::string& member_email,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) override;
  void LeaveGroup(
      const GroupId& group_id,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) override;
  bool IsLeavingOrDeletingGroup(const GroupId& group_id) override;
  std::vector<GroupEvent> GetGroupEventsSinceStartup() override;
  void HandleShareURLNavigationIntercepted(
      const GURL& url,
      std::unique_ptr<ShareURLInterceptionContext> context) override;
  std::unique_ptr<GURL> GetDataSharingUrl(const GroupData& group_data) override;
  void EnsureGroupVisibility(
      const GroupId& group_id,
      base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback)
      override;
  void GetSharedEntitiesPreview(
      const GroupToken& group_token,
      base::OnceCallback<void(const SharedDataPreviewOrFailureOutcome&)>
          callback) override;
  void GetAvatarImageForURL(
      const GURL& avatar_url,
      int size,
      base::OnceCallback<void(const gfx::Image&)> callback,
      image_fetcher::ImageFetcher* image_fetcher) override;
  void SetSDKDelegate(
      std::unique_ptr<DataSharingSDKDelegate> sdk_delegate) override;
  void SetUIDelegate(
      std::unique_ptr<DataSharingUIDelegate> ui_delegate) override;
  DataSharingUIDelegate* GetUiDelegate() override;
  Logger* GetLogger() override;
  void AddGroupDataForTesting(GroupData group_data) override;
  void SetPreviewServerProxyForTesting(
      std::unique_ptr<PreviewServerProxy> preview_server_proxy) override;
  PreviewServerProxy* GetPreviewServerProxyForTesting() override;
  void OnCollaborationGroupRemoved(const GroupId& group_id) override;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_EMPTY_DATA_SHARING_SERVICE_H_
