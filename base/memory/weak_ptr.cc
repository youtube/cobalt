// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"

namespace base {
namespace internal {

WeakReference::Flag::Flag(Flag** handle) : handle_(handle) {
}

void WeakReference::Flag::Invalidate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  handle_ = NULL;
}

bool WeakReference::Flag::IsValid() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return handle_ != NULL;
}

WeakReference::Flag::~Flag() {
  if (handle_)
    *handle_ = NULL;
}

WeakReference::WeakReference() {
}

WeakReference::WeakReference(Flag* flag) : flag_(flag) {
}

WeakReference::~WeakReference() {
}

bool WeakReference::is_valid() const {
  return flag_ && flag_->IsValid();
}

WeakReferenceOwner::WeakReferenceOwner() : flag_(NULL) {
}

WeakReferenceOwner::~WeakReferenceOwner() {
  Invalidate();
}

WeakReference WeakReferenceOwner::GetRef() const {
  if (!flag_)
    flag_ = new WeakReference::Flag(&flag_);
  return WeakReference(flag_);
}

void WeakReferenceOwner::Invalidate() {
  if (flag_) {
    flag_->Invalidate();
    flag_ = NULL;
  }
}

WeakPtrBase::WeakPtrBase() {
}

WeakPtrBase::~WeakPtrBase() {
}

WeakPtrBase::WeakPtrBase(const WeakReference& ref) : ref_(ref) {
}

}  // namespace internal
}  // namespace base
