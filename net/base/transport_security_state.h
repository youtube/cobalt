// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TRANSPORT_SECURITY_STATE_H_
#define NET_BASE_TRANSPORT_SECURITY_STATE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "net/base/net_export.h"
#include "net/base/x509_cert_types.h"

namespace net {

// TransportSecurityState
//
// Tracks which hosts have enabled *-Transport-Security. This object manages
// the in-memory store. A separate object must register itself with this object
// in order to persist the state to disk.
class NET_EXPORT TransportSecurityState
    : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // If non-empty, |hsts_hosts| is a JSON-formatted string to treat as if it
  // were a built-in entry (same format as persisted metadata in the
  // TransportSecurityState file).
  explicit TransportSecurityState(const std::string& hsts_hosts);
  ~TransportSecurityState();

  // A DomainState is the information that we persist about a given domain.
  struct NET_EXPORT DomainState {
    enum Mode {
      // Strict mode implies:
      //   * We generate internal redirects from HTTP -> HTTPS.
      //   * Certificate issues are fatal.
      MODE_STRICT = 0,
      // This used to be opportunistic HTTPS, but we removed support.
      MODE_OPPORTUNISTIC_REMOVED = 1,
      // SPDY_ONLY (aka X-Bodge-Transport-Security) is a hopefully temporary
      // measure. It implies:
      //   * We'll request HTTP URLs over HTTPS iff we have SPDY support.
      //   * Certificate issues are fatal.
      MODE_SPDY_ONLY = 2,
      // Pinning means there are no HTTP -> HTTPS redirects, however certificate
      // issues are still fatal and there may be public key pins.
      MODE_PINNING_ONLY = 3,
    };

    DomainState();
    ~DomainState();

    // IsChainOfPublicKeysPermitted takes a set of public key hashes and
    // returns true if:
    //   1) None of the hashes are in |bad_public_key_hashes| AND
    //   2) |public_key_hashes| is empty, i.e. no public keys have been pinned.
    //      OR
    //   3) |hashes| and |public_key_hashes| are not disjoint.
    //
    // |public_key_hashes| is intended to contain a number of trusted public
    // keys for the chain in question, any one of which is sufficient. The
    // public keys could be of a root CA, intermediate CA or leaf certificate,
    // depending on the security vs disaster recovery tradeoff selected.
    // (Pinning only to leaf certifiates increases security because you no
    // longer trust any CAs, but it hampers disaster recovery because you can't
    // just get a new certificate signed by the CA.)
    //
    // |bad_public_key_hashes| is intended to contain unwanted intermediate CA
    // certifciates that those trusted public keys may have issued but that we
    // don't want to trust.
    bool IsChainOfPublicKeysPermitted(
        const std::vector<SHA1Fingerprint>& hashes);

    // ShouldCertificateErrorsBeFatal returns true iff, given the |mode| of this
    // DomainState, certificate errors on this domain should be fatal (i.e. no
    // user bypass).
    bool ShouldCertificateErrorsBeFatal() const;

    // ShouldRedirectHTTPToHTTPS returns true iff, given the |mode| of this
    // DomainState, HTTP requests should be internally redirected to HTTPS.
    bool ShouldRedirectHTTPToHTTPS() const;

    // ShouldMixedScriptingBeBlocked returns true iff, given the |mode| of this
    // DomainState, mixed scripting (the loading of Javascript, CSS or plugins
    // over HTTP for an HTTPS page) should be blocked.
    bool ShouldMixedScriptingBeBlocked() const;

    Mode mode;
    base::Time created;  // when this host entry was first created
    base::Time expiry;  // the absolute time (UTC) when this record expires
    bool include_subdomains;  // subdomains included?
    std::vector<SHA1Fingerprint> public_key_hashes;  // optional; permitted keys
    std::vector<SHA1Fingerprint> bad_public_key_hashes; // optional;rejectd keys

    // The follow members are not valid when stored in |enabled_hosts_|.
    bool preloaded;  // is this a preloaded entry?
    std::string domain;  // the domain which matched
  };

  class Delegate {
   public:
    // This function may not block and may be called with internal locks held.
    // Thus it must not reenter the TransportSecurityState object.
    virtual void StateIsDirty(TransportSecurityState* state) = 0;

   protected:
    virtual ~Delegate() {}
  };

  void SetDelegate(Delegate*);

  // Enable TransportSecurity for |host|.
  void EnableHost(const std::string& host, const DomainState& state);

