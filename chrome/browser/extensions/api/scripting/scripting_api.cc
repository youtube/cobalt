// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/scripting/scripting_api.h"

#include <algorithm>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/common/extensions/api/scripting.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/browser/api/scripting/scripting_constants.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_user_script_loader.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/load_and_localize_file.h"
#include "extensions/browser/script_executor.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/css_origin.mojom-shared.h"
#include "extensions/common/mojom/execution_world.mojom-shared.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/utils/content_script_utils.h"
#include "extensions/common/utils/extension_types_utils.h"

namespace extensions {

namespace {

constexpr char kCouldNotLoadFileError[] = "Could not load file: '*'.";
constexpr char kDuplicateFileSpecifiedError[] =
    "Duplicate file specified: '*'.";
constexpr char kExactlyOneOfCssAndFilesError[] =
    "Exactly one of 'css' and 'files' must be specified.";
constexpr char kFilesExceededSizeLimitError[] =
    "Scripts could not be loaded because '*' exceeds the maximum script size "
    "or the extension's maximum total script size.";

// Note: CSS always injects as soon as possible, so we default to
// document_start. Because of tab loading, there's no guarantee this will
// *actually* inject before page load, but it will at least inject "soon".
constexpr mojom::RunLocation kCSSRunLocation =
    mojom::RunLocation::kDocumentStart;

// Converts the given `style_origin` to a CSSOrigin.
mojom::CSSOrigin ConvertStyleOriginToCSSOrigin(
    api::scripting::StyleOrigin style_origin) {
  mojom::CSSOrigin css_origin = mojom::CSSOrigin::kAuthor;
  switch (style_origin) {
    case api::scripting::STYLE_ORIGIN_NONE:
    case api::scripting::STYLE_ORIGIN_AUTHOR:
      css_origin = mojom::CSSOrigin::kAuthor;
      break;
    case api::scripting::STYLE_ORIGIN_USER:
      css_origin = mojom::CSSOrigin::kUser;
      break;
  }

  return css_origin;
}

mojom::ExecutionWorld ConvertExecutionWorld(
    api::scripting::ExecutionWorld world) {
  mojom::ExecutionWorld execution_world = mojom::ExecutionWorld::kIsolated;
  switch (world) {
    case api::scripting::EXECUTION_WORLD_NONE:
    case api::scripting::EXECUTION_WORLD_ISOLATED:
      break;  // Default to mojom::ExecutionWorld::kIsolated.
    case api::scripting::EXECUTION_WORLD_MAIN:
      execution_world = mojom::ExecutionWorld::kMain;
  }

  return execution_world;
}

api::scripting::ExecutionWorld ConvertExecutionWorldForAPI(
    mojom::ExecutionWorld world) {
  switch (world) {
    case mojom::ExecutionWorld::kIsolated:
      return api::scripting::EXECUTION_WORLD_ISOLATED;
    case mojom::ExecutionWorld::kMain:
      return api::scripting::EXECUTION_WORLD_MAIN;
    case mojom::ExecutionWorld::kUserScript:
      NOTREACHED() << "UserScript worlds are not supported in this API.";
  }

  NOTREACHED();
  return api::scripting::EXECUTION_WORLD_ISOLATED;
}

std::string InjectionKeyForCode(const mojom::HostID& host_id,
                                const std::string& code) {
  return ScriptExecutor::GenerateInjectionKey(host_id, /*script_url=*/GURL(),
                                              code);
}

std::string InjectionKeyForFile(const mojom::HostID& host_id,
                                const GURL& resource_url) {
  return ScriptExecutor::GenerateInjectionKey(host_id, resource_url,
                                              /*code=*/std::string());
}

// Constructs an array of file sources from the read file `data`.
std::vector<InjectedFileSource> ConstructFileSources(
    std::vector<std::unique_ptr<std::string>> data,
    std::vector<std::string> file_names) {
  // Note: CHECK (and not DCHECK) because if it fails, we have an out-of-bounds
  // access.
  CHECK_EQ(data.size(), file_names.size());
  const size_t num_sources = data.size();
  std::vector<InjectedFileSource> sources;
  sources.reserve(num_sources);
  for (size_t i = 0; i < num_sources; ++i)
    sources.emplace_back(std::move(file_names[i]), std::move(data[i]));

  return sources;
}

std::vector<mojom::JSSourcePtr> FileSourcesToJSSources(
    const Extension& extension,
    std::vector<InjectedFileSource> file_sources) {
  std::vector<mojom::JSSourcePtr> js_sources;
  js_sources.reserve(file_sources.size());
  for (auto& file_source : file_sources) {
    js_sources.push_back(
        mojom::JSSource::New(std::move(*file_source.data),
                             extension.GetResourceURL(file_source.file_name)));
  }

  return js_sources;
}

std::vector<mojom::CSSSourcePtr> FileSourcesToCSSSources(
    const Extension& extension,
    std::vector<InjectedFileSource> file_sources) {
  mojom::HostID host_id(mojom::HostID::HostType::kExtensions, extension.id());

  std::vector<mojom::CSSSourcePtr> css_sources;
  css_sources.reserve(file_sources.size());
  for (auto& file_source : file_sources) {
    css_sources.push_back(mojom::CSSSource::New(
        std::move(*file_source.data),
        InjectionKeyForFile(host_id,
                            extension.GetResourceURL(file_source.file_name))));
  }

  return css_sources;
}

// Checks `files` and populates `resources_out` with the appropriate extension
// resource. Returns true on success; on failure, populates `error_out`.
bool GetFileResources(const std::vector<std::string>& files,
                      const Extension& extension,
                      std::vector<ExtensionResource>* resources_out,
                      std::string* error_out) {
  if (files.empty()) {
    static constexpr char kAtLeastOneFileError[] =
        "At least one file must be specified.";
    *error_out = kAtLeastOneFileError;
    return false;
  }

  std::vector<ExtensionResource> resources;
  for (const auto& file : files) {
    ExtensionResource resource = extension.GetResource(file);
    if (resource.extension_root().empty() || resource.relative_path().empty()) {
      *error_out = ErrorUtils::FormatErrorMessage(kCouldNotLoadFileError, file);
      return false;
    }

    // ExtensionResource doesn't implement an operator==.
    if (base::Contains(resources, resource.relative_path(),
                       &ExtensionResource::relative_path)) {
      // Disallow duplicates. Note that we could allow this, if we wanted (and
      // there *might* be reason to with JS injection, to perform an operation
      // twice?). However, this matches content script behavior, and injecting
      // twice can be done by chaining calls to executeScript() / insertCSS().
      // This isn't a robust check, and could probably be circumvented by
      // passing two paths that look different but are the same - but in that
      // case, we just try to load and inject the script twice, which is
      // inefficient, but safe.
      *error_out =
          ErrorUtils::FormatErrorMessage(kDuplicateFileSpecifiedError, file);
      return false;
    }

    resources.push_back(std::move(resource));
  }

  resources_out->swap(resources);
  return true;
}

using ResourcesLoadedCallback =
    base::OnceCallback<void(std::vector<InjectedFileSource>,
                            absl::optional<std::string>)>;

// Checks the loaded content of extension resources. Invokes `callback` with
// the constructed file sources on success or with an error on failure.
void CheckLoadedResources(std::vector<std::string> file_names,
                          ResourcesLoadedCallback callback,
                          std::vector<std::unique_ptr<std::string>> file_data,
                          absl::optional<std::string> load_error) {
  if (load_error) {
    std::move(callback).Run({}, std::move(load_error));
    return;
  }

  std::vector<InjectedFileSource> file_sources =
      ConstructFileSources(std::move(file_data), std::move(file_names));

  for (const auto& source : file_sources) {
    DCHECK(source.data);
    // TODO(devlin): What necessitates this encoding requirement? Is it needed
    // for blink injection?
    if (!base::IsStringUTF8(*source.data)) {
      static constexpr char kBadFileEncodingError[] =
          "Could not load file '*'. It isn't UTF-8 encoded.";
      std::string error = ErrorUtils::FormatErrorMessage(kBadFileEncodingError,
                                                         source.file_name);
      std::move(callback).Run({}, std::move(error));
      return;
    }
  }

  std::move(callback).Run(std::move(file_sources), absl::nullopt);
}

// Checks the specified `files` for validity, and attempts to load and localize
// them, invoking `callback` with the result. Returns true on success; on
// failure, populates `error`.
bool CheckAndLoadFiles(std::vector<std::string> files,
                       const Extension& extension,
                       bool requires_localization,
                       ResourcesLoadedCallback callback,
                       std::string* error) {
  std::vector<ExtensionResource> resources;
  if (!GetFileResources(files, extension, &resources, error))
    return false;

  LoadAndLocalizeResources(
      extension, resources, requires_localization,
      script_parsing::GetMaxScriptLength(),
      base::BindOnce(&CheckLoadedResources, std::move(files),
                     std::move(callback)));
  return true;
}

// Returns an error message string for when an extension cannot access a page it
// is attempting to.
std::string GetCannotAccessPageErrorMessage(const PermissionsData& permissions,
                                            const GURL& url) {
  if (permissions.HasAPIPermission(mojom::APIPermissionID::kTab)) {
    return ErrorUtils::FormatErrorMessage(
        manifest_errors::kCannotAccessPageWithUrl, url.spec());
  }
  return manifest_errors::kCannotAccessPage;
}

// Returns true if the `permissions` allow for injection into the given `frame`.
// If false, populates `error`.
bool HasPermissionToInjectIntoFrame(const PermissionsData& permissions,
                                    int tab_id,
                                    content::RenderFrameHost* frame,
                                    std::string* error) {
  GURL committed_url = frame->GetLastCommittedURL();
  if (committed_url.is_empty()) {
    if (!frame->IsInPrimaryMainFrame()) {
      // We can't check the pending URL for subframes from the //chrome layer.
      // Assume the injection is allowed; the renderer has additional checks
      // later on.
      return true;
    }
    // Unknown URL, e.g. because no load was committed yet. In this case we look
    // for any pending entry on the NavigationController associated with the
    // WebContents for the frame.
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(frame);
    content::NavigationEntry* pending_entry =
        web_contents->GetController().GetPendingEntry();
    if (!pending_entry) {
      *error = manifest_errors::kCannotAccessPage;
      return false;
    }
    GURL pending_url = pending_entry->GetURL();
    if (pending_url.SchemeIsHTTPOrHTTPS() &&
        !permissions.CanAccessPage(pending_url, tab_id, error)) {
      // This catches the majority of cases where an extension tried to inject
      // on a newly-created navigating tab, saving us a potentially-costly IPC
      // and, maybe, slightly reducing (but not by any stretch eliminating) an
      // attack surface.
      *error = GetCannotAccessPageErrorMessage(permissions, pending_url);
      return false;
    }

    // Otherwise allow for now. The renderer has additional checks and will
    // fail the injection if needed.
    return true;
  }

  // TODO(devlin): Add more schemes here, in line with
  // https://crbug.com/55084.
  if (committed_url.SchemeIs(url::kAboutScheme) ||
      committed_url.SchemeIs(url::kDataScheme)) {
    url::Origin origin = frame->GetLastCommittedOrigin();
    const url::SchemeHostPort& tuple_or_precursor_tuple =
        origin.GetTupleOrPrecursorTupleIfOpaque();
    if (!tuple_or_precursor_tuple.IsValid()) {
      *error = GetCannotAccessPageErrorMessage(permissions, committed_url);
      return false;
    }

    committed_url = tuple_or_precursor_tuple.GetURL();
  }

  return permissions.CanAccessPage(committed_url, tab_id, error);
}

// Collects the frames for injection. Method will return false if an error is
// encountered.
bool CollectFramesForInjection(const api::scripting::InjectionTarget& target,
                               content::WebContents* tab,
                               std::set<int>& frame_ids,
                               std::set<content::RenderFrameHost*>& frames,
                               std::string* error_out) {
  if (target.document_ids) {
    for (const auto& id : *target.document_ids) {
      ExtensionApiFrameIdMap::DocumentId document_id =
          ExtensionApiFrameIdMap::DocumentIdFromString(id);

      if (!document_id) {
        *error_out = base::StringPrintf("Invalid document id %s", id.c_str());
        return false;
      }

      content::RenderFrameHost* frame =
          ExtensionApiFrameIdMap::Get()->GetRenderFrameHostByDocumentId(
              document_id);

      // If the frame was not found or it matched another tab reject this
      // request.
      if (!frame || content::WebContents::FromRenderFrameHost(frame) != tab) {
        *error_out =
            base::StringPrintf("No document with id %s in tab with id %d",
                               id.c_str(), target.tab_id);
        return false;
      }

      // Convert the documentId into a frameId since the content will be
      // injected synchronously.
      frame_ids.insert(ExtensionApiFrameIdMap::GetFrameId(frame));
      frames.insert(frame);
    }
  } else {
    if (target.frame_ids) {
      frame_ids.insert(target.frame_ids->begin(), target.frame_ids->end());
    } else {
      frame_ids.insert(ExtensionApiFrameIdMap::kTopFrameId);
    }

    for (int frame_id : frame_ids) {
      content::RenderFrameHost* frame =
          ExtensionApiFrameIdMap::GetRenderFrameHostById(tab, frame_id);
      if (!frame) {
        *error_out = base::StringPrintf("No frame with id %d in tab with id %d",
                                        frame_id, target.tab_id);
        return false;
      }
      frames.insert(frame);
    }
  }
  return true;
}

// Returns true if the `target` can be accessed with the given `permissions`.
// If the target can be accessed, populates `script_executor_out`,
// `frame_scope_out`, and `frame_ids_out` with the appropriate values;
// if the target cannot be accessed, populates `error_out`.
bool CanAccessTarget(const PermissionsData& permissions,
                     const api::scripting::InjectionTarget& target,
                     content::BrowserContext* browser_context,
                     bool include_incognito_information,
                     ScriptExecutor** script_executor_out,
                     ScriptExecutor::FrameScope* frame_scope_out,
                     std::set<int>* frame_ids_out,
                     std::string* error_out) {
  content::WebContents* tab = nullptr;
  TabHelper* tab_helper = nullptr;
  if (!ExtensionTabUtil::GetTabById(target.tab_id, browser_context,
                                    include_incognito_information, &tab) ||
      !(tab_helper = TabHelper::FromWebContents(tab))) {
    // TODO(devlin): Add a constant for this in a centrally-consumable location.
    *error_out = base::StringPrintf("No tab with id: %d", target.tab_id);
    return false;
  }

  if ((target.all_frames && *target.all_frames == true) &&
      (target.frame_ids || target.document_ids)) {
    *error_out =
        "Cannot specify 'allFrames' if either 'frameIds' or 'documentIds' is "
        "specified.";
    return false;
  }

  if (target.frame_ids && target.document_ids) {
    *error_out = "Cannot specify both 'frameIds' and 'documentIds'.";
    return false;
  }

  ScriptExecutor* script_executor = tab_helper->script_executor();
  DCHECK(script_executor);

  ScriptExecutor::FrameScope frame_scope =
      target.all_frames && *target.all_frames == true
          ? ScriptExecutor::INCLUDE_SUB_FRAMES
          : ScriptExecutor::SPECIFIED_FRAMES;

  std::set<int> frame_ids;
  std::set<content::RenderFrameHost*> frames;
  if (!CollectFramesForInjection(target, tab, frame_ids, frames, error_out))
    return false;

  // TODO(devlin): If `allFrames` is true, we error out if the extension
  // doesn't have access to the top frame (even if it may inject in child
  // frames). This is inconsistent with content scripts (which can execute on
  // child frames), but consistent with the old tabs.executeScript() API.
  for (content::RenderFrameHost* frame : frames) {
    DCHECK_EQ(content::WebContents::FromRenderFrameHost(frame), tab);
    if (!HasPermissionToInjectIntoFrame(permissions, target.tab_id, frame,
                                        error_out)) {
      return false;
    }
  }

  *frame_ids_out = std::move(frame_ids);
  *frame_scope_out = frame_scope;
  *script_executor_out = script_executor;
  return true;
}

std::unique_ptr<UserScript> ParseUserScript(
    content::BrowserContext* browser_context,
    const Extension& extension,
    const api::scripting::RegisteredContentScript& content_script,
    int definition_index,
    int valid_schemes,
    std::u16string* error) {
  auto result = std::make_unique<UserScript>();
  result->set_id(content_script.id);
  result->set_host_id(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension.id()));

