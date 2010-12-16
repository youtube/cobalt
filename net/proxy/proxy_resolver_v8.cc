// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdio>

#include "net/proxy/proxy_resolver_v8.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"
#include "net/base/host_cache.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
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
// - myIpAddressEx()
// - dnsResolveEx()
// - isResolvableEx()
// - isInNetEx()
// - sortIpAddressList()
//
// It is worth noting that the original PAC specification does not describe
// the return values on failure. Consequently, there are compatibility
// differences between browsers on what to return on failure, which are
// illustrated below:
//
// --------------------+-------------+-------------------+--------------
//                     | Firefox3    | InternetExplorer8 |  --> Us <---
// --------------------+-------------+-------------------+--------------
// myIpAddress()       | "127.0.0.1" |  ???              |  "127.0.0.1"
// dnsResolve()        | null        |  false            |  null
// myIpAddressEx()     | N/A         |  ""               |  ""
// sortIpAddressList() | N/A         |  false            |  false
// dnsResolveEx()      | N/A         |  ""               |  ""
// isInNetEx()         | N/A         |  false            |  false
// --------------------+-------------+-------------------+--------------
//
// TODO(eroman): The cell above reading ??? means I didn't test it.
//
// Another difference is in how dnsResolve() and myIpAddress() are
// implemented -- whether they should restrict to IPv4 results, or
// include both IPv4 and IPv6. The following table illustrates the
// differences:
//
// --------------------+-------------+-------------------+--------------
//                     | Firefox3    | InternetExplorer8 |  --> Us <---
// --------------------+-------------+-------------------+--------------
// myIpAddress()       | IPv4/IPv6   |  IPv4             |  IPv4
// dnsResolve()        | IPv4/IPv6   |  IPv4             |  IPv4
// isResolvable()      | IPv4/IPv6   |  IPv4             |  IPv4
// myIpAddressEx()     | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// dnsResolveEx()      | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// sortIpAddressList() | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// isResolvableEx()    | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// isInNetEx()         | N/A         |  IPv4/IPv6        |  IPv4/IPv6
// -----------------+-------------+-------------------+--------------

namespace net {

namespace {

// Pseudo-name for the PAC script.
const char kPacResourceName[] = "proxy-pac-script.js";
// Pseudo-name for the PAC utility script.
const char kPacUtilityResourceName[] = "proxy-pac-utility-script.js";

// External string wrapper so V8 can access the UTF16 string wrapped by
// ProxyResolverScriptData.
class V8ExternalStringFromScriptData
    : public v8::String::ExternalStringResource {
 public:
  explicit V8ExternalStringFromScriptData(
      const scoped_refptr<ProxyResolverScriptData>& script_data)
      : script_data_(script_data) {}

  virtual const uint16_t* data() const {
    return reinterpret_cast<const uint16*>(script_data_->utf16().data());
  }

  virtual size_t length() const {
    return script_data_->utf16().size();
  }

 private:
  const scoped_refptr<ProxyResolverScriptData> script_data_;
  DISALLOW_COPY_AND_ASSIGN(V8ExternalStringFromScriptData);
};

// External string wrapper so V8 can access a string literal.
class V8ExternalASCIILiteral : public v8::String::ExternalAsciiStringResource {
 public:
  // |ascii| must be a NULL-terminated C string, and must remain valid
  // throughout this object's lifetime.
  V8ExternalASCIILiteral(const char* ascii, size_t length)
      : ascii_(ascii), length_(length) {
    DCHECK(IsStringASCII(ascii));
  }

  virtual const char* data() const {
    return ascii_;
  }

  virtual size_t length() const {
    return length_;
  }

