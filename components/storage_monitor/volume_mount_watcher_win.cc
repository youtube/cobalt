// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/volume_mount_watcher_win.h"

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

#include <dbt.h>
#include <fileapi.h>
#include <shlobj.h>
#include <winioctl.h>

#include <algorithm>
#include <string>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "components/storage_monitor/media_storage_util.h"
#include "components/storage_monitor/storage_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace storage_monitor {

namespace {

const DWORD kMaxPathBufLen = MAX_PATH + 1;

enum DeviceType {
  FLOPPY,
  REMOVABLE,
  FIXED,
};

// We are trying to figure out whether the drive is a fixed volume,
// a removable storage, or a floppy. A "floppy" here means "a volume we
// want to basically ignore because it won't fit media and will spin
// if we touch it to get volume metadata." GetDriveType returns DRIVE_REMOVABLE
// on either floppy or removable volumes. The DRIVE_CDROM type is handled
// as a floppy, as are DRIVE_UNKNOWN and DRIVE_NO_ROOT_DIR, as there are
// reports that some floppy drives don't report as DRIVE_REMOVABLE.
DeviceType GetDeviceType(const std::wstring& mount_point) {
  UINT drive_type = GetDriveType(mount_point.c_str());
  if (drive_type == DRIVE_FIXED || drive_type == DRIVE_REMOTE ||
      drive_type == DRIVE_RAMDISK) {
    return FIXED;
  }
  if (drive_type != DRIVE_REMOVABLE)
    return FLOPPY;

  // Check device strings of the form "X:" and "\\.\X:"
  // For floppy drives, these will return strings like "/Device/Floppy0"
  std::wstring device = mount_point;
  if (base::EndsWith(mount_point, L"\\", base::CompareCase::INSENSITIVE_ASCII))
    device = mount_point.substr(0, mount_point.length() - 1);
  std::wstring device_path;
  std::wstring device_path_slash;
  DWORD dos_device = QueryDosDevice(
      device.c_str(), base::WriteInto(&device_path, kMaxPathBufLen),
      kMaxPathBufLen);
  std::wstring device_slash = std::wstring(L"\\\\.\\");
  device_slash += device;
  DWORD dos_device_slash = QueryDosDevice(
      device_slash.c_str(), base::WriteInto(&device_path_slash, kMaxPathBufLen),
      kMaxPathBufLen);
  if (dos_device == 0 && dos_device_slash == 0)
    return FLOPPY;
  if (device_path.find(L"Floppy") != std::wstring::npos ||
      device_path_slash.find(L"Floppy") != std::wstring::npos) {
    return FLOPPY;
  }

  return REMOVABLE;
}

// Returns 0 if the devicetype is not volume.
uint32_t GetVolumeBitMaskFromBroadcastHeader(LPARAM data) {
  DEV_BROADCAST_VOLUME* dev_broadcast_volume =
      reinterpret_cast<DEV_BROADCAST_VOLUME*>(data);
  if (dev_broadcast_volume->dbcv_devicetype == DBT_DEVTYP_VOLUME)
    return dev_broadcast_volume->dbcv_unitmask;
  return 0;
}

// Returns true if |data| represents a logical volume structure.
bool IsLogicalVolumeStructure(LPARAM data) {
  DEV_BROADCAST_HDR* broadcast_hdr =
      reinterpret_cast<DEV_BROADCAST_HDR*>(data);
  return broadcast_hdr && broadcast_hdr->dbch_devicetype == DBT_DEVTYP_VOLUME;
}

// Gets the total volume of the |mount_point| in bytes.
uint64_t GetVolumeSize(const base::FilePath& mount_point) {
  int64_t size = base::SysInfo::AmountOfTotalDiskSpace(mount_point);
  return std::max(size, static_cast<int64_t>(0));
}

// Gets mass storage device information given a |device_path|. On success,
// returns true and fills in |info|.
// The following msdn blog entry is helpful for understanding disk volumes
// and how they are treated in Windows:
// http://blogs.msdn.com/b/adioltean/archive/2005/04/16/408947.aspx.
bool GetDeviceDetails(const base::FilePath& device_path, StorageInfo* info) {
  DCHECK(info);

  std::wstring mount_point;
  if (!GetVolumePathName(device_path.value().c_str(),
                         base::WriteInto(&mount_point, kMaxPathBufLen),
                         kMaxPathBufLen)) {
    return false;
  }
  mount_point.resize(wcslen(mount_point.c_str()));

  // Note: experimentally this code does not spin a floppy drive. It
  // returns a GUID associated with the device, not the volume.
  std::wstring guid;
  if (!GetVolumeNameForVolumeMountPoint(mount_point.c_str(),
                                        base::WriteInto(&guid, kMaxPathBufLen),
                                        kMaxPathBufLen)) {
    return false;
  }
  // In case it has two GUID's (see above mentioned blog), do it again.
  if (!GetVolumeNameForVolumeMountPoint(guid.c_str(),
                                        base::WriteInto(&guid, kMaxPathBufLen),
                                        kMaxPathBufLen)) {
    return false;
  }

  // If we're adding a floppy drive, return without querying any more
  // drive metadata -- it will cause the floppy drive to seek.
  // Note: treats FLOPPY as FIXED_MASS_STORAGE. This is intentional.
  DeviceType device_type = GetDeviceType(mount_point);
  if (device_type == FLOPPY) {
    info->set_device_id(StorageInfo::MakeDeviceId(
        StorageInfo::FIXED_MASS_STORAGE, base::WideToUTF8(guid)));
    return true;
  }

  base::FilePath mount_path(mount_point);
  StorageInfo::Type type = StorageInfo::FIXED_MASS_STORAGE;
  if (device_type == REMOVABLE) {
    type = StorageInfo::REMOVABLE_MASS_STORAGE_NO_DCIM;
    if (MediaStorageUtil::HasDcim(mount_path))
      type = StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM;
  }

  // NOTE: experimentally, this function returns false if there is no volume
  // name set.
  std::wstring volume_label;
  GetVolumeInformationW(device_path.value().c_str(),
                        base::WriteInto(&volume_label, kMaxPathBufLen),
                        kMaxPathBufLen, nullptr, nullptr, nullptr, nullptr, 0);

  uint64_t total_size_in_bytes = GetVolumeSize(mount_path);
  std::string device_id =
      StorageInfo::MakeDeviceId(type, base::WideToUTF8(guid));

  // TODO(gbillock): if volume_label.empty(), get the vendor/model information
  // for the volume.
  *info = StorageInfo(device_id, mount_point, base::WideToUTF16(volume_label),
                      std::u16string(), std::u16string(), total_size_in_bytes);
  return true;
}

// Returns a vector of all the removable mass storage devices that are
// connected.
std::vector<base::FilePath> GetAttachedDevices() {
  std::vector<base::FilePath> result;
  std::wstring volume_name;
  HANDLE find_handle = FindFirstVolume(
      base::WriteInto(&volume_name, kMaxPathBufLen), kMaxPathBufLen);
  if (find_handle == INVALID_HANDLE_VALUE)
    return result;

  while (true) {
    std::wstring volume_path;
    DWORD return_count;
    if (GetVolumePathNamesForVolumeName(
            volume_name.c_str(), base::WriteInto(&volume_path, kMaxPathBufLen),
            kMaxPathBufLen, &return_count)) {
      result.push_back(base::FilePath(volume_path));
    }
    if (!FindNextVolume(find_handle,
                        base::WriteInto(&volume_name, kMaxPathBufLen),
                        kMaxPathBufLen)) {
      if (GetLastError() != ERROR_NO_MORE_FILES)
        DPLOG(ERROR);
      break;
    }
  }

  FindVolumeClose(find_handle);
  return result;
}

// Eject a removable volume at the specified |device| path. This works by
// 1) locking the volume,
// 2) unmounting the volume,
// 3) ejecting the volume.
// If the lock fails, it will re-schedule itself.
// See http://support.microsoft.com/kb/165721
void EjectDeviceInThreadPool(
    const base::FilePath& device,
    base::OnceCallback<void(StorageMonitor::EjectStatus)> callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    int iteration) {
  base::FilePath::StringType volume_name;
  base::FilePath::CharType drive_letter = device.value()[0];
  // Don't try to eject if the path isn't a simple one -- we're not
  // sure how to do that yet. Need to figure out how to eject volumes mounted
  // at not-just-drive-letter paths.
  if (drive_letter < L'A' || drive_letter > L'Z' ||
      device != device.DirName()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), StorageMonitor::EJECT_FAILURE));
    return;
  }
  base::SStringPrintf(&volume_name, L"\\\\.\\%lc:", drive_letter);

  base::win::ScopedHandle volume_handle(CreateFile(
      volume_name.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
      nullptr, OPEN_EXISTING, 0, nullptr));

  if (!volume_handle.IsValid()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), StorageMonitor::EJECT_FAILURE));
    return;
  }

  DWORD bytes_returned = 0;  // Unused, but necessary for ioctl's.

  // Lock the drive to be ejected (so that other processes can't open
  // files on it). If this fails, it means some other process has files
  // open on the device. Note that the lock is released when the volume
  // handle is closed, and this is done by the ScopedHandle above.
  BOOL locked = DeviceIoControl(volume_handle.Get(), FSCTL_LOCK_VOLUME, nullptr,
                                0, nullptr, 0, &bytes_returned, nullptr);
  if (!locked) {
    const int kNumLockRetries = 1;
    const base::TimeDelta kLockRetryInterval = base::Milliseconds(500);
    if (iteration < kNumLockRetries) {
      // Try again -- the lock may have been a transient one. This happens on
      // things like AV disk lock for some reason, or another process
      // transient disk lock.
      task_runner->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&EjectDeviceInThreadPool, device, std::move(callback),
                         task_runner, iteration + 1),
          kLockRetryInterval);
      return;
    }

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), StorageMonitor::EJECT_IN_USE));
    return;
  }

  // Unmount the device from the filesystem -- this will remove it from
  // the file picker, drive enumerations, etc.
  BOOL dismounted =
      DeviceIoControl(volume_handle.Get(), FSCTL_DISMOUNT_VOLUME, nullptr, 0,
                      nullptr, 0, &bytes_returned, nullptr);

  // Reached if we acquired a lock, but could not dismount. This might
  // occur if another process unmounted without locking. Call this OK,
  // since the volume is now unreachable.
  if (!dismounted) {
    DeviceIoControl(volume_handle.Get(), FSCTL_UNLOCK_VOLUME, nullptr, 0,
                    nullptr, 0, &bytes_returned, nullptr);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), StorageMonitor::EJECT_OK));
    return;
  }

  PREVENT_MEDIA_REMOVAL pmr_buffer;
  pmr_buffer.PreventMediaRemoval = FALSE;
  // Mark the device as safe to remove.
  if (!DeviceIoControl(volume_handle.Get(), IOCTL_STORAGE_MEDIA_REMOVAL,
                       &pmr_buffer, sizeof(PREVENT_MEDIA_REMOVAL), nullptr, 0,
                       &bytes_returned, nullptr)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), StorageMonitor::EJECT_FAILURE));
    return;
  }

  // Physically eject or soft-eject the device.
  if (!DeviceIoControl(volume_handle.Get(), IOCTL_STORAGE_EJECT_MEDIA, nullptr,
                       0, nullptr, 0, &bytes_returned, nullptr)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), StorageMonitor::EJECT_FAILURE));
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), StorageMonitor::EJECT_OK));
}

}  // namespace