  if (content_script.run_at != api::extension_types::RunAt::kNone) {
    result->set_run_location(ConvertRunLocation(content_script.run_at));
  }

  if (content_script.all_frames)
    result->set_match_all_frames(*content_script.all_frames);

  DCHECK(content_script.matches);
  if (!script_parsing::ParseMatchPatterns(
          *content_script.matches,
          base::OptionalToPtr(content_script.exclude_matches), definition_index,
          extension.creation_flags(), scripting::kScriptsCanExecuteEverywhere,
          valid_schemes, scripting::kAllUrlsIncludesChromeUrls, result.get(),
          error,
          /*wants_file_access=*/nullptr)) {
    return nullptr;
  }

  if (!script_parsing::ParseFileSources(
          &extension, base::OptionalToPtr(content_script.js),
          base::OptionalToPtr(content_script.css), definition_index,
          result.get(), error)) {
    return nullptr;
  }

  result->set_incognito_enabled(
      util::IsIncognitoEnabled(extension.id(), browser_context));
  result->set_execution_world(ConvertExecutionWorld(content_script.world));
  return result;
}

ValidateContentScriptsResult ValidateParsedScriptsOnFileThread(
    ExtensionResource::SymlinkPolicy symlink_policy,
    std::unique_ptr<UserScriptList> scripts) {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

  // Validate that claimed script resources actually exist, and are UTF-8
  // encoded.
  std::string error;
  std::vector<InstallWarning> warnings;
  bool are_script_files_valid = script_parsing::ValidateFileSources(
      *scripts, symlink_policy, &error, &warnings);

  // Script files over the per script/extension limit are recorded as warnings.
  // However, for the scripting API we should treat "install warnings" as
  // errors by turning this call into a no-op and returning an error.
  if (!warnings.empty() && error.empty()) {
    error = ErrorUtils::FormatErrorMessage(kFilesExceededSizeLimitError,
                                           warnings[0].specific);
    are_script_files_valid = false;
  }

  return std::make_pair(std::move(scripts), are_script_files_valid
                                                ? absl::nullopt
                                                : absl::make_optional(error));
}

// Converts a UserScript object to a api::scripting::RegisteredContentScript
// object, used for getRegisteredContentScripts.
api::scripting::RegisteredContentScript CreateRegisteredContentScriptInfo(
    const UserScript& script) {
  api::scripting::RegisteredContentScript script_info;
  script_info.id = script.id();

  script_info.matches.emplace();
  script_info.matches->reserve(script.url_patterns().size());
  for (const URLPattern& pattern : script.url_patterns())
    script_info.matches->push_back(pattern.GetAsString());

  if (!script.exclude_url_patterns().is_empty()) {
    script_info.exclude_matches.emplace();
    script_info.exclude_matches->reserve(script.exclude_url_patterns().size());
    for (const URLPattern& pattern : script.exclude_url_patterns())
      script_info.exclude_matches->push_back(pattern.GetAsString());
  }

  // File paths may be normalized in the returned object and can differ slightly
  // compared to what was originally passed into registerContentScripts.
  if (!script.js_scripts().empty()) {
    script_info.js.emplace();
    script_info.js->reserve(script.js_scripts().size());
    for (const auto& js_script : script.js_scripts())
      script_info.js->push_back(js_script->relative_path().AsUTF8Unsafe());
  }

  if (!script.css_scripts().empty()) {
    script_info.css.emplace();
    script_info.css->reserve(script.css_scripts().size());
    for (const auto& css_script : script.css_scripts())
      script_info.css->push_back(css_script->relative_path().AsUTF8Unsafe());
  }

  script_info.all_frames = script.match_all_frames();
  script_info.match_origin_as_fallback = script.match_origin_as_fallback() ==
                                         MatchOriginAsFallbackBehavior::kAlways;
  script_info.run_at = ConvertRunLocationForAPI(script.run_location());
  script_info.world = ConvertExecutionWorldForAPI(script.execution_world());

  return script_info;
}

}  // namespace

