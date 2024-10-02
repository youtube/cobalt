// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEEDBACK_REPORT_H_
#define COMPONENTS_FEEDBACK_FEEDBACK_REPORT_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace feedback {

class FeedbackReport;

// Repeating since for every feedback report file on disk, the callback to
// queue it in the uploader needs to be invoked.
using QueueCallback =
    base::RepeatingCallback<void(scoped_refptr<FeedbackReport>)>;

// This class holds a feedback report. Once a report is created, a disk backup
// for it is created automatically. This backup needs to explicitly be
// deleted by calling DeleteReportOnDisk.
class FeedbackReport : public base::RefCountedThreadSafe<FeedbackReport> {
 public:
  // Creates a new feedback report with the contents of |data|.
  FeedbackReport(const base::FilePath& path,
                 const base::Time& upload_at,
                 std::unique_ptr<std::string> data,
                 scoped_refptr<base::SequencedTaskRunner> task_runner,
                 bool has_email);

  // Creates a feedback report from an existing one on-disk at |path|, the
  // |upload_at| time should be set after construction.
  FeedbackReport(base::FilePath path,
                 std::unique_ptr<std::string> data,
                 scoped_refptr<base::SequencedTaskRunner> task_runner,
                 bool has_email);

  FeedbackReport(const FeedbackReport&) = delete;
  FeedbackReport& operator=(const FeedbackReport&) = delete;

  // The ID of the product specific data for the crash report IDs as stored by
  // the feedback server.
  static const char kCrashReportIdsKey[];

  // The ID of the product specific data for the list of all crash report IDs as
  // stored by the feedback server. Only used for @google.com emails.
  static const char kAllCrashReportIdsKey[];

  // The ID of the product specific data for the system logs entry containing
  // mem_usage entries with tab names.
  static const char kMemUsageWithTabTitlesKey[];

  // The ID of the product specific data indicating whether the users want to be
  // contacted back with any additional questions or updates about the issue
  // they are reporting.
  static const char kFeedbackUserCtlConsentKey[];

  // Loads the reports still on disk and queues then using the given callback.
  // This call blocks on the file reads.
  static void LoadReportsAndQueue(const base::FilePath& user_dir,
                                  const QueueCallback& callback);

  // Stops the disk write of the report and deletes the report file if already
  // written.
  void DeleteReportOnDisk();

  const base::Time& upload_at() const { return upload_at_; }
  void set_upload_at(const base::Time& time) { upload_at_ = time; }
  const std::string& data() const { return *data_; }
  bool has_email() const { return has_email_; }
  scoped_refptr<base::SequencedTaskRunner> reports_task_runner() const {
    return reports_task_runner_;
  }

 private:
  friend class base::RefCountedThreadSafe<FeedbackReport>;
  virtual ~FeedbackReport();

  // Name of the file corresponding to this report.
  base::FilePath file_;

  // True iff the report is being sent with an email.
  const bool has_email_;

  base::FilePath reports_path_;
  base::Time upload_at_;  // Upload this report at or after this time.
  std::unique_ptr<std::string> data_;

  scoped_refptr<base::SequencedTaskRunner> reports_task_runner_;
};

}  // namespace feedback

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_REPORT_H_
