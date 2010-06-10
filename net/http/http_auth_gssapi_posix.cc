// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_gssapi_posix.h"

#include <limits>

#include "base/base64.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

namespace net {

gss_OID_desc CHROME_GSS_C_NT_HOSTBASED_SERVICE_X_VAL = {
  6,
  const_cast<char *>("\x2b\x06\x01\x05\x06\x02")
};
gss_OID_desc CHROME_GSS_C_NT_HOSTBASED_SERVICE_VAL = {
  10,
  const_cast<char *>("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04")
};

gss_OID CHROME_GSS_C_NT_HOSTBASED_SERVICE_X =
    &CHROME_GSS_C_NT_HOSTBASED_SERVICE_X_VAL;
gss_OID CHROME_GSS_C_NT_HOSTBASED_SERVICE =
    &CHROME_GSS_C_NT_HOSTBASED_SERVICE_VAL;

GSSAPISharedLibrary::GSSAPISharedLibrary()
    : initialized_(false),
      gssapi_library_(NULL),
      import_name_(NULL),
      release_name_(NULL),
      release_buffer_(NULL),
      display_status_(NULL),
      init_sec_context_(NULL),
      wrap_size_limit_(NULL) {
}

GSSAPISharedLibrary::~GSSAPISharedLibrary() {
  if (gssapi_library_) {
    base::UnloadNativeLibrary(gssapi_library_);
    gssapi_library_ = NULL;
  }
}

bool GSSAPISharedLibrary::Init() {
  if (!initialized_)
    InitImpl();
  return initialized_;
}

bool GSSAPISharedLibrary::InitImpl() {
  DCHECK(!initialized_);
  gssapi_library_ = LoadSharedObject();
  if (gssapi_library_ == NULL)
    return false;
  if (!BindMethods())
    return false;
  initialized_ = true;
  return true;
}

base::NativeLibrary GSSAPISharedLibrary::LoadSharedObject() {
  static const char* kLibraryNames[] = {
#if defined(OS_MACOSX)
    "libgssapi_krb5.dylib"  // MIT Kerberos
#else
    "libgssapi_krb5.so.2",  // MIT Kerberos
    "libgssapi.so.4",       // Heimdal
    "libgssapi.so.1"        // Heimdal
#endif
  };
  static size_t num_lib_names = arraysize(kLibraryNames);

  for (size_t i = 0; i < num_lib_names; ++i) {
    const char* library_name = kLibraryNames[i];
    FilePath file_path(library_name);
    base::NativeLibrary lib = base::LoadNativeLibrary(file_path);
    if (lib)
      return lib;
  }
  return NULL;
}

template <typename T>
bool FindAndBind(base::NativeLibrary library, const char* name, T* t) {
  void* func = base::GetFunctionPointerFromNativeLibrary(library, name);
  if (func == NULL)
    return false;
  *t = reinterpret_cast<T>(func);
  return true;
}

bool GSSAPISharedLibrary::BindMethods() {
  DCHECK(gssapi_library_ != NULL);
  if (!FindAndBind(gssapi_library_, "gss_import_name", &import_name_))
    return false;
  if (!FindAndBind(gssapi_library_, "gss_release_name", &release_name_))
    return false;
  if (!FindAndBind(gssapi_library_, "gss_release_buffer", &release_buffer_))
    return false;
  if (!FindAndBind(gssapi_library_, "gss_display_status", &display_status_))
    return false;
  if (!FindAndBind(gssapi_library_, "gss_init_sec_context", &init_sec_context_))
    return false;
  if (!FindAndBind(gssapi_library_, "gss_wrap_size_limit", &wrap_size_limit_))
    return false;
  return true;
}

OM_uint32 GSSAPISharedLibrary::import_name(
    OM_uint32* minor_status,
    const gss_buffer_t input_name_buffer,
    const gss_OID input_name_type,
    gss_name_t* output_name) {
  DCHECK(initialized_);
  return import_name_(minor_status, input_name_buffer, input_name_type,
                      output_name);
}

OM_uint32 GSSAPISharedLibrary::release_name(
    OM_uint32* minor_status,
    gss_name_t* input_name) {
  DCHECK(initialized_);
  return release_name_(minor_status, input_name);
}

OM_uint32 GSSAPISharedLibrary::release_buffer(
    OM_uint32* minor_status,
    gss_buffer_t buffer) {
  DCHECK(initialized_);
  return release_buffer_(minor_status, buffer);
}

OM_uint32 GSSAPISharedLibrary::display_status(
    OM_uint32* minor_status,
    OM_uint32 status_value,
    int status_type,
    const gss_OID mech_type,
    OM_uint32* message_context,
    gss_buffer_t status_string) {
  DCHECK(initialized_);
  return display_status_(minor_status, status_value, status_type, mech_type,
                         message_context, status_string);
}

OM_uint32 GSSAPISharedLibrary::init_sec_context(
    OM_uint32* minor_status,
    const gss_cred_id_t initiator_cred_handle,
    gss_ctx_id_t* context_handle,
    const gss_name_t target_name,
    const gss_OID mech_type,
    OM_uint32 req_flags,
    OM_uint32 time_req,
    const gss_channel_bindings_t input_chan_bindings,
    const gss_buffer_t input_token,
    gss_OID* actual_mech_type,
    gss_buffer_t output_token,
    OM_uint32* ret_flags,
    OM_uint32* time_rec) {
  DCHECK(initialized_);
  return init_sec_context_(minor_status,
                           initiator_cred_handle,
                           context_handle,
                           target_name,
                           mech_type,
                           req_flags,
                           time_req,
                           input_chan_bindings,
                           input_token,
                           actual_mech_type,
                           output_token,
                           ret_flags,
                           time_rec);
}

OM_uint32 GSSAPISharedLibrary::wrap_size_limit(
    OM_uint32* minor_status,
    const gss_ctx_id_t context_handle,
    int conf_req_flag,
    gss_qop_t qop_req,
    OM_uint32 req_output_size,
    OM_uint32* max_input_size) {
  DCHECK(initialized_);
  return wrap_size_limit_(minor_status,
                          context_handle,
                          conf_req_flag,
                          qop_req,
                          req_output_size,
                          max_input_size);
}

GSSAPILibrary* GSSAPILibrary::GetDefault() {
  return Singleton<GSSAPISharedLibrary>::get();
}

namespace {

std::string DisplayStatus(OM_uint32 major_status,
                          OM_uint32 minor_status) {
  if (major_status == GSS_S_COMPLETE)
    return "OK";
  return StringPrintf("0x%08x 0x%08x", major_status, minor_status);
}

std::string DisplayCode(GSSAPILibrary* gssapi_lib,
                        OM_uint32 status,
                        OM_uint32 status_code_type) {
  const int kMaxDisplayIterations = 8;
  const size_t kMaxMsgLength = 4096;
  // msg_ctx needs to be outside the loop because it is invoked multiple times.
  OM_uint32 msg_ctx = 0;
  std::string rv = StringPrintf("(0x%08X)", status);

  // This loop should continue iterating until msg_ctx is 0 after the first
  // iteration. To be cautious and prevent an infinite loop, it stops after
  // a finite number of iterations as well. As an added sanity check, no
  // individual message may exceed |kMaxMsgLength|, and the final result
  // will not exceed |kMaxMsgLength|*2-1.
  for (int i = 0; i < kMaxDisplayIterations && rv.size() < kMaxMsgLength;
      ++i) {
    OM_uint32 min_stat;
    gss_buffer_desc_struct msg = GSS_C_EMPTY_BUFFER;
    OM_uint32 maj_stat =
        gssapi_lib->display_status(&min_stat, status, status_code_type,
                                   GSS_C_NULL_OID, &msg_ctx, &msg);
    if (maj_stat == GSS_S_COMPLETE) {
      int msg_len = (msg.length > kMaxMsgLength) ?
          static_cast<int>(kMaxMsgLength) :
          static_cast<int>(msg.length);
      if (msg_len > 0 && msg.value != NULL) {
        rv += StringPrintf(" %.*s", msg_len,
                           static_cast<char *>(msg.value));
      }
    }
    gssapi_lib->release_buffer(&min_stat, &msg);
    if (!msg_ctx)
      break;
  }
  return rv;
}

std::string DisplayExtendedStatus(GSSAPILibrary* gssapi_lib,
                                  OM_uint32 major_status,
                                  OM_uint32 minor_status) {
  if (major_status == GSS_S_COMPLETE)
    return "OK";
  std::string major = DisplayCode(gssapi_lib, major_status, GSS_C_GSS_CODE);
  std::string minor = DisplayCode(gssapi_lib, minor_status, GSS_C_MECH_CODE);
  return StringPrintf("Major: %s | Minor: %s", major.c_str(), minor.c_str());
}

// ScopedName releases a gss_name_t when it goes out of scope.
class ScopedName {
 public:
  ScopedName(gss_name_t name,
             GSSAPILibrary* gssapi_lib)
      : name_(name),
        gssapi_lib_(gssapi_lib) {
    DCHECK(gssapi_lib_);
  }