InjectedFileSource::InjectedFileSource(std::string file_name,
                                       std::unique_ptr<std::string> data)
    : file_name(std::move(file_name)), data(std::move(data)) {}
InjectedFileSource::InjectedFileSource(InjectedFileSource&&) = default;
InjectedFileSource::~InjectedFileSource() = default;

ScriptingExecuteScriptFunction::ScriptingExecuteScriptFunction() = default;
ScriptingExecuteScriptFunction::~ScriptingExecuteScriptFunction() = default;

ExtensionFunction::ResponseAction ScriptingExecuteScriptFunction::Run() {
  absl::optional<api::scripting::ExecuteScript::Params> params =
      api::scripting::ExecuteScript::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  injection_ = std::move(params->injection);

  // Silently alias `function` to `func` for backwards compatibility.
  // TODO(devlin): Remove this in M95.
  if (injection_.function) {
    if (injection_.func) {
      return RespondNow(
          Error("Both 'func' and 'function' were specified. "
                "Only 'func' should be used."));
    }
    injection_.func = std::move(injection_.function);
  }

  if ((injection_.files && injection_.func) ||
      (!injection_.files && !injection_.func)) {
    return RespondNow(
        Error("Exactly one of 'func' and 'files' must be specified"));
  }

  if (injection_.files) {
    if (injection_.args)
      return RespondNow(Error("'args' may not be used with file injections."));

    // JS files don't require localization.
    constexpr bool kRequiresLocalization = false;
    std::string error;
    if (!CheckAndLoadFiles(
            std::move(*injection_.files), *extension(), kRequiresLocalization,
            base::BindOnce(&ScriptingExecuteScriptFunction::DidLoadResources,
                           this),
            &error)) {
      return RespondNow(Error(std::move(error)));
    }
    return RespondLater();
  }

  DCHECK(injection_.func);

  // TODO(devlin): This (wrapping a function to create an IIFE) is pretty hacky,
  // and along with the JSON-serialization of the arguments to curry in.
  // Add support to the ScriptExecutor to better support this case.
  std::string args_expression;
  if (injection_.args) {
    std::vector<std::string> string_args;
    string_args.reserve(injection_.args->size());
    for (const auto& arg : *injection_.args) {
      std::string json;
      if (!base::JSONWriter::Write(arg, &json))
        return RespondNow(Error("Unserializable argument passed."));
      string_args.push_back(std::move(json));
    }
    args_expression = base::JoinString(string_args, ",");
  }

  std::string code_to_execute = base::StringPrintf(
      "(%s)(%s)", injection_.func->c_str(), args_expression.c_str());

  std::vector<mojom::JSSourcePtr> sources;
  sources.push_back(mojom::JSSource::New(std::move(code_to_execute), GURL()));

  std::string error;
  if (!Execute(std::move(sources), &error))
    return RespondNow(Error(std::move(error)));

  return RespondLater();
}