VolumeMountWatcherWin::VolumeMountWatcherWin()
    : device_info_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      notifications_(nullptr) {}

// static
base::FilePath VolumeMountWatcherWin::DriveNumberToFilePath(int drive_number) {
  if (drive_number < 0 || drive_number > 25)
    return base::FilePath();
  std::wstring path(L"A:\\");
  path[0] += drive_number;
  return base::FilePath(path);
}

// In order to get all the weak pointers created on the UI thread, and doing
// synchronous Windows calls in the worker pool, this kicks off a chain of
// events which will
// a) Enumerate attached devices
// b) Create weak pointers for which to send completion signals from
// c) Retrieve metadata on the volumes and then
// d) Notify that metadata to listeners.
void VolumeMountWatcherWin::Init() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // When VolumeMountWatcherWin is created, the message pumps are not running
  // so a posted task from the constructor would never run. Therefore, do all
  // the initializations here.
  device_info_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, GetAttachedDevicesCallback(),
      base::BindOnce(&VolumeMountWatcherWin::AddDevicesOnUIThread,
                     weak_factory_.GetWeakPtr()));
}

void VolumeMountWatcherWin::AddDevicesOnUIThread(
    std::vector<base::FilePath> removable_devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (size_t i = 0; i < removable_devices.size(); i++) {
    if (base::Contains(pending_device_checks_, removable_devices[i]))
      continue;
    pending_device_checks_.insert(removable_devices[i]);
    device_info_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VolumeMountWatcherWin::RetrieveInfoForDeviceAndAdd,
                       removable_devices[i], GetDeviceDetailsCallback(),
                       weak_factory_.GetWeakPtr()));
  }
}

