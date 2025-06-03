// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_UTIL_H_

#include <string>
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/volume.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/provider_interface.h"
#include "chrome/browser/platform_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace ash::cloud_upload {

struct ODFSMetadata {
  bool reauthentication_required = false;
  std::string user_email;
};

struct ODFSEntryMetadata {
  ODFSEntryMetadata();
  ODFSEntryMetadata(const ODFSEntryMetadata&);
  ~ODFSEntryMetadata();
  absl::optional<std::string> url;
};

typedef base::OnceCallback<void(
    base::expected<ODFSMetadata, base::File::Error> metadata_or_error)>
    GetODFSMetadataCallback;

typedef base::OnceCallback<void(
    base::expected<ODFSEntryMetadata, base::File::Error> metadata)>
    GetODFSEntryMetadataCallback;

// Type of the source location from which a given file is being uploaded.
enum class SourceType {
  LOCAL = 0,
  READ_ONLY = 1,
  CLOUD = 2,
  kMaxValue = CLOUD,
};

// Type of upload of a file to the Cloud.
enum class UploadType {
  kCopy = 0,
  kMove = 1,
  kMaxValue = kMove,
};

// List of UMA enum values for the cloud provider used when opening a file. The
// enum values must be kept in sync with CloudProvider in
// tools/metrics/histograms/enums.xml.
enum class CloudProvider {
  kNone = 0,
  kUnknown = 1,
  kGoogleDrive = 2,
  kOneDrive = 3,
  kMaxValue = kOneDrive,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OfficeFilesTransferRequired {
  kNotRequired = 0,
  kMove = 1,
  kCopy = 2,
  kMaxValue = kCopy,
};

// List of UMA enum values for Office File Handler task results for Drive. The
// enum values must be kept in sync with OfficeDriveOpenErrors in
// tools/metrics/histograms/enums.xml.
enum class OfficeDriveOpenErrors {
  kOffline = 0,
  kDriveFsInterface = 1,
  kTimeout = 2,
  kNoMetadata = 3,
  kInvalidAlternateUrl = 4,
  kDriveAlternateUrl = 5,
  kUnexpectedAlternateUrl = 6,
  kSuccess = 7,
  kDriveDisabled = 8,
  kNoDriveService = 9,
  kDriveAuthenticationNotReady = 10,
  kMeteredConnection = 11,
  kMaxValue = kMeteredConnection,
};

// List of UMA enum values for opening Office files from OneDrive, with the
// MS365 PWA. The enum values must be kept in sync with OfficeOneDriveOpenErrors
// in tools/metrics/histograms/enums.xml.
enum class OfficeOneDriveOpenErrors {
  kSuccess = 0,
  kOffline = 1,
  kNoProfile = 2,
  kNoFileSystemURL = 3,
  kInvalidFileSystemURL = 4,
  kGetActionsGenericError = 5,
  kGetActionsReauthRequired = 6,
  kGetActionsInvalidUrl = 7,
  kGetActionsNoUrl = 8,
  kGetActionsAccessDenied = 9,
  kGetActionsNoEmail = 10,
  kConversionToODFSUrlError = 11,
  kEmailsDoNotMatch = 12,
  kMaxValue = kEmailsDoNotMatch,
};

// Records the source volume that an office file is opened from. The values up
// to 12 must be kept in sync with file_manager::VolumeType.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OfficeFilesSourceVolume {
  kGoogleDrive = 0,
  kDownloadsDirectory = 1,
  kRemovableDiskPartition = 2,
  kMountedArchiveFile = 3,
  kProvided = 4,  // File system provided by FileSystemProvider API.
  kMtp = 5,
  kMediaView = 6,
  kCrostini = 7,
  kAndriodFiles = 8,
  kDocumentsProvider = 9,
  kSmb = 10,
  kSystemInternal = 11,  // Internal volume never exposed to users.
  kGuestOS = 12,         // Guest OS volumes (Crostini, Bruschetta, etc)
  kUnknown = 100,
  kMicrosoftOneDrive = 101,
  kMaxValue = kMicrosoftOneDrive,
};

// List of UMA enum value for Web Drive Office task results. The enum values
// must be kept in sync with OfficeTaskResult in
// tools/metrics/histograms/enums.xml.
enum class OfficeTaskResult {
  kFallbackQuickOffice = 0,
  kFallbackOther = 1,
  kOpened = 2,
  kMoved = 3,
  kCancelledAtConfirmation = 4,
  kFailedToUpload = 5,
  kFailedToOpen = 6,
  kCopied = 7,
  kCancelledAtFallback = 8,
  kCancelledAtSetup = 9,
  kLocalFileTask = 10,
  kMaxValue = kLocalFileTask,
};