void ScriptingExecuteScriptFunction::DidLoadResources(
    std::vector<InjectedFileSource> file_sources,
    absl::optional<std::string> load_error) {
  if (load_error) {
    Respond(Error(std::move(*load_error)));
    return;
  }

  DCHECK(!file_sources.empty());

  std::string error;
  if (!Execute(FileSourcesToJSSources(*extension(), std::move(file_sources)),
               &error)) {
    Respond(Error(std::move(error)));
  }
}

bool ScriptingExecuteScriptFunction::Execute(
    std::vector<mojom::JSSourcePtr> sources,
    std::string* error) {
  ScriptExecutor* script_executor = nullptr;
  ScriptExecutor::FrameScope frame_scope = ScriptExecutor::SPECIFIED_FRAMES;
  std::set<int> frame_ids;
  if (!CanAccessTarget(*extension()->permissions_data(), injection_.target,
                       browser_context(), include_incognito_information(),
                       &script_executor, &frame_scope, &frame_ids, error)) {
    return false;
  }

  mojom::ExecutionWorld execution_world =
      ConvertExecutionWorld(injection_.world);

  // Extensions can specify that the script should be injected "immediately".
  // In this case, we specify kDocumentStart as the injection time. Due to
  // inherent raciness between tab creation and load and this function
  // execution, there is no guarantee that it will actually happen at
  // document start, but the renderer will appropriately inject it
  // immediately if document start has already passed.
  mojom::RunLocation run_location =
      injection_.inject_immediately && *injection_.inject_immediately
          ? mojom::RunLocation::kDocumentStart
          : mojom::RunLocation::kDocumentIdle;
  script_executor->ExecuteScript(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension()->id()),
      mojom::CodeInjection::NewJs(mojom::JSInjection::New(
          std::move(sources), execution_world,
          blink::mojom::WantResultOption::kWantResult,
          user_gesture() ? blink::mojom::UserActivationOption::kActivate
                         : blink::mojom::UserActivationOption::kDoNotActivate,
          blink::mojom::PromiseResultOption::kAwait)),
      frame_scope, frame_ids, ScriptExecutor::MATCH_ABOUT_BLANK, run_location,
      ScriptExecutor::DEFAULT_PROCESS,
      /* webview_src */ GURL(),
      base::BindOnce(&ScriptingExecuteScriptFunction::OnScriptExecuted, this));

  return true;
}

