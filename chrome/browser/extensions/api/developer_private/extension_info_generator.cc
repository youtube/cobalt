// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/developer_private/inspectable_views_finder.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_allowlist.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/site_permissions_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_icon_placeholder.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/path_util.h"
#include "extensions/browser/ui_util.h"
#include "extensions/browser/warning_service.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/command.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permission_message_util.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skbitmap_operations.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

namespace extensions {

namespace developer = api::developer_private;

namespace {

// Given a Manifest::Type, converts it into its developer_private
// counterpart.
developer::ExtensionType GetExtensionType(Manifest::Type manifest_type) {
  developer::ExtensionType type = developer::EXTENSION_TYPE_EXTENSION;
  switch (manifest_type) {
    case Manifest::TYPE_EXTENSION:
      type = developer::EXTENSION_TYPE_EXTENSION;
      break;
    case Manifest::TYPE_THEME:
      type = developer::EXTENSION_TYPE_THEME;
      break;
    case Manifest::TYPE_HOSTED_APP:
      type = developer::EXTENSION_TYPE_HOSTED_APP;
      break;
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
      type = developer::EXTENSION_TYPE_LEGACY_PACKAGED_APP;
      break;
    case Manifest::TYPE_PLATFORM_APP:
      type = developer::EXTENSION_TYPE_PLATFORM_APP;
      break;
    case Manifest::TYPE_SHARED_MODULE:
      type = developer::EXTENSION_TYPE_SHARED_MODULE;
      break;
    case Manifest::TYPE_CHROMEOS_SYSTEM_EXTENSION:
      type = developer::EXTENSION_TYPE_EXTENSION;
      break;
    default:
      NOTREACHED();
  }
  return type;
}

// Populates the common fields of an extension error.
template <typename ErrorType>
void PopulateErrorBase(const ExtensionError& error, ErrorType* out) {
  CHECK(out);
  out->type = error.type() == ExtensionError::MANIFEST_ERROR ?
      developer::ERROR_TYPE_MANIFEST : developer::ERROR_TYPE_RUNTIME;
  out->extension_id = error.extension_id();
  out->from_incognito = error.from_incognito();
  out->source = base::UTF16ToUTF8(error.source());
  out->message = base::UTF16ToUTF8(error.message());
  out->id = error.id();
}

// Given a ManifestError object, converts it into its developer_private
// counterpart.
developer::ManifestError ConstructManifestError(const ManifestError& error) {
  developer::ManifestError result;
  PopulateErrorBase(error, &result);
  result.manifest_key = base::UTF16ToUTF8(error.manifest_key());
  if (!error.manifest_specific().empty()) {
    result.manifest_specific = base::UTF16ToUTF8(error.manifest_specific());
  }
  return result;
}

// Given a RuntimeError object, converts it into its developer_private
// counterpart.
developer::RuntimeError ConstructRuntimeError(const RuntimeError& error) {
  developer::RuntimeError result;
  PopulateErrorBase(error, &result);
  switch (error.level()) {
    case logging::LOG_VERBOSE:
    case logging::LOG_INFO:
      result.severity = developer::ERROR_LEVEL_LOG;
      break;
    case logging::LOG_WARNING:
      result.severity = developer::ERROR_LEVEL_WARN;
      break;
    case logging::LOG_FATAL:
    case logging::LOG_ERROR:
      result.severity = developer::ERROR_LEVEL_ERROR;
      break;
    default:
      NOTREACHED();
  }
  result.context_url = error.context_url().spec();
  result.occurrences = error.occurrences();
  // NOTE(devlin): This is called "render_view_id" in the api for legacy
  // reasons, but it's not a high priority to change.
  result.render_view_id = error.render_frame_id();
  result.render_process_id = error.render_process_id();
  result.can_inspect =
      content::RenderFrameHost::FromID(error.render_process_id(),
                                       error.render_frame_id()) != nullptr;
  for (const StackFrame& f : error.stack_trace()) {
    developer::StackFrame frame;
    frame.line_number = f.line_number;
    frame.column_number = f.column_number;
    frame.url = base::UTF16ToUTF8(f.source);
    frame.function_name = base::UTF16ToUTF8(f.function);
    result.stack_trace.push_back(std::move(frame));
  }
  return result;
}

// Constructs any commands for the extension with the given |id|, and adds them
// to the list of |commands|.
void ConstructCommands(CommandService* command_service,
                       const std::string& extension_id,
                       std::vector<developer::Command>* commands) {
  auto construct_command = [](const Command& command, bool active,
                              bool is_extension_action) {
    developer::Command command_value;
    command_value.description =
        is_extension_action
            ? l10n_util::GetStringUTF8(IDS_EXTENSION_COMMANDS_GENERIC_ACTIVATE)
            : base::UTF16ToUTF8(command.description());
    command_value.keybinding =
        base::UTF16ToUTF8(command.accelerator().GetShortcutText());
    command_value.name = command.command_name();
    command_value.is_active = active;
    command_value.scope = command.global() ? developer::COMMAND_SCOPE_GLOBAL
                                           : developer::COMMAND_SCOPE_CHROME;
    command_value.is_extension_action = is_extension_action;
    return command_value;
  };
  // TODO(https://crbug.com/1067130): Extensions shouldn't be able to specify
  // commands for actions they don't have, so we should just be able to query
  // for a single action type.
  for (auto action_type : {ActionInfo::TYPE_BROWSER, ActionInfo::TYPE_PAGE,
                           ActionInfo::TYPE_ACTION}) {
    bool active = false;
    Command action_command;
    if (command_service->GetExtensionActionCommand(extension_id, action_type,
                                                   CommandService::ALL,
                                                   &action_command, &active)) {
      commands->push_back(construct_command(action_command, active, true));
    }
  }

  CommandMap named_commands;
  if (command_service->GetNamedCommands(extension_id,
                                        CommandService::ALL,
                                        CommandService::ANY_SCOPE,
                                        &named_commands)) {
    for (auto& pair : named_commands) {
      Command& command_to_use = pair.second;
      // TODO(devlin): For some reason beyond my knowledge, FindCommandByName
      // returns different data than GetNamedCommands, including the
      // accelerators, but not the descriptions - and even then, only if the
      // command is active.
      // Unfortunately, some systems may be relying on the other data (which
      // more closely matches manifest data).
      // Until we can sort all this out, we merge the two command structures.
      Command active_command = command_service->FindCommandByName(
          extension_id, command_to_use.command_name());
      command_to_use.set_accelerator(active_command.accelerator());
      command_to_use.set_global(active_command.global());
      bool active = command_to_use.accelerator().key_code() != ui::VKEY_UNKNOWN;
      commands->push_back(construct_command(command_to_use, active, false));
    }
  }
}

// Creates and returns a SpecificSiteControls object for the given
// |granted_permissions| and |withheld_permissions|.
std::vector<developer::SiteControl> GetSpecificSiteControls(
    const PermissionSet& granted_permissions,
    const PermissionSet& withheld_permissions) {
  std::vector<developer::SiteControl> controls;

  std::vector<URLPattern> distinct_granted =
      ExtensionInfoGenerator::GetDistinctHosts(
          granted_permissions.effective_hosts());
  std::vector<URLPattern> distinct_withheld =
      ExtensionInfoGenerator::GetDistinctHosts(
          withheld_permissions.effective_hosts());
  controls.reserve(distinct_granted.size() + distinct_withheld.size());

  for (auto& host : distinct_granted) {
    developer::SiteControl host_control;
    host_control.host = host.GetAsString();
    host_control.granted = true;
    controls.push_back(std::move(host_control));
  }
  for (auto& host : distinct_withheld) {
    developer::SiteControl host_control;
    host_control.host = host.GetAsString();
    host_control.granted = false;
    controls.push_back(std::move(host_control));
  }

  return controls;
}

// Creates and returns a RuntimeHostPermissions object with the
// given extension's host permissions.
developer::RuntimeHostPermissions CreateRuntimeHostPermissionsInfo(
    content::BrowserContext* browser_context,
    const Extension& extension) {
  developer::RuntimeHostPermissions runtime_host_permissions;

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(browser_context);
  // "Effective" granted permissions are stored in different prefs, based on
  // whether host permissions are withheld.
  // TODO(devlin): Create a common helper method to retrieve granted prefs based
  // on whether host permissions are withheld?
  std::unique_ptr<const PermissionSet> granted_permissions;
  // Add the host access data, including the mode and any runtime-granted
  // hosts.
  if (!PermissionsManager::Get(browser_context)
           ->HasWithheldHostPermissions(extension)) {
    granted_permissions =
        extension_prefs->GetGrantedPermissions(extension.id());
    runtime_host_permissions.host_access = developer::HOST_ACCESS_ON_ALL_SITES;
  } else {
    granted_permissions =
        extension_prefs->GetRuntimeGrantedPermissions(extension.id());
    if (granted_permissions->effective_hosts().is_empty()) {
      runtime_host_permissions.host_access = developer::HOST_ACCESS_ON_CLICK;
    } else if (granted_permissions->ShouldWarnAllHosts(false)) {
      runtime_host_permissions.host_access =
          developer::HOST_ACCESS_ON_ALL_SITES;
    } else {
      runtime_host_permissions.host_access =
          developer::HOST_ACCESS_ON_SPECIFIC_SITES;
    }
  }

  runtime_host_permissions.hosts = GetSpecificSiteControls(
      *granted_permissions,
      extension.permissions_data()->withheld_permissions());
  constexpr bool kIncludeApiPermissions = false;
  runtime_host_permissions.has_all_hosts =
      extension.permissions_data()->withheld_permissions().ShouldWarnAllHosts(
          kIncludeApiPermissions) ||
      granted_permissions->ShouldWarnAllHosts(kIncludeApiPermissions);
  return runtime_host_permissions;
}

// Populates the |permissions| data for the given |extension|.
void AddPermissionsInfo(content::BrowserContext* browser_context,
                        const Extension& extension,
                        developer::Permissions* permissions) {
  auto get_permission_messages = [](const PermissionMessages& messages) {
    std::vector<developer::Permission> permissions;
    permissions.reserve(messages.size());
    for (const PermissionMessage& message : messages) {
      permissions.push_back(developer::Permission());
      developer::Permission& permission_message = permissions.back();
      permission_message.message = base::UTF16ToUTF8(message.message());
      permission_message.submessages.reserve(message.submessages().size());
      for (const auto& submessage : message.submessages())
        permission_message.submessages.push_back(base::UTF16ToUTF8(submessage));
    }
    return permissions;
  };

  PermissionsManager* permissions_manager =
      PermissionsManager::Get(browser_context);
  bool enable_runtime_host_permissions =
      permissions_manager->CanAffectExtension(extension);

  if (!enable_runtime_host_permissions) {
    // Without runtime host permissions, everything goes into
    // simple_permissions.
    permissions->simple_permissions = get_permission_messages(
        extension.permissions_data()->GetPermissionMessages());
    return;
  }

  // With runtime host permissions, we separate out API permission messages
  // from host permissions.
  const PermissionSet& active_permissions =
      extension.permissions_data()->active_permissions();
  PermissionSet non_host_permissions(
      active_permissions.apis().Clone(),
      active_permissions.manifest_permissions().Clone(), URLPatternSet(),
      URLPatternSet());
  const PermissionMessageProvider* message_provider =
      PermissionMessageProvider::Get();
  // Generate the messages for just the API (and manifest) permissions.
  PermissionMessages api_messages = message_provider->GetPermissionMessages(
      message_provider->GetAllPermissionIDs(non_host_permissions,
                                            extension.GetType()));
  permissions->simple_permissions = get_permission_messages(api_messages);

  permissions->runtime_host_permissions =
      CreateRuntimeHostPermissionsInfo(browser_context, extension);
}

}  // namespace

ExtensionInfoGenerator::ExtensionInfoGenerator(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      command_service_(CommandService::Get(browser_context)),
      extension_system_(ExtensionSystem::Get(browser_context)),
      extension_prefs_(ExtensionPrefs::Get(browser_context)),
      extension_action_api_(ExtensionActionAPI::Get(browser_context)),
      warning_service_(WarningService::Get(browser_context)),
      error_console_(ErrorConsole::Get(browser_context)),
      image_loader_(ImageLoader::Get(browser_context)),
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
      supervised_user_service_(
          SupervisedUserServiceFactory::GetForBrowserContext(browser_context)),
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
      pending_image_loads_(0u) {
}

ExtensionInfoGenerator::~ExtensionInfoGenerator() {
}

void ExtensionInfoGenerator::CreateExtensionInfo(
    const std::string& id,
    ExtensionInfosCallback callback) {
  DCHECK(callback_.is_null() && list_.empty()) <<
      "Only a single generation can be running at a time!";
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);

  developer::ExtensionState state = developer::EXTENSION_STATE_NONE;
  const Extension* ext = nullptr;
  if ((ext = registry->enabled_extensions().GetByID(id)) != nullptr)
    state = developer::EXTENSION_STATE_ENABLED;
  else if ((ext = registry->disabled_extensions().GetByID(id)) != nullptr)
    state = developer::EXTENSION_STATE_DISABLED;
  else if ((ext = registry->terminated_extensions().GetByID(id)) != nullptr)
    state = developer::EXTENSION_STATE_TERMINATED;

  if (ext && ui_util::ShouldDisplayInExtensionSettings(*ext))
    CreateExtensionInfoHelper(*ext, state);

  if (pending_image_loads_ == 0) {
    // Don't call the callback re-entrantly.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(list_)));
    list_.clear();
  } else {
    callback_ = std::move(callback);
  }
}

void ExtensionInfoGenerator::CreateExtensionsInfo(
    bool include_disabled,
    bool include_terminated,
    ExtensionInfosCallback callback) {
  auto add_to_list = [this](const ExtensionSet& extensions,
                            developer::ExtensionState state) {
    for (const scoped_refptr<const Extension>& extension : extensions) {
      if (ui_util::ShouldDisplayInExtensionSettings(*extension)) {
        CreateExtensionInfoHelper(*extension, state);
      }
    }
  };

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  add_to_list(registry->enabled_extensions(),
              developer::EXTENSION_STATE_ENABLED);
  if (include_disabled) {
    add_to_list(registry->disabled_extensions(),
                developer::EXTENSION_STATE_DISABLED);
    add_to_list(registry->blocklisted_extensions(),
                developer::EXTENSION_STATE_BLACKLISTED);
  }
  if (include_terminated) {
    add_to_list(registry->terminated_extensions(),
                developer::EXTENSION_STATE_TERMINATED);
  }

  if (pending_image_loads_ == 0) {
    // Don't call the callback re-entrantly.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(list_)));
    list_.clear();
  } else {
    callback_ = std::move(callback);
  }
}

std::vector<URLPattern> ExtensionInfoGenerator::GetDistinctHosts(
    const URLPatternSet& patterns) {
  std::vector<URLPattern> pathless_hosts;
  for (URLPattern pattern : patterns) {
    // We only allow addition/removal of full hosts (since from a
    // permissions point of view, path is irrelevant). We always make the
    // path wildcard when adding through this UI, but the optional
    // permissions API may allow adding permissions with paths.
    // TODO(devlin): Investigate, and possibly change the optional
    // permissions API.
    pattern.SetPath("/*");
    pathless_hosts.push_back(std::move(pattern));
  }

  // Iterate over the list of hosts and add any that aren't entirely contained
  // by another pattern. This is pretty inefficient, but the list of hosts
  // should be reasonably small.
  std::vector<URLPattern> distinct_hosts;
  for (const URLPattern& host : pathless_hosts) {
    // If the host is fully contained within the set, we don't add it again.
    bool consumed_by_other = false;
    for (const URLPattern& added_host : distinct_hosts) {
      if (added_host.Contains(host)) {
        consumed_by_other = true;
        break;
      }
    }
    if (consumed_by_other)
      continue;

    // Otherwise, add the host. This might mean we get to prune some hosts
    // from |distinct_hosts|.
    base::EraseIf(distinct_hosts, [host](const URLPattern& other_host) {
      return host.Contains(other_host);
    });

    distinct_hosts.push_back(host);
  }

  return distinct_hosts;
}

void ExtensionInfoGenerator::CreateExtensionInfoHelper(
    const Extension& extension,
    developer::ExtensionState state) {
  std::unique_ptr<developer::ExtensionInfo> info(
      new developer::ExtensionInfo());

  // Blocklist text.
  int blocklist_text = -1;
  BitMapBlocklistState blocklist_state =
      blocklist_prefs::GetExtensionBlocklistState(extension.id(),
                                                  extension_prefs_);
  switch (blocklist_state) {
    case BitMapBlocklistState::BLOCKLISTED_MALWARE:
      blocklist_text = IDS_EXTENSIONS_BLOCKLISTED_MALWARE;
      break;
    case BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY:
      blocklist_text = IDS_EXTENSIONS_BLOCKLISTED_SECURITY_VULNERABILITY;
      break;
    case BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION:
      blocklist_text = IDS_EXTENSIONS_BLOCKLISTED_CWS_POLICY_VIOLATION;
      break;
    case BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED:
      blocklist_text = IDS_EXTENSIONS_BLOCKLISTED_POTENTIALLY_UNWANTED;
      break;
    case BitMapBlocklistState::NOT_BLOCKLISTED:
      // no-op.
      break;
  }
  if (blocklist_text != -1) {
    info->blacklist_text = l10n_util::GetStringUTF8(blocklist_text);
  }

  if (extension_system_->extension_service()->allowlist()->ShouldDisplayWarning(
          extension.id())) {
    info->show_safe_browsing_allowlist_warning = true;
  }
  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(browser_context_);
  Profile* profile = Profile::FromBrowserContext(browser_context_);

  // ControlledInfo.
  bool is_policy_location = Manifest::IsPolicyLocation(extension.location());
  if (is_policy_location) {
    info->controlled_info.emplace();
    info->controlled_info->text =
        l10n_util::GetStringUTF8(IDS_EXTENSIONS_INSTALL_LOCATION_ENTERPRISE);
  }

  bool is_enabled = state == developer::EXTENSION_STATE_ENABLED;

  // Commands.
  if (is_enabled)
    ConstructCommands(command_service_, extension.id(), &info->commands);

  // Dependent extensions.
  if (extension.is_shared_module()) {
    std::unique_ptr<ExtensionSet> dependent_extensions =
        extension_system_->extension_service()
            ->shared_module_service()
            ->GetDependentExtensions(&extension);
    for (const scoped_refptr<const Extension>& dependent :
             *dependent_extensions) {
      developer::DependentExtension dependent_extension;
      dependent_extension.id = dependent->id();
      dependent_extension.name = dependent->name();
      info->dependent_extensions.push_back(std::move(dependent_extension));
    }
  }

  info->description = extension.description();

  // Disable reasons.
  int disable_reasons = extension_prefs_->GetDisableReasons(extension.id());
  info->disable_reasons.suspicious_install =
      (disable_reasons & disable_reason::DISABLE_NOT_VERIFIED) != 0;
  info->disable_reasons.corrupt_install =
      (disable_reasons & disable_reason::DISABLE_CORRUPTED) != 0;
  info->disable_reasons.update_required =
      (disable_reasons & disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY) !=
      0;
  info->disable_reasons.blocked_by_policy =
      (disable_reasons & disable_reason::DISABLE_BLOCKED_BY_POLICY) != 0;
  info->disable_reasons.reloading =
      (disable_reasons & disable_reason::DISABLE_RELOAD) != 0;
  bool custodian_approval_required =
      (disable_reasons & disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED) !=
      0;
  info->disable_reasons.custodian_approval_required =
      custodian_approval_required;
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  bool permissions_increase =
      (disable_reasons & disable_reason::DISABLE_PERMISSIONS_INCREASE) != 0;
  info->disable_reasons.parent_disabled_permissions =
      supervised_user_service_->AreExtensionsPermissionsEnabled() &&
      !profile->GetPrefs()->GetBoolean(
          prefs::kSupervisedUserExtensionsMayRequestPermissions) &&
      (custodian_approval_required || permissions_increase);
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

  // Error collection.
  bool error_console_enabled =
      error_console_->IsEnabledForChromeExtensionsPage();
  info->error_collection.is_enabled = error_console_enabled;
  info->error_collection.is_active =
      error_console_enabled &&
      error_console_->IsReportingEnabledForExtension(extension.id());

  // File access.
  ManagementPolicy* management_policy = extension_system_->management_policy();
  info->file_access.is_enabled =
      (extension.wants_file_access() ||
       Manifest::ShouldAlwaysAllowFileAccess(extension.location()));
  info->file_access.is_active =
      util::AllowFileAccess(extension.id(), browser_context_);

  // Home page.
  info->home_page.url = ManifestURL::GetHomepageURL(&extension).spec();
  info->home_page.specified = ManifestURL::SpecifiedHomepageURL(&extension);

  // Developer and web store URLs.
  // TODO(dschuyler) after MD extensions releases (expected in m64), look into
  // removing the |home_page.url| and |home_page.specified| above.
  info->manifest_home_page_url =
      ManifestURL::GetManifestHomePageURL(&extension).spec();
  info->web_store_url = ManifestURL::GetWebStoreURL(&extension).spec();

  info->id = extension.id();

  // Incognito access.
  info->incognito_access.is_enabled = util::CanBeIncognitoEnabled(&extension);
  info->incognito_access.is_active =
      util::IsIncognitoEnabled(extension.id(), browser_context_);

  // Install warnings, but only if unpacked, the error console isn't enabled
  // (otherwise it shows these), and we're in developer mode (normal users don't
  // need to see these).
  if (!error_console_enabled &&
      Manifest::IsUnpackedLocation(extension.location()) &&
      profile->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode)) {
    const std::vector<InstallWarning>& install_warnings =
        extension.install_warnings();
    for (const InstallWarning& warning : install_warnings)
      info->install_warnings.push_back(warning.message);
  }