  ~ScopedName() {
    if (name_ != GSS_C_NO_NAME) {
      OM_uint32 minor_status = 0;
      OM_uint32 major_status =
          gssapi_lib_->release_name(&minor_status, &name_);
      if (major_status != GSS_S_COMPLETE) {
        LOG(WARNING) << "Problem releasing name. "
                     << DisplayStatus(major_status, minor_status);
      }
      name_ = GSS_C_NO_NAME;
    }
  }

 private:
  gss_name_t name_;
  GSSAPILibrary* gssapi_lib_;

  DISALLOW_COPY_AND_ASSIGN(ScopedName);
};

// ScopedBuffer releases a gss_buffer_t when it goes out of scope.
class ScopedBuffer {
 public:
  ScopedBuffer(gss_buffer_t buffer,
               GSSAPILibrary* gssapi_lib)
      : buffer_(buffer),
        gssapi_lib_(gssapi_lib) {
    DCHECK(gssapi_lib_);
  }

  ~ScopedBuffer() {
    if (buffer_ != GSS_C_NO_BUFFER) {
      OM_uint32 minor_status = 0;
      OM_uint32 major_status =
          gssapi_lib_->release_buffer(&minor_status, buffer_);
      if (major_status != GSS_S_COMPLETE) {
        LOG(WARNING) << "Problem releasing buffer. "
                     << DisplayStatus(major_status, minor_status);
      }
      buffer_ = GSS_C_NO_BUFFER;
    }
  }