void ScriptingExecuteScriptFunction::OnScriptExecuted(
    std::vector<ScriptExecutor::FrameResult> frame_results) {
  // If only a single frame was included and the injection failed, respond with
  // an error.
  if (frame_results.size() == 1 && !frame_results[0].error.empty()) {
    Respond(Error(std::move(frame_results[0].error)));
    return;
  }

  // Otherwise, respond successfully. We currently just skip over individual
  // frames that failed. In the future, we can bubble up these error messages
  // to the extension.
  std::vector<api::scripting::InjectionResult> injection_results;
  for (auto& result : frame_results) {
    if (!result.error.empty())
      continue;
    api::scripting::InjectionResult injection_result;
    injection_result.result = std::move(result.value);
    injection_result.frame_id = result.frame_id;
    if (result.document_id)
      injection_result.document_id = result.document_id.ToString();

    // Put the top frame first; otherwise, any order.
    if (result.frame_id == ExtensionApiFrameIdMap::kTopFrameId) {
      injection_results.insert(injection_results.begin(),
                               std::move(injection_result));
    } else {
      injection_results.push_back(std::move(injection_result));
    }
  }

  Respond(ArgumentList(
      api::scripting::ExecuteScript::Results::Create(injection_results)));
}

ScriptingInsertCSSFunction::ScriptingInsertCSSFunction() = default;
ScriptingInsertCSSFunction::~ScriptingInsertCSSFunction() = default;

ExtensionFunction::ResponseAction ScriptingInsertCSSFunction::Run() {
  absl::optional<api::scripting::InsertCSS::Params> params =
      api::scripting::InsertCSS::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  injection_ = std::move(params->injection);

  if ((injection_.files && injection_.css) ||
      (!injection_.files && !injection_.css)) {
    return RespondNow(Error(kExactlyOneOfCssAndFilesError));
  }

  if (injection_.files) {
    // CSS files require localization.
    constexpr bool kRequiresLocalization = true;
    std::string error;
    if (!CheckAndLoadFiles(
            std::move(*injection_.files), *extension(), kRequiresLocalization,
            base::BindOnce(&ScriptingInsertCSSFunction::DidLoadResources, this),
            &error)) {
      return RespondNow(Error(std::move(error)));
    }
    return RespondLater();
  }

  DCHECK(injection_.css);

  mojom::HostID host_id(mojom::HostID::HostType::kExtensions,
                        extension()->id());

  std::vector<mojom::CSSSourcePtr> sources;
  sources.push_back(
      mojom::CSSSource::New(std::move(*injection_.css),
                            InjectionKeyForCode(host_id, *injection_.css)));

  std::string error;
  if (!Execute(std::move(sources), &error)) {
    return RespondNow(Error(std::move(error)));
  }

  return RespondLater();
}

void ScriptingInsertCSSFunction::DidLoadResources(
    std::vector<InjectedFileSource> file_sources,
    absl::optional<std::string> load_error) {
  if (load_error) {
    Respond(Error(std::move(*load_error)));
    return;
  }

  DCHECK(!file_sources.empty());
  std::vector<mojom::CSSSourcePtr> sources =
      FileSourcesToCSSSources(*extension(), std::move(file_sources));

  std::string error;
  if (!Execute(std::move(sources), &error))
    Respond(Error(std::move(error)));
}

bool ScriptingInsertCSSFunction::Execute(
    std::vector<mojom::CSSSourcePtr> sources,
    std::string* error) {
  ScriptExecutor* script_executor = nullptr;
  ScriptExecutor::FrameScope frame_scope = ScriptExecutor::SPECIFIED_FRAMES;
  std::set<int> frame_ids;
  if (!CanAccessTarget(*extension()->permissions_data(), injection_.target,
                       browser_context(), include_incognito_information(),
                       &script_executor, &frame_scope, &frame_ids, error)) {
    return false;
  }
  DCHECK(script_executor);

  script_executor->ExecuteScript(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension()->id()),
      mojom::CodeInjection::NewCss(mojom::CSSInjection::New(
          std::move(sources), ConvertStyleOriginToCSSOrigin(injection_.origin),
          mojom::CSSInjection::Operation::kAdd)),
      frame_scope, frame_ids, ScriptExecutor::MATCH_ABOUT_BLANK,
      kCSSRunLocation, ScriptExecutor::DEFAULT_PROCESS,
      /* webview_src */ GURL(),
      base::BindOnce(&ScriptingInsertCSSFunction::OnCSSInserted, this));

  return true;
}

