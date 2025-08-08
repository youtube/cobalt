// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_
#define CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "content/public/browser/session_storage_namespace.h"

namespace content {
class DOMStorageContextWrapper;

class SessionStorageNamespaceImpl : public SessionStorageNamespace {
 public:
  // Constructs a SessionStorageNamespaceImpl and allocates a new ID for it.
  //
  // The allows TestRenderViewHost to instantiate these.
  static scoped_refptr<SessionStorageNamespaceImpl> Create(
      scoped_refptr<DOMStorageContextWrapper> context);

  // If there is an existing SessionStorageNamespaceImpl with the given id in
  // the DOMStorageContextWrapper, this will return that object. Otherwise this
  // constructs a SessionStorageNamespaceImpl and assigns |namespace_id| to it.
  static scoped_refptr<SessionStorageNamespaceImpl> Create(
      scoped_refptr<DOMStorageContextWrapper> context,
      std::string namespace_id);

  // Constructs a |SessionStorageNamespaceImpl| with id |namespace_id| by
  // cloning |namespace_to_clone|. Allocates it a new ID.
  // Only set |immediately| to true to cause the clone to immediately happen,
  // where there definitely will not be a |Clone| call from the
  // SessionStorageNamespace mojo object.
  static scoped_refptr<SessionStorageNamespaceImpl> CloneFrom(
      scoped_refptr<DOMStorageContextWrapper> context,
      std::string namespace_id,
      const std::string& namespace_id_to_clone,
      bool immediately = false);

  SessionStorageNamespaceImpl(const SessionStorageNamespaceImpl&) = delete;
  SessionStorageNamespaceImpl& operator=(const SessionStorageNamespaceImpl&) =
      delete;

  DOMStorageContextWrapper* context() const { return context_wrapper_.get(); }

  // SessionStorageNamespace implementation.
  const std::string& id() override;
  void SetShouldPersist(bool should_persist) override;
  bool should_persist() override;

  scoped_refptr<SessionStorageNamespaceImpl> Clone();
  bool IsFromContext(DOMStorageContextWrapper* context);

 private:
  // Creates a mojo version.
  SessionStorageNamespaceImpl(scoped_refptr<DOMStorageContextWrapper> context,
                              std::string namespace_id);

  ~SessionStorageNamespaceImpl() override;

  static void DeleteSessionNamespaceFromUIThread(
      scoped_refptr<DOMStorageContextWrapper> context_wrapper,
      std::string namespace_id,
      bool should_persist);

  scoped_refptr<DOMStorageContextWrapper> context_wrapper_;
  std::string namespace_id_;
  bool should_persist_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_IMPL_H_
