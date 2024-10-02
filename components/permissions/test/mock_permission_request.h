// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_REQUEST_H_
#define COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_REQUEST_H_

#include "components/permissions/permission_request.h"
#include "url/gurl.h"

namespace permissions {
enum class RequestType;

class MockPermissionRequest : public PermissionRequest {
 public:
  static constexpr const char* kDefaultOrigin = "https://www.google.com";

  explicit MockPermissionRequest(RequestType request_type);
  MockPermissionRequest(const GURL& requesting_origin,
                        RequestType request_type);
  MockPermissionRequest(RequestType request_type,
                        PermissionRequestGestureType gesture_type);
  MockPermissionRequest(const GURL& requesting_origin,
                        RequestType request_type,
                        PermissionRequestGestureType gesture_type);

  ~MockPermissionRequest() override;

  void PermissionDecided(ContentSetting result,
                         bool is_one_time,
                         bool is_final_decision);
  void MarkFinished();

  bool granted();
  bool cancelled();
  bool finished();

  std::unique_ptr<MockPermissionRequest> CreateDuplicateRequest() const;

 private:
  bool granted_;
  bool cancelled_;
  bool finished_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_REQUEST_H_
