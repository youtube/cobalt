// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_SHELL_BROWSER_SHELL_FILE_SELECT_HELPER_H_
#define COBALT_SHELL_BROWSER_SHELL_FILE_SELECT_HELPER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom-forward.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace ui {
struct SelectedFileInfo;
}

namespace content {

class FileSelectListener;
class RenderFrameHost;
class WebContents;

// This class handles file-selection requests coming from renderer processes.
// It implements both the initialisation and listener functions for
// file-selection dialogs.
//
// Since ShellFileSelectHelper listens to observations of a widget, it needs to
// live on and be destroyed on the UI thread. References to
// ShellFileSelectHelper may be passed on to other threads.
class ShellFileSelectHelper
    : public base::RefCountedThreadSafe<ShellFileSelectHelper,
                                        BrowserThread::DeleteOnUIThread>,
      public ui::SelectFileDialog::Listener {
 public:
  ShellFileSelectHelper(const ShellFileSelectHelper&) = delete;
  ShellFileSelectHelper& operator=(const ShellFileSelectHelper&) = delete;

  // Show the file chooser dialog.
  static void RunFileChooser(content::RenderFrameHost* render_frame_host,
                             scoped_refptr<FileSelectListener> listener,
                             const blink::mojom::FileChooserParams& params);

 private:
  friend class base::RefCountedThreadSafe<ShellFileSelectHelper>;
  friend class base::DeleteHelper<ShellFileSelectHelper>;
  friend struct content::BrowserThread::DeleteOnThread<BrowserThread::UI>;

  ShellFileSelectHelper();
  ~ShellFileSelectHelper() override;

  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<FileSelectListener> listener,
                      blink::mojom::FileChooserParamsPtr params);

  // Cleans up and releases this instance. This must be called after the last
  // callback is received from the file chooser dialog.
  void RunFileChooserEnd();

  // SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectedWithExtraInfo(const ui::SelectedFileInfo& file,
                                 int index,
                                 void* params) override;
  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override;
  void MultiFilesSelectedWithExtraInfo(
      const std::vector<ui::SelectedFileInfo>& files,
      void* params) override;
  void FileSelectionCanceled(void* params) override;

  // This method is called after the user has chosen the file(s) in the UI in
  // order to process and filter the list before returning the final result to
  // the caller.
  void ConvertToFileChooserFileInfoList(
      const std::vector<ui::SelectedFileInfo>& files);

  // A weak pointer to the WebContents of the RenderFrameHost, for life checks.
  base::WeakPtr<WebContents> web_contents_;

  // |listener_| receives the result of the ShellFileSelectHelper.
  scoped_refptr<FileSelectListener> listener_;

  // Dialog box used for choosing files to upload from file form fields.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  std::unique_ptr<ui::SelectFileDialog::FileTypeInfo> select_file_types_;

  // The type of file dialog last shown.
  ui::SelectFileDialog::Type dialog_type_ =
      ui::SelectFileDialog::SELECT_OPEN_FILE;

  // The mode of file dialog last shown.
  blink::mojom::FileChooserParams::Mode dialog_mode_ =
      blink::mojom::FileChooserParams::Mode::kOpen;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_FILE_SELECT_HELPER_H_
