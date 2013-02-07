// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/public/pref_member.h"

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/prefs/public/pref_service_base.h"
#include "base/value_conversions.h"

using base::MessageLoopProxy;

namespace subtle {

PrefMemberBase::PrefMemberBase()
    : prefs_(NULL),
      setting_value_(false) {
}

PrefMemberBase::~PrefMemberBase() {
  Destroy();
}

void PrefMemberBase::Init(const char* pref_name,
                          PrefServiceBase* prefs,
                          const NamedChangeCallback& observer) {
  observer_ = observer;
  Init(pref_name, prefs);
}

void PrefMemberBase::Init(const char* pref_name,
                          PrefServiceBase* prefs) {
  DCHECK(pref_name);
  DCHECK(prefs);
  DCHECK(pref_name_.empty());  // Check that Init is only called once.
  prefs_ = prefs;
  pref_name_ = pref_name;
  // Check that the preference is registered.
  DCHECK(prefs_->FindPreference(pref_name_.c_str()))
      << pref_name << " not registered.";

  // Add ourselves as a pref observer so we can keep our local value in sync.
  prefs_->AddPrefObserver(pref_name, this);
}

void PrefMemberBase::Destroy() {
  if (prefs_ && !pref_name_.empty()) {
    prefs_->RemovePrefObserver(pref_name_.c_str(), this);
    prefs_ = NULL;
  }
}

void PrefMemberBase::MoveToThread(
    const scoped_refptr<MessageLoopProxy>& message_loop) {
  VerifyValuePrefName();
  // Load the value from preferences if it hasn't been loaded so far.
  if (!internal())
    UpdateValueFromPref(base::Closure());
  internal()->MoveToThread(message_loop);
}

void PrefMemberBase::OnPreferenceChanged(PrefServiceBase* service,
                                         const std::string& pref_name) {
  VerifyValuePrefName();
  UpdateValueFromPref((!setting_value_ && !observer_.is_null()) ?
      base::Bind(observer_, pref_name) : base::Closure());
}

void PrefMemberBase::UpdateValueFromPref(const base::Closure& callback) const {
  VerifyValuePrefName();
  const PrefServiceBase::Preference* pref =
      prefs_->FindPreference(pref_name_.c_str());
  DCHECK(pref);
  if (!internal())
    CreateInternal();
  internal()->UpdateValue(pref->GetValue()->DeepCopy(),
                          pref->IsManaged(),
                          pref->IsUserModifiable(),
                          callback);
}

void PrefMemberBase::VerifyPref() const {
  VerifyValuePrefName();
  if (!internal())
    UpdateValueFromPref(base::Closure());
}

void PrefMemberBase::InvokeUnnamedCallback(const base::Closure& callback,
                                           const std::string& pref_name) {
  callback.Run();
}

PrefMemberBase::Internal::Internal()
    : thread_loop_(MessageLoopProxy::current()),
      is_managed_(false) {
}
PrefMemberBase::Internal::~Internal() { }

bool PrefMemberBase::Internal::IsOnCorrectThread() const {
  // In unit tests, there may not be a message loop.
  return thread_loop_ == NULL || thread_loop_->BelongsToCurrentThread();
}

void PrefMemberBase::Internal::UpdateValue(
    Value* v,
    bool is_managed,
    bool is_user_modifiable,
    const base::Closure& callback) const {
  scoped_ptr<Value> value(v);
  base::ScopedClosureRunner closure_runner(callback);
  if (IsOnCorrectThread()) {
    bool rv = UpdateValueInternal(*value);
    DCHECK(rv);
    is_managed_ = is_managed;
    is_user_modifiable_ = is_user_modifiable;
  } else {
    bool may_run = thread_loop_->PostTask(
        FROM_HERE,
        base::Bind(&PrefMemberBase::Internal::UpdateValue, this,
                   value.release(), is_managed, is_user_modifiable,
                   closure_runner.Release()));
    DCHECK(may_run);
  }
}

void PrefMemberBase::Internal::MoveToThread(
    const scoped_refptr<MessageLoopProxy>& message_loop) {
  CheckOnCorrectThread();
  thread_loop_ = message_loop;
}

bool PrefMemberVectorStringUpdate(const Value& value,
                                  std::vector<std::string>* string_vector) {
  if (!value.IsType(Value::TYPE_LIST))
    return false;
  const ListValue* list = static_cast<const ListValue*>(&value);

  std::vector<std::string> local_vector;
  for (ListValue::const_iterator it = list->begin(); it != list->end(); ++it) {
    std::string string_value;
    if (!(*it)->GetAsString(&string_value))
      return false;

    local_vector.push_back(string_value);
  }

  string_vector->swap(local_vector);
  return true;
}

}  // namespace subtle

template <>
void PrefMember<bool>::UpdatePref(const bool& value) {
  prefs()->SetBoolean(pref_name().c_str(), value);
}

template <>
bool PrefMember<bool>::Internal::UpdateValueInternal(const Value& value) const {
  return value.GetAsBoolean(&value_);
}

template <>
void PrefMember<int>::UpdatePref(const int& value) {
  prefs()->SetInteger(pref_name().c_str(), value);
}

template <>
bool PrefMember<int>::Internal::UpdateValueInternal(const Value& value) const {
  return value.GetAsInteger(&value_);
}

template <>
void PrefMember<double>::UpdatePref(const double& value) {
  prefs()->SetDouble(pref_name().c_str(), value);
}

template <>
bool PrefMember<double>::Internal::UpdateValueInternal(const Value& value)
    const {
  return value.GetAsDouble(&value_);
}

template <>
void PrefMember<std::string>::UpdatePref(const std::string& value) {
  prefs()->SetString(pref_name().c_str(), value);
}

template <>
bool PrefMember<std::string>::Internal::UpdateValueInternal(const Value& value)
    const {
  return value.GetAsString(&value_);
}

template <>
void PrefMember<FilePath>::UpdatePref(const FilePath& value) {
  prefs()->SetFilePath(pref_name().c_str(), value);
}

template <>
bool PrefMember<FilePath>::Internal::UpdateValueInternal(const Value& value)
    const {
  return base::GetValueAsFilePath(value, &value_);
}

template <>
void PrefMember<std::vector<std::string> >::UpdatePref(
    const std::vector<std::string>& value) {
  ListValue list_value;
  list_value.AppendStrings(value);
  prefs()->Set(pref_name().c_str(), list_value);
}

template <>
bool PrefMember<std::vector<std::string> >::Internal::UpdateValueInternal(
    const Value& value) const {
  return subtle::PrefMemberVectorStringUpdate(value, &value_);
}