// The result of the "Upload to cloud" workflow for Office files.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// The enum values must be kept in sync with OfficeFilesUploadResult in
// tools/metrics/histograms/enums.xml.
enum class OfficeFilesUploadResult {
  kSuccess = 0,
  kOtherError = 1,
  kFileSystemNotFound = 2,
  kMoveOperationCancelled = 3,
  kMoveOperationError = 4,
  kMoveOperationNeedPassword = 5,
  kCopyOperationCancelled = 6,
  kCopyOperationError = 7,
  kCopyOperationNeedPassword = 8,
  kPinningFailedDiskFull = 9,
  kCloudAccessDenied = 10,
  kCloudMetadataError = 11,
  kCloudQuotaFull = 12,
  kCloudError = 13,
  kNoConnection = 14,
  kDestinationUrlError = 15,
  kInvalidURL = 16,
  kCloudReauthRequired = 17,
  kInvalidAlternateUrl = 18,
  kUnexpectedAlternateUrlHost = 19,
  kSyncError = 20,
  kSyncCancelledAndDeleted = 21,
  kSyncCancelledAndTrashed = 22,
  kMaxValue = kSyncCancelledAndTrashed,
};

constexpr char kGoogleDriveTaskResultMetricName[] =
    "FileBrowser.OfficeFiles.TaskResult.Drive";
constexpr char kGoogleDriveTaskResultMetricStateMetricName[] =
    "FileBrowser.OfficeFiles.TaskResult.GoogleDrive.MetricState";

constexpr char kOneDriveTaskResultMetricName[] =
    "FileBrowser.OfficeFiles.TaskResult.OneDrive";
constexpr char kOneDriveTaskResultMetricStateMetricName[] =
    "FileBrowser.OfficeFiles.TaskResult.OneDrive.MetricState";

constexpr char kGoogleDriveUploadResultMetricName[] =
    "FileBrowser.OfficeFiles.Open.UploadResult.GoogleDrive";
constexpr char kGoogleDriveUploadResultMetricStateMetricName[] =
    "FileBrowser.OfficeFiles.Open.UploadResult.GoogleDrive.MetricState";

constexpr char kOneDriveUploadResultMetricName[] =
    "FileBrowser.OfficeFiles.Open.UploadResult.OneDrive";
constexpr char kOneDriveUploadResultMetricStateMetricName[] =
    "FileBrowser.OfficeFiles.Open.UploadResult.OneDrive.MetricState";

constexpr char kGoogleDriveMoveErrorMetricName[] =
    "FileBrowser.OfficeFiles.Open.IOTaskError.GoogleDrive.Move";
constexpr char kGoogleDriveMoveErrorMetricStateMetricName[] =
    "FileBrowser.OfficeFiles.Open.IOTaskError.GoogleDrive.Move.MetricState";

constexpr char kGoogleDriveCopyErrorMetricName[] =
    "FileBrowser.OfficeFiles.Open.IOTaskError.GoogleDrive.Copy";
constexpr char kGoogleDriveCopyErrorMetricStateMetricName[] =
    "FileBrowser.OfficeFiles.Open.IOTaskError.GoogleDrive.Copy.MetricState";

constexpr char kOneDriveMoveErrorMetricName[] =
    "FileBrowser.OfficeFiles.Open.IOTaskError.OneDrive.Move";
constexpr char kOneDriveMoveErrorMetricStateMetricName[] =
    "FileBrowser.OfficeFiles.Open.IOTaskError.OneDrive.Move.MetricState";

constexpr char kOneDriveCopyErrorMetricName[] =
    "FileBrowser.OfficeFiles.Open.IOTaskError.OneDrive.Copy";
constexpr char kOneDriveCopyErrorMetricStateMetricName[] =
    "FileBrowser.OfficeFiles.Open.IOTaskError.OneDrive.Copy.MetricState";

constexpr char kDriveOpenSourceVolumeMetric[] =
    "FileBrowser.OfficeFiles.Open.SourceVolume.GoogleDrive";
constexpr char kDriveOpenSourceVolumeMetricStateMetric[] =
    "FileBrowser.OfficeFiles.Open.SourceVolume.GoogleDrive.MetricState";

constexpr char kOneDriveOpenSourceVolumeMetric[] =
    "FileBrowser.OfficeFiles.Open.SourceVolume.MicrosoftOneDrive";
constexpr char kOneDriveOpenSourceVolumeMetricStateMetric[] =
    "FileBrowser.OfficeFiles.Open.SourceVolume.OneDrive.MetricState";

