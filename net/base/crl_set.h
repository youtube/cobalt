// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CRL_SET_H_
#define NET_BASE_CRL_SET_H_
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/time.h"
#include "net/base/net_export.h"

namespace net {

// A CRLSet is a structure that lists the serial numbers of revoked
// certificates from a number of issuers where issuers are identified by the
// SHA256 of their SubjectPublicKeyInfo.
class NET_EXPORT CRLSet : public base::RefCountedThreadSafe<CRLSet> {
 public:
  enum Result {
    REVOKED,  // the certificate should be rejected.
    UNKNOWN,  // there was an error in processing.
    GOOD,  // the certificate is not listed.
  };

  ~CRLSet();

  // Parse parses the bytes in |data| and, on success, puts a new CRLSet in
  // |out_crl_set| and returns true.
  static bool Parse(base::StringPiece data,
                    scoped_refptr<CRLSet>* out_crl_set);

  // CheckCertificate returns the information contained in the set for a given
  // certificate:
  //   serial_number: the serial number of the certificate
  //   issuer_spki_hash: the SHA256 of the SubjectPublicKeyInfo of the CRL
  //       signer
  //
  // This does not check that the CRLSet is timely. See |next_update|.
  Result CheckCertificate(
      const base::StringPiece& serial_number,
      const base::StringPiece& issuer_spki_hash) const;

  // ApplyDelta returns a new CRLSet in |out_crl_set| that is the result of
  // updating the current CRL set with the delta information in |delta_bytes|.
  bool ApplyDelta(base::StringPiece delta_bytes,
                  scoped_refptr<CRLSet>* out_crl_set);

  // next_update returns the time at which a new CRLSet may be availible.
  base::Time next_update() const;

  // update_window returns the length of the update window. Once the
  // |next_update| time has occured, the client should schedule a fetch,
  // uniformly at random, within |update_window|. This aims to smooth the load
  // on the server.
  base::TimeDelta update_window() const;

  // sequence returns the sequence number of this CRL set. CRL sets generated
  // by the same source are given strictly monotonically increasing sequence
  // numbers.
  uint32 sequence() const;

  // CRLList contains a list of (issuer SPKI hash, revoked serial numbers)
  // pairs.
  typedef std::vector< std::pair<std::string, std::vector<std::string> > >
      CRLList;

  // crls returns the internal state of this CRLSet. It should only be used in
  // testing.
  const CRLList& crls() const;

 private:
  CRLSet();

  static CRLSet* CRLSetFromHeader(base::StringPiece header);

  base::Time next_update_;
  base::TimeDelta update_window_;
  uint32 sequence_;

  CRLList crls_;
  // crls_index_by_issuer_ maps from issuer SPKI hashes to the index in |crls_|
  // where the information for that issuer can be found. We have both |crls_|
  // and |crls_index_by_issuer_| because, when applying a delta update, we need
  // to identify a CRL by index.
  std::map<std::string, size_t> crls_index_by_issuer_;
};

}  // namespace net

#endif  // NET_BASE_CRL_SET_H_
