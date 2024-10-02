// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/platform_notification_service_proxy.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "content/browser/notifications/devtools_event_logging.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

PlatformNotificationServiceProxy::PlatformNotificationServiceProxy(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    BrowserContext* browser_context)
    : service_worker_context_(service_worker_context),
      browser_context_(browser_context),
      notification_service_(browser_context->GetPlatformNotificationService()) {
}

PlatformNotificationServiceProxy::~PlatformNotificationServiceProxy() = default;

void PlatformNotificationServiceProxy::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  weak_ptr_factory_ui_.InvalidateWeakPtrs();
}

base::WeakPtr<PlatformNotificationServiceProxy>
PlatformNotificationServiceProxy::AsWeakPtr() {
  return weak_ptr_factory_ui_.GetWeakPtr();
}

void PlatformNotificationServiceProxy::DoDisplayNotification(
    const NotificationDatabaseData& data,
    const GURL& service_worker_scope,
    DisplayResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (notification_service_) {
    notification_service_->DisplayPersistentNotification(
        data.notification_id, service_worker_scope, data.origin,
        data.notification_data,
        data.notification_resources.value_or(blink::NotificationResources()));
    notifications::LogNotificationDisplayedEventToDevTools(browser_context_,
                                                           data);
  }
  std::move(callback).Run(/* success= */ true, data.notification_id);
}

void PlatformNotificationServiceProxy::VerifyServiceWorkerScope(
    const NotificationDatabaseData& data,
    DisplayResultCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::OnceClosure task;

  if (status == blink::ServiceWorkerStatusCode::kOk &&
      registration->key().origin().GetURL() == data.origin) {
    DoDisplayNotification(data, registration->scope(), std::move(callback));
  } else {
    std::move(callback).Run(/* success= */ false, /* notification_id= */ "");
  }
}

void PlatformNotificationServiceProxy::DisplayNotification(
    const NotificationDatabaseData& data,
    DisplayResultCallback callback) {
  if (!service_worker_context_) {
    GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
        ->PostTask(FROM_HERE,
                   base::BindOnce(
                       &PlatformNotificationServiceProxy::DoDisplayNotification,
                       AsWeakPtr(), data, GURL(), std::move(callback)));
    return;
  }

  GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ServiceWorkerContextWrapper::FindReadyRegistrationForId,
              service_worker_context_, data.service_worker_registration_id,
              blink::StorageKey::CreateFirstParty(
                  url::Origin::Create(data.origin)),
              base::BindOnce(
                  &PlatformNotificationServiceProxy::VerifyServiceWorkerScope,
                  weak_ptr_factory_io_.GetWeakPtr(), data,
                  std::move(callback))));
}

void PlatformNotificationServiceProxy::CloseNotifications(
    const std::set<std::string>& notification_ids) {
  if (!notification_service_)
    return;
  GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &PlatformNotificationServiceProxy::DoCloseNotifications,
                     AsWeakPtr(), notification_ids));
}

void PlatformNotificationServiceProxy::DoCloseNotifications(
    const std::set<std::string>& notification_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const std::string& notification_id : notification_ids)
    notification_service_->ClosePersistentNotification(notification_id);
}

void PlatformNotificationServiceProxy::ScheduleTrigger(base::Time timestamp) {
  if (!notification_service_)
    return;
  GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&PlatformNotificationServiceProxy::DoScheduleTrigger,
                         AsWeakPtr(), timestamp));
}

void PlatformNotificationServiceProxy::DoScheduleTrigger(base::Time timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notification_service_->ScheduleTrigger(timestamp);
}

void PlatformNotificationServiceProxy::ScheduleNotification(
    const NotificationDatabaseData& data) {
  DCHECK(data.notification_data.show_trigger_timestamp.has_value());
  if (!notification_service_)
    return;
  GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &PlatformNotificationServiceProxy::DoScheduleNotification,
                     AsWeakPtr(), data));
}

void PlatformNotificationServiceProxy::DoScheduleNotification(
    const NotificationDatabaseData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time show_trigger_timestamp =
      data.notification_data.show_trigger_timestamp.value();
  notifications::LogNotificationScheduledEventToDevTools(
      browser_context_, data, show_trigger_timestamp);
  notification_service_->ScheduleTrigger(show_trigger_timestamp);
}

base::Time PlatformNotificationServiceProxy::GetNextTrigger() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!notification_service_)
    return base::Time::Max();
  return notification_service_->ReadNextTriggerTimestamp();
}

void PlatformNotificationServiceProxy::RecordNotificationUkmEvent(
    const NotificationDatabaseData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!notification_service_)
    return;
  notification_service_->RecordNotificationUkmEvent(data);
}

bool PlatformNotificationServiceProxy::ShouldLogClose(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return notifications::ShouldLogNotificationEventToDevTools(browser_context_,
                                                             origin);
}

void PlatformNotificationServiceProxy::LogClose(
    const NotificationDatabaseData& data) {
  GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&PlatformNotificationServiceProxy::DoLogClose,
                                AsWeakPtr(), data));
}

void PlatformNotificationServiceProxy::DoLogClose(
    const NotificationDatabaseData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notifications::LogNotificationClosedEventToDevTools(browser_context_, data);
}

}  // namespace content
