// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_v8.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"
#include "net/base/host_cache.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_resolver_js_bindings.h"
#include "net/proxy/proxy_resolver_request_context.h"
#include "net/proxy/proxy_resolver_script.h"
#include "v8/include/v8.h"

// Notes on the javascript environment:
//
// For the majority of the PAC utility functions, we use the same code
// as Firefox. See the javascript library that proxy_resolver_scipt.h
// pulls in.
//
// In addition, we implement a subset of Microsoft's extensions to PAC.
// TODO(eroman): Implement the rest.
//
//   - myIpAddressEx()
//   - dnsResolveEx()
//   - isResolvableEx()
//
// It is worth noting that the original PAC specification does not describe
// the return values on failure. Consequently, there are compatibility
// differences between browsers on what to return on failure, which are
// illustrated below:
//
// ----------------+-------------+-------------------+--------------
//                 | Firefox3    | InternetExplorer8 |  --> Us <---
// ----------------+-------------+-------------------+--------------
// myIpAddress()   | "127.0.0.1" |  ???              |  "127.0.0.1"
// dnsResolve()    | null        |  false            |  null
// myIpAddressEx() | N/A         |  ""               |  ""
// dnsResolveEx()  | N/A         |  ""               |  ""
// ----------------+-------------+-------------------+--------------
//
// TODO(eroman): The cell above reading ??? means I didn't test it.
//
// Another difference is in how dnsResolve() and myIpAddress() are
// implemented -- whether they should restrict to IPv4 results, or
// include both IPv4 and IPv6. The following table illustrates the
// differences:
//
// -----------------+-------------+-------------------+--------------
//                  | Firefox3    | InternetExplorer8 |  --> Us <---
// -----------------+-------------+-------------------+--------------
// myIpAddress()    | IPv4/IPv6   |  IPv4             |  IPv4
// dnsResolve()     | IPv4/IPv6   |  IPv4             |  IPv4
// isResolvable()   | IPv4/IPv6   |  IPv4             |  IPv4
// myIpAddressEx()  | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// dnsResolveEx()   | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// isResolvableEx() | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// -----------------+-------------+-------------------+--------------

namespace net {

namespace {

// Pseudo-name for the PAC script.
const char kPacResourceName[] = "proxy-pac-script.js";
// Pseudo-name for the PAC utility script.
const char kPacUtilityResourceName[] = "proxy-pac-utility-script.js";

// Converts a V8 String to a UTF16 string16.
string16 V8StringToUTF16(v8::Handle<v8::String> s) {
  int len = s->Length();
  string16 result;
  // Note that the reinterpret cast is because on Windows string16 is an alias
  // to wstring, and hence has character type wchar_t not uint16_t.
  s->Write(reinterpret_cast<uint16_t*>(WriteInto(&result, len + 1)), 0, len);
  return result;
}

// Converts a std::string (UTF8) to a V8 string.
v8::Local<v8::String> UTF8StdStringToV8String(const std::string& s) {
  return v8::String::New(s.data(), s.size());
}

// Stringizes a V8 object by calling its toString() method. Returns true
// on success. This may fail if the toString() throws an exception.
bool V8ObjectToUTF16String(v8::Handle<v8::Value> object,
                           string16* utf16_result) {
  if (object.IsEmpty())
    return false;

  v8::HandleScope scope;
  v8::Local<v8::String> str_object = object->ToString();
  if (str_object.IsEmpty())
    return false;
  *utf16_result = V8StringToUTF16(str_object);
  return true;
}

// Extracts an hostname argument from |args|. On success returns true
// and fills |*hostname| with the result.
bool GetHostnameArgument(const v8::Arguments& args, std::string* hostname) {
  // The first argument should be a string.
  if (args.Length() == 0 || args[0].IsEmpty() || !args[0]->IsString())
    return false;

  const string16 hostname_utf16 = V8StringToUTF16(args[0]->ToString());

  // If the hostname is already in ASCII, simply return it as is.
  if (IsStringASCII(hostname_utf16)) {
    *hostname = UTF16ToASCII(hostname_utf16);
    return true;
  }

  // Otherwise try to convert it from IDN to punycode.
  const int kInitialBufferSize = 256;
  url_canon::RawCanonOutputT<char16, kInitialBufferSize> punycode_output;
  if (!url_canon::IDNToASCII(hostname_utf16.data(),
                             hostname_utf16.length(),
                             &punycode_output)) {
    return false;
  }

  // |punycode_output| should now be ASCII; convert it to a std::string.
  // (We could use UTF16ToASCII() instead, but that requires an extra string
  // copy. Since ASCII is a subset of UTF8 the following is equivalent).
  bool success = UTF16ToUTF8(punycode_output.data(),
                             punycode_output.length(),
                             hostname);
  DCHECK(success);
  DCHECK(IsStringASCII(*hostname));
  return success;
}

}  // namespace

// ProxyResolverV8::Context ---------------------------------------------------

class ProxyResolverV8::Context {
 public:
  explicit Context(ProxyResolverJSBindings* js_bindings)
      : js_bindings_(js_bindings) {
    DCHECK(js_bindings != NULL);
  }

