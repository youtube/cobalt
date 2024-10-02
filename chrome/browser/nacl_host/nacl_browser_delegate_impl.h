// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_BROWSER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_NACL_HOST_NACL_BROWSER_DELEGATE_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/nacl/browser/nacl_browser_delegate.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/url_pattern.h"
#endif

class ProfileManager;

class NaClBrowserDelegateImpl : public NaClBrowserDelegate {
 public:
  explicit NaClBrowserDelegateImpl(ProfileManager* profile_manager);

  NaClBrowserDelegateImpl(const NaClBrowserDelegateImpl&) = delete;
  NaClBrowserDelegateImpl& operator=(const NaClBrowserDelegateImpl&) = delete;

  ~NaClBrowserDelegateImpl() override;

  void ShowMissingArchInfobar(int render_process_id,
                              int render_frame_id) override;
  bool DialogsAreSuppressed() override;
  bool GetCacheDirectory(base::FilePath* cache_dir) override;
  bool GetPluginDirectory(base::FilePath* plugin_dir) override;
  bool GetPnaclDirectory(base::FilePath* pnacl_dir) override;
  bool GetUserDirectory(base::FilePath* user_dir) override;
  std::string GetVersionString() const override;
  ppapi::host::HostFactory* CreatePpapiHostFactory(
      content::BrowserPpapiHost* ppapi_host) override;
  MapUrlToLocalFilePathCallback GetMapUrlToLocalFilePathCallback(
      const base::FilePath& profile_directory) override;
  void SetDebugPatterns(const std::string& debug_patterns) override;
  bool URLMatchesDebugPatterns(const GURL& manifest_url) override;

 private:
  // Creates a NaCl infobar and delegate for the given render process and frame
  // IDs.  Should be called on the UI thread.
  static void CreateInfoBarOnUiThread(int render_process_id,
                                      int render_frame_id);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::vector<URLPattern> debug_patterns_;
#endif

  raw_ptr<ProfileManager> profile_manager_;
  bool inverse_debug_patterns_;
};

#endif  // CHROME_BROWSER_NACL_HOST_NACL_BROWSER_DELEGATE_IMPL_H_
