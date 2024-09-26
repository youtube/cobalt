// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/renderer/content_settings_agent_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings.mojom.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_view.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

using blink::WebDocument;
using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebSecurityOrigin;
using blink::WebString;
using blink::WebURL;
using blink::WebView;

namespace content_settings {
namespace {
bool IsFrameWithOpaqueOrigin(WebFrame* frame) {
  // Storage access is keyed off the top origin and the frame's origin.
  // It will be denied any opaque origins so have this method to return early
  // instead of making a Sync IPC call.
  return frame->GetSecurityOrigin().IsOpaque() ||
         frame->Top()->GetSecurityOrigin().IsOpaque();
}

}  // namespace

ContentSettingsAgentImpl::Delegate::~Delegate() = default;

bool ContentSettingsAgentImpl::Delegate::IsSchemeAllowlisted(
    const std::string& scheme) {
  return false;
}

absl::optional<bool>
ContentSettingsAgentImpl::Delegate::AllowReadFromClipboard() {
  return absl::nullopt;
}

absl::optional<bool>
ContentSettingsAgentImpl::Delegate::AllowWriteToClipboard() {
  return absl::nullopt;
}

absl::optional<bool> ContentSettingsAgentImpl::Delegate::AllowMutationEvents() {
  return absl::nullopt;
}

ContentSettingsAgentImpl::ContentSettingsAgentImpl(
    content::RenderFrame* render_frame,
    bool should_allowlist,
    std::unique_ptr<Delegate> delegate)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<ContentSettingsAgentImpl>(
          render_frame),
      should_allowlist_(should_allowlist),
      delegate_(std::move(delegate)) {
  DCHECK(delegate_);
  ClearBlockedContentSettings();
  render_frame->GetWebFrame()->SetContentSettingsClient(this);

  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<mojom::ContentSettingsAgent>(base::BindRepeating(
          &ContentSettingsAgentImpl::OnContentSettingsAgentRequest,
          base::Unretained(this)));

  content::RenderFrame* main_frame = render_frame->GetMainRenderFrame();
  // TODO(nasko): The main frame is not guaranteed to be in the same process
  // with this frame with --site-per-process. This code needs to be updated
  // to handle this case. See https://crbug.com/496670.
  if (main_frame && main_frame != render_frame) {
    // Copy all the settings from the main render frame to avoid race conditions
    // when initializing this data. See https://crbug.com/333308.
    ContentSettingsAgentImpl* parent =
        ContentSettingsAgentImpl::Get(main_frame);
    allow_running_insecure_content_ = parent->allow_running_insecure_content_;
  }
}

ContentSettingsAgentImpl::~ContentSettingsAgentImpl() = default;

mojom::ContentSettingsManager&
ContentSettingsAgentImpl::GetContentSettingsManager() {
  if (!content_settings_manager_)
    BindContentSettingsManager(&content_settings_manager_);
  return *content_settings_manager_;
}

void ContentSettingsAgentImpl::DidBlockContentType(
    ContentSettingsType settings_type) {
  bool newly_blocked = content_blocked_.insert(settings_type).second;
  if (newly_blocked)
    GetContentSettingsManager().OnContentBlocked(routing_id(), settings_type);
}

namespace {
template <typename URL>
ContentSetting GetContentSettingFromRules(
    const ContentSettingsForOneType& rules,
    const URL& secondary_url) {
  // If there is only one rule, it's the default rule and we don't need to match
  // the patterns.
  if (rules.size() == 1) {
    DCHECK(rules[0].primary_pattern == ContentSettingsPattern::Wildcard());
    DCHECK(rules[0].secondary_pattern == ContentSettingsPattern::Wildcard());
    return rules[0].GetContentSetting();
  }
  // The primary pattern has already been matched in the browser process (see
  // PageSpecificContentSettings::WebContentsHandler::ReadyToCommitNavigation),
  // and the rules received by the renderer are a subset of the existing rules
  // that have the correct primary pattern. So we only need to check the
  // secondary pattern below.
  const GURL& secondary_gurl = secondary_url;
  for (const auto& rule : rules) {
    if (rule.secondary_pattern.Matches(secondary_gurl)) {
      return rule.GetContentSetting();
    }
  }
  NOTREACHED();
  return CONTENT_SETTING_DEFAULT;
}
}  // namespace

void ContentSettingsAgentImpl::BindContentSettingsManager(
    mojo::Remote<mojom::ContentSettingsManager>* manager) {
  DCHECK(!*manager);
  content::ChildThread::Get()->BindHostReceiver(
      manager->BindNewPipeAndPassReceiver());
}

void ContentSettingsAgentImpl::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  // Clear "block" flags for the new page. This needs to happen before any of
  // `allowScript()`, `allowScriptFromSource()`, `allowImage()`, or
  // `allowPlugins()` is called for the new page so that these functions can
  // correctly detect that a piece of content flipped from "not blocked" to
  // "blocked".
  ClearBlockedContentSettings();

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (frame->Parent())
    return;  // Not a top-level navigation.

#if DCHECK_IS_ON()
  GURL url = frame->GetDocument().Url();
  // If we start failing this DCHECK, please makes sure we don't regress
  // this bug: http://code.google.com/p/chromium/issues/detail?id=79304
  DCHECK(frame->GetDocument().GetSecurityOrigin().ToString() == "null" ||
         !url.SchemeIs(url::kDataScheme));
#endif
}

