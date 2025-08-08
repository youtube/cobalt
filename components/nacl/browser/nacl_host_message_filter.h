// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_BROWSER_NACL_HOST_MESSAGE_FILTER_H_
#define COMPONENTS_NACL_BROWSER_NACL_HOST_MESSAGE_FILTER_H_

#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/nacl/browser/nacl_browser_delegate.h"
#include "content/public/browser/browser_message_filter.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

class GURL;

namespace nacl {
struct NaClLaunchParams;
struct NaClResourcePrefetchResult;
struct PnaclCacheInfo;
}

namespace nacl {

// This class filters out incoming Chrome-specific IPC messages for the renderer
// process on the IPC thread.
class NaClHostMessageFilter : public content::BrowserMessageFilter {
 public:
  NaClHostMessageFilter(int render_process_id,
                        bool is_off_the_record,
                        const base::FilePath& profile_directory);

  NaClHostMessageFilter(const NaClHostMessageFilter&) = delete;
  NaClHostMessageFilter& operator=(const NaClHostMessageFilter&) = delete;

  // content::BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelClosing() override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;

  int render_process_id() { return render_process_id_; }
  bool off_the_record() { return off_the_record_; }
  const base::FilePath& profile_directory() const { return profile_directory_; }

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<NaClHostMessageFilter>;

  ~NaClHostMessageFilter() override;

  void OnLaunchNaCl(const NaClLaunchParams& launch_params,
                    IPC::Message* reply_msg);
  void BatchOpenResourceFiles(
      const nacl::NaClLaunchParams& launch_params,
      IPC::Message* reply_msg,
      ppapi::PpapiPermissions permissions,
      NaClBrowserDelegate::MapUrlToLocalFilePathCallback map_url_callback);
  void LaunchNaClContinuation(
      const nacl::NaClLaunchParams& launch_params,
      IPC::Message* reply_msg,
      NaClBrowserDelegate::MapUrlToLocalFilePathCallback map_url_callback);
  void LaunchNaClContinuationOnUIThread(
      const nacl::NaClLaunchParams& launch_params,
      IPC::Message* reply_msg,
      const std::vector<NaClResourcePrefetchResult>& prefetched_resource_files,
      ppapi::PpapiPermissions permissions,
      NaClBrowserDelegate::MapUrlToLocalFilePathCallback map_url_callback);
  void OnGetReadonlyPnaclFd(const std::string& filename,
                            bool is_executable,
                            IPC::Message* reply_msg);
  void OnNaClCreateTemporaryFile(IPC::Message* reply_msg);
  void OnNaClGetNumProcessors(int* num_processors);
  void OnGetNexeFd(int pp_instance, const PnaclCacheInfo& cache_info);
  void OnTranslationFinished(int instance, bool success);
  void OnMissingArchError(int render_view_id);
  void OnOpenNaClExecutable(int render_frame_id,
                            const GURL& file_url,
                            IPC::Message* reply_msg);
  void SyncReturnTemporaryFile(IPC::Message* reply_msg,
                               base::File file);
  void AsyncReturnTemporaryFile(int pp_instance,
                                const base::File& file,
                                bool is_hit);
  void OnNaClDebugEnabledForURL(const GURL& nmf_url, bool* should_debug);

  int render_process_id_;

  // off_the_record_ is copied from the profile partly so that it can be
  // read on the IO thread.
  bool off_the_record_;
  base::FilePath profile_directory_;

  base::WeakPtrFactory<NaClHostMessageFilter> weak_ptr_factory_{this};
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_BROWSER_NACL_HOST_MESSAGE_FILTER_H_
