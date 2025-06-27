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

#ifndef COBALT_SHELL_BROWSER_SHELL_DOWNLOAD_MANAGER_DELEGATE_H_
#define COBALT_SHELL_BROWSER_SHELL_DOWNLOAD_MANAGER_DELEGATE_H_

#include <stdint.h>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/download_manager_delegate.h"

namespace content {

class DownloadManager;

class ShellDownloadManagerDelegate : public DownloadManagerDelegate {
 public:
  ShellDownloadManagerDelegate();

  ShellDownloadManagerDelegate(const ShellDownloadManagerDelegate&) = delete;
  ShellDownloadManagerDelegate& operator=(const ShellDownloadManagerDelegate&) =
      delete;

  ~ShellDownloadManagerDelegate() override;

  void SetDownloadManager(DownloadManager* manager);

  void Shutdown() override;
  bool DetermineDownloadTarget(download::DownloadItem* download,
                               DownloadTargetCallback* callback) override;
  bool ShouldOpenDownload(download::DownloadItem* item,
                          DownloadOpenDelayedCallback callback) override;
  void GetNextId(DownloadIdCallback callback) override;

  // Inhibits prompting and sets the default download path.
  void SetDownloadBehaviorForTesting(
      const base::FilePath& default_download_path);

 private:
  friend class base::RefCountedThreadSafe<ShellDownloadManagerDelegate>;

  using FilenameDeterminedCallback =
      base::OnceCallback<void(const base::FilePath&)>;

  static void GenerateFilename(const GURL& url,
                               const std::string& content_disposition,
                               const std::string& suggested_filename,
                               const std::string& mime_type,
                               const base::FilePath& suggested_directory,
                               FilenameDeterminedCallback callback);
  void OnDownloadPathGenerated(uint32_t download_id,
                               DownloadTargetCallback callback,
                               const base::FilePath& suggested_path);
  void ChooseDownloadPath(uint32_t download_id,
                          DownloadTargetCallback callback,
                          const base::FilePath& suggested_path);

  raw_ptr<DownloadManager> download_manager_;
  base::FilePath default_download_path_;
  bool suppress_prompting_;
  base::WeakPtrFactory<ShellDownloadManagerDelegate> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_DOWNLOAD_MANAGER_DELEGATE_H_
