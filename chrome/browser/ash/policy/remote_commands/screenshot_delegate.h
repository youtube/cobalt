// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_SCREENSHOT_DELEGATE_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_SCREENSHOT_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_screenshot_job.h"
#include "chrome/browser/ash/policy/uploading/upload_job.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/snapshot/snapshot.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {

class BrowserPolicyConnectorAsh;

// An implementation of the |DeviceCommandScreenshotJob::Delegate| that uses
// aura's GrabWindowSnapshotAsPNG() to acquire the window snapshot.
class ScreenshotDelegate : public DeviceCommandScreenshotJob::Delegate {
 public:
  // `shared_url_loader_factory` must be non-null.
  // `browser_policy_connector` must be non-null and must outlive `this`.
  ScreenshotDelegate(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      BrowserPolicyConnectorAsh* browser_policy_connector);

  ScreenshotDelegate(const ScreenshotDelegate&) = delete;
  ScreenshotDelegate& operator=(const ScreenshotDelegate&) = delete;

  ~ScreenshotDelegate() override;

  // DeviceCommandScreenshotJob::Delegate:
  bool IsScreenshotAllowed() override;
  void TakeSnapshot(gfx::NativeWindow window,
                    const gfx::Rect& source_rect,
                    OnScreenshotTakenCallback upload) override;
  std::unique_ptr<UploadJob> CreateUploadJob(
      const GURL& upload_url,
      UploadJob::Delegate* delegate) override;

 private:
  void OnScreenshotTaken(OnScreenshotTakenCallback callback,
                         scoped_refptr<base::RefCountedMemory> png_data);

  const scoped_refptr<network::SharedURLLoaderFactory>
      shared_url_loader_factory_;
  const raw_ref<BrowserPolicyConnectorAsh> browser_policy_connector_;

  base::WeakPtrFactory<ScreenshotDelegate> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_SCREENSHOT_DELEGATE_H_
