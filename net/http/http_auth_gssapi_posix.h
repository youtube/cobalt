// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_GSSAPI_POSIX_H_
#define NET_HTTP_HTTP_AUTH_GSSAPI_POSIX_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/native_library.h"
#include "net/http/http_auth.h"

#define GSS_USE_FUNCTION_POINTERS
#include "net/third_party/gssapi/gssapi.h"

class GURL;

namespace net {

class HttpRequestInfo;
class ProxyInfo;

// GSSAPILibrary is introduced so unit tests can mock the calls to the GSSAPI
// library. The default implementation attempts to load one of the standard
// GSSAPI library implementations, then simply passes the arguments on to
// that implementation.
class GSSAPILibrary {
 public:
  virtual ~GSSAPILibrary() {}

  // Initializes the library, including any necessary dynamic libraries.
  virtual bool Init() = 0;

  // These methods match the ones in the GSSAPI library.
  virtual gssapi::OM_uint32 import_name(
      gssapi::OM_uint32* minor_status,
      const gssapi::gss_buffer_t input_name_buffer,
      const gssapi::gss_OID input_name_type,
      gssapi::gss_name_t* output_name) = 0;
  virtual gssapi::OM_uint32 release_name(
      gssapi::OM_uint32* minor_status,
      gssapi::gss_name_t* input_name) = 0;
  virtual gssapi::OM_uint32 release_buffer(
      gssapi::OM_uint32* minor_status,
      gssapi::gss_buffer_t buffer) = 0;
  virtual gssapi::OM_uint32 display_status(
      gssapi::OM_uint32* minor_status,
      gssapi::OM_uint32 status_value,
      int status_type,
      const gssapi::gss_OID mech_type,
      gssapi::OM_uint32* message_contex,
      gssapi::gss_buffer_t status_string) = 0;
  virtual gssapi::OM_uint32 init_sec_context(
      gssapi::OM_uint32* minor_status,
      const gssapi::gss_cred_id_t initiator_cred_handle,
      gssapi::gss_ctx_id_t* context_handle,
      const gssapi::gss_name_t target_name,
      const gssapi::gss_OID mech_type,
      gssapi::OM_uint32 req_flags,
      gssapi::OM_uint32 time_req,
      const gssapi::gss_channel_bindings_t input_chan_bindings,
      const gssapi::gss_buffer_t input_token,
      gssapi::gss_OID* actual_mech_type,
      gssapi::gss_buffer_t output_token,
      gssapi::OM_uint32* ret_flags,
      gssapi::OM_uint32* time_rec) = 0;
  virtual gssapi::OM_uint32 wrap_size_limit(
      gssapi::OM_uint32* minor_status,
      const gssapi::gss_ctx_id_t context_handle,
      int conf_req_flag,
      gssapi::gss_qop_t qop_req,
      gssapi::OM_uint32 req_output_size,
      gssapi::OM_uint32* max_input_size) = 0;

  // Get the default GSSPILibrary instance. The object returned is a singleton
  // instance, and the caller should not delete it.
  static GSSAPILibrary* GetDefault();
};

// GSSAPISharedLibrary class is defined here so that unit tests can access it.
class GSSAPISharedLibrary : public GSSAPILibrary {
 public:
  GSSAPISharedLibrary();
  virtual ~GSSAPISharedLibrary();

  // GSSAPILibrary methods:
  virtual bool Init();
  virtual gssapi::OM_uint32 import_name(
      gssapi::OM_uint32* minor_status,
      const gssapi::gss_buffer_t input_name_buffer,
      const gssapi::gss_OID input_name_type,
      gssapi::gss_name_t* output_name);
  virtual gssapi::OM_uint32 release_name(
      gssapi::OM_uint32* minor_status,
      gssapi::gss_name_t* input_name);
  virtual gssapi::OM_uint32 release_buffer(
      gssapi::OM_uint32* minor_status,
      gssapi::gss_buffer_t buffer);
  virtual gssapi::OM_uint32 display_status(
      gssapi::OM_uint32* minor_status,
      gssapi::OM_uint32 status_value,
      int status_type,
      const gssapi::gss_OID mech_type,
      gssapi::OM_uint32* message_contex,
      gssapi::gss_buffer_t status_string);
  virtual gssapi::OM_uint32 init_sec_context(
      gssapi::OM_uint32* minor_status,
      const gssapi::gss_cred_id_t initiator_cred_handle,
      gssapi::gss_ctx_id_t* context_handle,
      const gssapi::gss_name_t target_name,
      const gssapi::gss_OID mech_type,
      gssapi::OM_uint32 req_flags,
      gssapi::OM_uint32 time_req,
      const gssapi::gss_channel_bindings_t input_chan_bindings,
      const gssapi::gss_buffer_t input_token,
      gssapi::gss_OID* actual_mech_type,
      gssapi::gss_buffer_t output_token,
      gssapi::OM_uint32* ret_flags,
      gssapi::OM_uint32* time_rec);
  virtual gssapi::OM_uint32 wrap_size_limit(
      gssapi::OM_uint32* minor_status,
      const gssapi::gss_ctx_id_t context_handle,
      int conf_req_flag,
      gssapi::gss_qop_t qop_req,
      gssapi::OM_uint32 req_output_size,
      gssapi::OM_uint32* max_input_size);

 private:
  FRIEND_TEST_ALL_PREFIXES(HttpAuthGSSAPIPOSIXTest, GSSAPIStartup);

  bool InitImpl();
  static base::NativeLibrary LoadSharedObject();
  bool BindMethods();

  bool initialized_;

  // Need some way to invalidate the library.
  base::NativeLibrary gssapi_library_;

  // Function pointers
  gssapi::gss_import_name_type import_name_;
  gssapi::gss_release_name_type release_name_;
  gssapi::gss_release_buffer_type release_buffer_;
  gssapi::gss_display_status_type display_status_;
  gssapi::gss_init_sec_context_type init_sec_context_;
  gssapi::gss_wrap_size_limit_type wrap_size_limit_;
};

// TODO(cbentzel): Share code with HttpAuthSSPI.
class HttpAuthGSSAPI {
 public:
  HttpAuthGSSAPI(GSSAPILibrary* library,
                 const std::string& scheme,
                 const gssapi::gss_OID gss_oid);
  ~HttpAuthGSSAPI();

  bool NeedsIdentity() const;
  bool IsFinalRound() const;

  bool ParseChallenge(HttpAuth::ChallengeTokenizer* tok);

  // Generates an authentication token.
  // The return value is an error code. If it's not |OK|, the value of
  // |*auth_token| is unspecified.
  // |spn| is the Service Principal Name of the server that the token is
  // being generated for.
  // If this is the first round of a multiple round scheme, credentials are
  // obtained using |*username| and |*password|. If |username| and |password|
  // are NULL, the default credentials are used instead.
  int GenerateAuthToken(const std::wstring* username,
                        const std::wstring* password,
                        const std::wstring& spn,
                        const HttpRequestInfo* request,
                        const ProxyInfo* proxy,
                        std::string* out_credentials);

 private:
  int OnFirstRound(const std::wstring* username,
                   const std::wstring* password);
  int GetNextSecurityToken(const std::wstring& spn,
                           gssapi::gss_buffer_t in_token,
                           gssapi::gss_buffer_t out_token);

  std::string scheme_;
  std::wstring username_;
  std::wstring password_;
  gssapi::gss_OID gss_oid_;
  GSSAPILibrary* library_;
  std::string decoded_server_auth_token_;
  gssapi::gss_ctx_id_t sec_context_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_GSSAPI_POSIX_H_
