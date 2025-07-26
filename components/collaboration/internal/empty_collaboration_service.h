// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_EMPTY_COLLABORATION_SERVICE_H_
#define COMPONENTS_COLLABORATION_INTERNAL_EMPTY_COLLABORATION_SERVICE_H_

#include "components/collaboration/public/collaboration_service.h"

namespace collaboration {

// An empty implementation of CollaborationService that can be used when the
// data sharing feature is disabled.
class EmptyCollaborationService : public CollaborationService {
 public:
  EmptyCollaborationService();
  ~EmptyCollaborationService() override;

  // CollaborationService implementation.
  bool IsEmptyService() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void StartJoinFlow(std::unique_ptr<CollaborationControllerDelegate> delegate,
                     const GURL& url) override;
  void StartShareOrManageFlow(
      std::unique_ptr<CollaborationControllerDelegate> delegate,
      const tab_groups::EitherGroupID& group_id) override;
  ServiceStatus GetServiceStatus() override;
  data_sharing::MemberRole GetCurrentUserRoleForGroup(
      const data_sharing::GroupId& group_id) override;
  std::optional<data_sharing::GroupData> GetGroupData(
      const data_sharing::GroupId& group_id) override;
  void DeleteGroup(const data_sharing::GroupId& group_id,
                   base::OnceCallback<void(bool)> callback) override;
  void LeaveGroup(const data_sharing::GroupId& group_id,
                  base::OnceCallback<void(bool)> callback) override;
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_INTERNAL_EMPTY_COLLABORATION_SERVICE_H_
