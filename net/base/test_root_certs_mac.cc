// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_root_certs.h"

#include <Security/Security.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "net/base/x509_certificate.h"

namespace net {

namespace {

#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
// Declared in <Security/SecBase.h> of the 10.6 SDK.
enum {
  errSecUnimplemented = -4,
};
#endif

typedef OSStatus (*SecTrustSetAnchorCertificatesOnlyFuncPtr)(SecTrustRef,
                                                             Boolean);

Boolean OurSecCertificateEqual(const void* value1, const void* value2) {
  if (CFGetTypeID(value1) != SecCertificateGetTypeID() ||
      CFGetTypeID(value2) != SecCertificateGetTypeID())
    return CFEqual(value1, value2);
  return X509Certificate::IsSameOSCert(
      reinterpret_cast<SecCertificateRef>(const_cast<void*>(value1)),
      reinterpret_cast<SecCertificateRef>(const_cast<void*>(value2)));
}

const void* RetainWrapper(CFAllocatorRef unused, const void* value) {
  return CFRetain(value);
}

void ReleaseWrapper(CFAllocatorRef unused, const void* value) {
  CFRelease(value);
}

// CFEqual prior to 10.6 only performed pointer checks on SecCertificateRefs,
// rather than checking if they were the same (logical) certificate, so a
// custom structure is used for the array callbacks.
const CFArrayCallBacks kCertArrayCallbacks = {
  0,  // version
  RetainWrapper,
  ReleaseWrapper,
  CFCopyDescription,
  OurSecCertificateEqual,
};

}  // namespace

bool TestRootCerts::Add(X509Certificate* certificate) {
  if (CFArrayContainsValue(temporary_roots_,
                           CFRangeMake(0, CFArrayGetCount(temporary_roots_)),
                           certificate->os_cert_handle()))
    return true;
  CFArrayAppendValue(temporary_roots_, certificate->os_cert_handle());
  return true;
}

void TestRootCerts::Clear() {
  CFArrayRemoveAllValues(temporary_roots_);
}

bool TestRootCerts::IsEmpty() const {
  return CFArrayGetCount(temporary_roots_) == 0;
}

OSStatus TestRootCerts::FixupSecTrustRef(SecTrustRef trust_ref) const {
  if (IsEmpty())
    return noErr;

  CFBundleRef bundle =
      CFBundleGetBundleWithIdentifier(CFSTR("com.apple.security"));
  SecTrustSetAnchorCertificatesOnlyFuncPtr set_anchor_certificates_only = NULL;
  if (bundle) {
    set_anchor_certificates_only =
        reinterpret_cast<SecTrustSetAnchorCertificatesOnlyFuncPtr>(
            CFBundleGetFunctionPointerForName(bundle,
                CFSTR("SecTrustSetAnchorCertificatesOnly")));
  }

  OSStatus status = noErr;
  if (set_anchor_certificates_only) {
    // OS X 10.6 includes a function where the system trusts can be
    // preserved while appending application trusts. This is preferable,
    // because it preserves any user trust settings (explicit distrust),
    // which the naive copy in 10.5 does not. Unfortunately, though the
    // function pointer may be available, it is not always implemented. If it
    // returns errSecUnimplemented, fall through to the 10.5 behaviour.
    status = SecTrustSetAnchorCertificates(trust_ref, temporary_roots_);
    if (status)
      return status;
    status = set_anchor_certificates_only(trust_ref, false);
    if (status != errSecUnimplemented)
      return status;

    // Restore the original settings before falling back.
    status = SecTrustSetAnchorCertificates(trust_ref, NULL);
    if (status)
      return status;
  }

  // On 10.5, the system certificates have to be copied and merged into
  // the application trusts, and may override any user trust settings.
  CFArrayRef system_roots = NULL;
  status = SecTrustCopyAnchorCertificates(&system_roots);
  if (status)
    return status;

  base::mac::ScopedCFTypeRef<CFArrayRef> scoped_system_roots(system_roots);
  base::mac::ScopedCFTypeRef<CFMutableArrayRef> scoped_roots(
      CFArrayCreateMutableCopy(kCFAllocatorDefault, 0,
                               scoped_system_roots));
  DCHECK(scoped_roots.get());

  CFArrayAppendArray(scoped_roots, temporary_roots_,
                     CFRangeMake(0, CFArrayGetCount(temporary_roots_)));
  return SecTrustSetAnchorCertificates(trust_ref, scoped_roots);
}

TestRootCerts::~TestRootCerts() {}

void TestRootCerts::Init() {
  temporary_roots_.reset(CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                              &kCertArrayCallbacks));
}

}  // namespace net