// static
void VolumeMountWatcherWin::RetrieveInfoForDeviceAndAdd(
    const base::FilePath& device_path,
    GetDeviceDetailsCallbackType get_device_details_callback,
    base::WeakPtr<VolumeMountWatcherWin> volume_watcher) {
  StorageInfo info;
  if (!std::move(get_device_details_callback).Run(device_path, &info)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&VolumeMountWatcherWin::DeviceCheckComplete,
                                  volume_watcher, device_path));
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&VolumeMountWatcherWin::HandleDeviceAttachEventOnUIThread,
                     volume_watcher, device_path, info));
}

void VolumeMountWatcherWin::DeviceCheckComplete(
    const base::FilePath& device_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  pending_device_checks_.erase(device_path);

  if (pending_device_checks_.empty()) {
    if (notifications_)
      notifications_->MarkInitialized();
  }
}

VolumeMountWatcherWin::GetAttachedDevicesCallbackType
    VolumeMountWatcherWin::GetAttachedDevicesCallback() const {
  return base::BindOnce(&GetAttachedDevices);
}

VolumeMountWatcherWin::GetDeviceDetailsCallbackType
    VolumeMountWatcherWin::GetDeviceDetailsCallback() const {
  return base::BindOnce(&GetDeviceDetails);
}

