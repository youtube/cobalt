// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_UTIL_H_
#define NET_BASE_NET_UTIL_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <ws2tcpip.h>
#elif defined(OS_POSIX)
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include <list>
#include <string>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "net/base/escape.h"
#include "net/base/net_export.h"

struct addrinfo;
class FilePath;
class GURL;

namespace base {
class Time;
}

namespace url_canon {
struct CanonHostInfo;
}

namespace url_parse {
struct Parsed;
}

namespace net {

// Used by FormatUrl to specify handling of certain parts of the url.
typedef uint32 FormatUrlType;
typedef uint32 FormatUrlTypes;

// Used by GetHeaderParamValue to determine how to handle quotes in the value.
class QuoteRule {
 public:
  enum Type {
    KEEP_OUTER_QUOTES,
    REMOVE_OUTER_QUOTES,
  };

 private:
  QuoteRule();
};

// Nothing is ommitted.
NET_EXPORT extern const FormatUrlType kFormatUrlOmitNothing;

// If set, any username and password are removed.
NET_EXPORT extern const FormatUrlType kFormatUrlOmitUsernamePassword;

// If the scheme is 'http://', it's removed.
NET_EXPORT extern const FormatUrlType kFormatUrlOmitHTTP;

// Omits the path if it is just a slash and there is no query or ref.  This is
// meaningful for non-file "standard" URLs.
NET_EXPORT extern const FormatUrlType kFormatUrlOmitTrailingSlashOnBareHostname;

// Convenience for omitting all unecessary types.
NET_EXPORT extern const FormatUrlType kFormatUrlOmitAll;

// Returns the number of explicitly allowed ports; for testing.
NET_EXPORT_PRIVATE extern size_t GetCountOfExplicitlyAllowedPorts();

// Given the full path to a file name, creates a file: URL. The returned URL
// may not be valid if the input is malformed.
NET_EXPORT GURL FilePathToFileURL(const FilePath& path);

// Converts a file: URL back to a filename that can be passed to the OS. The
// file URL must be well-formed (GURL::is_valid() must return true); we don't
// handle degenerate cases here. Returns true on success, false if it isn't a
// valid file URL. On failure, *file_path will be empty.
NET_EXPORT bool FileURLToFilePath(const GURL& url, FilePath* file_path);

// Splits an input of the form <host>[":"<port>] into its consitituent parts.
// Saves the result into |*host| and |*port|. If the input did not have
// the optional port, sets |*port| to -1.
// Returns true if the parsing was successful, false otherwise.
// The returned host is NOT canonicalized, and may be invalid. If <host> is
// an IPv6 literal address, the returned host includes the square brackets.
NET_EXPORT bool ParseHostAndPort(
    std::string::const_iterator host_and_port_begin,
    std::string::const_iterator host_and_port_end,
    std::string* host,
    int* port);
NET_EXPORT bool ParseHostAndPort(
    const std::string& host_and_port,
    std::string* host,
    int* port);

// Returns a host:port string for the given URL.
NET_EXPORT std::string GetHostAndPort(const GURL& url);

// Returns a host[:port] string for the given URL, where the port is omitted
// if it is the default for the URL's scheme.
NET_EXPORT_PRIVATE std::string GetHostAndOptionalPort(const GURL& url);

// Returns the string representation of an address, like "192.168.0.1".
// Returns empty string on failure.
NET_EXPORT std::string NetAddressToString(const struct addrinfo* net_address);
NET_EXPORT std::string NetAddressToString(const struct sockaddr* net_address,
                                          socklen_t address_len);

// Same as NetAddressToString, but additionally includes the port number. For
// example: "192.168.0.1:99" or "[::1]:80".
NET_EXPORT std::string NetAddressToStringWithPort(
    const struct addrinfo* net_address);
NET_EXPORT std::string NetAddressToStringWithPort(
    const struct sockaddr* net_address,
    socklen_t address_len);

// Returns the hostname of the current system. Returns empty string on failure.
NET_EXPORT std::string GetHostName();

// Extracts the unescaped username/password from |url|, saving the results
// into |*username| and |*password|.
NET_EXPORT_PRIVATE void GetIdentityFromURL(const GURL& url,
                        string16* username,
                        string16* password);

// Returns either the host from |url|, or, if the host is empty, the full spec.
NET_EXPORT std::string GetHostOrSpecFromURL(const GURL& url);

// Return the value of the HTTP response header with name 'name'.  'headers'
// should be in the format that URLRequest::GetResponseHeaders() returns.
// Returns the empty string if the header is not found.
NET_EXPORT std::string GetSpecificHeader(const std::string& headers,
                                         const std::string& name);

// Return the value of the HTTP response header field's parameter named
// 'param_name'.  Returns the empty string if the parameter is not found or is
// improperly formatted.
NET_EXPORT std::string GetHeaderParamValue(const std::string& header,
                                           const std::string& param_name,
                                           QuoteRule::Type quote_rule);

// Return the filename extracted from Content-Disposition header. The following
// formats are tried in order listed below:
//
// 1. RFC 5987
// 2. RFC 2047
// 3. Raw-8bit-characters :
//    a. UTF-8, b. referrer_charset, c. default os codepage.
// 4. %-escaped UTF-8.
//
// In step 3, if referrer_charset is empty(i.e. unknown), 3b is skipped.
// In step 4, the fallback charsets tried in step 3 are not tried. We
// can consider doing that later.
//
// When a param value is ASCII, but is not in format #2 or format #4 above,
// it is returned as it is unless it's pretty close to two supported
// formats but not well-formed. In that case, an empty string is returned.
//
// In any case, a caller must check for the empty return value and resort to
// another means to get a filename (e.g. url).
//
// This function does not do any escaping and callers are responsible for
// escaping 'unsafe' characters (e.g. (back)slash, colon) as they see fit.
NET_EXPORT_PRIVATE std::string GetFileNameFromCD(
    const std::string& header,
    const std::string& referrer_charset);

// Converts the given host name to unicode characters. This can be called for
// any host name, if the input is not IDN or is invalid in some way, we'll just
// return the ASCII source so it is still usable.
//
// The input should be the canonicalized ASCII host name from GURL. This
// function does NOT accept UTF-8!
//
// |languages| is a comma separated list of ISO 639 language codes. It
// is used to determine whether a hostname is 'comprehensible' to a user
// who understands languages listed. |host| will be converted to a
// human-readable form (Unicode) ONLY when each component of |host| is
// regarded as 'comprehensible'. Scipt-mixing is not allowed except that
// Latin letters in the ASCII range can be mixed with a limited set of
// script-language pairs (currently Han, Kana and Hangul for zh,ja and ko).
// When |languages| is empty, even that mixing is not allowed.
NET_EXPORT string16 IDNToUnicode(const std::string& host,
                                 const std::string& languages);

// Canonicalizes |host| and returns it.  Also fills |host_info| with
// IP address information.  |host_info| must not be NULL.
NET_EXPORT std::string CanonicalizeHost(const std::string& host,
                                        url_canon::CanonHostInfo* host_info);

// Returns true if |host| is not an IP address and is compliant with a set of
// rules based on RFC 1738 and tweaked to be compatible with the real world.
// The rules are:
//   * One or more components separated by '.'
//   * Each component begins with an alphanumeric character or '-'
//   * Each component contains only alphanumeric characters and '-' or '_'
//   * Each component ends with an alphanumeric character
//   * The last component begins with an alphabetic character
//   * Optional trailing dot after last component (means "treat as FQDN")
// If |desired_tld| is non-NULL, the host will only be considered invalid if
// appending it as a trailing component still results in an invalid host.  This
// helps us avoid marking as "invalid" user attempts to open "www.401k.com" by
// typing 4-0-1-k-<ctrl>+<enter>.
//
// NOTE: You should only pass in hosts that have been returned from
// CanonicalizeHost(), or you may not get accurate results.
NET_EXPORT bool IsCanonicalizedHostCompliant(const std::string& host,
                                             const std::string& desired_tld);

// Call these functions to get the html snippet for a directory listing.
// The return values of both functions are in UTF-8.
NET_EXPORT std::string GetDirectoryListingHeader(const string16& title);

// Given the name of a file in a directory (ftp or local) and
// other information (is_dir, size, modification time), it returns
// the html snippet to add the entry for the file to the directory listing.
// Currently, it's a script tag containing a call to a Javascript function
// |addRow|.
//
// |name| is the file name to be displayed. |raw_bytes| will be used
// as the actual target of the link (so for example, ftp links should use
// server's encoding). If |raw_bytes| is an empty string, UTF-8 encoded |name|
// will be used.
//
// Both |name| and |raw_bytes| are escaped internally.
NET_EXPORT std::string GetDirectoryListingEntry(const string16& name,
                                                const std::string& raw_bytes,
                                                bool is_dir, int64 size,
                                                base::Time modified);

// If text starts with "www." it is removed, otherwise text is returned
// unmodified.
NET_EXPORT string16 StripWWW(const string16& text);

// Generates a filename using the first successful method from the following (in
// order):
//
// 1) The raw Content-Disposition header in |content_disposition| (as read from
//    the network.  |referrer_charset| is used as described in the comment for
//    GetFileNameFromCD().
// 2) |suggested_name| if specified.  |suggested_name| is assumed to be in
//    UTF-8.
// 3) The filename extracted from the |url|.  |referrer_charset| will be used to
//    interpret the URL if there are non-ascii characters.
// 4) |default_name|.  If non-empty, |default_name| is assumed to be a filename
//    and shouldn't contain a path.  |default_name| is not subject to validation
//    or sanitization, and therefore shouldn't be a user supplied string.
// 5) The hostname portion from the |url|
//
// Then, leading and trailing '.'s will be removed.  On Windows, trailing spaces
// are also removed.  The string "download" is the final fallback if no filename
// is found or the filename is empty.
//
// Any illegal characters in the filename will be replaced by '-'.  If the
// filename doesn't contain an extension, and a |mime_type| is specified, the
// preferred extension for the |mime_type| will be appended to the filename.
// The resulting filename is then checked against a list of reserved names on
// Windows.  If the name is reserved, an underscore will be prepended to the
// filename.
//
// Note: |mime_type| should only be specified if this function is called from a
// thread that allows IO.
NET_EXPORT string16 GetSuggestedFilename(const GURL& url,
                                         const std::string& content_disposition,
                                         const std::string& referrer_charset,
                                         const std::string& suggested_name,
                                         const std::string& mime_type,
                                         const std::string& default_name);

// Similar to GetSuggestedFilename(), but returns a FilePath.
NET_EXPORT FilePath GenerateFileName(const GURL& url,
                                     const std::string& content_disposition,
                                     const std::string& referrer_charset,
                                     const std::string& suggested_name,
                                     const std::string& mime_type,
                                     const std::string& default_name);

// Ensures that the filename and extension is safe to use in the filesystem.
//
// Assumes that |file_path| already contains a valid path or file name.  On
// Windows if the extension causes the file to have an unsafe interaction with
// the shell (see net_util::IsShellIntegratedExtension()), then it will be
// replaced by the string 'download'.  If |file_path| doesn't contain an
// extension or |ignore_extension| is true then the preferred extension, if one
// exists, for |mime_type| will be used as the extension.
//
// On Windows, the filename will be checked against a set of reserved names, and
// if so, an underscore will be prepended to the name.
//
// |file_name| can either be just the file name or it can be a full path to a
// file.
//
// Note: |mime_type| should only be non-empty if this function is called from a
// thread that allows IO.
NET_EXPORT void GenerateSafeFileName(const std::string& mime_type,
                                     bool ignore_extension,
                                     FilePath* file_path);

// Checks |port| against a list of ports which are restricted by default.
// Returns true if |port| is allowed, false if it is restricted.
NET_EXPORT bool IsPortAllowedByDefault(int port);

// Checks |port| against a list of ports which are restricted by the FTP
// protocol.  Returns true if |port| is allowed, false if it is restricted.
bool IsPortAllowedByFtp(int port);

// Check if banned |port| has been overriden by an entry in
// |explicitly_allowed_ports_|.
bool IsPortAllowedByOverride(int port);

// Set socket to non-blocking mode
NET_EXPORT int SetNonBlocking(int fd);

// Formats the host in |url| and appends it to |output|.  The host formatter
// takes the same accept languages component as ElideURL().
NET_EXPORT void AppendFormattedHost(const GURL& url,
                                    const std::string& languages,
                                    string16* output);

// Creates a string representation of |url|. The IDN host name may be in Unicode
// if |languages| accepts the Unicode representation. |format_type| is a bitmask
// of FormatUrlTypes, see it for details. |unescape_rules| defines how to clean
// the URL for human readability. You will generally want |UnescapeRule::SPACES|
// for display to the user if you can handle spaces, or |UnescapeRule::NORMAL|
// if not. If the path part and the query part seem to be encoded in %-encoded
// UTF-8, decodes %-encoding and UTF-8.
//
// The last three parameters may be NULL.
// |new_parsed| will be set to the parsing parameters of the resultant URL.
// |prefix_end| will be the length before the hostname of the resultant URL.
//
// (|offset[s]_for_adjustment|) specifies one or more offsets into the original
// |url|'s spec(); each offset will be modified to reflect changes this function
// makes to the output string. For example, if |url| is "http://a:b@c.com/",
// |omit_username_password| is true, and an offset is 12 (the offset of '.'),
// then on return the output string will be "http://c.com/" and the offset will
// be 8.  If an offset cannot be successfully adjusted (e.g. because it points
// into the middle of a component that was entirely removed, past the end of the
// string, or into the middle of an encoding sequence), it will be set to
// string16::npos.
NET_EXPORT string16 FormatUrl(const GURL& url,
                              const std::string& languages,
                              FormatUrlTypes format_types,
                              UnescapeRule::Type unescape_rules,
                              url_parse::Parsed* new_parsed,
                              size_t* prefix_end,
                              size_t* offset_for_adjustment);
NET_EXPORT string16 FormatUrlWithOffsets(
    const GURL& url,
    const std::string& languages,
    FormatUrlTypes format_types,
    UnescapeRule::Type unescape_rules,
    url_parse::Parsed* new_parsed,
    size_t* prefix_end,
    std::vector<size_t>* offsets_for_adjustment);

// This is a convenience function for FormatUrl() with
// format_types = kFormatUrlOmitAll and unescape = SPACES.  This is the typical
// set of flags for "URLs to display to the user".  You should be cautious about
// using this for URLs which will be parsed or sent to other applications.
inline string16 FormatUrl(const GURL& url, const std::string& languages) {
  return FormatUrl(url, languages, kFormatUrlOmitAll, UnescapeRule::SPACES,
                   NULL, NULL, NULL);
}

// Returns whether FormatUrl() would strip a trailing slash from |url|, given a
// format flag including kFormatUrlOmitTrailingSlashOnBareHostname.
NET_EXPORT bool CanStripTrailingSlash(const GURL& url);

// Strip the portions of |url| that aren't core to the network request.
//   - user name / password
//   - reference section
NET_EXPORT_PRIVATE GURL SimplifyUrlForRequest(const GURL& url);

NET_EXPORT void SetExplicitlyAllowedPorts(const std::string& allowed_ports);

class NET_EXPORT ScopedPortException {
 public:
  ScopedPortException(int port);
  ~ScopedPortException();