void ScriptingInsertCSSFunction::OnCSSInserted(
    std::vector<ScriptExecutor::FrameResult> results) {
  // If only a single frame was included and the injection failed, respond with
  // an error.
  if (results.size() == 1 && !results[0].error.empty()) {
    Respond(Error(std::move(results[0].error)));
    return;
  }

  Respond(NoArguments());
}

ScriptingRemoveCSSFunction::ScriptingRemoveCSSFunction() = default;
ScriptingRemoveCSSFunction::~ScriptingRemoveCSSFunction() = default;

ExtensionFunction::ResponseAction ScriptingRemoveCSSFunction::Run() {
  absl::optional<api::scripting::RemoveCSS::Params> params =
      api::scripting::RemoveCSS::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  api::scripting::CSSInjection& injection = params->injection;

  if ((injection.files && injection.css) ||
      (!injection.files && !injection.css)) {
    return RespondNow(Error(kExactlyOneOfCssAndFilesError));
  }

  ScriptExecutor* script_executor = nullptr;
  ScriptExecutor::FrameScope frame_scope = ScriptExecutor::SPECIFIED_FRAMES;
  std::set<int> frame_ids;
  std::string error;
  if (!CanAccessTarget(*extension()->permissions_data(), injection.target,
                       browser_context(), include_incognito_information(),
                       &script_executor, &frame_scope, &frame_ids, &error)) {
    return RespondNow(Error(std::move(error)));
  }
  DCHECK(script_executor);

  mojom::HostID host_id(mojom::HostID::HostType::kExtensions,
                        extension()->id());
  std::vector<mojom::CSSSourcePtr> sources;

  if (injection.files) {
    std::vector<ExtensionResource> resources;
    if (!GetFileResources(*injection.files, *extension(), &resources, &error))
      return RespondNow(Error(std::move(error)));

    // Note: Since we're just removing the CSS, we don't actually need to load
    // the file here. It's okay for `code` to be empty in this case.
    const std::string empty_code;
    sources.reserve(injection.files->size());

    for (const auto& file : *injection.files) {
      sources.push_back(mojom::CSSSource::New(
          empty_code,
          InjectionKeyForFile(host_id, extension()->GetResourceURL(file))));
    }
  } else {
    DCHECK(injection.css);
    sources.push_back(
        mojom::CSSSource::New(std::move(*injection.css),
                              InjectionKeyForCode(host_id, *injection.css)));
  }

  script_executor->ExecuteScript(
      std::move(host_id),
      mojom::CodeInjection::NewCss(mojom::CSSInjection::New(
          std::move(sources), ConvertStyleOriginToCSSOrigin(injection.origin),
          mojom::CSSInjection::Operation::kRemove)),
      frame_scope, frame_ids, ScriptExecutor::MATCH_ABOUT_BLANK,
      kCSSRunLocation, ScriptExecutor::DEFAULT_PROCESS,
      /* webview_src */ GURL(),
      base::BindOnce(&ScriptingRemoveCSSFunction::OnCSSRemoved, this));

  return RespondLater();
}

void ScriptingRemoveCSSFunction::OnCSSRemoved(
    std::vector<ScriptExecutor::FrameResult> results) {
  // If only a single frame was included and the injection failed, respond with
  // an error.
  if (results.size() == 1 && !results[0].error.empty()) {
    Respond(Error(std::move(results[0].error)));
    return;
  }

  Respond(NoArguments());
}

ScriptingRegisterContentScriptsFunction::
    ScriptingRegisterContentScriptsFunction() = default;
ScriptingRegisterContentScriptsFunction::
    ~ScriptingRegisterContentScriptsFunction() = default;

ExtensionFunction::ResponseAction
ScriptingRegisterContentScriptsFunction::Run() {
  absl::optional<api::scripting::RegisterContentScripts::Params> params =
      api::scripting::RegisterContentScripts::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<api::scripting::RegisteredContentScript>& scripts =
      params->scripts;

  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(browser_context())
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension()->id());
  std::set<std::string> existing_script_ids = loader->GetDynamicScriptIDs();
  std::set<std::string> new_script_ids;
  for (const auto& script : scripts) {
    if (script.id.empty())
      return RespondNow(Error("Content script's ID must not be empty"));

    if (script.id[0] == UserScript::kGeneratedIDPrefix) {
      return RespondNow(Error(base::StringPrintf(
          "Content script's ID '%s' must not start with '%c'",
          script.id.c_str(), UserScript::kGeneratedIDPrefix)));
    }

    if (base::Contains(existing_script_ids, script.id) ||
        base::Contains(new_script_ids, script.id)) {
      return RespondNow(Error(
          base::StringPrintf("Duplicate script ID '%s'", script.id.c_str())));
    }

    new_script_ids.insert(script.id);
  }

  std::u16string parse_error;
  auto parsed_scripts = std::make_unique<UserScriptList>();
  std::set<std::string> persistent_script_ids;
  const int valid_schemes = UserScript::ValidUserScriptSchemes(
      scripting::kScriptsCanExecuteEverywhere);

  parsed_scripts->reserve(scripts.size());
  for (size_t i = 0; i < scripts.size(); ++i) {
    if (!scripts[i].matches) {
      return RespondNow(
          Error(base::StringPrintf("Script with ID '%s' must specify 'matches'",
                                   scripts[i].id.c_str())));
    }

    // Parse/Create user script.
    std::unique_ptr<UserScript> user_script =
        ParseUserScript(browser_context(), *extension(), scripts[i], i,
                        valid_schemes, &parse_error);
    if (!user_script)
      return RespondNow(Error(base::UTF16ToASCII(parse_error)));

    // Scripts will persist across sessions by default.
    if (!scripts[i].persist_across_sessions ||
        *scripts[i].persist_across_sessions) {
      persistent_script_ids.insert(user_script->id());
    }
    parsed_scripts->push_back(std::move(user_script));
  }

  // Add new script IDs now in case another call with the same script IDs is
  // made immediately following this one.
  loader->AddPendingDynamicScriptIDs(std::move(new_script_ids));

  GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ValidateParsedScriptsOnFileThread,
                     script_parsing::GetSymlinkPolicy(extension()),
                     std::move(parsed_scripts)),
      base::BindOnce(&ScriptingRegisterContentScriptsFunction::
                         OnContentScriptFilesValidated,
                     this, std::move(persistent_script_ids)));

  // Balanced in `OnContentScriptFilesValidated()` or
  // `OnContentScriptsRegistered()`.
  AddRef();
  return RespondLater();
}

