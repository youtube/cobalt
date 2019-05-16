// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "cobalt/base/c_val.h"

#include <sstream>

namespace base {

CValManager* CValManager::GetInstance() {
  return base::Singleton<
      CValManager, base::StaticMemorySingletonTraits<CValManager> >::get();
}

CValManager::CValManager() : value_lock_refptr_(new RefCountedThreadSafeLock) {
  // Allocate these dynamically since CValManager may live across DLL boundaries
  // and so this allows its size to be more consistent (and avoids compiler
  // warnings on some platforms).
  registered_vars_ = new NameVarMap();
#if defined(ENABLE_DEBUG_C_VAL)
  on_changed_hook_set_ = new base::hash_set<OnChangedHook*>();
#endif  // ENABLE_DEBUG_C_VAL
}

CValManager::~CValManager() {
  // Lock the value lock prior to notifying the surviving CVals that the manager
  // has been destroyed. This ensures that they will not attempt to make calls
  // into the manager during destruction.
  base::AutoLock auto_lock(value_lock_refptr_->GetLock());
  for (NameVarMap::iterator iter = registered_vars_->begin();
       iter != registered_vars_->end(); ++iter) {
    iter->second->OnManagerDestroyed();
  }

#if defined(ENABLE_DEBUG_C_VAL)
  delete on_changed_hook_set_;
#endif  // ENABLE_DEBUG_C_VAL
  delete registered_vars_;
}

void CValManager::RegisterCVal(
    const CValDetail::CValBase* cval,
    scoped_refptr<base::RefCountedThreadSafeLock>* value_lock) {
  base::AutoLock auto_lock(cvals_lock_);

  // CVals cannot share name.  If this assert is triggered, we are trying to
  // register more than one CVal with the same name, which this system is
  // not designed to handle.
  DCHECK(registered_vars_->find(cval->GetName()) == registered_vars_->end());

  (*registered_vars_)[cval->GetName()] = cval;
  *value_lock = value_lock_refptr_;
}

void CValManager::UnregisterCVal(const CValDetail::CValBase* cval) {
  base::AutoLock auto_lock(cvals_lock_);

  NameVarMap::iterator found = registered_vars_->find(cval->GetName());
  DCHECK(found != registered_vars_->end());
  if (found != registered_vars_->end()) {
    registered_vars_->erase(found);
  }
}

#if defined(ENABLE_DEBUG_C_VAL)
void CValManager::PushValueChangedEvent(const CValDetail::CValBase* cval,
                                        const CValGenericValue& value) {
  base::AutoLock auto_lock(hooks_lock_);

  // Iterate through the hooks sending each of them the value changed event.
  for (OnChangeHookSet::iterator iter = on_changed_hook_set_->begin();
       iter != on_changed_hook_set_->end(); ++iter) {
    (*iter)->OnValueChanged(cval->GetName(), value);
  }
}

CValManager::OnChangedHook::OnChangedHook() {
  CValManager* cvm = CValManager::GetInstance();
  base::AutoLock auto_lock(cvm->hooks_lock_);

  // Register this hook with the CValManager
  bool was_inserted ALLOW_UNUSED_TYPE =
      cvm->on_changed_hook_set_->insert(this).second;
  DCHECK(was_inserted);
}

CValManager::OnChangedHook::~OnChangedHook() {
  CValManager* cvm = CValManager::GetInstance();
  base::AutoLock auto_lock(cvm->hooks_lock_);

  // Deregister this hook with the CValManager
  size_t values_erased ALLOW_UNUSED_TYPE =
      cvm->on_changed_hook_set_->erase(this);
  DCHECK_EQ(values_erased, 1);
}
#endif  // ENABLE_DEBUG_C_VAL

std::set<std::string> CValManager::GetOrderedCValNames() {
  std::set<std::string> ret;
  base::AutoLock auto_lock(cvals_lock_);

  // Return all registered CVal names, as an std::set to show they are sorted.
  for (NameVarMap::const_iterator iter = registered_vars_->begin();
       iter != registered_vars_->end(); ++iter) {
    ret.insert(iter->first);
  }

  return ret;
}

Optional<std::string> CValManager::GetCValStringValue(const std::string& name,
                                                      bool pretty) {
  base::AutoLock auto_lock(cvals_lock_);

  // Return the value of a CVal, if it exists.  If it does not exist,
  // indicate that it does not exist.
  NameVarMap::const_iterator found = registered_vars_->find(name);
  if (found == registered_vars_->end()) {
    return nullopt;
  } else {
    return (pretty ? found->second->GetValueAsPrettyString()
                   : found->second->GetValueAsString());
  }
}

Optional<std::string> CValManager::GetValueAsString(const std::string& name) {
  return GetCValStringValue(name, false);
}

Optional<std::string> CValManager::GetValueAsPrettyString(
    const std::string& name) {
  return GetCValStringValue(name, true);
}

}  // namespace base
