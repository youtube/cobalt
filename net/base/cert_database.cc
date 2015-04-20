// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_database.h"

#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"

namespace net {

// static
CertDatabase* CertDatabase::GetInstance() {
  return Singleton<CertDatabase>::get();
}

void CertDatabase::AddObserver(Observer* observer) {
  observer_list_->AddObserver(observer);
}

void CertDatabase::RemoveObserver(Observer* observer) {
  observer_list_->RemoveObserver(observer);
}

void CertDatabase::NotifyObserversOfCertAdded(const X509Certificate* cert) {
  observer_list_->Notify(&Observer::OnCertAdded, make_scoped_refptr(cert));
}

void CertDatabase::NotifyObserversOfCertRemoved(const X509Certificate* cert) {
  observer_list_->Notify(&Observer::OnCertRemoved, make_scoped_refptr(cert));
}

void CertDatabase::NotifyObserversOfCertTrustChanged(
    const X509Certificate* cert) {
  observer_list_->Notify(
      &Observer::OnCertTrustChanged, make_scoped_refptr(cert));
}

}  // namespace net