 private:
  gss_buffer_t buffer_;
  GSSAPILibrary* gssapi_lib_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBuffer);
};

}  // namespace

HttpAuthGSSAPI::HttpAuthGSSAPI(GSSAPILibrary* library,
                               const std::string& scheme,
                               gss_OID gss_oid)
    : scheme_(scheme),
      gss_oid_(gss_oid),
      library_(library),
      sec_context_(NULL) {
}

HttpAuthGSSAPI::~HttpAuthGSSAPI() {
}

bool HttpAuthGSSAPI::NeedsIdentity() const {
  return decoded_server_auth_token_.empty();
}

bool HttpAuthGSSAPI::IsFinalRound() const {
  return !NeedsIdentity();
}

bool HttpAuthGSSAPI::ParseChallenge(HttpAuth::ChallengeTokenizer* tok) {
  // Verify the challenge's auth-scheme.
  if (!tok->valid() ||
      !LowerCaseEqualsASCII(tok->scheme(), StringToLowerASCII(scheme_).c_str()))
    return false;

  tok->set_expect_base64_token(true);
  if (!tok->GetNext()) {
    decoded_server_auth_token_.clear();
    return true;
  }

  std::string encoded_auth_token = tok->value();
  std::string decoded_auth_token;
  bool base64_rv = base::Base64Decode(encoded_auth_token, &decoded_auth_token);
  if (!base64_rv) {
    LOG(ERROR) << "Base64 decoding of auth token failed.";
    return false;
  }
  decoded_server_auth_token_ = decoded_auth_token;
  return true;
}

