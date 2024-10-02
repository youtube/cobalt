// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_H_

#include <vector>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace file_manager::io_task {

enum class State {
  // Task has been queued, but not yet started.
  kQueued,

  // Task has started, but some initial scanning is performed.
  kScanning,

  // Task is currently running.
  kInProgress,

  // Task is currently paused.
  kPaused,

  // Task has been successfully completed.
  kSuccess,

  // Task has completed with errors.
  kError,

  // Task has failed to finish due to missing password.
  kNeedPassword,

  // Task has been canceled without finishing.
  kCancelled,
};

enum class OperationType {
  kCopy,
  kDelete,
  kEmptyTrash,
  kExtract,
  kMove,

  // This restores to the location supplied in the .trashinfo folder, recreating
  // the parent hierarchy as required. As .Trash folders reside on the same
  // filesystem as trashed files, this implies an intra filesystem move.
  kRestore,

  // This restores to a supplied destination only extracting the file name to
  // properly name the destination file. The destination folder is expected to
  // exist and items can be restored cross filesystem.
  kRestoreToDestination,
  kTrash,
  kZip,
};

// Unique identifier for any type of task.
using IOTaskId = uint64_t;

// I/O task state::PAUSED parameters. Currently, only CopyOrMoveIOTask can
// pause to resolve a file name conflict.
struct PauseParams {
  // The conflict file name.
  std::string conflict_name;

  // True if the conflict file name is a directory.
  bool conflict_is_directory = false;

  // Set true if there are potentially multiple conflicted file names.
  bool conflict_multiple = false;

  // The conflict copy or move target URL.
  std::string conflict_target_url;
};

// Resume I/O task parameters.
struct ResumeParams {
  // How to resolve a CopyOrMoveIOTask file name conflict: either 'keepboth'
  // or 'replace'.
  std::string conflict_resolve;

  // True if |conflict_resolve| should apply to future file name conflicts.
  bool conflict_apply_to_all = false;
};

// Represents the status of a particular entry in an I/O task.
struct EntryStatus {
  EntryStatus(storage::FileSystemURL file_url,
              absl::optional<base::File::Error> file_error);
  ~EntryStatus();

  EntryStatus(EntryStatus&& other);
  EntryStatus& operator=(EntryStatus&& other);

  // The entry FileSystemURL.
  storage::FileSystemURL url;

  // May be empty if the entry has not been fully processed yet.
  absl::optional<base::File::Error> error;

  // True if entry is a directory when its metadata is processed.
  bool is_directory = false;
};

// Represents the current progress of an I/O task.
class ProgressStatus {
 public:
  // Out-of-line constructors to appease the style linter.
  ProgressStatus();
  ProgressStatus(const ProgressStatus& other) = delete;
  ProgressStatus& operator=(const ProgressStatus& other) = delete;
  ~ProgressStatus();

  // Allow ProgressStatus to be moved.
  ProgressStatus(ProgressStatus&& other);
  ProgressStatus& operator=(ProgressStatus&& other);

  // True if the task is in kPaused state.
  bool IsPaused() const;

  // True if the task is in a terminal state and won't receive further updates.
  bool IsCompleted() const;

  // Returns a default method for obtaining the source name.
  std::string GetSourceName(Profile* profile) const;

  // Setter for the destination folder and the destination volume id.
  void SetDestinationFolder(storage::FileSystemURL folder,
                            Profile* profile = nullptr);
  const storage::FileSystemURL& GetDestinationFolder() const {
    return destination_folder_;
  }
  const std::string& GetDestinationVolumeId() const {
    return destination_volume_id_;
  }

  // Task state.
  State state;

  // I/O Operation type (e.g. copy, move).
  OperationType type;

  // Files the operation processes.
  std::vector<EntryStatus> sources;

  // The file name to use when reporting progress.
  std::string source_name;

  // Entries created by the I/O task. These files aren't necessarily related to
  // |sources|.
  std::vector<EntryStatus> outputs;

  // I/O task state::PAUSED parameters.
  PauseParams pause_params;

  // ProgressStatus over all |sources|.
  int64_t bytes_transferred = 0;

  // Total size of all |sources|.
  int64_t total_bytes = 0;

  // The task id for this progress status.
  IOTaskId task_id = 0;

  // The estimate time to finish the operation.
  double remaining_seconds = 0;

  // Whether notifications should be shown on progress status.
  bool show_notification = true;

 private:
  // Optional destination folder for operations that transfer files to a
  // directory (e.g. copy or move).
  storage::FileSystemURL destination_folder_;

  // Volume id of the destination directory for operations that transfer files
  // to a directory (e.g. copy or move).
  std::string destination_volume_id_;
};

// An IOTask represents an I/O operation over multiple files, and is responsible
// for executing the operation and providing progress/completion reports.
class IOTask {
 public:
  IOTask() = delete;
  IOTask(const IOTask& other) = delete;
  IOTask& operator=(const IOTask& other) = delete;
  virtual ~IOTask() = default;

  using ProgressCallback = base::RepeatingCallback<void(const ProgressStatus&)>;
  using CompleteCallback = base::OnceCallback<void(ProgressStatus)>;

  // Executes the task. |progress_callback| should be called every so often to
  // give updates, and |complete_callback| should be only called once at the end
  // to signify completion with a |kSuccess|, |kError| or |kCancelled| state.
  // |progress_callback| should be called on the same sequeuence Execute() was.
  virtual void Execute(ProgressCallback progress_callback,
                       CompleteCallback complete_callback) = 0;

  // Resumes a task.
  virtual void Resume(ResumeParams params);

  // Cancels the task. This should set the progress state to be |kCancelled|,
  // but not call any of Execute()'s callbacks. The task will be deleted
  // synchronously after this call returns.
  virtual void Cancel() = 0;

  // Gets the current progress status of the task.
  const ProgressStatus& progress() { return progress_; }

  // Sets the task id.
  void SetTaskID(IOTaskId task_id) { progress_.task_id = task_id; }

 protected:
  explicit IOTask(bool show_notification) {
    progress_.show_notification = show_notification;
  }

  ProgressStatus progress_;
};

// No-op IO Task for testing.
// TODO(austinct): Move into io_task_controller_unittest file when the other
// IOTasks have been implemented.
class DummyIOTask : public IOTask {
 public:
  DummyIOTask(std::vector<storage::FileSystemURL> source_urls,
              storage::FileSystemURL destination_folder,
              OperationType type,
              bool show_notification = true);
  ~DummyIOTask() override;

  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;

  void Cancel() override;

 private:
  void DoProgress();
  void DoComplete();

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  base::WeakPtrFactory<DummyIOTask> weak_ptr_factory_{this};
};

}  // namespace file_manager::io_task

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_H_
