// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ev_root_ca_metadata.h"

#include "net/base/cert_test_util.h"
#include "net/base/x509_cert_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

static const char kVerisignPolicy[] = "2.16.840.1.113733.1.7.23.6";
static const char kThawtePolicy[] = "2.16.840.1.113733.1.7.48.1";
static const char kFakePolicy[] = "2.16.840.1.42";
static const SHA1Fingerprint kVerisignFingerprint =
    { { 0x74, 0x2c, 0x31, 0x92, 0xe6, 0x07, 0xe4, 0x24, 0xeb, 0x45,
        0x49, 0x54, 0x2b, 0xe1, 0xbb, 0xc5, 0x3e, 0x61, 0x74, 0xe2 } };
static const SHA1Fingerprint kFakeFingerprint =
    { { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 } };

#if defined(USE_NSS)

TEST(EVRootCAMetadataTest, Basic) {
  EVRootCAMetadata* ev_metadata(EVRootCAMetadata::GetInstance());
  std::vector<EVRootCAMetadata::PolicyOID> oids;

  EXPECT_TRUE(ev_metadata->GetPolicyOIDsForCA(kVerisignFingerprint, &oids));
  EXPECT_LT(0u, oids.size());
  oids.clear();

  EXPECT_FALSE(ev_metadata->GetPolicyOIDsForCA(kFakeFingerprint, &oids));
  EXPECT_EQ(0u, oids.size());
}

TEST(EVRootCAMetadataTest, AddRemove) {
  EVRootCAMetadata* ev_metadata(EVRootCAMetadata::GetInstance());
  std::vector<EVRootCAMetadata::PolicyOID> oids;

  EXPECT_FALSE(ev_metadata->GetPolicyOIDsForCA(kFakeFingerprint, &oids));

  {
    ScopedTestEVPolicy test_ev_policy(ev_metadata, kFakeFingerprint,
                                      kFakePolicy);

    EXPECT_TRUE(ev_metadata->GetPolicyOIDsForCA(kFakeFingerprint, &oids));
    EXPECT_EQ(1u, oids.size());
  }

  EXPECT_FALSE(ev_metadata->GetPolicyOIDsForCA(kFakeFingerprint, &oids));
}

#elif defined(OS_WIN)

TEST(EVRootCAMetadataTest, Basic) {
  EVRootCAMetadata* ev_metadata(EVRootCAMetadata::GetInstance());

  EXPECT_TRUE(ev_metadata->IsEVPolicyOID(kVerisignPolicy));
  EXPECT_FALSE(ev_metadata->IsEVPolicyOID(kFakePolicy));
  EXPECT_TRUE(ev_metadata->HasEVPolicyOID(kVerisignFingerprint,
                                          kVerisignPolicy));
  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kFakeFingerprint,
                                           kVerisignPolicy));
  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kVerisignFingerprint,
                                           kFakePolicy));
  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kVerisignFingerprint,
                                           kThawtePolicy));
}

TEST(EVRootCAMetadataTest, AddRemove) {
  EVRootCAMetadata* ev_metadata(EVRootCAMetadata::GetInstance());

  EXPECT_FALSE(ev_metadata->IsEVPolicyOID(kFakePolicy));
  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kFakeFingerprint,
                                           kFakePolicy));

  {
    ScopedTestEVPolicy test_ev_policy(ev_metadata, kFakeFingerprint,
                                      kFakePolicy);

    EXPECT_TRUE(ev_metadata->IsEVPolicyOID(kFakePolicy));
    EXPECT_TRUE(ev_metadata->HasEVPolicyOID(kFakeFingerprint,
                                            kFakePolicy));
  }

  EXPECT_FALSE(ev_metadata->IsEVPolicyOID(kFakePolicy));
  EXPECT_FALSE(ev_metadata->HasEVPolicyOID(kFakeFingerprint,
                                           kFakePolicy));
}

#endif // defined(OS_WIN)

}  // namespace net
