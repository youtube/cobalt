// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_MOCK_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_MOCK_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_

#include <string>

#include "base/files/file_path.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-forward.h"

namespace content {
// Mock FileSystemAccessPermissionContext implementation.
class MockFileSystemAccessPermissionContext
    : public FileSystemAccessPermissionContext {
 public:
  MockFileSystemAccessPermissionContext();
  ~MockFileSystemAccessPermissionContext() override;

  MOCK_METHOD(scoped_refptr<FileSystemAccessPermissionGrant>,
              GetReadPermissionGrant,
              (const url::Origin& origin,
               const base::FilePath& path,
               HandleType handle_type,
               FileSystemAccessPermissionContext::UserAction user_action),
              (override));

  MOCK_METHOD(scoped_refptr<FileSystemAccessPermissionGrant>,
              GetWritePermissionGrant,
              (const url::Origin& origin,
               const base::FilePath& path,
               HandleType handle_type,
               FileSystemAccessPermissionContext::UserAction user_action),
              (override));

  void ConfirmSensitiveEntryAccess(
      const url::Origin& origin,
      PathType path_type,
      const base::FilePath& path,
      HandleType handle_type,
      UserAction user_action,
      GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(SensitiveEntryResult)> callback) override;
  MOCK_METHOD(void,
              ConfirmSensitiveEntryAccess_,
              (const url::Origin& origin,
               PathType path_type,
               const base::FilePath& path,
               HandleType handle_type,
               UserAction user_action,
               GlobalRenderFrameHostId frame_id,
               base::OnceCallback<void(SensitiveEntryResult)>& callback));

  void PerformAfterWriteChecks(
      std::unique_ptr<FileSystemAccessWriteItem> item,
      GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(AfterWriteCheckResult)> callback) override;
  MOCK_METHOD(void,
              PerformAfterWriteChecks_,
              (FileSystemAccessWriteItem * item,
               GlobalRenderFrameHostId frame_id,
               base::OnceCallback<void(AfterWriteCheckResult)>& callback));

  MOCK_METHOD(bool,
              CanObtainReadPermission,
              (const url::Origin& origin),
              (override));
  MOCK_METHOD(bool,
              CanObtainWritePermission,
              (const url::Origin& origin),
              (override));

  MOCK_METHOD(void,
              SetLastPickedDirectory,
              (const url::Origin& origin,
               const std::string& id,
               const base::FilePath& path,
               const PathType type),
              (override));
  MOCK_METHOD(PathInfo,
              GetLastPickedDirectory,
              (const url::Origin& origin, const std::string& id),
              (override));

  MOCK_METHOD(base::FilePath,
              GetWellKnownDirectoryPath,
              (blink::mojom::WellKnownDirectory directory,
               const url::Origin& origin),
              (override));

  MOCK_METHOD(std::u16string,
              GetPickerTitle,
              (const blink::mojom::FilePickerOptionsPtr& options),
              (override));

  MOCK_METHOD(void,
              NotifyEntryMoved,
              (const url::Origin& origin,
               const base::FilePath& old_path,
               const base::FilePath& new_path),
              (override));
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_MOCK_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