 private:
  int port_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPortException);
};

// Perform a simplistic test to see if IPv6 is supported by trying to create an
// IPv6 socket.
// TODO(jar): Make test more in-depth as needed.
NET_EXPORT bool IPv6Supported();

// Returns true if it can determine that only loopback addresses are configured.
// i.e. if only 127.0.0.1 and ::1 are routable.
bool HaveOnlyLoopbackAddresses();

// IPAddressNumber is used to represent an IP address's numeric value as an
// array of bytes, from most significant to least significant. This is the
// network byte ordering.
//
// IPv4 addresses will have length 4, whereas IPv6 address will have length 16.
typedef std::vector<unsigned char> IPAddressNumber;
typedef std::vector<IPAddressNumber> IPAddressList;

static const size_t kIPv4AddressSize = 4;
static const size_t kIPv6AddressSize = 16;

// Parses an IP address literal (either IPv4 or IPv6) to its numeric value.
// Returns true on success and fills |ip_number| with the numeric value.
NET_EXPORT_PRIVATE bool ParseIPLiteralToNumber(const std::string& ip_literal,
                                               IPAddressNumber* ip_number);

// Converts an IPv4 address to an IPv4-mapped IPv6 address.
// For example 192.168.0.1 would be converted to ::ffff:192.168.0.1.
NET_EXPORT_PRIVATE IPAddressNumber ConvertIPv4NumberToIPv6Number(
    const IPAddressNumber& ipv4_number);