int HttpAuthGSSAPI::GenerateAuthToken(const std::wstring* username,
                                      const std::wstring* password,
                                      const std::wstring& spn,
                                      const HttpRequestInfo* request,
                                      const ProxyInfo* proxy,
                                      std::string* out_credentials) {
  DCHECK(library_);
  DCHECK((username == NULL) == (password == NULL));

  library_->Init();

  if (!IsFinalRound()) {
    int rv = OnFirstRound(username, password);
    if (rv != OK)
      return rv;
  }

  gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
  input_token.length = decoded_server_auth_token_.length();
  input_token.value = const_cast<char *>(decoded_server_auth_token_.data());
  gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
  int rv = GetNextSecurityToken(spn, &input_token, &output_token);
  if (rv != OK)
    return rv;

  // Base64 encode data in output buffer and prepend the scheme.
  std::string encode_input(static_cast<char*>(output_token.value),
                           output_token.length);
  std::string encode_output;
  bool ok = base::Base64Encode(encode_input, &encode_output);
  OM_uint32 minor_status = 0;
  library_->release_buffer(&minor_status, &output_token);
  if (!ok)
    return ERR_UNEXPECTED;
  *out_credentials = scheme_ + " " + encode_output;
  return OK;
}

int HttpAuthGSSAPI::OnFirstRound(const std::wstring* username,
                                 const std::wstring* password) {
  // TODO(cbentzel): Acquire credentials?
  DCHECK((username == NULL) == (password == NULL));
  username_.clear();
  password_.clear();
  if (username) {
    username_ = *username;
    password_ = *password;
  }
  return OK;
}

int HttpAuthGSSAPI::GetNextSecurityToken(const std::wstring& spn,
                                         gss_buffer_t in_token,
                                         gss_buffer_t out_token) {
  // Create a name for the principal
  // TODO(cbentzel): Should this be username@spn? What about domain?
  // TODO(cbentzel): Just do this on the first pass?
  const GURL spn_url(WideToASCII(spn));
  std::string spn_principal = GetHostAndPort(spn_url);
  gss_buffer_desc spn_buffer = GSS_C_EMPTY_BUFFER;
  spn_buffer.value = const_cast<char *>(spn_principal.data());
  spn_buffer.length = spn_principal.size() + 1;
  OM_uint32 minor_status = 0;
  gss_name_t principal_name;
  OM_uint32 major_status = library_->import_name(
      &minor_status,
      &spn_buffer,
      CHROME_GSS_C_NT_HOSTBASED_SERVICE,
      &principal_name);
  if (major_status != GSS_S_COMPLETE) {
    LOG(WARNING) << "Problem importing name. "
                 << DisplayExtendedStatus(library_,
                                          major_status,
                                          minor_status);
    return ERR_UNEXPECTED;
  }
  ScopedName scoped_name(principal_name, library_);

  // Create a security context.
  OM_uint32 req_flags = 0;
  major_status = library_->init_sec_context(
      &minor_status,
      GSS_C_NO_CREDENTIAL,
      &sec_context_,
      principal_name,
      gss_oid_,
      req_flags,
      GSS_C_INDEFINITE,
      GSS_C_NO_CHANNEL_BINDINGS,
      in_token,
      NULL,  // actual_mech_type
      out_token,
      NULL,  // ret flags
      NULL);
  if (major_status != GSS_S_COMPLETE &&
      major_status != GSS_S_CONTINUE_NEEDED) {
    LOG(WARNING) << "Problem initializing context. "
                 << DisplayExtendedStatus(library_,
                                          major_status,
                                          minor_status);
    return ERR_UNEXPECTED;
  }

  return (major_status != GSS_S_COMPLETE) ? ERR_IO_PENDING : OK;
}

}  // namespace net