void ScriptingRegisterContentScriptsFunction::OnContentScriptFilesValidated(
    std::set<std::string> persistent_script_ids,
    ValidateContentScriptsResult result) {
  // We cannot proceed if the `browser_context` is not valid as the
  // `ExtensionSystem` will not exist.
  if (!browser_context()) {
    return;
  }

  auto error = std::move(result.second);
  auto scripts = std::move(result.first);
  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(browser_context())
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension()->id());

  if (error.has_value()) {
    std::set<std::string> ids_to_remove;
    for (const auto& script : *scripts)
      ids_to_remove.insert(script->id());

    loader->RemovePendingDynamicScriptIDs(std::move(ids_to_remove));
    Respond(Error(std::move(*error)));
    Release();  // Matches the `AddRef()` in `Run()`.
    return;
  }

  loader->AddDynamicScripts(
      std::move(scripts), std::move(persistent_script_ids),
      base::BindOnce(
          &ScriptingRegisterContentScriptsFunction::OnContentScriptsRegistered,
          this));
}

void ScriptingRegisterContentScriptsFunction::OnContentScriptsRegistered(
    const absl::optional<std::string>& error) {
  if (error.has_value())
    Respond(Error(std::move(*error)));
  else
    Respond(NoArguments());
  Release();  // Matches the `AddRef()` in `Run()`.
}

ScriptingGetRegisteredContentScriptsFunction::
    ScriptingGetRegisteredContentScriptsFunction() = default;
ScriptingGetRegisteredContentScriptsFunction::
    ~ScriptingGetRegisteredContentScriptsFunction() = default;

ExtensionFunction::ResponseAction
ScriptingGetRegisteredContentScriptsFunction::Run() {
  absl::optional<api::scripting::GetRegisteredContentScripts::Params> params =
      api::scripting::GetRegisteredContentScripts::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const absl::optional<api::scripting::ContentScriptFilter>& filter =
      params->filter;
  std::set<std::string> id_filter;
  if (filter && filter->ids) {
    id_filter.insert(std::make_move_iterator(filter->ids->begin()),
                     std::make_move_iterator(filter->ids->end()));
  }

  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(browser_context())
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension()->id());
  const UserScriptList& dynamic_scripts = loader->GetLoadedDynamicScripts();

  std::vector<api::scripting::RegisteredContentScript> script_infos;
  std::set<std::string> persistent_script_ids =
      loader->GetPersistentDynamicScriptIDs();
  for (const std::unique_ptr<UserScript>& script : dynamic_scripts) {
    if (id_filter.empty() || base::Contains(id_filter, script->id())) {
      auto registered_script = CreateRegisteredContentScriptInfo(*script);
      registered_script.persist_across_sessions =
          base::Contains(persistent_script_ids, script->id());
      script_infos.push_back(std::move(registered_script));
    }
  }

  return RespondNow(
      ArgumentList(api::scripting::GetRegisteredContentScripts::Results::Create(
          script_infos)));
}

ScriptingUnregisterContentScriptsFunction::
    ScriptingUnregisterContentScriptsFunction() = default;
ScriptingUnregisterContentScriptsFunction::
    ~ScriptingUnregisterContentScriptsFunction() = default;

ExtensionFunction::ResponseAction
ScriptingUnregisterContentScriptsFunction::Run() {
  auto params =
      api::scripting::UnregisterContentScripts::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  absl::optional<api::scripting::ContentScriptFilter>& filter = params->filter;
  std::set<std::string> ids_to_remove;

  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(browser_context())
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension()->id());
  std::set<std::string> existing_script_ids = loader->GetDynamicScriptIDs();
  if (filter && filter->ids) {
    for (const auto& id : *filter->ids) {
      if (UserScript::IsIDGenerated(id)) {
        return RespondNow(Error(base::StringPrintf(
            "Content script's ID '%s' must not start with '%c'", id.c_str(),
            UserScript::kGeneratedIDPrefix)));
      }

      if (!base::Contains(existing_script_ids, id)) {
        return RespondNow(Error(
            base::StringPrintf("Nonexistent script ID '%s'", id.c_str())));
      }

      ids_to_remove.insert(id);
    }
  }

  if (ids_to_remove.empty()) {
    loader->ClearDynamicScripts(
        base::BindOnce(&ScriptingUnregisterContentScriptsFunction::
                           OnContentScriptsUnregistered,
                       this));
  } else {
    loader->RemoveDynamicScripts(
        std::move(ids_to_remove),
        base::BindOnce(&ScriptingUnregisterContentScriptsFunction::
                           OnContentScriptsUnregistered,
                       this));
  }

  return RespondLater();
}

void ScriptingUnregisterContentScriptsFunction::OnContentScriptsUnregistered(
    const absl::optional<std::string>& error) {
  if (error.has_value())
    Respond(Error(std::move(*error)));
  else
    Respond(NoArguments());
}

ScriptingUpdateContentScriptsFunction::ScriptingUpdateContentScriptsFunction() =
    default;
ScriptingUpdateContentScriptsFunction::
    ~ScriptingUpdateContentScriptsFunction() = default;