void ContentSettingsAgentImpl::OnDestruct() {
  delete this;
}

void ContentSettingsAgentImpl::SetAllowRunningInsecureContent() {
  allow_running_insecure_content_ = true;

  // Reload if we are the main frame.
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame->Parent())
    frame->StartReload(blink::WebFrameLoadType::kReload);
}

void ContentSettingsAgentImpl::SetDisabledMixedContentUpgrades() {
  mixed_content_autoupgrades_disabled_ = true;
}

void ContentSettingsAgentImpl::SendRendererContentSettingRules(
    const RendererContentSettingRules& renderer_settings) {
  content_setting_rules_ = std::make_unique<RendererContentSettingRules>(
      std::move(renderer_settings));
}

void ContentSettingsAgentImpl::OnContentSettingsAgentRequest(
    mojo::PendingAssociatedReceiver<mojom::ContentSettingsAgent> receiver) {
  receivers_.Add(this, std::move(receiver));
}

mojom::ContentSettingsManager::StorageType
ContentSettingsAgentImpl::ConvertToMojoStorageType(StorageType storage_type) {
  switch (storage_type) {
    case StorageType::kDatabase:
      return mojom::ContentSettingsManager::StorageType::DATABASE;
    case StorageType::kIndexedDB:
      return mojom::ContentSettingsManager::StorageType::INDEXED_DB;
    case StorageType::kCacheStorage:
      return mojom::ContentSettingsManager::StorageType::CACHE;
    case StorageType::kWebLocks:
      return mojom::ContentSettingsManager::StorageType::WEB_LOCKS;
    case StorageType::kFileSystem:
      return mojom::ContentSettingsManager::StorageType::FILE_SYSTEM;
    case StorageType::kLocalStorage:
      return mojom::ContentSettingsManager::StorageType::LOCAL_STORAGE;
    case StorageType::kSessionStorage:
      return mojom::ContentSettingsManager::StorageType::SESSION_STORAGE;
  }
}