bool VolumeMountWatcherWin::GetDeviceInfo(const base::FilePath& device_path,
                                          StorageInfo* info) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(info);
  base::FilePath path(device_path);
  MountPointDeviceMetadataMap::const_iterator iter =
      device_metadata_.find(path);
  while (iter == device_metadata_.end() && path.DirName() != path) {
    path = path.DirName();
    iter = device_metadata_.find(path);
  }

  if (iter == device_metadata_.end())
    return false;

  *info = iter->second;
  return true;
}

void VolumeMountWatcherWin::OnWindowMessage(UINT event_type, LPARAM data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  switch (event_type) {
    case DBT_DEVICEARRIVAL: {
      if (IsLogicalVolumeStructure(data)) {
        DWORD unitmask = GetVolumeBitMaskFromBroadcastHeader(data);
        std::vector<base::FilePath> paths;
        for (int i = 0; unitmask; ++i, unitmask >>= 1) {
          if (!(unitmask & 0x01))
            continue;
          paths.push_back(DriveNumberToFilePath(i));
        }
        AddDevicesOnUIThread(paths);
      }
      break;
    }
    case DBT_DEVICEREMOVECOMPLETE: {
      if (IsLogicalVolumeStructure(data)) {
        DWORD unitmask = GetVolumeBitMaskFromBroadcastHeader(data);
        for (int i = 0; unitmask; ++i, unitmask >>= 1) {
          if (!(unitmask & 0x01))
            continue;
          HandleDeviceDetachEventOnUIThread(DriveNumberToFilePath(i).value());
        }
      }
      break;
    }
  }
}

void VolumeMountWatcherWin::OnMediaChange(WPARAM wparam, LPARAM lparam) {
  if (lparam == SHCNE_MEDIAINSERTED || lparam == SHCNE_MEDIAREMOVED) {
    struct _ITEMIDLIST* pidl = *reinterpret_cast<struct _ITEMIDLIST**>(
        wparam);
    wchar_t sPath[MAX_PATH];
    if (!SHGetPathFromIDList(pidl, sPath)) {
      DVLOG(1) << "MediaInserted: SHGetPathFromIDList failed";
      return;
    }
    switch (lparam) {
      case SHCNE_MEDIAINSERTED: {
        std::vector<base::FilePath> paths;
        paths.push_back(base::FilePath(sPath));
        AddDevicesOnUIThread(paths);
        break;
      }
      case SHCNE_MEDIAREMOVED: {
        HandleDeviceDetachEventOnUIThread(sPath);
        break;
      }
    }
  }
}

void VolumeMountWatcherWin::SetNotifications(
    StorageMonitor::Receiver* notifications) {
  notifications_ = notifications;
}

VolumeMountWatcherWin::~VolumeMountWatcherWin() {
  weak_factory_.InvalidateWeakPtrs();
}

void VolumeMountWatcherWin::HandleDeviceAttachEventOnUIThread(
    const base::FilePath& device_path,
    const StorageInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  device_metadata_[device_path] = info;

  if (notifications_)
    notifications_->ProcessAttach(info);

  DeviceCheckComplete(device_path);
}

void VolumeMountWatcherWin::HandleDeviceDetachEventOnUIThread(
    const std::wstring& device_location) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  MountPointDeviceMetadataMap::const_iterator device_info =
      device_metadata_.find(base::FilePath(device_location));
  // If the device isn't type removable (like a CD), it won't be there.
  if (device_info == device_metadata_.end())
    return;

  if (notifications_)
    notifications_->ProcessDetach(device_info->second.device_id());
  device_metadata_.erase(device_info);
}

void VolumeMountWatcherWin::EjectDevice(
    const std::string& device_id,
    base::OnceCallback<void(StorageMonitor::EjectStatus)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::FilePath device = MediaStorageUtil::FindDevicePathById(device_id);
  if (device.empty()) {
    std::move(callback).Run(StorageMonitor::EJECT_FAILURE);
    return;
  }
  if (device_metadata_.erase(device) == 0) {
    std::move(callback).Run(StorageMonitor::EJECT_FAILURE);
    return;
  }

  device_info_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&EjectDeviceInThreadPool, device, std::move(callback),
                     device_info_task_runner_, 0));
}

}  // namespace storage_monitor
