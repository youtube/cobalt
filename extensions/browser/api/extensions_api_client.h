// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_EXTENSIONS_API_CLIENT_H_
#define EXTENSIONS_BROWSER_API_EXTENSIONS_API_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/api/clipboard/clipboard_api.h"
#include "extensions/browser/api/declarative_content/content_rules_registry.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/common/api/clipboard.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

class GURL;

namespace content {
class BrowserContext;
class WebContents;
}

namespace guest_view {
class GuestViewManagerDelegate;
}  // namespace guest_view

namespace value_store {
class ValueStoreFactory;
}

namespace extensions {

class AutomationInternalApiDelegate;
class AppViewGuestDelegate;
class ContentRulesRegistry;
class DevicePermissionsPrompt;
class DisplayInfoProvider;
class ExtensionOptionsGuest;
class ExtensionOptionsGuestDelegate;
class FeedbackPrivateDelegate;
class FileSystemDelegate;
class ManagementAPIDelegate;
class MediaPerceptionAPIDelegate;
class MessagingDelegate;
class MetricsPrivateDelegate;
class MimeHandlerViewGuest;
class MimeHandlerViewGuestDelegate;
class NonNativeFileSystemDelegate;
class RulesCacheDelegate;
class SupervisedUserExtensionsDelegate;
class ValueStoreCache;
class VirtualKeyboardDelegate;
struct WebRequestInfo;
class WebViewGuest;
class WebViewGuestDelegate;
class WebViewPermissionHelper;
class WebViewPermissionHelperDelegate;

#if BUILDFLAG(IS_CHROMEOS)
class ConsentProvider;
#endif  // BUILDFLAG(IS_CHROMEOS)

// Allows the embedder of the extensions module to customize its support for
// API features. The embedder must create a single instance in the browser
// process. Provides a default implementation that does nothing.
class ExtensionsAPIClient {
 public:
  // Construction sets the single instance.
  ExtensionsAPIClient();

  // Destruction clears the single instance.
  virtual ~ExtensionsAPIClient();

  // Returns the single instance of |this|.
  static ExtensionsAPIClient* Get();

  // Storage API support.

  // Add any additional value store caches (e.g. for chrome.storage.managed)
  // to |caches|. By default adds nothing.
  virtual void AddAdditionalValueStoreCaches(
      content::BrowserContext* context,
      const scoped_refptr<value_store::ValueStoreFactory>& factory,
      SettingsChangedCallback observer,
      std::map<settings_namespace::Namespace, ValueStoreCache*>* caches);

  // Attaches any extra web contents helpers (like ExtensionWebContentsObserver)
  // to |web_contents|.
  virtual void AttachWebContentsHelpers(content::WebContents* web_contents)
      const;

  // Returns true if the header should be hidden to extensions.
  virtual bool ShouldHideResponseHeader(const GURL& url,
                                        const std::string& header_name) const;

  // Returns true if the given |request| should be hidden from extensions. This
  // should be invoked on the UI thread.
  virtual bool ShouldHideBrowserNetworkRequest(
      content::BrowserContext* context,
      const WebRequestInfo& request) const;

  // Notifies that an extension failed to act on a network request because the
  // access to request was withheld.
  virtual void NotifyWebRequestWithheld(int render_process_id,
                                        int render_frame_id,
                                        const ExtensionId& extension_id);

  // Updates an extension's matched action count stored in an ExtensionAction
  // and optionally clears the extension's explicitly set badge text for the
  // tab specified by |tab_id|.
  virtual void UpdateActionCount(content::BrowserContext* context,
                                 const ExtensionId& extension_id,
                                 int tab_id,
                                 int action_count,
                                 bool clear_badge_text);

  // Clears an extension's matched action count stored in an ExtensionAction.
  virtual void ClearActionCount(content::BrowserContext* context,
                                const Extension& extension);

  // A method to open file: URL for tests.
  virtual void OpenFileUrl(const GURL& file_url,
                           content::BrowserContext* browser_context);

  // Creates the AppViewGuestDelegate.
  virtual AppViewGuestDelegate* CreateAppViewGuestDelegate() const;

  // Returns a delegate for ExtensionOptionsGuest. The caller owns the returned
  // ExtensionOptionsGuestDelegate.
  virtual ExtensionOptionsGuestDelegate* CreateExtensionOptionsGuestDelegate(
      ExtensionOptionsGuest* guest) const;