 private:
  const char* ascii_;
  size_t length_;
  DISALLOW_COPY_AND_ASSIGN(V8ExternalASCIILiteral);
};

// When creating a v8::String from a C++ string we have two choices: create
// a copy, or create a wrapper that shares the same underlying storage.
// For small strings it is better to just make a copy, whereas for large
// strings there are savings by sharing the storage. This number identifies
// the cutoff length for when to start wrapping rather than creating copies.
const size_t kMaxStringBytesForCopy = 256;

// Converts a V8 String to a UTF8 std::string.
std::string V8StringToUTF8(v8::Handle<v8::String> s) {
  std::string result;
  s->WriteUtf8(WriteInto(&result, s->Length() + 1));
  return result;
}

// Converts a V8 String to a UTF16 string16.
string16 V8StringToUTF16(v8::Handle<v8::String> s) {
  int len = s->Length();
  string16 result;
  // Note that the reinterpret cast is because on Windows string16 is an alias
  // to wstring, and hence has character type wchar_t not uint16_t.
  s->Write(reinterpret_cast<uint16_t*>(WriteInto(&result, len + 1)), 0, len);
  return result;
}

// Converts an ASCII std::string to a V8 string.
v8::Local<v8::String> ASCIIStringToV8String(const std::string& s) {
  DCHECK(IsStringASCII(s));
  return v8::String::New(s.data(), s.size());
}

// Converts a UTF16 string16 (warpped by a ProxyResolverScriptData) to a
// V8 string.
v8::Local<v8::String> ScriptDataToV8String(
    const scoped_refptr<ProxyResolverScriptData>& s) {
  if (s->utf16().size() * 2 <= kMaxStringBytesForCopy) {
    return v8::String::New(
        reinterpret_cast<const uint16_t*>(s->utf16().data()),
        s->utf16().size());
  }
  return v8::String::NewExternal(new V8ExternalStringFromScriptData(s));
}

// Converts an ASCII string literal to a V8 string.
v8::Local<v8::String> ASCIILiteralToV8String(const char* ascii) {
  DCHECK(IsStringASCII(ascii));
  size_t length = strlen(ascii);
  if (length <= kMaxStringBytesForCopy)
    return v8::String::New(ascii, length);
  return v8::String::NewExternal(new V8ExternalASCIILiteral(ascii, length));
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

// Wrapper for passing around IP address strings and IPAddressNumber objects.
struct IPAddress {
  IPAddress(const std::string& ip_string, const IPAddressNumber& ip_number)
      : string_value(ip_string),
        ip_address_number(ip_number) {
  }

  // Used for sorting IP addresses in ascending order in SortIpAddressList().
  // IP6 addresses are placed ahead of IPv4 addresses.
  bool operator<(const IPAddress& rhs) const {
    const IPAddressNumber& ip1 = this->ip_address_number;
    const IPAddressNumber& ip2 = rhs.ip_address_number;
    if (ip1.size() != ip2.size())
      return ip1.size() > ip2.size();  // IPv6 before IPv4.
    DCHECK(ip1.size() == ip2.size());
    return memcmp(&ip1[0], &ip2[0], ip1.size()) < 0;  // Ascending order.
  }

  std::string string_value;
  IPAddressNumber ip_address_number;
};

// Handler for "sortIpAddressList(IpAddressList)". |ip_address_list| is a
// semi-colon delimited string containing IP addresses.
// |sorted_ip_address_list| is the resulting list of sorted semi-colon delimited
// IP addresses or an empty string if unable to sort the IP address list.
// Returns 'true' if the sorting was successful, and 'false' if the input was an
// empty string, a string of separators (";" in this case), or if any of the IP
// addresses in the input list failed to parse.
bool SortIpAddressList(const std::string& ip_address_list,
                       std::string* sorted_ip_address_list) {
  sorted_ip_address_list->clear();

  // Strip all whitespace (mimics IE behavior).
  std::string cleaned_ip_address_list;
  RemoveChars(ip_address_list, " \t", &cleaned_ip_address_list);
  if (cleaned_ip_address_list.empty())
    return false;

  // Split-up IP addresses and store them in a vector.
  std::vector<IPAddress> ip_vector;
  IPAddressNumber ip_num;
  StringTokenizer str_tok(cleaned_ip_address_list, ";");
  while (str_tok.GetNext()) {
    if (!ParseIPLiteralToNumber(str_tok.token(), &ip_num))
      return false;
    ip_vector.push_back(IPAddress(str_tok.token(), ip_num));
  }

  if (ip_vector.empty())  // Can happen if we have something like
    return false;         // sortIpAddressList(";") or sortIpAddressList("; ;")

  DCHECK(!ip_vector.empty());

  // Sort lists according to ascending numeric value.
  if (ip_vector.size() > 1)
    std::stable_sort(ip_vector.begin(), ip_vector.end());

  // Return a semi-colon delimited list of sorted addresses (IPv6 followed by
  // IPv4).
  for (size_t i = 0; i < ip_vector.size(); ++i) {
    if (i > 0)
      *sorted_ip_address_list += ";";
    *sorted_ip_address_list += ip_vector[i].string_value;
  }
  return true;
}

// Handler for "isInNetEx(ip_address, ip_prefix)". |ip_address| is a string
// containing an IPv4/IPv6 address, and |ip_prefix| is a string containg a
// slash-delimited IP prefix with the top 'n' bits specified in the bit
// field. This returns 'true' if the address is in the same subnet, and
// 'false' otherwise. Also returns 'false' if the prefix is in an incorrect
// format, or if an address and prefix of different types are used (e.g. IPv6
// address and IPv4 prefix).
bool IsInNetEx(const std::string& ip_address, const std::string& ip_prefix) {
  IPAddressNumber address;
  if (!ParseIPLiteralToNumber(ip_address, &address))
    return false;

  IPAddressNumber prefix;
  size_t prefix_length_in_bits;
  if (!ParseCIDRBlock(ip_prefix, &prefix, &prefix_length_in_bits))
    return false;

  // Both |address| and |prefix| must be of the same type (IPv4 or IPv6).
  if (address.size() != prefix.size())
    return false;

  DCHECK((address.size() == 4 && prefix.size() == 4) ||
         (address.size() == 16 && prefix.size() == 16));

  return IPNumberMatchesPrefix(address, prefix, prefix_length_in_bits);
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

    // Run the V8 garbage collector. We do this to be sure the
    // ExternalStringResource objects we allocated get properly disposed.
    // Otherwise when running the unit-tests they may get leaked.
    // See crbug.com/48145.
    PurgeMemory();
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
      ASCIIStringToV8String(query_url.spec()),
      ASCIIStringToV8String(query_url.host()),
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

  int InitV8(const scoped_refptr<ProxyResolverScriptData>& pac_script) {
    v8::Locker locked;
    v8::HandleScope scope;

    v8_this_ = v8::Persistent<v8::External>::New(v8::External::New(this));
    v8::Local<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New();

    // Attach the javascript bindings.
    v8::Local<v8::FunctionTemplate> alert_template =
        v8::FunctionTemplate::New(&AlertCallback, v8_this_);
    global_template->Set(ASCIILiteralToV8String("alert"), alert_template);

    v8::Local<v8::FunctionTemplate> my_ip_address_template =
        v8::FunctionTemplate::New(&MyIpAddressCallback, v8_this_);
    global_template->Set(ASCIILiteralToV8String("myIpAddress"),
        my_ip_address_template);

    v8::Local<v8::FunctionTemplate> dns_resolve_template =
        v8::FunctionTemplate::New(&DnsResolveCallback, v8_this_);
    global_template->Set(ASCIILiteralToV8String("dnsResolve"),
        dns_resolve_template);

    // Microsoft's PAC extensions:

    v8::Local<v8::FunctionTemplate> dns_resolve_ex_template =
        v8::FunctionTemplate::New(&DnsResolveExCallback, v8_this_);
    global_template->Set(ASCIILiteralToV8String("dnsResolveEx"),
                         dns_resolve_ex_template);

    v8::Local<v8::FunctionTemplate> my_ip_address_ex_template =
        v8::FunctionTemplate::New(&MyIpAddressExCallback, v8_this_);
    global_template->Set(ASCIILiteralToV8String("myIpAddressEx"),
                         my_ip_address_ex_template);

    v8::Local<v8::FunctionTemplate> sort_ip_address_list_template =
        v8::FunctionTemplate::New(&SortIpAddressListCallback, v8_this_);
    global_template->Set(ASCIILiteralToV8String("sortIpAddressList"),
                         sort_ip_address_list_template);

    v8::Local<v8::FunctionTemplate> is_in_net_ex_template =
        v8::FunctionTemplate::New(&IsInNetExCallback, v8_this_);
    global_template->Set(ASCIILiteralToV8String("isInNetEx"),
                         is_in_net_ex_template);

    v8_context_ = v8::Context::New(NULL, global_template);

    v8::Context::Scope ctx(v8_context_);

    // Add the PAC utility functions to the environment.
    // (This script should never fail, as it is a string literal!)
    // Note that the two string literals are concatenated.
    int rv = RunScript(
        ASCIILiteralToV8String(
            PROXY_RESOLVER_SCRIPT
            PROXY_RESOLVER_SCRIPT_EX),
        kPacUtilityResourceName);
    if (rv != OK) {
      NOTREACHED();
      return rv;
    }

    // Add the user's PAC code to the environment.
    rv = RunScript(ScriptDataToV8String(pac_script), kPacResourceName);
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
    *function = v8_context_->Global()->Get(
        ASCIILiteralToV8String("FindProxyForURL"));
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

  // Compiles and runs |script| in the current V8 context.
  // Returns OK on success, otherwise an error code.
  int RunScript(v8::Handle<v8::String> script, const char* script_name) {
    v8::TryCatch try_catch;

    // Compile the script.
    v8::ScriptOrigin origin =
        v8::ScriptOrigin(ASCIILiteralToV8String(script_name));
    v8::Local<v8::Script> code = v8::Script::Compile(script, &origin);

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

      // We shouldn't be called with any arguments, but will not complain if
      // we are.
      success = context->js_bindings_->MyIpAddress(&result);
    }

    if (!success)
      return ASCIILiteralToV8String("127.0.0.1");
    return ASCIIStringToV8String(result);
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

      // We shouldn't be called with any arguments, but will not complain if
      // we are.
      success = context->js_bindings_->MyIpAddressEx(&ip_address_list);
    }

    if (!success)
      ip_address_list = std::string();
    return ASCIIStringToV8String(ip_address_list);
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
      success = context->js_bindings_->DnsResolve(hostname, &ip_address);
    }

    return success ? ASCIIStringToV8String(ip_address) : v8::Null();
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
      success = context->js_bindings_->DnsResolveEx(hostname, &ip_address_list);
    }