// Parses an IP block specifier from CIDR notation to an
// (IP address, prefix length) pair. Returns true on success and fills
// |*ip_number| with the numeric value of the IP address and sets
// |*prefix_length_in_bits| with the length of the prefix.
//
// CIDR notation literals can use either IPv4 or IPv6 literals. Some examples:
//
//    10.10.3.1/20
//    a:b:c::/46
//    ::1/128
NET_EXPORT bool ParseCIDRBlock(const std::string& cidr_literal,
                               IPAddressNumber* ip_number,
                               size_t* prefix_length_in_bits);

// Compares an IP address to see if it falls within the specified IP block.
// Returns true if it does, false otherwise.
//
// The IP block is given by (|ip_prefix|, |prefix_length_in_bits|) -- any
// IP address whose |prefix_length_in_bits| most significant bits match
// |ip_prefix| will be matched.
//
// In cases when an IPv4 address is being compared to an IPv6 address prefix
// and vice versa, the IPv4 addresses will be converted to IPv4-mapped
// (IPv6) addresses.
NET_EXPORT_PRIVATE bool IPNumberMatchesPrefix(const IPAddressNumber& ip_number,
                                              const IPAddressNumber& ip_prefix,
                                              size_t prefix_length_in_bits);

// Makes a copy of |info|. The dynamically-allocated parts are copied as well.
// If |recursive| is true, chained entries via ai_next are copied too.
// The copy returned by this function should be freed using
// FreeCopyOfAddrinfo(), and NOT freeaddrinfo().
struct addrinfo* CreateCopyOfAddrinfo(const struct addrinfo* info,
                                      bool recursive);