  // Returns a delegate for GuestViewManagerDelegate.
  virtual std::unique_ptr<guest_view::GuestViewManagerDelegate>
  CreateGuestViewManagerDelegate() const;

  // Creates a delegate for MimeHandlerViewGuest.
  virtual std::unique_ptr<MimeHandlerViewGuestDelegate>
  CreateMimeHandlerViewGuestDelegate(MimeHandlerViewGuest* guest) const;

  // Returns a delegate for some of WebViewGuest's behavior. The caller owns the
  // returned WebViewGuestDelegate.
  virtual WebViewGuestDelegate* CreateWebViewGuestDelegate(
      WebViewGuest* web_view_guest) const;

  // Returns a delegate for some of WebViewPermissionHelper's behavior. The
  // caller owns the returned WebViewPermissionHelperDelegate.
  virtual WebViewPermissionHelperDelegate*
  CreateWebViewPermissionHelperDelegate(
      WebViewPermissionHelper* web_view_permission_helper) const;

#if BUILDFLAG(IS_CHROMEOS)
  // Returns an interface for requesting consent for file system API. The caller
  // owns the returned ConsentProvider.
  virtual std::unique_ptr<ConsentProvider> CreateConsentProvider(
      content::BrowserContext* browser_context) const;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // TODO(wjmaclean): Remove this when (if) ContentRulesRegistry code moves
  // to extensions/browser/api.
  virtual scoped_refptr<ContentRulesRegistry> CreateContentRulesRegistry(
      content::BrowserContext* browser_context,
      RulesCacheDelegate* cache_delegate) const;

  // Creates a DevicePermissionsPrompt appropriate for the embedder.
  virtual std::unique_ptr<DevicePermissionsPrompt>
  CreateDevicePermissionsPrompt(content::WebContents* web_contents) const;

#if BUILDFLAG(IS_CHROMEOS)
  // Returns true if device policy allows detaching a given USB device.
  virtual bool ShouldAllowDetachingUsb(int vid, int pid) const;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Returns a delegate for some of VirtualKeyboardAPI's behavior.
  virtual std::unique_ptr<VirtualKeyboardDelegate>
  CreateVirtualKeyboardDelegate(content::BrowserContext* browser_context) const;

  // Creates a delegate for handling the management extension api.
  virtual ManagementAPIDelegate* CreateManagementAPIDelegate() const;

  // Creates a delegate for calling into the SupervisedUserService from the
  // Management API.
  virtual std::unique_ptr<SupervisedUserExtensionsDelegate>
  CreateSupervisedUserExtensionsDelegate(
      content::BrowserContext* browser_context) const;

  // Creates and returns the DisplayInfoProvider used by the
  // chrome.system.display extension API.
  virtual std::unique_ptr<DisplayInfoProvider> CreateDisplayInfoProvider()
      const;

  // If supported by the embedder, returns a delegate for embedder-dependent
  // MetricsPrivateAPI behavior.
  virtual MetricsPrivateDelegate* GetMetricsPrivateDelegate();

  // Returns a delegate for embedder-specific chrome.fileSystem behavior.
  virtual FileSystemDelegate* GetFileSystemDelegate();

  // Returns a delegate for embedder-specific extension messaging.
  virtual MessagingDelegate* GetMessagingDelegate();

  // Returns a delegate for the chrome.feedbackPrivate API.
  virtual FeedbackPrivateDelegate* GetFeedbackPrivateDelegate();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If supported by the embedder, returns a delegate for querying non-native
  // file systems.
  virtual NonNativeFileSystemDelegate* GetNonNativeFileSystemDelegate();

  // Returns a delegate for embedder-specific chrome.mediaPerceptionPrivate API
  // behavior.
  virtual MediaPerceptionAPIDelegate* GetMediaPerceptionAPIDelegate();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
  // Saves image data on clipboard.
  virtual void SaveImageDataToClipboard(
      std::vector<uint8_t> image_data,
      api::clipboard::ImageType type,
      AdditionalDataItemList additional_items,
      base::OnceClosure success_callback,
      base::OnceCallback<void(const std::string&)> error_callback);
#endif  // BUILDFLAG(IS_CHROMEOS)

  virtual AutomationInternalApiDelegate* GetAutomationInternalApiDelegate();

  // Gets keyed service factories that are used in the other methods on this
  // class.
  virtual std::vector<KeyedServiceBaseFactory*> GetFactoryDependencies();

  // NOTE: If this interface gains too many methods (perhaps more than 20) it
  // should be split into one interface per API.
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_EXTENSIONS_API_CLIENT_H_
