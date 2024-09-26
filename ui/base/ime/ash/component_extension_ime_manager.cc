// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/component_extension_ime_manager.h"

#include <stddef.h>
#include <utility>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/ime/ash/extension_ime_util.h"

namespace ash {

namespace {

// Gets the input method category according to the given input method id.
// This is used for sorting a list of input methods.
int GetInputMethodCategory(const std::string& id) {
  const std::string engine_id =
      extension_ime_util::GetComponentIDByInputMethodID(id);
  if (base::StartsWith(engine_id, "xkb:", base::CompareCase::SENSITIVE))
    return 0;
  if (base::StartsWith(engine_id, "vkd_", base::CompareCase::SENSITIVE))
    return 1;
  if (engine_id.find("-t-i0-") != std::string::npos &&
      !base::StartsWith(engine_id, "zh-", base::CompareCase::SENSITIVE)) {
    return 2;
  }
  return 3;
}

bool InputMethodCompare(const input_method::InputMethodDescriptor& im1,
                        const input_method::InputMethodDescriptor& im2) {
  return GetInputMethodCategory(im1.id()) < GetInputMethodCategory(im2.id());
}

} // namespace

ComponentExtensionEngine::ComponentExtensionEngine() = default;

ComponentExtensionEngine::ComponentExtensionEngine(
    const ComponentExtensionEngine& other) = default;

ComponentExtensionEngine::~ComponentExtensionEngine() = default;

ComponentExtensionIME::ComponentExtensionIME() = default;

ComponentExtensionIME::ComponentExtensionIME(
    const ComponentExtensionIME& other) = default;

ComponentExtensionIME::~ComponentExtensionIME() = default;

ComponentExtensionIMEManager::ComponentExtensionIMEManager(
    std::unique_ptr<ComponentExtensionIMEManagerDelegate> delegate)
    : delegate_(std::move(delegate)) {
  // Creates internal mapping between input method id and engine components.
  std::vector<ComponentExtensionIME> ext_list = delegate_->ListIME();
  for (const auto& ext : ext_list) {
    bool extension_exists = IsAllowlistedExtension(ext.id);
    if (!extension_exists)
      component_extension_imes_[ext.id] = ext;
    for (const auto& ime : ext.engines) {
      const std::string input_method_id =
          extension_ime_util::GetComponentInputMethodID(ext.id, ime.engine_id);
      if (extension_exists && !IsAllowlisted(input_method_id))
        component_extension_imes_[ext.id].engines.push_back(ime);
      input_method_id_set_.insert(input_method_id);
    }
  }
}

ComponentExtensionIMEManager::~ComponentExtensionIMEManager() = default;

bool ComponentExtensionIMEManager::LoadComponentExtensionIME(
    Profile* profile,
    const std::string& input_method_id,
    std::set<std::string>* extension_loaded) {
  TRACE_EVENT0("ime",
               "ComponentExtensionIMEManager::LoadComponentExtensionIME");
  ComponentExtensionIME ime;
  if (FindEngineEntry(input_method_id, &ime)) {
    bool will_load = extension_loaded == nullptr;
    if (!will_load &&
        extension_loaded->find(ime.id) == extension_loaded->end()) {
      extension_loaded->insert(ime.id);
      will_load = true;
    }
    if (will_load)
      delegate_->Load(profile, ime.id, ime.manifest, ime.path);
    return will_load;
  }
  return false;
}

bool ComponentExtensionIMEManager::IsAllowlisted(
    const std::string& input_method_id) {
  return input_method_id_set_.find(input_method_id) !=
         input_method_id_set_.end();
}

bool ComponentExtensionIMEManager::IsAllowlistedExtension(
    const std::string& extension_id) {
  return component_extension_imes_.find(extension_id) !=
         component_extension_imes_.end();
}

input_method::InputMethodDescriptors
    ComponentExtensionIMEManager::GetAllIMEAsInputMethodDescriptor() {
  input_method::InputMethodDescriptors result;
  for (std::map<std::string, ComponentExtensionIME>::const_iterator it =
          component_extension_imes_.begin();
       it != component_extension_imes_.end(); ++it) {
    const ComponentExtensionIME& ext = it->second;
    for (size_t j = 0; j < ext.engines.size(); ++j) {
      const ComponentExtensionEngine& ime = ext.engines[j];
      const std::string input_method_id =
          extension_ime_util::GetComponentInputMethodID(
              ext.id, ime.engine_id);
      result.push_back(input_method::InputMethodDescriptor(
          input_method_id, ime.display_name, ime.indicator, ime.layout,
          ime.language_codes,
          // Enables extension based xkb keyboards on login screen.
          extension_ime_util::IsKeyboardLayoutExtension(input_method_id) &&
              delegate_->IsInLoginLayoutAllowlist(ime.layout),
          ime.options_page_url, ime.input_view_url));
    }
  }
  std::stable_sort(result.begin(), result.end(), InputMethodCompare);
  return result;
}

input_method::InputMethodDescriptors
ComponentExtensionIMEManager::GetXkbIMEAsInputMethodDescriptor() {
  input_method::InputMethodDescriptors result;
  const input_method::InputMethodDescriptors& descriptors =
      GetAllIMEAsInputMethodDescriptor();
  for (const auto& descriptor : descriptors) {
    if (extension_ime_util::IsKeyboardLayoutExtension(descriptor.id()))
      result.push_back(descriptor);
  }
  return result;
}

bool ComponentExtensionIMEManager::FindEngineEntry(
    const std::string& input_method_id,
    ComponentExtensionIME* out_extension) {
  if (!IsAllowlisted(input_method_id))
    return false;

  std::string extension_id =
      extension_ime_util::GetExtensionIDFromInputMethodID(input_method_id);
  auto it = component_extension_imes_.find(extension_id);
  if (it == component_extension_imes_.end())
    return false;

  if (out_extension)
    *out_extension = it->second;
  return true;
}

}  // namespace ash
