// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_database.h"

#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "net/base/x509_certificate.h"

namespace net {

CertDatabase::ImportCertFailure::ImportCertFailure(
    X509Certificate* cert, int err)
    : certificate(cert), net_error(err) {
}

CertDatabase::ImportCertFailure::~ImportCertFailure() {
}

// CertDatabaseNotifier notifies registered observers when new user certificates
// are added to the database.
class CertDatabaseNotifier {
 public:
  CertDatabaseNotifier()
      : observer_list_(new ObserverListThreadSafe<CertDatabase::Observer>) {
  }

  static CertDatabaseNotifier* GetInstance() {
    return Singleton<CertDatabaseNotifier>::get();
  }

  friend struct DefaultSingletonTraits<CertDatabaseNotifier>;
  friend class CertDatabase;

 private:
  const scoped_refptr<ObserverListThreadSafe<CertDatabase::Observer> >
      observer_list_;
};

void CertDatabase::AddObserver(Observer* observer) {
  CertDatabaseNotifier::GetInstance()->observer_list_->AddObserver(observer);
}

void CertDatabase::RemoveObserver(Observer* observer) {
  CertDatabaseNotifier::GetInstance()->observer_list_->RemoveObserver(observer);
}

void CertDatabase::NotifyObserversOfUserCertAdded(const X509Certificate* cert) {
  CertDatabaseNotifier::GetInstance()->observer_list_->Notify(
      &CertDatabase::Observer::OnUserCertAdded, make_scoped_refptr(cert));
}

void CertDatabase::NotifyObserversOfCertTrustChanged(
    const X509Certificate* cert) {
  CertDatabaseNotifier::GetInstance()->observer_list_->Notify(
      &CertDatabase::Observer::OnCertTrustChanged, make_scoped_refptr(cert));
}

}  // namespace net