    if (!success)
      ip_address_list = std::string();

    return ASCIIStringToV8String(ip_address_list);
  }

  // V8 callback for when "sortIpAddressList()" is invoked by the PAC script.
  static v8::Handle<v8::Value> SortIpAddressListCallback(
      const v8::Arguments& args) {
    // We need at least one string argument.
    if (args.Length() == 0 || args[0].IsEmpty() || !args[0]->IsString())
      return v8::Null();

    std::string ip_address_list = V8StringToUTF8(args[0]->ToString());
    if (!IsStringASCII(ip_address_list))
      return v8::Null();
    std::string sorted_ip_address_list;
    bool success = SortIpAddressList(ip_address_list, &sorted_ip_address_list);
    if (!success)
      return v8::False();
    return ASCIIStringToV8String(sorted_ip_address_list);
  }

  // V8 callback for when "isInNetEx()" is invoked by the PAC script.
  static v8::Handle<v8::Value> IsInNetExCallback(const v8::Arguments& args) {
    // We need at least 2 string arguments.
    if (args.Length() < 2 || args[0].IsEmpty() || !args[0]->IsString() ||
        args[1].IsEmpty() || !args[1]->IsString())
      return v8::Null();

    std::string ip_address = V8StringToUTF8(args[0]->ToString());
    if (!IsStringASCII(ip_address))
      return v8::False();
    std::string ip_prefix = V8StringToUTF8(args[1]->ToString());
    if (!IsStringASCII(ip_prefix))
      return v8::False();
    return IsInNetEx(ip_address, ip_prefix) ? v8::True() : v8::False();
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

void ProxyResolverV8::CancelSetPacScript() {
  NOTREACHED();
}

void ProxyResolverV8::PurgeMemory() {
  context_->PurgeMemory();
}

void ProxyResolverV8::Shutdown() {
  js_bindings_->Shutdown();
}

int ProxyResolverV8::SetPacScript(
    const scoped_refptr<ProxyResolverScriptData>& script_data,
    CompletionCallback* /*callback*/) {
  DCHECK(script_data.get());
  context_.reset();
  if (script_data->utf16().empty())
    return ERR_PAC_SCRIPT_FAILED;

  // Try parsing the PAC script.
  scoped_ptr<Context> context(new Context(js_bindings_.get()));
  int rv = context->InitV8(script_data);
  if (rv == OK)
    context_.reset(context.release());
  return rv;
}

}  // namespace net