  // Delete any entry for |host|. If |host| doesn't have an exact entry then no
  // action is taken. Returns true iff an entry was deleted.
  bool DeleteHost(const std::string& host);

  // Returns true if |host| has TransportSecurity enabled, in the context of
  // |sni_available|. You should check |result->mode| before acting on this
  // because the modes can be quite different.
  //
  // Note that *result is always overwritten on every call.
  bool GetDomainState(DomainState* result,
                      const std::string& host,
                      bool sni_available);

  // Returns true if |host| has any SSL certificate pinning, in the context of
  // |sni_available|. In that case, *result is filled out.
  // Note that *result is always overwritten on every call.
  bool HasPinsForHost(DomainState* result,
                      const std::string& host,
                      bool sni_available);

  // Returns true if |host| has any HSTS metadata, in the context of
  // |sni_available|. (This include cert-pin-only metadata).
  // In that case, *result is filled out.
  // Note that *result is always overwritten on every call.
  bool HasMetadata(DomainState* result,
                   const std::string& host,
                   bool sni_available);

  // Returns true if we have a preloaded certificate pin for the |host| and if
  // its set of required certificates is the set we expect for Google
  // properties. If |sni_available| is true, searches the preloads defined for
  // SNI-using hosts as well as the usual preload list.
  //
  // Note that like HasMetadata, if |host| matches both an exact entry and is a
  // subdomain of another entry, the exact match determines the return value.
  //
  // This function is used by ChromeFraudulentCertificateReporter to determine
  // whether or not we can automatically post fraudulent certificate reports to
  // Google; we only do so automatically in cases when the user was trying to
  // connect to Google in the first place.
  static bool IsGooglePinnedProperty(const std::string& host,
                                     bool sni_available);

  // Reports UMA statistics upon public key pin failure. Reports only down to
  // the second-level domain of |host| (e.g. google.com if |host| is
  // mail.google.com), and only if |host| is a preloaded STS host.
  static void ReportUMAOnPinFailure(const std::string& host);

  // Deletes all records created since a given time.
  void DeleteSince(const base::Time& time);

  // Returns |true| if |value| parses as a valid *-Transport-Security
  // header value.  The values of max-age and and includeSubDomains are
  // returned in |max_age| and |include_subdomains|, respectively.  The out
  // parameters are not modified if the function returns |false|.
  static bool ParseHeader(const std::string& value,
                          int* max_age,
                          bool* include_subdomains);

  // ParseSidePin attempts to parse a side pin from |side_info| which signs the
  // SubjectPublicKeyInfo in |leaf_spki|. A side pin is a way for a site to
  // sign their public key with a key that is offline but still controlled by
  // them. If successful, the hash of the public key used to sign |leaf_spki|
  // is put into |out_pub_key_hash|.
  static bool ParseSidePin(const base::StringPiece& leaf_spki,
                           const base::StringPiece& side_info,
                           std::vector<SHA1Fingerprint> *out_pub_key_hash);

  bool Serialise(std::string* output);
  // Existing non-preloaded entries are cleared and repopulated from the
  // passed JSON string.
  bool LoadEntries(const std::string& state, bool* dirty);

  // The maximum number of seconds for which we'll cache an HSTS request.
  static const long int kMaxHSTSAgeSecs;

 private:
  FRIEND_TEST_ALL_PREFIXES(TransportSecurityStateTest, IsPreloaded);

  // If we have a callback configured, call it to let our serialiser know that
  // our state is dirty.
  void DirtyNotify();
  bool IsPreloadedSTS(const std::string& canonicalized_host,
                      bool sni_available,
                      DomainState* out);

  static std::string CanonicalizeHost(const std::string& host);
  static bool Deserialise(const std::string& state,
                          bool* dirty,
                          std::map<std::string, DomainState>* out);

  // The set of hosts that have enabled TransportSecurity. The keys here
  // are SHA256(DNSForm(domain)) where DNSForm converts from dotted form
  // ('www.google.com') to the form used in DNS: "\x03www\x06google\x03com"
  std::map<std::string, DomainState> enabled_hosts_;

  // These hosts are extra rules to treat as built-in, passed in the
  // constructor (typically originating from the command line).
  std::map<std::string, DomainState> forced_hosts_;

  // Our delegate who gets notified when we are dirtied, or NULL.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TransportSecurityState);
};

}  // namespace net

#endif  // NET_BASE_TRANSPORT_SECURITY_STATE_H_
