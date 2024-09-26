// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides miscellaneous API functions, which don't belong to
// other files.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/extensions/file_manager/logged_extension_function.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/services/file_util/public/cpp/zip_file_creator.h"
#include "google_apis/common/api_error_codes.h"
#include "storage/browser/file_system/file_system_url.h"

namespace ash {
class RecentFile;
}

namespace crostini {
enum class CrostiniResult;
struct LinuxPackageInfo;
}  // namespace crostini

namespace file_manager {
namespace util {
struct EntryDefinition;
typedef std::vector<EntryDefinition> EntryDefinitionList;
}  // namespace util
}  // namespace file_manager

namespace google_apis {
class AuthServiceInterface;
}  // namespace google_apis

namespace extensions {

// Implements the chrome.fileManagerPrivate.getPreferences method.
// Gets settings for the Files app.
class FileManagerPrivateGetPreferencesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getPreferences",
                             FILEMANAGERPRIVATE_GETPREFERENCES)

 protected:
  ~FileManagerPrivateGetPreferencesFunction() override = default;

  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.setPreferences method.
// Sets settings for the Files app.
class FileManagerPrivateSetPreferencesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.setPreferences",
                             FILEMANAGERPRIVATE_SETPREFERENCES)

 protected:
  ~FileManagerPrivateSetPreferencesFunction() override = default;

  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.zipSelection method.
// Creates a ZIP file for the selected files and folders.
class FileManagerPrivateInternalZipSelectionFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.zipSelection",
                             FILEMANAGERPRIVATEINTERNAL_ZIPSELECTION)

  FileManagerPrivateInternalZipSelectionFunction();

 private:
  ~FileManagerPrivateInternalZipSelectionFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  // Computes the total number of bytes of all the items to zip.
  void ComputeSize();

  // Zips the items to zip.
  void ZipItems();

  // Absolute path of the source directory.
  base::FilePath src_dir_;

  // Relative paths of the items to zip. These paths are relative to |src_dir_|.
  std::vector<base::FilePath> src_files_;

  // Absolute path of the ZIP to create.
  base::FilePath dest_file_;

  // Total number of bytes of all the items to zip.
  int64_t total_bytes_;
};

// Implements the chrome.fileManagerPrivate.cancelZip method.
// Cancels an ongoing ZIP operation.
class FileManagerPrivateCancelZipFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.cancelZip",
                             FILEMANAGERPRIVATE_CANCELZIP)

  FileManagerPrivateCancelZipFunction();

 private:
  ~FileManagerPrivateCancelZipFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.getZipProgress method.
// Gets the progress of an ongoing ZIP operation.
class FileManagerPrivateGetZipProgressFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getZipProgress",
                             FILEMANAGERPRIVATE_GETZIPPROGRESS)

  FileManagerPrivateGetZipProgressFunction();

 private:
  ~FileManagerPrivateGetZipProgressFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  // Receives the progress from ZipFileCreator.
  void OnProgress();

  // Creates the response value.
  ResponseValue ZipProgressValue(const ZipFileCreator::Progress& progress);

  // Current ZIP task ID.
  int zip_id_ = 0;

  // Matching ZipFileCreator object.
  scoped_refptr<ZipFileCreator> creator_;
};

// Implements the chrome.fileManagerPrivate.zoom method.
// Changes the zoom level of the file manager by modifying the zoom level of the
// WebContents.
// TODO(hirono): Remove this function once the zoom level change is supported
// for all apps. crbug.com/227175.
class FileManagerPrivateZoomFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.zoom", FILEMANAGERPRIVATE_ZOOM)

 protected:
  ~FileManagerPrivateZoomFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class FileManagerPrivateRequestWebStoreAccessTokenFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.requestWebStoreAccessToken",
                             FILEMANAGERPRIVATE_REQUESTWEBSTOREACCESSTOKEN)

  FileManagerPrivateRequestWebStoreAccessTokenFunction();

 protected:
  ~FileManagerPrivateRequestWebStoreAccessTokenFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  std::unique_ptr<google_apis::AuthServiceInterface> auth_service_;

  void OnAccessTokenFetched(google_apis::ApiErrorCode code,
                            const std::string& access_token);
};

class FileManagerPrivateGetProfilesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getProfiles",
                             FILEMANAGERPRIVATE_GETPROFILES)

 protected:
  ~FileManagerPrivateGetProfilesFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.openInspector method.
class FileManagerPrivateOpenInspectorFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.openInspector",
                             FILEMANAGERPRIVATE_OPENINSPECTOR)

 protected:
  ~FileManagerPrivateOpenInspectorFunction() override = default;

  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.openSettingsSubpage method.
class FileManagerPrivateOpenSettingsSubpageFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.openSettingsSubpage",
                             FILEMANAGERPRIVATE_OPENSETTINGSSUBPAGE)

 protected:
  ~FileManagerPrivateOpenSettingsSubpageFunction() override = default;

  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.getMimeType method.
class FileManagerPrivateInternalGetMimeTypeFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getMimeType",
                             FILEMANAGERPRIVATEINTERNAL_GETMIMETYPE)

  FileManagerPrivateInternalGetMimeTypeFunction();

 protected:
  ~FileManagerPrivateInternalGetMimeTypeFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  void OnGetMimeType(const std::string& mimeType);
};

// Implements the chrome.fileManagerPrivate.getProviders method.
class FileManagerPrivateGetProvidersFunction : public ExtensionFunction {
 public:
  FileManagerPrivateGetProvidersFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getProviders",
                             FILEMANAGERPRIVATE_GETPROVIDERS)

  FileManagerPrivateGetProvidersFunction(
      const FileManagerPrivateGetProvidersFunction&) = delete;
  FileManagerPrivateGetProvidersFunction& operator=(
      const FileManagerPrivateGetProvidersFunction&) = delete;

 protected:
  ~FileManagerPrivateGetProvidersFunction() override = default;

 private:
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.addProvidedFileSystem method.
class FileManagerPrivateAddProvidedFileSystemFunction
    : public ExtensionFunction {
 public:
  FileManagerPrivateAddProvidedFileSystemFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.addProvidedFileSystem",
                             FILEMANAGERPRIVATE_ADDPROVIDEDFILESYSTEM)

  FileManagerPrivateAddProvidedFileSystemFunction(
      const FileManagerPrivateAddProvidedFileSystemFunction&) = delete;
  FileManagerPrivateAddProvidedFileSystemFunction& operator=(
      const FileManagerPrivateAddProvidedFileSystemFunction&) = delete;

 protected:
  ~FileManagerPrivateAddProvidedFileSystemFunction() override = default;

 private:
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.configureVolume method.
class FileManagerPrivateConfigureVolumeFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateConfigureVolumeFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.configureVolume",
                             FILEMANAGERPRIVATE_CONFIGUREVOLUME)

  FileManagerPrivateConfigureVolumeFunction(
      const FileManagerPrivateConfigureVolumeFunction&) = delete;
  FileManagerPrivateConfigureVolumeFunction& operator=(
      const FileManagerPrivateConfigureVolumeFunction&) = delete;

 protected:
  ~FileManagerPrivateConfigureVolumeFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnCompleted(base::File::Error result);
};

// Implements the chrome.fileManagerPrivate.mountCrostini method.
// Starts and mounts crostini container.
class FileManagerPrivateMountCrostiniFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.mountCrostini",
                             FILEMANAGERPRIVATE_MOUNTCROSTINI)
  FileManagerPrivateMountCrostiniFunction();

  FileManagerPrivateMountCrostiniFunction(
      const FileManagerPrivateMountCrostiniFunction&) = delete;
  FileManagerPrivateMountCrostiniFunction& operator=(
      const FileManagerPrivateMountCrostiniFunction&) = delete;

 protected:
  ~FileManagerPrivateMountCrostiniFunction() override;

  ResponseAction Run() override;
  void RestartCallback(crostini::CrostiniResult);
  void MountCallback(crostini::CrostiniResult);
};

// Implements the chrome.fileManagerPrivate.importCrostiniImage method.
// Starts importing the crostini .tini image.
class FileManagerPrivateInternalImportCrostiniImageFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.importCrostiniImage",
                             FILEMANAGERPRIVATEINTERNAL_IMPORTCROSTINIIMAGE)
  FileManagerPrivateInternalImportCrostiniImageFunction();

  FileManagerPrivateInternalImportCrostiniImageFunction(
      const FileManagerPrivateInternalImportCrostiniImageFunction&) = delete;
  FileManagerPrivateInternalImportCrostiniImageFunction& operator=(
      const FileManagerPrivateInternalImportCrostiniImageFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalImportCrostiniImageFunction() override;

 private:
  ResponseAction Run() override;

  std::string image_path_;
};