  ~Context() {
    v8::Locker locked;

    v8_this_.Dispose();
    v8_context_.Dispose();
  }

  int ResolveProxy(const GURL& query_url, ProxyInfo* results) {
    v8::Locker locked;
    v8::HandleScope scope;

    v8::Context::Scope function_scope(v8_context_);

    v8::Local<v8::Value> function;
    if (!GetFindProxyForURL(&function)) {
      js_bindings_->OnError(
          -1, ASCIIToUTF16("FindProxyForURL() is undefined."));
      return ERR_PAC_SCRIPT_FAILED;
    }

    v8::Handle<v8::Value> argv[] = {
      UTF8StdStringToV8String(query_url.spec()),
      UTF8StdStringToV8String(query_url.host()),
    };

    v8::TryCatch try_catch;
    v8::Local<v8::Value> ret = v8::Function::Cast(*function)->Call(
        v8_context_->Global(), arraysize(argv), argv);

    if (try_catch.HasCaught()) {
      HandleError(try_catch.Message());
      return ERR_PAC_SCRIPT_FAILED;
    }

    if (!ret->IsString()) {
      js_bindings_->OnError(
          -1, ASCIIToUTF16("FindProxyForURL() did not return a string."));
      return ERR_PAC_SCRIPT_FAILED;
    }

    string16 ret_str = V8StringToUTF16(ret->ToString());

    if (!IsStringASCII(ret_str)) {
      // TODO(eroman): Rather than failing when a wide string is returned, we
      //               could extend the parsing to handle IDNA hostnames by
      //               converting them to ASCII punycode.
      //               crbug.com/47234
      string16 error_message =
          ASCIIToUTF16("FindProxyForURL() returned a non-ASCII string "
                       "(crbug.com/47234): ") + ret_str;
      js_bindings_->OnError(-1, error_message);
      return ERR_PAC_SCRIPT_FAILED;
    }

    results->UsePacString(UTF16ToASCII(ret_str));
    return OK;
  }

  int InitV8(const std::string& pac_data_utf8) {
    v8::Locker locked;
    v8::HandleScope scope;

    v8_this_ = v8::Persistent<v8::External>::New(v8::External::New(this));
    v8::Local<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New();

    // Attach the javascript bindings.
    v8::Local<v8::FunctionTemplate> alert_template =
        v8::FunctionTemplate::New(&AlertCallback, v8_this_);
    global_template->Set(v8::String::New("alert"), alert_template);

    v8::Local<v8::FunctionTemplate> my_ip_address_template =
        v8::FunctionTemplate::New(&MyIpAddressCallback, v8_this_);
    global_template->Set(v8::String::New("myIpAddress"),
        my_ip_address_template);

    v8::Local<v8::FunctionTemplate> dns_resolve_template =
        v8::FunctionTemplate::New(&DnsResolveCallback, v8_this_);
    global_template->Set(v8::String::New("dnsResolve"),
        dns_resolve_template);

    // Microsoft's PAC extensions (incomplete):

    v8::Local<v8::FunctionTemplate> dns_resolve_ex_template =
        v8::FunctionTemplate::New(&DnsResolveExCallback, v8_this_);
    global_template->Set(v8::String::New("dnsResolveEx"),
                         dns_resolve_ex_template);

    v8::Local<v8::FunctionTemplate> my_ip_address_ex_template =
        v8::FunctionTemplate::New(&MyIpAddressExCallback, v8_this_);
    global_template->Set(v8::String::New("myIpAddressEx"),
                         my_ip_address_ex_template);

    v8_context_ = v8::Context::New(NULL, global_template);

    v8::Context::Scope ctx(v8_context_);

    // Add the PAC utility functions to the environment.
    // (This script should never fail, as it is a string literal!)
    // Note that the two string literals are concatenated.
    int rv = RunScript(PROXY_RESOLVER_SCRIPT
                       PROXY_RESOLVER_SCRIPT_EX,
                       kPacUtilityResourceName);
    if (rv != OK) {
      NOTREACHED();
      return rv;
    }

    // Add the user's PAC code to the environment.
    rv = RunScript(pac_data_utf8, kPacResourceName);
    if (rv != OK)
      return rv;

    // At a minimum, the FindProxyForURL() function must be defined for this
    // to be a legitimiate PAC script.
    v8::Local<v8::Value> function;
    if (!GetFindProxyForURL(&function))
      return ERR_PAC_SCRIPT_FAILED;

    return OK;
  }