ExtensionFunction::ResponseAction ScriptingUpdateContentScriptsFunction::Run() {
  absl::optional<api::scripting::UpdateContentScripts::Params> params =
      api::scripting::UpdateContentScripts::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<api::scripting::RegisteredContentScript>& scripts =
      params->scripts;

  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(browser_context())
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension()->id());

  std::map<std::string, api::scripting::RegisteredContentScript>
      loaded_scripts_metadata;
  const UserScriptList& dynamic_scripts = loader->GetLoadedDynamicScripts();
  for (const std::unique_ptr<UserScript>& script : dynamic_scripts) {
    loaded_scripts_metadata.emplace(script->id(),
                                    CreateRegisteredContentScriptInfo(*script));
  }

  std::set<std::string> ids_to_update;
  for (const auto& script : scripts) {
    if (loaded_scripts_metadata.find(script.id) ==
        loaded_scripts_metadata.end()) {
      return RespondNow(
          Error(base::StringPrintf("Content script with ID '%s' does not exist "
                                   "or is not fully registered",
                                   script.id.c_str())));
    }

    DCHECK(!script.id.empty());
    DCHECK(!UserScript::IsIDGenerated(script.id));

    if (base::Contains(ids_to_update, script.id)) {
      return RespondNow(Error(
          base::StringPrintf("Duplicate script ID '%s'", script.id.c_str())));
    }

    ids_to_update.insert(script.id);
  }

  std::u16string parse_error;
  auto parsed_scripts = std::make_unique<UserScriptList>();
  const int valid_schemes = UserScript::ValidUserScriptSchemes(
      scripting::kScriptsCanExecuteEverywhere);

  std::set<std::string> updated_script_ids_to_persist;
  std::set<std::string> persistent_script_ids =
      loader->GetPersistentDynamicScriptIDs();

  parsed_scripts->reserve(scripts.size());
  for (size_t i = 0; i < scripts.size(); ++i) {
    api::scripting::RegisteredContentScript& update_delta = scripts[i];
    DCHECK(base::Contains(loaded_scripts_metadata, update_delta.id));

    api::scripting::RegisteredContentScript& updated_script =
        loaded_scripts_metadata[update_delta.id];

    if (update_delta.matches)
      updated_script.matches = std::move(update_delta.matches);

    if (update_delta.exclude_matches)
      updated_script.exclude_matches = std::move(update_delta.exclude_matches);

    if (update_delta.js)
      updated_script.js = std::move(update_delta.js);

    if (update_delta.css)
      updated_script.css = std::move(update_delta.css);

    if (update_delta.all_frames)
      *updated_script.all_frames = *update_delta.all_frames;

    if (update_delta.match_origin_as_fallback) {
      *updated_script.match_origin_as_fallback =
          *update_delta.match_origin_as_fallback;
    }

    if (update_delta.run_at != api::extension_types::RunAt::kNone) {
      updated_script.run_at = update_delta.run_at;
    }

    // Parse/Create user script.
    std::unique_ptr<UserScript> user_script =
        ParseUserScript(browser_context(), *extension(), updated_script, i,
                        valid_schemes, &parse_error);
    if (!user_script)
      return RespondNow(Error(base::UTF16ToASCII(parse_error)));

    // Persist the updated script if the flag is specified as true, or if the
    // original script is persisted and the flag is not specified.
    if ((update_delta.persist_across_sessions &&
         *update_delta.persist_across_sessions) ||
        (!update_delta.persist_across_sessions &&
         base::Contains(persistent_script_ids, update_delta.id))) {
      updated_script_ids_to_persist.insert(update_delta.id);
    }

    parsed_scripts->push_back(std::move(user_script));
  }

  // Add new script IDs now in case another call with the same script IDs is
  // made immediately following this one.
  loader->AddPendingDynamicScriptIDs(std::move(ids_to_update));

  GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ValidateParsedScriptsOnFileThread,
                     script_parsing::GetSymlinkPolicy(extension()),
                     std::move(parsed_scripts)),
      base::BindOnce(
          &ScriptingUpdateContentScriptsFunction::OnContentScriptFilesValidated,
          this, std::move(updated_script_ids_to_persist)));

  // Balanced in `OnContentScriptFilesValidated()` or
  // `OnContentScriptsRegistered()`.
  AddRef();
  return RespondLater();
}

void ScriptingUpdateContentScriptsFunction::OnContentScriptFilesValidated(
    std::set<std::string> persistent_script_ids,
    ValidateContentScriptsResult result) {
  // We cannot proceed if the `browser_context` is not valid as the
  // `ExtensionSystem` will not exist.
  if (!browser_context()) {
    return;
  }

  auto error = std::move(result.second);
  auto scripts = std::move(result.first);
  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(browser_context())
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension()->id());

  std::set<std::string> script_ids;
  for (const auto& script : *scripts)
    script_ids.insert(script->id());

  if (error.has_value()) {
    loader->RemovePendingDynamicScriptIDs(script_ids);
    Respond(Error(std::move(*error)));
    Release();  // Matches the `AddRef()` in `Run()`.
    return;
  }

  // To guarantee that scripts are updated, they need to be removed then added
  // again. It should be guaranteed that the new scripts are added after the old
  // ones are removed.
  loader->RemoveDynamicScripts(script_ids, /*callback=*/base::DoNothing());

  // Since RemoveDynamicScripts will remove pending script IDs, but
  // AddDynamicScripts will only add scripts that are marked as pending, we must
  // mark `script_ids` as pending again here.
  loader->AddPendingDynamicScriptIDs(std::move(script_ids));

  loader->AddDynamicScripts(
      std::move(scripts), std::move(persistent_script_ids),
      base::BindOnce(
          &ScriptingUpdateContentScriptsFunction::OnContentScriptsUpdated,
          this));
}

void ScriptingUpdateContentScriptsFunction::OnContentScriptsUpdated(
    const absl::optional<std::string>& error) {
  if (error.has_value())
    Respond(Error(std::move(*error)));
  else
    Respond(NoArguments());
  Release();  // Matches the `AddRef()` in `Run()`.
}

}  // namespace extensions