// Implements the chrome.fileManagerPrivate.sharePathsWithCrostini
// method.  Shares specified paths.
class FileManagerPrivateInternalSharePathsWithCrostiniFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileManagerPrivateInternal.sharePathsWithCrostini",
      FILEMANAGERPRIVATEINTERNAL_SHAREPATHSWITHCROSTINI)
  FileManagerPrivateInternalSharePathsWithCrostiniFunction() = default;

  FileManagerPrivateInternalSharePathsWithCrostiniFunction(
      const FileManagerPrivateInternalSharePathsWithCrostiniFunction&) = delete;
  FileManagerPrivateInternalSharePathsWithCrostiniFunction& operator=(
      const FileManagerPrivateInternalSharePathsWithCrostiniFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalSharePathsWithCrostiniFunction() override =
      default;

 private:
  ResponseAction Run() override;
  void SharePathsCallback(bool success, const std::string& failure_reason);
};

// Implements the chrome.fileManagerPrivate.unsharePathWithCrostini
// method.  Unshares specified path.
class FileManagerPrivateInternalUnsharePathWithCrostiniFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileManagerPrivateInternal.unsharePathWithCrostini",
      FILEMANAGERPRIVATEINTERNAL_UNSHAREPATHWITHCROSTINI)
  FileManagerPrivateInternalUnsharePathWithCrostiniFunction() = default;

  FileManagerPrivateInternalUnsharePathWithCrostiniFunction(
      const FileManagerPrivateInternalUnsharePathWithCrostiniFunction&) =
      delete;
  FileManagerPrivateInternalUnsharePathWithCrostiniFunction& operator=(
      const FileManagerPrivateInternalUnsharePathWithCrostiniFunction&) =
      delete;

 protected:
  ~FileManagerPrivateInternalUnsharePathWithCrostiniFunction() override =
      default;

 private:
  ResponseAction Run() override;
  void UnsharePathCallback(bool success, const std::string& failure_reason);
};

// Implements the chrome.fileManagerPrivate.getCrostiniSharedPaths
// method.  Returns list of file entries.
class FileManagerPrivateInternalGetCrostiniSharedPathsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileManagerPrivateInternal.getCrostiniSharedPaths",
      FILEMANAGERPRIVATEINTERNAL_GETCROSTINISHAREDPATHS)
  FileManagerPrivateInternalGetCrostiniSharedPathsFunction() = default;

  FileManagerPrivateInternalGetCrostiniSharedPathsFunction(
      const FileManagerPrivateInternalGetCrostiniSharedPathsFunction&) = delete;
  FileManagerPrivateInternalGetCrostiniSharedPathsFunction operator=(
      const FileManagerPrivateInternalGetCrostiniSharedPathsFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalGetCrostiniSharedPathsFunction() override =
      default;

 private:
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.getLinuxPackageInfo method.
// Retrieves information about a Linux package.
class FileManagerPrivateInternalGetLinuxPackageInfoFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getLinuxPackageInfo",
                             FILEMANAGERPRIVATEINTERNAL_GETLINUXPACKAGEINFO)
  FileManagerPrivateInternalGetLinuxPackageInfoFunction() = default;

  FileManagerPrivateInternalGetLinuxPackageInfoFunction(
      const FileManagerPrivateInternalGetLinuxPackageInfoFunction&) = delete;
  FileManagerPrivateInternalGetLinuxPackageInfoFunction operator=(
      const FileManagerPrivateInternalGetLinuxPackageInfoFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalGetLinuxPackageInfoFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnGetLinuxPackageInfo(
      const crostini::LinuxPackageInfo& linux_package_info);
};

// Implements the chrome.fileManagerPrivate.installLinuxPackage method.
// Starts installation of a Linux package.
class FileManagerPrivateInternalInstallLinuxPackageFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.installLinuxPackage",
                             FILEMANAGERPRIVATEINTERNAL_INSTALLLINUXPACKAGE)
  FileManagerPrivateInternalInstallLinuxPackageFunction() = default;

  FileManagerPrivateInternalInstallLinuxPackageFunction(
      const FileManagerPrivateInternalInstallLinuxPackageFunction&) = delete;
  FileManagerPrivateInternalInstallLinuxPackageFunction operator=(
      const FileManagerPrivateInternalInstallLinuxPackageFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalInstallLinuxPackageFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnInstallLinuxPackage(crostini::CrostiniResult result);
};

