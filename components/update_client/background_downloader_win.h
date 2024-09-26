// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_BACKGROUND_DOWNLOADER_WIN_H_
#define COMPONENTS_UPDATE_CLIENT_BACKGROUND_DOWNLOADER_WIN_H_

#include <bits.h>
#include <windows.h>
#include <wrl/client.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/update_client/crx_downloader.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace update_client {

// Implements a downloader in terms of the BITS service. The public interface
// of this class and the CrxDownloader overrides are expected to be called
// from the main sequence. The rest of the class code runs on a sequenced
// task runner. The task runner must initialize COM.
//
// This class manages a COM client for Windows BITS. The client uses polling,
// triggered by an one-shot timer, to get state updates from BITS. Since the
// timer has sequence afinity, the callbacks from the timer are delegated to
// a sequenced task runner, which handles all client COM interaction with
// the BITS service.
class BackgroundDownloader : public CrxDownloader {
 public:
  explicit BackgroundDownloader(scoped_refptr<CrxDownloader> successor);

 private:
  // Overrides for CrxDownloader.
  ~BackgroundDownloader() override;
  void DoStartDownload(const GURL& url) override;

  // Called asynchronously on the |com_task_runner_| at different stages during
  // the download. |OnDownloading| can be called multiple times.
  // |EndDownload| switches the execution flow from the |com_task_runner_| to
  // the main sequence.
  void BeginDownload(const GURL& url);
  void OnDownloading();
  void EndDownload(HRESULT hr);

  HRESULT BeginDownloadHelper(const GURL& url);

  // Handles the job state transitions to a final state. Returns true always
  // since the download has reached a final state and no further processing for
  // this download is needed.
  bool OnStateTransferred();
  bool OnStateError();
  bool OnStateCancelled();
  bool OnStateAcknowledged();

  // Handles the transition to a transient state where the job is in the
  // queue but not actively transferring data. Returns true if the download has
  // been in this state for too long and it will be abandoned, or false, if
  // further processing for this download is needed.
  bool OnStateQueued();

  // Handles the job state transition to a transient error state, which may or
  // may not be considered final, depending on the error. Returns true if
  // the state is final, or false, if the download is allowed to continue.
  bool OnStateTransientError();

  // Handles the job state corresponding to transferring data. Returns false
  // always since this is never a final state.
  bool OnStateTransferring();

  void StartTimer();
  void OnTimer();

  // Creates or opens a job for the given url and queues it up. Returns S_OK if
  // a new job was created or S_FALSE if an existing job for the |url| was found
  // in the BITS queue.
  HRESULT QueueBitsJob(const GURL& url,
                       Microsoft::WRL::ComPtr<IBackgroundCopyJob>* job);
  HRESULT CreateOrOpenJob(const GURL& url,
                          Microsoft::WRL::ComPtr<IBackgroundCopyJob>* job);
  HRESULT InitializeNewJob(
      const Microsoft::WRL::ComPtr<IBackgroundCopyJob>& job,
      const GURL& url);

  // Returns true if at the time of the call, it appears that the job
  // has not been making progress toward completion.
  bool IsStuck();

  // Makes the downloaded file available to the caller by renaming the
  // temporary file to its destination and removing it from the BITS queue.
  HRESULT CompleteJob();

  // Revokes the interface pointers from GIT.
  HRESULT ClearGit();

  // Updates the BITS interface pointers so that the COM functions of these
  // interfaces can be called in this COM STA apartment.
  HRESULT UpdateInterfacePointers();

  // Resets the BITS interface pointers.
  void ResetInterfacePointers();

  // Returns the number of jobs in the BITS queue which were created by this
  // downloader.
  HRESULT GetBackgroundDownloaderJobCount(size_t* num_jobs);

  // Cleans up incompleted jobs that are too old.
  void CleanupStaleJobs();

  // This sequence checker is bound to the main sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Executes blocking COM calls to BITS.
  scoped_refptr<base::SequencedTaskRunner> com_task_runner_;

  // The timer has sequence affinity. This member is created and destroyed
  // on the main task runner.
  std::unique_ptr<base::OneShotTimer> timer_;

  DWORD git_cookie_bits_manager_;
  DWORD git_cookie_job_;

  // COM interface pointers are valid for the thread that called
  // |UpdateInterfacePointers| to get pointers to COM proxies, which are valid
  // for that thread only.
  Microsoft::WRL::ComPtr<IBackgroundCopyManager> bits_manager_;
  Microsoft::WRL::ComPtr<IBackgroundCopyJob> job_;

  // Contains the time when the download of the current url has started.
  base::TimeTicks download_start_time_;

  // Contains the time when the BITS job is last seen making progress.
  base::TimeTicks job_stuck_begin_time_;

  // Contains the path of the downloaded file if the download was successful.
  base::FilePath response_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_BACKGROUND_DOWNLOADER_WIN_H_