  // Launch url.
  if (extension.is_app()) {
    info->launch_url = AppLaunchInfo::GetFullLaunchURL(&extension).spec();
  }

  // Location.
  bool updates_from_web_store =
      extension_management->UpdatesFromWebstore(extension);
  if (extension.location() == mojom::ManifestLocation::kInternal &&
      updates_from_web_store) {
    info->location = developer::LOCATION_FROM_STORE;
  } else if (Manifest::IsUnpackedLocation(extension.location())) {
    info->location = developer::LOCATION_UNPACKED;
  } else if (extension.was_installed_by_default() &&
             !extension.was_installed_by_oem() && updates_from_web_store) {
    info->location = developer::LOCATION_INSTALLED_BY_DEFAULT;
  } else if (Manifest::IsExternalLocation(extension.location()) &&
             updates_from_web_store) {
    info->location = developer::LOCATION_THIRD_PARTY;
  } else {
    info->location = developer::LOCATION_UNKNOWN;
  }

  // Location text.
  int location_text = -1;
  if (info->location == developer::LOCATION_UNKNOWN)
    location_text = IDS_EXTENSIONS_INSTALL_LOCATION_UNKNOWN;
  else if (extension.location() == mojom::ManifestLocation::kExternalRegistry)
    location_text = IDS_EXTENSIONS_INSTALL_LOCATION_3RD_PARTY;
  else if (extension.is_shared_module())
    location_text = IDS_EXTENSIONS_INSTALL_LOCATION_SHARED_MODULE;
  if (location_text != -1) {
    info->location_text = l10n_util::GetStringUTF8(location_text);
  }

