// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/proxy/proxy_resolver_v8.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_log.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_resolver_js_bindings.h"
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

// Convert a V8 String to a std::string.
std::string V8StringToStdString(v8::Handle<v8::String> s) {
  int len = s->Utf8Length();
  std::string result;
  s->WriteUtf8(WriteInto(&result, len + 1), len);
  return result;
}

// Convert a std::string (UTF8) to a V8 string.
v8::Local<v8::String> StdStringToV8String(const std::string& s) {
  return v8::String::New(s.data(), s.size());
}

// String-ize a V8 object by calling its toString() method. Returns true
// on success. This may fail if the toString() throws an exception.
bool V8ObjectToString(v8::Handle<v8::Value> object, std::string* result) {
  if (object.IsEmpty())
    return false;

  v8::HandleScope scope;
  v8::Local<v8::String> str_object = object->ToString();
  if (str_object.IsEmpty())
    return false;
  *result = V8StringToStdString(str_object);
  return true;
}

}  // namespace

// ProxyResolverV8::Context ---------------------------------------------------

class ProxyResolverV8::Context {
 public:
  explicit Context(ProxyResolverJSBindings* js_bindings)
      : js_bindings_(js_bindings), current_request_load_log_(NULL) {
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
      js_bindings_->OnError(-1, "FindProxyForURL() is undefined.");
      return ERR_PAC_SCRIPT_FAILED;
    }

    v8::Handle<v8::Value> argv[] = {
      StdStringToV8String(query_url.spec()),
      StdStringToV8String(query_url.host()),
    };

    v8::TryCatch try_catch;
    v8::Local<v8::Value> ret = v8::Function::Cast(*function)->Call(
        v8_context_->Global(), arraysize(argv), argv);

    if (try_catch.HasCaught()) {
      HandleError(try_catch.Message());
      return ERR_PAC_SCRIPT_FAILED;
    }

    if (!ret->IsString()) {
      js_bindings_->OnError(-1, "FindProxyForURL() did not return a string.");
      return ERR_PAC_SCRIPT_FAILED;
    }

    std::string ret_str = V8StringToStdString(ret->ToString());

    results->UsePacString(ret_str);

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

  void SetCurrentRequestLoadLog(LoadLog* load_log) {
    current_request_load_log_ = load_log;
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
    std::string error_message;
    V8ObjectToString(message->Get(), &error_message);
    js_bindings_->OnError(line_number, error_message);
  }

  // Compiles and runs |script_utf8| in the current V8 context.
  // Returns OK on success, otherwise an error code.
  int RunScript(const std::string& script_utf8, const char* script_name) {
    v8::TryCatch try_catch;

    // Compile the script.
    v8::Local<v8::String> text = StdStringToV8String(script_utf8);
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
    std::string message;
    if (args.Length() == 0) {
      message = "undefined";
    } else {
      if (!V8ObjectToString(args[0], &message))
        return v8::Undefined();  // toString() threw an exception.
    }

    context->js_bindings_->Alert(message);
    return v8::Undefined();
  }

  // V8 callback for when "myIpAddress()" is invoked by the PAC script.
  static v8::Handle<v8::Value> MyIpAddressCallback(const v8::Arguments& args) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    LoadLog::BeginEvent(context->current_request_load_log_,
                        LoadLog::TYPE_PROXY_RESOLVER_V8_MY_IP_ADDRESS);

    // We shouldn't be called with any arguments, but will not complain if
    // we are.
    std::string result = context->js_bindings_->MyIpAddress();

    LoadLog::EndEvent(context->current_request_load_log_,
                      LoadLog::TYPE_PROXY_RESOLVER_V8_MY_IP_ADDRESS);

    if (result.empty())
      result = "127.0.0.1";
    return StdStringToV8String(result);
  }

  // V8 callback for when "myIpAddressEx()" is invoked by the PAC script.
  static v8::Handle<v8::Value> MyIpAddressExCallback(
      const v8::Arguments& args) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    LoadLog::BeginEvent(context->current_request_load_log_,
                        LoadLog::TYPE_PROXY_RESOLVER_V8_MY_IP_ADDRESS_EX);

    // We shouldn't be called with any arguments, but will not complain if
    // we are.
    std::string result = context->js_bindings_->MyIpAddressEx();

    LoadLog::EndEvent(context->current_request_load_log_,
                      LoadLog::TYPE_PROXY_RESOLVER_V8_MY_IP_ADDRESS_EX);

    return StdStringToV8String(result);
  }

  // V8 callback for when "dnsResolve()" is invoked by the PAC script.
  static v8::Handle<v8::Value> DnsResolveCallback(const v8::Arguments& args) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    // We need at least one argument.
    std::string host;
    if (args.Length() == 0) {
      host = "undefined";
    } else {
      if (!V8ObjectToString(args[0], &host))
        return v8::Undefined();
    }

    LoadLog::BeginEvent(context->current_request_load_log_,
                        LoadLog::TYPE_PROXY_RESOLVER_V8_DNS_RESOLVE);

    std::string result = context->js_bindings_->DnsResolve(host);

    LoadLog::EndEvent(context->current_request_load_log_,
                      LoadLog::TYPE_PROXY_RESOLVER_V8_DNS_RESOLVE);

    // DnsResolve() returns empty string on failure.
    return result.empty() ? v8::Null() : StdStringToV8String(result);
  }

  // V8 callback for when "dnsResolveEx()" is invoked by the PAC script.
  static v8::Handle<v8::Value> DnsResolveExCallback(const v8::Arguments& args) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    // We need at least one argument.
    std::string host;
    if (args.Length() == 0) {
      host = "undefined";
    } else {
      if (!V8ObjectToString(args[0], &host))
        return v8::Undefined();
    }

    LoadLog::BeginEvent(context->current_request_load_log_,
                        LoadLog::TYPE_PROXY_RESOLVER_V8_DNS_RESOLVE_EX);

    std::string result = context->js_bindings_->DnsResolveEx(host);

    LoadLog::EndEvent(context->current_request_load_log_,
                      LoadLog::TYPE_PROXY_RESOLVER_V8_DNS_RESOLVE_EX);

    return StdStringToV8String(result);
  }

  ProxyResolverJSBindings* js_bindings_;
  LoadLog* current_request_load_log_;
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
                                    LoadLog* load_log) {
  // If the V8 instance has not been initialized (either because
  // SetPacScript() wasn't called yet, or because it failed.
  if (!context_.get())
    return ERR_FAILED;

  // Otherwise call into V8.
  context_->SetCurrentRequestLoadLog(load_log);
  int rv = context_->ResolveProxy(query_url, results);
  context_->SetCurrentRequestLoadLog(NULL);

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