void ContentSettingsAgentImpl::AllowStorageAccess(
    StorageType storage_type,
    base::OnceCallback<void(bool)> callback) {
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (IsFrameWithOpaqueOrigin(frame)) {
    std::move(callback).Run(false);
    return;
  }

  StoragePermissionsKey key(url::Origin(frame->GetSecurityOrigin()),
                            storage_type);
  const auto permissions = cached_storage_permissions_.find(key);
  if (permissions != cached_storage_permissions_.end()) {
    std::move(callback).Run(permissions->second);
    return;
  }

  // Passing the `cache_storage_permissions_` ref to the callback is safe here
  // as the mojo::Remote is owned by `this` and won't invoke the callback if
  // `this` (and in turn `cache_storage_permissions_`) is destroyed.
  base::OnceCallback<void(bool)> new_cb = base::BindOnce(
      [](base::OnceCallback<void(bool)> original_cb, StoragePermissionsKey key,
         base::flat_map<StoragePermissionsKey, bool>& cache_map, bool result) {
        cache_map[key] = result;
        std::move(original_cb).Run(result);
      },
      std::move(callback), key, std::ref(cached_storage_permissions_));

  GetContentSettingsManager().AllowStorageAccess(
      routing_id(), ConvertToMojoStorageType(storage_type),
      frame->GetSecurityOrigin(), frame->GetDocument().SiteForCookies(),
      frame->GetDocument().TopFrameOrigin(), std::move(new_cb));
}

bool ContentSettingsAgentImpl::AllowStorageAccessSync(
    StorageType storage_type) {
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (IsFrameWithOpaqueOrigin(frame))
    return false;

  StoragePermissionsKey key(url::Origin(frame->GetSecurityOrigin()),
                            storage_type);
  const auto permissions = cached_storage_permissions_.find(key);
  if (permissions != cached_storage_permissions_.end())
    return permissions->second;

  SCOPED_UMA_HISTOGRAM_TIMER("ContentSettings.AllowStorageAccessSync");
  bool result = false;
  GetContentSettingsManager().AllowStorageAccess(
      routing_id(), ConvertToMojoStorageType(storage_type),
      frame->GetSecurityOrigin(), frame->GetDocument().SiteForCookies(),
      frame->GetDocument().TopFrameOrigin(), &result);
  cached_storage_permissions_[key] = result;
  return result;
}

bool ContentSettingsAgentImpl::AllowImage(bool enabled_per_settings,
                                          const WebURL& image_url) {
  bool allow = enabled_per_settings;
  if (enabled_per_settings) {
    if (IsAllowlistedForContentSettings())
      return true;

    if (content_setting_rules_) {
      allow = GetContentSettingFromRules(content_setting_rules_->image_rules,
                                         image_url) != CONTENT_SETTING_BLOCK;
    }
  }
  if (!allow)
    DidBlockContentType(ContentSettingsType::IMAGES);
  return allow;
}

bool ContentSettingsAgentImpl::AllowScript(bool enabled_per_settings) {
  if (!enabled_per_settings)
    return false;

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  const auto it = cached_script_permissions_.find(frame);
  if (it != cached_script_permissions_.end())
    return it->second;

  // Evaluate the content setting rules before
  // IsAllowlistedForContentSettings(); if there is only the default rule
  // allowing all scripts, it's quicker this way.
  bool allow = true;
  if (content_setting_rules_) {
    ContentSetting setting = GetContentSettingFromRules(
        content_setting_rules_->script_rules,
        url::Origin(frame->GetDocument().GetSecurityOrigin()).GetURL());
    allow = setting != CONTENT_SETTING_BLOCK;
  }
  allow = allow || IsAllowlistedForContentSettings();

  cached_script_permissions_[frame] = allow;
  return allow;
}

bool ContentSettingsAgentImpl::AllowScriptFromSource(
    bool enabled_per_settings,
    const blink::WebURL& script_url) {
  if (!enabled_per_settings)
    return false;

  bool allow = true;
  if (content_setting_rules_) {
    ContentSetting setting = GetContentSettingFromRules(
        content_setting_rules_->script_rules, script_url);
    allow = setting != CONTENT_SETTING_BLOCK;
  }
  return allow || IsAllowlistedForContentSettings();
}