  void SetCurrentRequestContext(ProxyResolverRequestContext* context) {
    js_bindings_->set_current_request_context(context);
  }

  void PurgeMemory() {
    v8::Locker locked;
    // Repeatedly call the V8 idle notification until it returns true ("nothing
    // more to free").  Note that it makes more sense to do this than to
    // implement a new "delete everything" pass because object references make
    // it difficult to free everything possible in just one pass.
    while (!v8::V8::IdleNotification())
      ;
  }

 private:
  bool GetFindProxyForURL(v8::Local<v8::Value>* function) {
    *function = v8_context_->Global()->Get(v8::String::New("FindProxyForURL"));
    return (*function)->IsFunction();
  }

  // Handle an exception thrown by V8.
  void HandleError(v8::Handle<v8::Message> message) {
    if (message.IsEmpty())
      return;

    // Otherwise dispatch to the bindings.
    int line_number = message->GetLineNumber();
    string16 error_message;
    V8ObjectToUTF16String(message->Get(), &error_message);
    js_bindings_->OnError(line_number, error_message);
  }

  // Compiles and runs |script_utf8| in the current V8 context.
  // Returns OK on success, otherwise an error code.
  int RunScript(const std::string& script_utf8, const char* script_name) {
    v8::TryCatch try_catch;

    // Compile the script.
    v8::Local<v8::String> text = UTF8StdStringToV8String(script_utf8);
    v8::ScriptOrigin origin = v8::ScriptOrigin(v8::String::New(script_name));
    v8::Local<v8::Script> code = v8::Script::Compile(text, &origin);

    // Execute.
    if (!code.IsEmpty())
      code->Run();

    // Check for errors.
    if (try_catch.HasCaught()) {
      HandleError(try_catch.Message());
      return ERR_PAC_SCRIPT_FAILED;
    }

    return OK;
  }

  // V8 callback for when "alert()" is invoked by the PAC script.
  static v8::Handle<v8::Value> AlertCallback(const v8::Arguments& args) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    // Like firefox we assume "undefined" if no argument was specified, and
    // disregard any arguments beyond the first.
    string16 message;
    if (args.Length() == 0) {
      message = ASCIIToUTF16("undefined");
    } else {
      if (!V8ObjectToUTF16String(args[0], &message))
        return v8::Undefined();  // toString() threw an exception.
    }

    context->js_bindings_->Alert(message);
    return v8::Undefined();
  }

  // V8 callback for when "myIpAddress()" is invoked by the PAC script.
  static v8::Handle<v8::Value> MyIpAddressCallback(const v8::Arguments& args) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    std::string result;
    bool success;

    {
      v8::Unlocker unlocker;

      LogEventToCurrentRequest(context,
                               NetLog::PHASE_BEGIN,
                               NetLog::TYPE_PROXY_RESOLVER_V8_MY_IP_ADDRESS,
                               NULL);

      // We shouldn't be called with any arguments, but will not complain if
      // we are.
      success = context->js_bindings_->MyIpAddress(&result);

      LogEventToCurrentRequest(context,
                               NetLog::PHASE_END,
                               NetLog::TYPE_PROXY_RESOLVER_V8_MY_IP_ADDRESS,
                               NULL);
    }

