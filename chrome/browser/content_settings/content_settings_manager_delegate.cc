// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_manager_delegate.h"

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif

namespace chrome {
namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
void OnFileSystemAccessedInGuestViewContinuation(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    base::OnceCallback<void(bool)> callback,
    bool allowed) {
  content_settings::PageSpecificContentSettings::StorageAccessed(
      content_settings::mojom::ContentSettingsManager::StorageType::FILE_SYSTEM,
      render_process_id, render_frame_id, url, !allowed);
  std::move(callback).Run(allowed);
}

void OnFileSystemAccessedInGuestView(int render_process_id,
                                     int render_frame_id,
                                     const GURL& url,
                                     bool allowed,
                                     base::OnceCallback<void(bool)> callback) {
  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromRenderFrameHostId(
          content::GlobalRenderFrameHostId(render_process_id, render_frame_id));
  auto continuation = base::BindOnce(
      &OnFileSystemAccessedInGuestViewContinuation, render_process_id,
      render_frame_id, url, std::move(callback));
  if (!web_view_permission_helper) {
    std::move(continuation).Run(allowed);
    return;
  }
  web_view_permission_helper->RequestFileSystemPermission(
      url, allowed, std::move(continuation));
}

void PostTaskOnSequence(scoped_refptr<base::SequencedTaskRunner> task_runner,
                        base::OnceCallback<void(bool)> callback,
                        bool result) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), result));
}
#endif

}  // namespace

ContentSettingsManagerDelegate::ContentSettingsManagerDelegate() = default;

ContentSettingsManagerDelegate::~ContentSettingsManagerDelegate() = default;

scoped_refptr<content_settings::CookieSettings>
ContentSettingsManagerDelegate::GetCookieSettings(
    content::BrowserContext* browser_context) {
  return CookieSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
}

bool ContentSettingsManagerDelegate::AllowStorageAccess(
    int render_process_id,
    int render_frame_id,
    content_settings::mojom::ContentSettingsManager::StorageType storage_type,
    const GURL& url,
    bool allowed,
    base::OnceCallback<void(bool)>* callback) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (storage_type == content_settings::mojom::ContentSettingsManager::
                          StorageType::FILE_SYSTEM &&
      extensions::WebViewRendererState::GetInstance()->IsGuest(
          render_process_id)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &OnFileSystemAccessedInGuestView, render_process_id,
            render_frame_id, url, allowed,
            base::BindOnce(&PostTaskOnSequence,
                           base::SequencedTaskRunner::GetCurrentDefault(),
                           std::move(*callback))));

    return true;
  }
#endif
  return false;
}

std::unique_ptr<content_settings::ContentSettingsManagerImpl::Delegate>
ContentSettingsManagerDelegate::Clone() {
  return std::make_unique<ContentSettingsManagerDelegate>();
}

}  // namespace chrome