bool ContentSettingsAgentImpl::AllowAutoDarkWebContent(
    bool enabled_per_settings) {
  if (!enabled_per_settings)
    return false;

  bool allow = true;
  if (content_setting_rules_) {
    ContentSetting setting = GetContentSettingFromRules(
        content_setting_rules_->auto_dark_content_rules, GURL());
    allow = setting != CONTENT_SETTING_BLOCK;
  }
  allow = allow || IsAllowlistedForContentSettings();
  return allow;
}

bool ContentSettingsAgentImpl::AllowReadFromClipboard(bool default_value) {
  return delegate_->AllowReadFromClipboard().value_or(default_value);
}

bool ContentSettingsAgentImpl::AllowWriteToClipboard(bool default_value) {
  return delegate_->AllowWriteToClipboard().value_or(default_value);
}

bool ContentSettingsAgentImpl::AllowMutationEvents(bool default_value) {
  return delegate_->AllowMutationEvents().value_or(default_value);
}

bool ContentSettingsAgentImpl::AllowRunningInsecureContent(
    bool allowed_per_settings,
    const blink::WebURL& resource_url) {
  if (allowed_per_settings || allow_running_insecure_content_)
    return true;

  if (content_setting_rules_) {
    ContentSetting setting = GetContentSettingFromRules(
        content_setting_rules_->mixed_content_rules, GURL());
    if (setting == CONTENT_SETTING_ALLOW)
      return true;
  }

  return false;
}

bool ContentSettingsAgentImpl::AllowPopupsAndRedirects(bool default_value) {
  if (!content_setting_rules_)
    return default_value;
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  return GetContentSettingFromRules(
             content_setting_rules_->popup_redirect_rules,
             url::Origin(frame->GetDocument().GetSecurityOrigin()).GetURL()) ==
         CONTENT_SETTING_ALLOW;
}

bool ContentSettingsAgentImpl::ShouldAutoupgradeMixedContent() {
  if (mixed_content_autoupgrades_disabled_)
    return false;

  if (content_setting_rules_) {
    auto setting = GetContentSettingFromRules(
        content_setting_rules_->mixed_content_rules, GURL());
    return setting != CONTENT_SETTING_ALLOW;
  }
  return true;
}

RendererContentSettingRules*
ContentSettingsAgentImpl::GetRendererContentSettingRules() {
  return content_setting_rules_.get();
}

void ContentSettingsAgentImpl::SetRendererContentSettingRulesForTest(
    const RendererContentSettingRules& rules) {
  content_setting_rules_ = std::make_unique<RendererContentSettingRules>(rules);
}

void ContentSettingsAgentImpl::DidNotAllowScript() {
  DidBlockContentType(ContentSettingsType::JAVASCRIPT);
}

void ContentSettingsAgentImpl::ClearBlockedContentSettings() {
  content_blocked_.clear();
  cached_storage_permissions_.clear();
  cached_script_permissions_.clear();
}

bool ContentSettingsAgentImpl::IsAllowlistedForContentSettings() const {
  if (should_allowlist_)
    return true;

  const WebDocument& document = render_frame()->GetWebFrame()->GetDocument();
  WebSecurityOrigin origin = document.GetSecurityOrigin();
  WebURL document_url = document.Url();
  if (document_url.GetString() == content::kUnreachableWebDataURL)
    return true;

  if (origin.IsNull() || origin.IsOpaque())
    return false;  // Uninitialized document?

  blink::WebString protocol = origin.Protocol();

  if (protocol == content::kChromeUIScheme)
    return true;  // Browser UI elements should still work.

  if (protocol == content::kChromeDevToolsScheme)
    return true;  // DevTools UI elements should still work.

  if (delegate_->IsSchemeAllowlisted(protocol.Utf8()))
    return true;

  // If the scheme is file:, an empty file name indicates a directory listing,
  // which requires JavaScript to function properly.
  if (protocol == url::kFileScheme &&
      document_url.ProtocolIs(url::kFileScheme)) {
    return GURL(document_url).ExtractFileName().empty();
  }
  return false;
}

}  // namespace content_settings
