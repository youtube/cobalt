// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/file_chooser.h"

#import <Cocoa/Cocoa.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface FileTransferOpenPanelDelegate : NSObject <NSOpenSavePanelDelegate> {
}
- (BOOL)panel:(id)sender shouldEnableURL:(NSURL*)url;
- (BOOL)panel:(id)sender validateURL:(NSURL*)url error:(NSError**)outError;
@end

@implementation FileTransferOpenPanelDelegate
- (BOOL)panel:(id)sender shouldEnableURL:(NSURL*)url {
  return [url isFileURL];
}

- (BOOL)panel:(id)sender validateURL:(NSURL*)url error:(NSError**)outError {
  // Refuse to accept users closing the dialog with a key repeat, since the key
  // may have been first pressed while the user was looking at something else.
  if ([[NSApp currentEvent] type] == NSEventTypeKeyDown &&
      [[NSApp currentEvent] isARepeat]) {
    return NO;
  }

  return YES;
}
@end

namespace remoting {

namespace {

class FileChooserMac;

class MacFileChooserOnUiThread {
 public:
  MacFileChooserOnUiThread(
      scoped_refptr<base::SequencedTaskRunner> caller_task_runner,
      base::WeakPtr<FileChooserMac> file_chooser_mac);

  MacFileChooserOnUiThread(const MacFileChooserOnUiThread&) = delete;
  MacFileChooserOnUiThread& operator=(const MacFileChooserOnUiThread&) = delete;

  ~MacFileChooserOnUiThread();

  void Show();

 private:
  void RunCallback(FileChooser::Result result);

  base::scoped_nsobject<FileTransferOpenPanelDelegate> delegate_;
  base::scoped_nsobject<NSOpenPanel> open_panel_;
  scoped_refptr<base::SequencedTaskRunner> caller_task_runner_;
  base::WeakPtr<FileChooserMac> file_chooser_mac_;
};

class FileChooserMac : public FileChooser {
 public:
  FileChooserMac(scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
                 ResultCallback callback);

  FileChooserMac(const FileChooserMac&) = delete;
  FileChooserMac& operator=(const FileChooserMac&) = delete;

  ~FileChooserMac() override;

  // FileChooser implementation.
  void Show() override;

  void RunCallback(FileChooser::Result result);

 private:
  FileChooser::ResultCallback callback_;
  base::SequenceBound<MacFileChooserOnUiThread> mac_file_chooser_on_ui_thread_;
  base::WeakPtrFactory<FileChooserMac> weak_ptr_factory_;
};

MacFileChooserOnUiThread::MacFileChooserOnUiThread(
    scoped_refptr<base::SequencedTaskRunner> caller_task_runner,
    base::WeakPtr<FileChooserMac> file_chooser_mac)
    : delegate_([FileTransferOpenPanelDelegate new]),
      caller_task_runner_(std::move(caller_task_runner)),
      file_chooser_mac_(std::move(file_chooser_mac)) {}

MacFileChooserOnUiThread::~MacFileChooserOnUiThread() {
  if (open_panel_) {
    // Will synchronously invoke completion handler.
    [open_panel_ cancel:open_panel_];
  }
}

void MacFileChooserOnUiThread::Show() {
  DCHECK(!open_panel_);
  open_panel_.reset([NSOpenPanel openPanel], base::scoped_policy::RETAIN);
  [open_panel_
      setMessage:l10n_util::GetNSString(IDS_DOWNLOAD_FILE_DIALOG_TITLE)];
  [open_panel_ setAllowsMultipleSelection:NO];
  [open_panel_ setCanChooseFiles:YES];
  [open_panel_ setCanChooseDirectories:NO];
  [open_panel_ setDelegate:delegate_];
  [open_panel_ beginWithCompletionHandler:^(NSModalResponse result) {
    if (result == NSModalResponseOK) {
      NSURL* url = [open_panel_ URLs][0];
      if (![url isFileURL]) {
        // Delegate should prevent this.
        RunCallback(protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
      }
      RunCallback(base::mac::NSStringToFilePath([url path]));
    } else {
      RunCallback(protocol::MakeFileTransferError(
          FROM_HERE, protocol::FileTransfer_Error_Type_CANCELED));
    }
    open_panel_.reset();
  }];
  // Bring to front.
  [NSApp activateIgnoringOtherApps:YES];
}

void MacFileChooserOnUiThread::RunCallback(FileChooser::Result result) {
  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FileChooserMac::RunCallback, file_chooser_mac_,
                                std::move(result)));
}

FileChooserMac::FileChooserMac(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback)
    : callback_(std::move(callback)), weak_ptr_factory_(this) {
  mac_file_chooser_on_ui_thread_ =
      base::SequenceBound<MacFileChooserOnUiThread>(
          ui_task_runner, base::SequencedTaskRunner::GetCurrentDefault(),
          weak_ptr_factory_.GetWeakPtr());
}

void FileChooserMac::Show() {
  mac_file_chooser_on_ui_thread_.AsyncCall(&MacFileChooserOnUiThread::Show);
}

void FileChooserMac::RunCallback(FileChooser::Result result) {
  std::move(callback_).Run(std::move(result));
}

FileChooserMac::~FileChooserMac() = default;

}  // namespace

std::unique_ptr<FileChooser> FileChooser::Create(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback) {
  return std::make_unique<FileChooserMac>(std::move(ui_task_runner),
                                          std::move(callback));
}

}  // namespace remoting
