// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_TEST_TEST_CREDENTIAL_PROVIDER_H_
#define CHROME_CREDENTIAL_PROVIDER_TEST_TEST_CREDENTIAL_PROVIDER_H_

#include <unknwn.h>

#include "base/win/scoped_bstr.h"

namespace credential_provider {

namespace testing {

class DECLSPEC_UUID("630c4843-933b-45d7-83fd-1c28d5849e5b")
    ITestCredentialProvider : public IUnknown {
 public:
  virtual const base::win::ScopedBstr& STDMETHODCALLTYPE username() const = 0;
  virtual const base::win::ScopedBstr& STDMETHODCALLTYPE password() const = 0;
  virtual const base::win::ScopedBstr& STDMETHODCALLTYPE sid() const = 0;
  virtual bool STDMETHODCALLTYPE credentials_changed_fired() const = 0;
  virtual void STDMETHODCALLTYPE ResetCredentialsChangedFired() = 0;
};

}  // namespace testing
}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_TEST_TEST_CREDENTIAL_PROVIDER_H_