// Implements the chrome.fileManagerPrivate.getCustomActions method.
class FileManagerPrivateInternalGetCustomActionsFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalGetCustomActionsFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getCustomActions",
                             FILEMANAGERPRIVATEINTERNAL_GETCUSTOMACTIONS)

  FileManagerPrivateInternalGetCustomActionsFunction(
      const FileManagerPrivateInternalGetCustomActionsFunction&) = delete;
  FileManagerPrivateInternalGetCustomActionsFunction operator=(
      const FileManagerPrivateInternalGetCustomActionsFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalGetCustomActionsFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnCompleted(const ash::file_system_provider::Actions& actions,
                   base::File::Error result);
};

// Implements the chrome.fileManagerPrivate.executeCustomAction method.
class FileManagerPrivateInternalExecuteCustomActionFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalExecuteCustomActionFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.executeCustomAction",
                             FILEMANAGERPRIVATEINTERNAL_EXECUTECUSTOMACTION)

  FileManagerPrivateInternalExecuteCustomActionFunction(
      const FileManagerPrivateInternalExecuteCustomActionFunction&) = delete;
  FileManagerPrivateInternalExecuteCustomActionFunction operator=(
      const FileManagerPrivateInternalExecuteCustomActionFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalExecuteCustomActionFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnCompleted(base::File::Error result);
};

// Implements the chrome.fileManagerPrivateInternal.getRecentFiles method.
class FileManagerPrivateInternalGetRecentFilesFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalGetRecentFilesFunction();
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getRecentFiles",
                             FILEMANAGERPRIVATE_GETRECENTFILES)

  FileManagerPrivateInternalGetRecentFilesFunction(
      const FileManagerPrivateInternalGetRecentFilesFunction&) = delete;
  FileManagerPrivateInternalGetRecentFilesFunction& operator=(
      FileManagerPrivateInternalGetRecentFilesFunction&) = delete;

 protected:
  ~FileManagerPrivateInternalGetRecentFilesFunction() override = default;

 private:
  ResponseAction Run() override;
  void OnGetRecentFiles(
      api::file_manager_private::SourceRestriction restriction,
      const std::vector<ash::RecentFile>& files);
  void OnConvertFileDefinitionListToEntryDefinitionList(
      std::unique_ptr<file_manager::util::EntryDefinitionList>
          entry_definition_list);
};

// Implements the chrome.fileManagerPrivate.getFrameColor method.
// Returns the Chrome app frame color to launch foreground windows.
// TODO(crbug.com/1212768): Remove this once Files app SWA has fully launched.
class FileManagerPrivateGetFrameColorFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getFrameColor",
                             FILEMANAGERPRIVATE_GETFRAMECOLOR)
  FileManagerPrivateGetFrameColorFunction() = default;

  FileManagerPrivateGetFrameColorFunction(
      const FileManagerPrivateGetFrameColorFunction&) = delete;
  FileManagerPrivateGetFrameColorFunction operator=(
      const FileManagerPrivateGetFrameColorFunction&) = delete;

 protected:
  ~FileManagerPrivateGetFrameColorFunction() override = default;

 private:
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.isTabletModeEnabled method.
class FileManagerPrivateIsTabletModeEnabledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.isTabletModeEnabled",
                             FILEMANAGERPRIVATE_ISTABLETMODEENABLED)

 protected:
  ~FileManagerPrivateIsTabletModeEnabledFunction() override = default;

 private:
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.openURL method.
class FileManagerPrivateOpenURLFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.openURL",
                             FILEMANAGERPRIVATE_OPENURL)

 protected:
  ~FileManagerPrivateOpenURLFunction() override = default;

 private:
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.openWindow method.
class FileManagerPrivateOpenWindowFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.openWindow",
                             FILEMANAGERPRIVATE_OPENWINDOW)

 protected:
  ~FileManagerPrivateOpenWindowFunction() override = default;

 private:
  ResponseAction Run() override;
};

// Implements the chrome.fileManagerPrivate.sendFeedback method.
class FileManagerPrivateSendFeedbackFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.sendFeedback",
                             FILEMANAGERPRIVATE_SENDFEEDBACK)

 protected:
  ~FileManagerPrivateSendFeedbackFunction() override = default;

 private:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MISC_H_