  // Runtime/Manifest errors.
  if (error_console_enabled) {
    const ErrorList& errors =
        error_console_->GetErrorsForExtension(extension.id());
    for (const auto& error : errors) {
      switch (error->type()) {
        case ExtensionError::MANIFEST_ERROR:
          info->manifest_errors.push_back(ConstructManifestError(
              static_cast<const ManifestError&>(*error)));
          break;
        case ExtensionError::RUNTIME_ERROR:
          info->runtime_errors.push_back(ConstructRuntimeError(
              static_cast<const RuntimeError&>(*error)));
          break;
        case ExtensionError::INTERNAL_ERROR:
          // TODO(wittman): Support InternalError in developer tools:
          // https://crbug.com/503427.
          break;
        case ExtensionError::NUM_ERROR_TYPES:
          NOTREACHED();
          break;
      }
    }
  }

  info->must_remain_installed =
      management_policy->MustRemainInstalled(&extension, nullptr);

  info->name = extension.name();
  info->offline_enabled = OfflineEnabledInfo::IsOfflineEnabled(&extension);

  // Options page.
  if (OptionsPageInfo::HasOptionsPage(&extension)) {
    info->options_page.emplace();
    info->options_page->open_in_tab =
        OptionsPageInfo::ShouldOpenInTab(&extension);
    info->options_page->url =
        OptionsPageInfo::GetOptionsPage(&extension).spec();
  }

