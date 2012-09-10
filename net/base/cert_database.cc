// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_database.h"

#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"

#if defined(USE_NSS)
#include "net/base/nss_cert_database.h"
#endif

namespace net {

#if defined(USE_NSS)
// Helper that observes events from the NSSCertDatabase and forwards them to
// the given CertDatabase.
class CertDatabase::Notifier : public NSSCertDatabase::Observer {
 public:
  explicit Notifier(CertDatabase* cert_db) : cert_db_(cert_db) {
    NSSCertDatabase::GetInstance()->AddObserver(this);
  }

  virtual ~Notifier() {
    NSSCertDatabase::GetInstance()->RemoveObserver(this);
  }

  // NSSCertDatabase::Observer implementation:
  virtual void OnCertAdded(const X509Certificate* cert) OVERRIDE {
    cert_db_->NotifyObserversOfCertAdded(cert);
  }

  virtual void OnCertRemoved(const X509Certificate* cert) OVERRIDE {
    cert_db_->NotifyObserversOfCertRemoved(cert);
  }

  virtual void OnCertTrustChanged(const X509Certificate* cert) OVERRIDE {
    cert_db_->NotifyObserversOfCertTrustChanged(cert);
  }

 private:
  CertDatabase* cert_db_;

  DISALLOW_COPY_AND_ASSIGN(Notifier);
};
#endif

// static
CertDatabase* CertDatabase::GetInstance() {
  return Singleton<CertDatabase>::get();
}

CertDatabase::CertDatabase()
    : observer_list_(new ObserverListThreadSafe<Observer>) {
#if defined(USE_NSS)
  // Observe NSSCertDatabase events and forward them to observers of
  // CertDatabase. This also makes sure that NSS has been initialized.
  notifier_.reset(new Notifier(this));
#endif
}

CertDatabase::~CertDatabase() {}

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