// Frees an addrinfo that was created by CreateCopyOfAddrinfo().
void FreeCopyOfAddrinfo(struct addrinfo* info);

// Returns the port field of the sockaddr in |info|.
const uint16* GetPortFieldFromAddrinfo(const struct addrinfo* info);
uint16* GetPortFieldFromAddrinfo(struct addrinfo* info);

// Returns the value of |info's| port (in host byte ordering).
uint16 GetPortFromAddrinfo(const struct addrinfo* info);

// Same except for struct sockaddr.
const uint16* GetPortFieldFromSockaddr(const struct sockaddr* address,
                                       socklen_t address_len);
NET_EXPORT_PRIVATE int GetPortFromSockaddr(const struct sockaddr* address,
                                           socklen_t address_len);

// Sets every addrinfo in the linked list |head| as having a port field of
// |port|.
NET_EXPORT_PRIVATE void SetPortForAllAddrinfos(struct addrinfo* head,
                                               uint16 port);

// Returns true if |host| is one of the names (e.g. "localhost") or IP
// addresses (IPv4 127.0.0.0/8 or IPv6 ::1) that indicate a loopback.
//
// Note that this function does not check for IP addresses other than
// the above, although other IP addresses may point to the local
// machine.
NET_EXPORT_PRIVATE bool IsLocalhost(const std::string& host);

// struct that is used by GetNetworkList() to represent a network
// interface.
struct NET_EXPORT NetworkInterface {
  NetworkInterface();
  NetworkInterface(const std::string& name, const IPAddressNumber& address);
  ~NetworkInterface();

  std::string name;
  IPAddressNumber address;
};

typedef std::vector<NetworkInterface> NetworkInterfaceList;

// Returns list of network interfaces except loopback interface. If an
// interface has more than one address, a separate entry is added to
// the list for each address.
// Can be called only on a thread that allows IO.
NET_EXPORT bool GetNetworkList(NetworkInterfaceList* networks);

}  // namespace net

#endif  // NET_BASE_NET_UTIL_H_