  // Path.
  if (Manifest::IsUnpackedLocation(extension.location())) {
    info->path = extension.path().AsUTF8Unsafe();
    info->prettified_path =
        extensions::path_util::PrettifyPath(extension.path()).AsUTF8Unsafe();
  }

  AddPermissionsInfo(browser_context_, extension, &info->permissions);

  // Runtime warnings.
  std::vector<std::string> warnings =
      warning_service_->GetWarningMessagesForExtension(extension.id());
  for (const std::string& warning : warnings)
    info->runtime_warnings.push_back(warning);

  info->state = state;

  info->type = GetExtensionType(extension.manifest()->type());

  info->update_url =
      extension_management->GetEffectiveUpdateURL(extension).spec();

  info->user_may_modify =
      management_policy->UserMayModifySettings(&extension, nullptr);

  info->version = extension.GetVersionForDisplay();

  if (state != developer::EXTENSION_STATE_TERMINATED) {
    info->views = InspectableViewsFinder(profile).
                      GetViewsForExtension(extension, is_enabled);
  }

  // Show access requests in toolbar.
  info->show_access_requests_in_toolbar =
      SitePermissionsHelper(profile).ShowAccessRequestsInToolbar(
          extension.id());

  // The icon.
  ExtensionResource icon =
      IconsInfo::GetIconResource(&extension,
                                 extension_misc::EXTENSION_ICON_MEDIUM,
                                 ExtensionIconSet::MATCH_BIGGER);
  if (icon.empty()) {
    info->icon_url = GetDefaultIconUrl(extension.name());
    list_.push_back(std::move(*info));
  } else {
    ++pending_image_loads_;
    // Max size of 128x128 is a random guess at a nice balance between being
    // overly eager to resize and sending across gigantic data urls. (The icon
    // used by the url is 48x48).
    gfx::Size max_size(128, 128);
    image_loader_->LoadImageAsync(
        &extension, icon, max_size,
        base::BindOnce(&ExtensionInfoGenerator::OnImageLoaded,
                       weak_factory_.GetWeakPtr(), std::move(info)));
  }
}

