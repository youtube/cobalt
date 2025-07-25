// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_features.h"
#include "base/feature_list.h"

namespace extensions_features {

///////////////////////////////////////////////////////////////////////////////
// API Features
///////////////////////////////////////////////////////////////////////////////

// Controls the availability of the ReadingList API.
BASE_FEATURE(kApiReadingList,
             "ApiReadingList",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the availability of the runtime.getContexts() API.
BASE_FEATURE(kApiRuntimeGetContexts,
             "ApiRuntimeGetContexts",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the availability of the sidePanel.open() API.
BASE_FEATURE(kApiSidePanelOpen,
             "ApiSidePanelOpen",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the availability of the userScripts API.
BASE_FEATURE(kApiUserScripts,
             "ApiUserScripts",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the availability of navigation to file URLs.
BASE_FEATURE(kRestrictFileURLNavigation,
             "RestrictFileURLNavigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

///////////////////////////////////////////////////////////////////////////////
// Other Features
///////////////////////////////////////////////////////////////////////////////

// For historical reasons, this includes some APIs. Please don't add more.

// Whether extension contexts can use SharedArrayBuffers unconditionally (i.e.
// without requiring cross origin isolation).
// TODO(crbug.com/1184892): Flip this in M95.
BASE_FEATURE(kAllowSharedArrayBuffersUnconditionally,
             "AllowSharedArrayBuffersUnconditionally",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the UI in the install prompt which lets a user choose to withhold
// requested host permissions by default.
BASE_FEATURE(kAllowWithholdingExtensionPermissionsOnInstall,
             "AllowWithholdingExtensionPermissionsOnInstall",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, calls RenderFrame::SetAllowsCrossBrowsingInstanceFrameLookup() in
// DidCreateScriptContext() instead of DidCommitProvisionalLoad() to avoid
// creating the script context too early which can be bad for performance.
BASE_FEATURE(kAvoidEarlyExtensionScriptContextCreation,
             "AvoidEarlyExtensionScriptContextCreation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, then bad_message::ReceivedBadMessage will be called when
// browser receives an IPC from a content script and the IPC that unexpectedly
// claims to act on behalf of a given extension id, (i.e. even if the browser
// process things that renderer process never run content scripts from the
// extension).
BASE_FEATURE(kCheckingNoExtensionIdInExtensionIpcs,
             "EMF_NO_EXTENSION_ID_FOR_EXTENSION_SOURCE",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables support for the "match_origin_as_fallback" property in content
// scripts.
BASE_FEATURE(kContentScriptsMatchOriginAsFallback,
             "ContentScriptsMatchOriginAsFallback",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Determine if dynamic extension URLs are handled and redirected.
BASE_FEATURE(kExtensionDynamicURLRedirection,
             "ExtensionDynamicURLRedirection",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Side panel API availability.
BASE_FEATURE(kExtensionSidePanelIntegration,
             "ExtensionSidePanelIntegration",
             base::FEATURE_ENABLED_BY_DEFAULT);

// IsValidSourceUrl enforcement for ExtensionHostMsg_OpenChannelToExtension IPC.
BASE_FEATURE(kExtensionSourceUrlEnforcement,
             "ExtensionSourceUrlEnforcement",
             base::FEATURE_ENABLED_BY_DEFAULT);

// File Handlers.
BASE_FEATURE(kExtensionWebFileHandlers,
             "ExtensionWebFileHandlers",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, only manifest v3 extensions is allowed while v2 will be disabled.
// Note that this feature is now only checked by `ExtensionManagement` which
// represents enterprise extension configurations. Flip the feature will block
// mv2 extension by default but the error messages will improperly mention
// enterprise policy.
BASE_FEATURE(kExtensionsManifestV3Only,
             "ExtensionsManifestV3Only",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables enhanced site control for extensions and allowing the user to control
// site permissions.
BASE_FEATURE(kExtensionsMenuAccessControl,
             "ExtensionsMenuAccessControl",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, user permitted sites are granted access. This should only happen
// if kExtensionsMenuAccessControl is enabled, since it's the only entry point
// where user could set permitted sites.
BASE_FEATURE(kExtensionsMenuAccessControlWithPermittedSites,
             "ExtensionsMenuAccessControlWithPermittedSitesName",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Forces requests to go through WebRequestProxyingURLLoaderFactory.
BASE_FEATURE(kForceWebRequestProxyForTest,
             "ForceWebRequestProxyForTest",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Launches Native Host executables directly on Windows rather than using a
// cmd.exe process as a proxy.
BASE_FEATURE(kLaunchWindowsNativeHostsDirectly,
             "LaunchWindowsNativeHostsDirectly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether extensions can use the new favicon fetching in Manifest V3.
BASE_FEATURE(kNewExtensionFaviconHandling,
             "ExtensionsNewFaviconHandling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, allows APIs used by the webstore to be exposed on the URL for the
// new webstore.
BASE_FEATURE(kNewWebstoreDomain,
             "NewWebstoreDomain",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReportKeepaliveUkm,
             "ReportKeepaliveUkm",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, causes extensions to allow access to certain APIs only if the
// user is in the developer mode.
BASE_FEATURE(kRestrictDeveloperModeAPIs,
             "RestrictDeveloperModeAPIs",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Reports Extensions.WebRequest.KeepaliveRequestFinished when enabled.
// Automatically disable extensions not included in the Safe Browsing CRX
// allowlist if the user has turned on Enhanced Safe Browsing (ESB). The
// extensions can be disabled at ESB opt-in time or when an extension is moved
// out of the allowlist.
BASE_FEATURE(kSafeBrowsingCrxAllowlistAutoDisable,
             "SafeBrowsingCrxAllowlistAutoDisable",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether we show an install friction dialog when an Enhanced Safe
// Browsing user tries to install an extension that is not included in the
// Safe Browsing CRX allowlist. This feature also controls if we show a warning
// in 'chrome://extensions' for extensions not included in the allowlist.
BASE_FEATURE(kSafeBrowsingCrxAllowlistShowWarnings,
             "SafeBrowsingCrxAllowlistShowWarnings",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, causes Manifest V3 (and greater) extensions to use structured
// cloning (instead of JSON serialization) for extension messaging, except when
// communicating with native messaging hosts.
BASE_FEATURE(kStructuredCloningForMV3Messaging,
             "StructuredCloningForMV3Messaging",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, APIs of the Telemetry Extension platform that have pending
// approval will be enabled. Read more about the platform here:
// https://chromium.googlesource.com/chromium/src/+/master/docs/telemetry_extension/README.md.
BASE_FEATURE(kTelemetryExtensionPendingApprovalApi,
             "TelemetryExtensionPendingApprovalApi",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, calling WebRequestEventRouter::Get will return an instance of the
// per-BrowserContext WebRequestEventRouter instead of the global singleton
// ExtensionWebRequestEventRouter.
BASE_FEATURE(kUsePerBrowserContextWebRequestEventRouter,
             "kUsePerBrowserContextWebRequestEventRouter",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls the <webview> tag behaviour changes proposed as part of the guest
// view MPArch migration. See
// https://docs.google.com/document/d/1RVbtvklXUg9QCNvMT0r-1qDwJNeQFGoTCOD1Ur9mDa4/edit?usp=sharing
// for details.
BASE_FEATURE(kWebviewTagMPArchBehavior,
             "WebviewTagMPArchBehavior",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, extensions installed from .zip files (from dev mode) are changed
// from installing in base::TEMP_DIR to .../<profile_dir>/UnpackedExtensions and
// persist until removed by the user.
BASE_FEATURE(kExtensionsZipFileInstalledInProfileDir,
             "ExtensionsZipFileInstalledInProfileDir",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, extensions with service workers use an optimized event
// dispatching flow that does not start the worker for every event. It only
// starts a worker if it is not already running.
BASE_FEATURE(kExtensionsServiceWorkerOptimizedEventDispatch,
             "ExtensionsServiceWorkerOptimizedEventDispatch",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the button for visiting the chrome webstore in both the
// extensions menu in the app menu and the chrome://extensions sidebar will send
// the user to the new chrome webstore URL.
BASE_FEATURE(kNewWebstoreURL,
             "NewWebstoreURL",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables a relaxed rule count for "safe" dynqmic or session scoped rules above
// the current limit. If disabled, all dynamic and session scoped rules are
// treated as "safe" but the rule limit's value will be the stricter "unsafe"
// limit.
BASE_FEATURE(kDeclarativeNetRequestSafeRuleLimits,
             "DeclarativeNetRequestSafeDynamicRules",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace extensions_features