constexpr char kOpenCloudProviderMetric[] =
    "FileBrowser.OfficeFiles.Open.CloudProvider";

constexpr char kDriveTransferRequiredMetric[] =
    "FileBrowser.OfficeFiles.Open.TransferRequired.GoogleDrive";
constexpr char kDriveTransferRequiredMetricStateMetric[] =
    "FileBrowser.OfficeFiles.Open.TransferRequired.GoogleDrive.MetricState";

constexpr char kOneDriveTransferRequiredMetric[] =
    "FileBrowser.OfficeFiles.Open.TransferRequired.OneDrive";
constexpr char kOneDriveTransferRequiredMetricStateMetric[] =
    "FileBrowser.OfficeFiles.Open.TransferRequired.OneDrive.MetricState";

constexpr char kDriveErrorMetricName[] = "FileBrowser.OfficeFiles.Errors.Drive";
constexpr char kDriveErrorMetricStateMetricName[] =
    "FileBrowser.OfficeFiles.Errors.GoogleDrive.MetricState";

constexpr char kOneDriveErrorMetricName[] =
    "FileBrowser.OfficeFiles.Errors.OneDrive";
constexpr char kOneDriveErrorMetricStateMetricName[] =
    "FileBrowser.OfficeFiles.Errors.OneDrive.MetricState";

// Query actions for this path to get ODFS Metadata.
const char kODFSMetadataQueryPath[] = "/";

// Custom action ids passed from ODFS.
const char kOneDriveUrlActionId[] = "HIDDEN_ONEDRIVE_URL";
const char kUserEmailActionId[] = "HIDDEN_ONEDRIVE_USER_EMAIL";
const char kReauthenticationRequiredId[] =
    "HIDDEN_ONEDRIVE_REAUTHENTICATION_REQUIRED";

// Get generic error message for uploading office files.
std::string GetGenericErrorMessage();
// Get Microsoft authentication error message for uploading office files.
std::string GetReauthenticationRequiredMessage();
// Get access denied error message.
std::string GetGenericOneDriveAccessErrorMessage();

// Converts an absolute FilePath into a filesystem URL.
storage::FileSystemURL FilePathToFileSystemURL(
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    base::FilePath file_path);

// Creates a directory from a filesystem URL. The callback is called without
// error if the directory already exists.
void CreateDirectoryOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    storage::FileSystemURL destination_folder_url,
    base::OnceCallback<void(base::File::Error)> complete_callback);

// Converts the `volume_type` to the equivalent `OfficeFilesSourceVolume`.
OfficeFilesSourceVolume VolumeTypeToSourceVolume(
    ::file_manager::VolumeType volume_type);

// Returns the type of the source location from which the file is getting
// uploaded (see SourceType values).
SourceType GetSourceType(Profile* profile,
                         const storage::FileSystemURL& source_path);

// Returns the upload type (move or copy) for the upload flow based on the
// source path of the file to upload.
UploadType GetUploadType(Profile* profile,
                         const storage::FileSystemURL& source_path);

// Request ODFS be mounted. If there is an existing mount, ODFS will unmount
// that one after authentication of the new mount.
void RequestODFSMount(Profile* profile,
                      file_system_provider::RequestMountCallback callback);

// Get information of the currently provided ODFS. Expect there to be exactly
// one ODFS.
absl::optional<file_system_provider::ProvidedFileSystemInfo> GetODFSInfo(
    Profile* profile);

// Get currently provided ODFS, or null if not mounted.
file_system_provider::ProvidedFileSystemInterface* GetODFS(Profile* profile);

bool IsODFSMounted(Profile* profile);
bool IsODFSInstalled(Profile* profile);
bool IsOfficeWebAppInstalled(Profile* profile);

// Get ODFS metadata as actions by doing a special GetActions request (for the
// root directory) and return the actions to |OnODFSMetadataActions| which will
// be converted to |ODFSMetadata| and passed to |callback|.
void GetODFSMetadata(
    file_system_provider::ProvidedFileSystemInterface* file_system,
    GetODFSMetadataCallback callback);

// Get ODFS-specific file metadata as actions by doing a GetActions request for
// this path and post-processing the list of actions into a struct.
void GetODFSEntryMetadata(
    file_system_provider::ProvidedFileSystemInterface* file_system,
    const base::FilePath& path,
    GetODFSEntryMetadataCallback callback);

// Get the first task error that is not `base::File::Error::FILE_OK`.
absl::optional<base::File::Error> GetFirstTaskError(
    const ::file_manager::io_task::ProgressStatus& status);

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_UTIL_H_
