// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/fake_drive_service_helper.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "google_apis/drive/drive_api_parser.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"

#define FPL(path) FILE_PATH_LITERAL(path)

using google_apis::AboutResource;
using google_apis::ApiErrorCode;
using google_apis::FileList;
using google_apis::FileResource;

namespace sync_file_system {
namespace drive_backend {

namespace {

void UploadResultCallback(ApiErrorCode* error_out,
                          std::unique_ptr<FileResource>* entry_out,
                          ApiErrorCode error,
                          const GURL& upload_location,
                          std::unique_ptr<FileResource> entry) {
  ASSERT_TRUE(error_out);
  ASSERT_TRUE(entry_out);
  *error_out = error;
  *entry_out = std::move(entry);
}

void DownloadResultCallback(ApiErrorCode* error_out,
                            ApiErrorCode error,
                            const base::FilePath& local_file) {
  ASSERT_TRUE(error_out);
  *error_out = error;
}

}  // namespace

FakeDriveServiceHelper::FakeDriveServiceHelper(
    drive::FakeDriveService* fake_drive_service,
    drive::DriveUploaderInterface* drive_uploader,
    const std::string& sync_root_folder_title)
    : fake_drive_service_(fake_drive_service),
      drive_uploader_(drive_uploader),
      sync_root_folder_title_(sync_root_folder_title) {
  Initialize();
}

FakeDriveServiceHelper::~FakeDriveServiceHelper() = default;

ApiErrorCode FakeDriveServiceHelper::AddOrphanedFolder(const std::string& title,
                                                       std::string* folder_id) {
  std::string root_folder_id = fake_drive_service_->GetRootResourceId();
  ApiErrorCode error = AddFolder(root_folder_id, title, folder_id);
  if (error != google_apis::HTTP_CREATED)
    return error;

  error = google_apis::OTHER_ERROR;
  fake_drive_service_->RemoveResourceFromDirectory(
      root_folder_id, *folder_id, CreateResultReceiver(&error));
  base::RunLoop().RunUntilIdle();

  if (error != google_apis::HTTP_NO_CONTENT && folder_id)
    return error;
  return google_apis::HTTP_CREATED;
}

ApiErrorCode FakeDriveServiceHelper::AddFolder(
    const std::string& parent_folder_id,
    const std::string& title,
    std::string* folder_id) {
  ApiErrorCode error = google_apis::OTHER_ERROR;
  std::unique_ptr<FileResource> folder;
  drive::AddNewDirectoryOptions options;
  options.visibility = google_apis::drive::FILE_VISIBILITY_PRIVATE;
  fake_drive_service_->AddNewDirectory(parent_folder_id, title, options,
                                       CreateResultReceiver(&error, &folder));
  base::RunLoop().RunUntilIdle();

  if (error == google_apis::HTTP_CREATED && folder_id)
    *folder_id = folder->file_id();
  return error;
}

ApiErrorCode FakeDriveServiceHelper::AddFile(
    const std::string& parent_folder_id,
    const std::string& title,
    const std::string& content,
    std::string* file_id) {
  base::FilePath temp_file = WriteToTempFile(content);

  ApiErrorCode error = google_apis::OTHER_ERROR;
  std::unique_ptr<FileResource> file;
  drive_uploader_->UploadNewFile(
      parent_folder_id, temp_file, title, "application/octet-stream",
      drive::UploadNewFileOptions(),
      base::BindOnce(&UploadResultCallback, &error, &file),
      google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

  if (error == google_apis::HTTP_SUCCESS && file_id)
    *file_id = file->file_id();
  return error;
}

ApiErrorCode FakeDriveServiceHelper::UpdateFile(const std::string& file_id,
                                                const std::string& content) {
  base::FilePath temp_file = WriteToTempFile(content);
  ApiErrorCode error = google_apis::OTHER_ERROR;
  std::unique_ptr<FileResource> file;
  drive_uploader_->UploadExistingFile(
      file_id, temp_file, "application/octet-stream",
      drive::UploadExistingFileOptions(),
      base::BindOnce(&UploadResultCallback, &error, &file),
      google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();
  return error;
}

ApiErrorCode FakeDriveServiceHelper::DeleteResource(
    const std::string& file_id) {
  ApiErrorCode error = google_apis::OTHER_ERROR;
  fake_drive_service_->DeleteResource(file_id,
                                      std::string(),  // etag
                                      CreateResultReceiver(&error));
  base::RunLoop().RunUntilIdle();
  return error;
}

ApiErrorCode FakeDriveServiceHelper::TrashResource(const std::string& file_id) {
  ApiErrorCode error = google_apis::OTHER_ERROR;
  fake_drive_service_->TrashResource(file_id, CreateResultReceiver(&error));
  base::RunLoop().RunUntilIdle();
  return error;
}

ApiErrorCode FakeDriveServiceHelper::UpdateModificationTime(
    const std::string& file_id,
    const base::Time& modification_time) {
  ApiErrorCode error = google_apis::OTHER_ERROR;
  std::unique_ptr<FileResource> entry;
  error = GetFileResource(file_id, &entry);
  if (error != google_apis::HTTP_SUCCESS)
    return error;

  fake_drive_service_->UpdateResource(
      file_id, std::string(), entry->title(), modification_time,
      entry->last_viewed_by_me_date(), google_apis::drive::Properties(),
      CreateResultReceiver(&error, &entry));
  base::RunLoop().RunUntilIdle();
  return error;
}

ApiErrorCode FakeDriveServiceHelper::RenameResource(
    const std::string& file_id,
    const std::string& new_title) {
  ApiErrorCode error = google_apis::OTHER_ERROR;
  std::unique_ptr<FileResource> entry;
  fake_drive_service_->UpdateResource(
      file_id, std::string(), new_title, base::Time(), base::Time(),
      google_apis::drive::Properties(), CreateResultReceiver(&error, &entry));
  base::RunLoop().RunUntilIdle();
  return error;
}

ApiErrorCode FakeDriveServiceHelper::AddResourceToDirectory(
    const std::string& parent_folder_id,
    const std::string& file_id) {
  ApiErrorCode error = google_apis::OTHER_ERROR;
  fake_drive_service_->AddResourceToDirectory(parent_folder_id, file_id,
                                              CreateResultReceiver(&error));
  base::RunLoop().RunUntilIdle();
  return error;
}

ApiErrorCode FakeDriveServiceHelper::RemoveResourceFromDirectory(
    const std::string& parent_folder_id,
    const std::string& file_id) {
  ApiErrorCode error = google_apis::OTHER_ERROR;
  fake_drive_service_->RemoveResourceFromDirectory(
      parent_folder_id, file_id, CreateResultReceiver(&error));
  base::RunLoop().RunUntilIdle();
  return error;
}

ApiErrorCode FakeDriveServiceHelper::GetSyncRootFolderID(
    std::string* sync_root_folder_id) {
  ApiErrorCode error = google_apis::OTHER_ERROR;
  std::unique_ptr<FileList> resource_list;
  fake_drive_service_->SearchByTitle(
      sync_root_folder_title_, std::string(),
      CreateResultReceiver(&error, &resource_list));
  base::RunLoop().RunUntilIdle();
  if (error != google_apis::HTTP_SUCCESS)
    return error;

  const std::vector<std::unique_ptr<FileResource>>& items =
      resource_list->items();
  for (const auto& item : items) {
    if (item->parents().empty()) {
      *sync_root_folder_id = item->file_id();
      return google_apis::HTTP_SUCCESS;
    }
  }
  return google_apis::HTTP_NOT_FOUND;
}

ApiErrorCode FakeDriveServiceHelper::ListFilesInFolder(
    const std::string& folder_id,
    std::vector<std::unique_ptr<FileResource>>* entries) {
  ApiErrorCode error = google_apis::OTHER_ERROR;
  std::unique_ptr<FileList> list;
  fake_drive_service_->GetFileListInDirectory(
      folder_id, CreateResultReceiver(&error, &list));
  base::RunLoop().RunUntilIdle();
  if (error != google_apis::HTTP_SUCCESS)
    return error;

  return CompleteListing(std::move(list), entries);
}

ApiErrorCode FakeDriveServiceHelper::SearchByTitle(
    const std::string& folder_id,
    const std::string& title,
    std::vector<std::unique_ptr<FileResource>>* entries) {
  ApiErrorCode error = google_apis::OTHER_ERROR;
  std::unique_ptr<FileList> list;
  fake_drive_service_->SearchByTitle(title, folder_id,
                                     CreateResultReceiver(&error, &list));
  base::RunLoop().RunUntilIdle();
  if (error != google_apis::HTTP_SUCCESS)
    return error;

  return CompleteListing(std::move(list), entries);
}

ApiErrorCode FakeDriveServiceHelper::GetFileResource(
    const std::string& file_id,
    std::unique_ptr<FileResource>* entry) {
  ApiErrorCode error = google_apis::OTHER_ERROR;
  fake_drive_service_->GetFileResource(file_id,
                                       CreateResultReceiver(&error, entry));
  base::RunLoop().RunUntilIdle();
  return error;
}

ApiErrorCode FakeDriveServiceHelper::GetFileVisibility(
    const std::string& file_id,
    google_apis::drive::FileVisibility* visibility) {
  return fake_drive_service_->GetFileVisibility(file_id, visibility);
}

ApiErrorCode FakeDriveServiceHelper::ReadFile(const std::string& file_id,
                                              std::string* file_content) {
  std::unique_ptr<google_apis::FileResource> file;
  ApiErrorCode error = GetFileResource(file_id, &file);
  if (error != google_apis::HTTP_SUCCESS)
    return error;
  if (!file)
    return google_apis::PARSE_ERROR;

  error = google_apis::OTHER_ERROR;
  base::FilePath temp_file;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_, &temp_file));
  fake_drive_service_->DownloadFile(
      temp_file, file->file_id(),
      base::BindOnce(&DownloadResultCallback, &error),
      google_apis::GetContentCallback(), google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();
  if (error != google_apis::HTTP_SUCCESS)
    return error;

  return base::ReadFileToString(temp_file, file_content)
             ? google_apis::HTTP_SUCCESS
             : google_apis::DRIVE_FILE_ERROR;
}

ApiErrorCode FakeDriveServiceHelper::GetAboutResource(
    std::unique_ptr<AboutResource>* about_resource) {
  ApiErrorCode error = google_apis::OTHER_ERROR;
  fake_drive_service_->GetAboutResource(
      CreateResultReceiver(&error, about_resource));
  base::RunLoop().RunUntilIdle();
  return error;
}

ApiErrorCode FakeDriveServiceHelper::CompleteListing(
    std::unique_ptr<FileList> list,
    std::vector<std::unique_ptr<FileResource>>* entries) {
  while (true) {
    entries->reserve(entries->size() + list->items().size());
    std::move(list->mutable_items()->begin(), list->mutable_items()->end(),
              std::back_inserter(*entries));
    list->mutable_items()->clear();

    GURL next_feed = list->next_link();
    if (next_feed.is_empty())
      return google_apis::HTTP_SUCCESS;

    ApiErrorCode error = google_apis::OTHER_ERROR;
    list.reset();
    fake_drive_service_->GetRemainingFileList(
        next_feed, CreateResultReceiver(&error, &list));
    base::RunLoop().RunUntilIdle();
    if (error != google_apis::HTTP_SUCCESS)
      return error;
  }
}

void FakeDriveServiceHelper::Initialize() {
  ASSERT_TRUE(base_dir_.CreateUniqueTempDir());
  temp_dir_ = base_dir_.GetPath().Append(FPL("tmp"));
  ASSERT_TRUE(base::CreateDirectory(temp_dir_));
}

base::FilePath FakeDriveServiceHelper::WriteToTempFile(
    const std::string& content) {
  base::FilePath temp_file;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_, &temp_file));
  EXPECT_TRUE(base::WriteFile(temp_file, content));
  return temp_file;
}

void FakeDriveServiceHelper::AddTeamDrive(const std::string& team_drive_id,
                                          const std::string& team_drive_name) {
  fake_drive_service_->AddTeamDrive(team_drive_id, team_drive_name);
}

}  // namespace drive_backend
}  // namespace sync_file_system
