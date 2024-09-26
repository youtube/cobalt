// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/file_utils.h"

#include "base/files/file_path.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

namespace apps {

std::vector<storage::FileSystemURL> GetFileSystemURL(
    Profile* profile,
    const std::vector<GURL>& file_urls) {
  storage::FileSystemContext* file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(profile);

  std::vector<storage::FileSystemURL> file_system_urls;
  for (const GURL& file_url : file_urls) {
    file_system_urls.push_back(
        file_system_context->CrackURLInFirstPartyContext(file_url));
  }
  return file_system_urls;
}

storage::FileSystemURL GetFileSystemURL(Profile* profile,
                                        const GURL& file_url) {
  storage::FileSystemContext* file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(profile);

  storage::FileSystemURL file_system_url;
  file_system_url = file_system_context->CrackURLInFirstPartyContext(file_url);

  return file_system_url;
}

std::vector<GURL> GetFileSystemUrls(
    Profile* profile,
    const std::vector<base::FilePath>& file_paths) {
  std::vector<GURL> file_urls;
  for (auto& file_path : file_paths) {
    GURL file_url;
    if (file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
            profile, file_path, file_manager::util::GetFileManagerURL(),
            &file_url)) {
      file_urls.push_back(file_url);
    }
  }
  return file_urls;
}

GURL GetFileSystemUrl(Profile* profile, const base::FilePath& file_path) {
  GURL file_url;
  if (file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, file_path, file_manager::util::GetFileManagerURL(),
          &file_url)) {
    return file_url;
  }
  return GURL();
}

}  // namespace apps