std::string ExtensionInfoGenerator::GetDefaultIconUrl(const std::string& name) {
  return GetIconUrlFromImage(ExtensionIconPlaceholder::CreateImage(
      extension_misc::EXTENSION_ICON_MEDIUM, name));
}

std::string ExtensionInfoGenerator::GetIconUrlFromImage(
    const gfx::Image& image) {
  scoped_refptr<base::RefCountedMemory> data;
  data = image.As1xPNGBytes();
  std::string base_64;
  base::Base64Encode(base::StringPiece(data->front_as<char>(), data->size()),
                     &base_64);
  const char kDataUrlPrefix[] = "data:image/png;base64,";
  return GURL(kDataUrlPrefix + base_64).spec();
}

void ExtensionInfoGenerator::OnImageLoaded(
    std::unique_ptr<developer::ExtensionInfo> info,
    const gfx::Image& icon) {
  if (!icon.IsEmpty()) {
    info->icon_url = GetIconUrlFromImage(icon);
  } else {
    info->icon_url = GetDefaultIconUrl(info->name);
  }

  list_.push_back(std::move(*info));

  --pending_image_loads_;

  if (pending_image_loads_ == 0) {  // All done!
    ExtensionInfoList list = std::move(list_);
    list_.clear();
    std::move(callback_).Run(std::move(list));
    // WARNING: |this| is possibly deleted after this line!
  }
}

}  // namespace extensions
