// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/default_server_bound_cert_store.h"

#include "base/bind.h"
#include "base/message_loop.h"

namespace net {

// static
const size_t DefaultServerBoundCertStore::kMaxCerts = 3300;

DefaultServerBoundCertStore::DefaultServerBoundCertStore(
    PersistentStore* store)
    : initialized_(false),
      store_(store) {}

void DefaultServerBoundCertStore::FlushStore(
    const base::Closure& completion_task) {
  base::AutoLock autolock(lock_);

  if (initialized_ && store_)
    store_->Flush(completion_task);
  else if (!completion_task.is_null())
    MessageLoop::current()->PostTask(FROM_HERE, completion_task);
}

bool DefaultServerBoundCertStore::GetServerBoundCert(
    const std::string& server_identifier,
    SSLClientCertType* type,
    base::Time* creation_time,
    base::Time* expiration_time,
    std::string* private_key_result,
    std::string* cert_result) {
  base::AutoLock autolock(lock_);
  InitIfNecessary();

  ServerBoundCertMap::iterator it = server_bound_certs_.find(server_identifier);

  if (it == server_bound_certs_.end())
    return false;

  ServerBoundCert* cert = it->second;
  *type = cert->type();
  *creation_time = cert->creation_time();
  *expiration_time = cert->expiration_time();
  *private_key_result = cert->private_key();
  *cert_result = cert->cert();

  return true;
}

void DefaultServerBoundCertStore::SetServerBoundCert(
    const std::string& server_identifier,
    SSLClientCertType type,
    base::Time creation_time,
    base::Time expiration_time,
    const std::string& private_key,
    const std::string& cert) {
  base::AutoLock autolock(lock_);
  InitIfNecessary();

  InternalDeleteServerBoundCert(server_identifier);
  InternalInsertServerBoundCert(
      server_identifier,
      new ServerBoundCert(
          server_identifier, type, creation_time, expiration_time, private_key,
          cert));
}

void DefaultServerBoundCertStore::DeleteServerBoundCert(
    const std::string& server_identifier) {
  base::AutoLock autolock(lock_);
  InitIfNecessary();
  InternalDeleteServerBoundCert(server_identifier);
}

void DefaultServerBoundCertStore::DeleteAllCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end) {
  base::AutoLock autolock(lock_);
  InitIfNecessary();
  for (ServerBoundCertMap::iterator it = server_bound_certs_.begin();
       it != server_bound_certs_.end();) {
    ServerBoundCertMap::iterator cur = it;
    ++it;
    ServerBoundCert* cert = cur->second;
    if ((delete_begin.is_null() || cert->creation_time() >= delete_begin) &&
        (delete_end.is_null() || cert->creation_time() < delete_end)) {
      if (store_)
        store_->DeleteServerBoundCert(*cert);
      delete cert;
      server_bound_certs_.erase(cur);
    }
  }
}

void DefaultServerBoundCertStore::DeleteAll() {
  DeleteAllCreatedBetween(base::Time(), base::Time());
}

void DefaultServerBoundCertStore::GetAllServerBoundCerts(
    ServerBoundCertList* server_bound_certs) {
  base::AutoLock autolock(lock_);
  InitIfNecessary();
  for (ServerBoundCertMap::iterator it = server_bound_certs_.begin();
       it != server_bound_certs_.end(); ++it) {
    server_bound_certs->push_back(*it->second);
  }
}

int DefaultServerBoundCertStore::GetCertCount() {
  base::AutoLock autolock(lock_);
  InitIfNecessary();

  return server_bound_certs_.size();
}

void DefaultServerBoundCertStore::SetForceKeepSessionState() {
  base::AutoLock autolock(lock_);
  InitIfNecessary();

  if (store_)
    store_->SetForceKeepSessionState();
}

DefaultServerBoundCertStore::~DefaultServerBoundCertStore() {
  DeleteAllInMemory();
}

void DefaultServerBoundCertStore::DeleteAllInMemory() {
  base::AutoLock autolock(lock_);

  for (ServerBoundCertMap::iterator it = server_bound_certs_.begin();
       it != server_bound_certs_.end(); ++it) {
    delete it->second;
  }
  server_bound_certs_.clear();
}

void DefaultServerBoundCertStore::InitStore() {
  lock_.AssertAcquired();

  DCHECK(store_) << "Store must exist to initialize";

  // Initialize the store and sync in any saved persistent certs.
  std::vector<ServerBoundCert*> certs;
  // Reserve space for the maximum amount of certs a database should have.
  // This prevents multiple vector growth / copies as we append certs.
  certs.reserve(kMaxCerts);
  store_->Load(&certs);

  for (std::vector<ServerBoundCert*>::const_iterator it = certs.begin();
       it != certs.end(); ++it) {
    server_bound_certs_[(*it)->server_identifier()] = *it;
  }
}

void DefaultServerBoundCertStore::InternalDeleteServerBoundCert(
    const std::string& server_identifier) {
  lock_.AssertAcquired();

  ServerBoundCertMap::iterator it = server_bound_certs_.find(server_identifier);
  if (it == server_bound_certs_.end())
    return;  // There is nothing to delete.

  ServerBoundCert* cert = it->second;
  if (store_)
    store_->DeleteServerBoundCert(*cert);
  server_bound_certs_.erase(it);
  delete cert;
}

void DefaultServerBoundCertStore::InternalInsertServerBoundCert(
    const std::string& server_identifier,
    ServerBoundCert* cert) {
  lock_.AssertAcquired();

  if (store_)
    store_->AddServerBoundCert(*cert);
  server_bound_certs_[server_identifier] = cert;
}

DefaultServerBoundCertStore::PersistentStore::PersistentStore() {}

DefaultServerBoundCertStore::PersistentStore::~PersistentStore() {}

}  // namespace net