    if (!success)
      result = "127.0.0.1";
    return UTF8StdStringToV8String(result);
  }

  // V8 callback for when "myIpAddressEx()" is invoked by the PAC script.
  static v8::Handle<v8::Value> MyIpAddressExCallback(
      const v8::Arguments& args) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    std::string ip_address_list;
    bool success;

    {
      v8::Unlocker unlocker;

      LogEventToCurrentRequest(context,
                               NetLog::PHASE_BEGIN,
                               NetLog::TYPE_PROXY_RESOLVER_V8_MY_IP_ADDRESS_EX,
                               NULL);

      // We shouldn't be called with any arguments, but will not complain if
      // we are.
      success = context->js_bindings_->MyIpAddressEx(&ip_address_list);

      LogEventToCurrentRequest(context,
                               NetLog::PHASE_END,
                               NetLog::TYPE_PROXY_RESOLVER_V8_MY_IP_ADDRESS_EX,
                               NULL);
    }

    if (!success)
      ip_address_list = std::string();
    return UTF8StdStringToV8String(ip_address_list);
  }

  // V8 callback for when "dnsResolve()" is invoked by the PAC script.
  static v8::Handle<v8::Value> DnsResolveCallback(const v8::Arguments& args) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    // We need at least one string argument.
    std::string hostname;
    if (!GetHostnameArgument(args, &hostname))
      return v8::Null();

    std::string ip_address;
    bool success;

    {
      v8::Unlocker unlocker;

      LogEventToCurrentRequest(context,
                               NetLog::PHASE_BEGIN,
                               NetLog::TYPE_PROXY_RESOLVER_V8_DNS_RESOLVE,
                               NULL);

      success = context->js_bindings_->DnsResolve(hostname, &ip_address);

      LogEventToCurrentRequest(context,
                               NetLog::PHASE_END,
                               NetLog::TYPE_PROXY_RESOLVER_V8_DNS_RESOLVE,
                               NULL);
    }

    return success ? UTF8StdStringToV8String(ip_address) : v8::Null();
  }

  // V8 callback for when "dnsResolveEx()" is invoked by the PAC script.
  static v8::Handle<v8::Value> DnsResolveExCallback(const v8::Arguments& args) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    // We need at least one string argument.
    std::string hostname;
    if (!GetHostnameArgument(args, &hostname))
      return v8::Undefined();

    std::string ip_address_list;
    bool success;

    {
      v8::Unlocker unlocker;

      LogEventToCurrentRequest(context,
                               NetLog::PHASE_BEGIN,
                               NetLog::TYPE_PROXY_RESOLVER_V8_DNS_RESOLVE_EX,
                               NULL);

      success = context->js_bindings_->DnsResolveEx(hostname,
                                                    &ip_address_list);

      LogEventToCurrentRequest(context,
                               NetLog::PHASE_END,
                               NetLog::TYPE_PROXY_RESOLVER_V8_DNS_RESOLVE_EX,
                               NULL);
    }

    if (!success)
      ip_address_list = std::string();

    return UTF8StdStringToV8String(ip_address_list);
  }

  static void LogEventToCurrentRequest(Context* context,
                                       NetLog::EventPhase phase,
                                       NetLog::EventType type,
                                       NetLog::EventParameters* params) {
    if (context->js_bindings_->current_request_context()) {
      context->js_bindings_->current_request_context()->net_log->AddEntry(
          type, phase, params);
    }
  }

  ProxyResolverJSBindings* js_bindings_;
  v8::Persistent<v8::External> v8_this_;
  v8::Persistent<v8::Context> v8_context_;
};

// ProxyResolverV8 ------------------------------------------------------------

ProxyResolverV8::ProxyResolverV8(
    ProxyResolverJSBindings* custom_js_bindings)
    : ProxyResolver(true /*expects_pac_bytes*/),
      js_bindings_(custom_js_bindings) {
}

ProxyResolverV8::~ProxyResolverV8() {}

int ProxyResolverV8::GetProxyForURL(const GURL& query_url,
                                    ProxyInfo* results,
                                    CompletionCallback* /*callback*/,
                                    RequestHandle* /*request*/,
                                    const BoundNetLog& net_log) {
  // If the V8 instance has not been initialized (either because
  // SetPacScript() wasn't called yet, or because it failed.
  if (!context_.get())
    return ERR_FAILED;

  // Associate some short-lived context with this request. This context will be
  // available to any of the javascript "bindings" that are subsequently invoked
  // from the javascript.
  //
  // In particular, we create a HostCache that is aggressive about caching
  // failed DNS resolves.
  HostCache host_cache(
      50,
      base::TimeDelta::FromMinutes(5),
      base::TimeDelta::FromMinutes(5));

  ProxyResolverRequestContext request_context(&net_log, &host_cache);

  // Otherwise call into V8.
  context_->SetCurrentRequestContext(&request_context);
  int rv = context_->ResolveProxy(query_url, results);
  context_->SetCurrentRequestContext(NULL);

  return rv;
}

void ProxyResolverV8::CancelRequest(RequestHandle request) {
  // This is a synchronous ProxyResolver; no possibility for async requests.
  NOTREACHED();
}

void ProxyResolverV8::PurgeMemory() {
  context_->PurgeMemory();
}

int ProxyResolverV8::SetPacScript(const GURL& /*url*/,
                                  const std::string& bytes_utf8,
                                  CompletionCallback* /*callback*/) {
  context_.reset();
  if (bytes_utf8.empty())
    return ERR_PAC_SCRIPT_FAILED;

  // Try parsing the PAC script.
  scoped_ptr<Context> context(new Context(js_bindings_.get()));
  int rv = context->InitV8(bytes_utf8);
  if (rv == OK)
    context_.reset(context.release());
  return rv;
}

}  // namespace net
