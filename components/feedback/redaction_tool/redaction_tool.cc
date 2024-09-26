// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/redaction_tool/redaction_tool.h"

#include <set>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "build/chromeos_buildflags.h"
#include "components/feedback/redaction_tool/ip_address.h"
#include "components/feedback/redaction_tool/pii_types.h"
#ifdef USE_SYSTEM_RE2
#include <re2/re2.h>
#else
#include "third_party/re2/src/re2/re2.h"
#endif  // USE_SYSTEM_RE2

using re2::RE2;
using redaction_internal::IPAddress;

namespace redaction {

namespace {

// The |kCustomPatternsWithContext| array defines patterns to match and
// redact. Each pattern needs to define three capturing parentheses groups:
//
// - a group for the pattern before the identifier to be redacted;
// - a group for the identifier to be redacted;
// - a group for the pattern after the identifier to be redacted.
//
// The first and the last capture group are the origin of the "WithContext"
// suffix in the name of this constant.
//
// Every matched identifier (in the context of the whole pattern) is redacted
// by replacing it with an incremental instance identifier. Every different
// pattern defines a separate instance identifier space. See the unit test for
// RedactionToolTest::RedactCustomPatterns for pattern redaction examples.
//
// Useful regular expression syntax:
//
// +? is a non-greedy (lazy) +.
// \b matches a word boundary.
// (?i) turns on case insensitivity for the remainder of the regex.
// (?-s) turns off "dot matches newline" for the remainder of the regex.
// (?:regex) denotes non-capturing parentheses group.
CustomPatternWithAlias kCustomPatternsWithContext[] = {
    // ModemManager
    {"CellID", "(\\bCell ID: ')([0-9a-fA-F]+)(')", PIIType::kLocationInfo},
    {"LocAC", "(\\bLocation area code: ')([0-9a-fA-F]+)(')",
     PIIType::kLocationInfo},

    // Android. Must run first since this expression matches the replacement.
    //
    // If we don't get helpful delimiters like a single/double quote, then we
    // can only try our best and take out the next 32 characters, the max length
    // of a SSID. Require at least one non-quote character though so we skip
    // over the quoted SSIDs (which the following patterns will catch and
    // redact).
    {"SSID", "(?i-s)(\\bSSID: )([^'\"]{1,32})(.*)", PIIType::kSSID},
    // Replace any SSID inside quotes.
    {"SSID", "(?i-s)(\\bSSID: ['\"])(.+)(['\"])", PIIType::kSSID},
    // Special WifiNetworkSpecifier#toString.
    {"SSID", "(?i-s)(\\bSSID Match pattern=[^ ]*\\s?)(.+)(\\})",
     PIIType::kSSID},

    // wpa_supplicant
    {"SSID", "(?i-s)(\\bssid[= ]')(.+)(')", PIIType::kSSID},
    {"SSID", "(?i-s)(\\bssid[= ]\")(.+)(\")", PIIType::kSSID},
    {"SSID", "(\\* SSID=)(.+)($)", PIIType::kSSID},
    {"SSIDHex", "(?-s)(\\bSSID - hexdump\\(len=[0-9]+\\): )(.+)()",
     PIIType::kSSID},

    // shill
    {"SSID", "(?-s)(\\[SSID=)(.+?)(\\])", PIIType::kSSID},

    // Serial numbers. The actual serial number itself can include any alphanum
    // char as well as dashes, periods, colons, slashes and unprintable ASCII
    // chars (except newline). The second one is for a special case in
    // edid-decode, where if we genericized it further then we would catch too
    // many other cases that we don't want to redact.
    {"Serial",
     "(?i-s)(\\bserial\\s*_?(?:number)?['\"]?\\s*[:=|]\\s*['\"]?)"
     "([0-9a-zA-Z\\-.:\\/\\\\\\x00-\\x09\\x0B-\\x1F]+)(\\b)",
     PIIType::kSerial},
    {"Serial", "( Serial Number )(\\d+)(\\b)", PIIType::kSerial},
    // The attested device id, a serial number, that comes from vpd_2.0.txt.
    // The pattern was recently clarified as being a case insensitive string of
    // ASCII letters and digits, plus the dash/hyphen character. The dash cannot
    // appear first or last
    {"Serial", "(\"attested_device_id\"=\")([^-][0-9a-zA-Z-]+[^-])(\")",
     PIIType::kSerial},
    // PSM identifier is a 4-character brand code, which can be encoded as 8 hex
    // digits, followed by a slash ('/') and a serial number.
    {"PSM ID",
     "(?i)(PSM.*\\s+.*\\b)((?:[a-z]{4}|[0-9a-f]{8})\\/"
     "[0-9a-z\\-.:\\/\\\\\\x00-\\x09\\x0B-\\x1F]+)(\\b)",
     PIIType::kSerial},

    // GAIA IDs
    {"GAIA", R"xxx((\"?\bgaia_id\"?[=:]['\"])(\d+)(\b['\"]))xxx",
     PIIType::kGaiaID},
    {"GAIA", R"xxx((\{id: )(\d+)(, email:))xxx", PIIType::kGaiaID},

    // UUIDs given by the 'blkid' tool. These don't necessarily look like
    // standard UUIDs, so treat them specially.
    {"UUID", R"xxx((UUID=")([0-9a-zA-Z-]+)("))xxx", PIIType::kStableIdentifier},
    // Also cover UUIDs given by the 'lvs' and 'pvs' tools, which similarly
    // don't necessarily look like standard UUIDs.
    {"UUID", R"xxx(("[lp]v_uuid":")([0-9a-zA-Z-]+)("))xxx",
     PIIType::kStableIdentifier},

    // Volume labels presented in the 'blkid' tool, and as part of removable
    // media paths shown in various logs such as cros-disks (in syslog).
    // There isn't a well-defined format for these. For labels in blkid,
    // capture everything between the open and closing quote.
    {"Volume Label", R"xxx((LABEL=")([^"]+)("))xxx", PIIType::kVolumeLabel},
    // For paths, this is harder. The only restricted characters are '/' and
    // NUL, so use a simple heuristic. cros-disks generally quotes paths using
    // single-quotes, so capture everything until a quote character. For lsblk,
    // capture everything until the end of the line, since the mount path is the
    // last field.
    {"Volume Label", R"xxx((/media/removable/)(.+?)(['"/\n]|$))xxx",
     PIIType::kVolumeLabel},

    // IPP (Internet Printing Protocol) Addresses
    {"IPP Address", R"xxx((ipp:\/\/)(.+?)(\/ipp))xxx", PIIType::kIPPAddress},
};

bool MaybeUnmapAddress(IPAddress* addr) {
  if (!addr->IsIPv4MappedIPv6()) {
    return false;
  }

  *addr = ConvertIPv4MappedIPv6ToIPv4(*addr);
  return true;
}

bool MaybeUntranslateAddress(IPAddress* addr) {
  if (!addr->IsIPv6()) {
    return false;
  }

  static const IPAddress kTranslated6To4(0, 0x64, 0xff, 0x9b, 0, 0, 0, 0, 0, 0,
                                         0, 0, 0, 0, 0, 0);
  if (!IPAddressMatchesPrefix(*addr, kTranslated6To4, 96)) {
    return false;
  }

  const auto bytes = addr->bytes();
  *addr = IPAddress(bytes[12], bytes[13], bytes[14], bytes[15]);
  return true;
}

// If |addr| points to a valid IPv6 address, this function truncates it at /32.
bool MaybeTruncateIPv6(IPAddress* addr) {
  if (!addr->IsIPv6()) {
    return false;
  }

  const auto bytes = addr->bytes();
  *addr = IPAddress(bytes[0], bytes[1], bytes[2], bytes[3], 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0);
  return true;
}

// Returns an appropriately scrubbed version of |addr| if applicable.
std::string MaybeScrubIPAddress(const std::string& addr) {
  struct {
    IPAddress ip_addr;
    int prefix_length;
    bool scrub;
  } static const kNonIdentifyingIPRanges[] = {
      // Private.
      {IPAddress(10, 0, 0, 0), 8, true},
      {IPAddress(172, 16, 0, 0), 12, true},
      {IPAddress(192, 168, 0, 0), 16, true},
      // Chrome OS containers and VMs.
      {IPAddress(100, 115, 92, 0), 24, false},
      // Loopback.
      {IPAddress(127, 0, 0, 0), 8, true},
      // Any.
      {IPAddress(0, 0, 0, 0), 8, true},
      // DNS.
      {IPAddress(8, 8, 8, 8), 32, false},
      {IPAddress(8, 8, 4, 4), 32, false},
      {IPAddress(1, 1, 1, 1), 32, false},
      // Multicast.
      {IPAddress(224, 0, 0, 0), 4, true},
      // Link local.
      {IPAddress(169, 254, 0, 0), 16, true},
      {IPAddress(0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 10,
       true},
      // Broadcast.
      {IPAddress(255, 255, 255, 255), 32, false},
      // IPv6 loopback, unspecified and non-address strings.
      {IPAddress::IPv6AllZeros(), 112, false},
      // IPv6 multicast all nodes and routers.
      {IPAddress(0xff, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1), 128,
       false},
      {IPAddress(0xff, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2), 128,
       false},
      {IPAddress(0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1), 128,
       false},
      {IPAddress(0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2), 128,
       false},
      // IPv6 other multicast (link and interface local).
      {IPAddress(0xff, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 16,
       true},
      {IPAddress(0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 16,
       true},

  };
  IPAddress input_addr;
  if (input_addr.AssignFromIPLiteral(addr) && input_addr.IsValid()) {
    bool mapped = MaybeUnmapAddress(&input_addr);
    bool translated = !mapped ? MaybeUntranslateAddress(&input_addr) : false;
    for (const auto& range : kNonIdentifyingIPRanges) {
      if (IPAddressMatchesPrefix(input_addr, range.ip_addr,
                                 range.prefix_length)) {
        std::string prefix;
        std::string out_addr = addr;
        if (mapped) {
          prefix = "M ";
          out_addr = input_addr.ToString();
        } else if (translated) {
          prefix = "T ";
          out_addr = input_addr.ToString();
        }
        if (range.scrub) {
          out_addr = base::StringPrintf(
              "%s/%d", range.ip_addr.ToString().c_str(), range.prefix_length);
        }
        return base::StrCat({prefix, out_addr});
      }
    }
    // |addr| may have been over-aggressively matched as an IPv6 address when
    // it's really just an arbitrary part of a sentence. If the string is the
    // same as the coarsely truncated address then keep it because even if
    // it happens to be a real address, there is no leak.
    if (MaybeTruncateIPv6(&input_addr) && input_addr.ToString() == addr) {
      return addr;
    }
  }
  return "";
}

// Some strings can contain pieces that match like IPv4 addresses but aren't.
// This function can be used to determine if this was the case by evaluating
// the skipped piece. It returns true, if the matched address was erroneous
// and should be skipped instead.
bool ShouldSkipIPAddress(const re2::StringPiece& skipped) {
  // MomdemManager can dump out firmware revision fields that can also
  // confuse the IPv4 matcher e.g. "Revision: 81600.0000.00.29.19.16_DO"
  // so ignore the replacement if the skipped piece looks like
  // "Revision: .*<ipv4>". Note however that if this field contains
  // values delimited by multiple spaces, any matches after the first
  // will lose the context and be redacted.
  static const re2::StringPiece rev("Revision: ");
  static const re2::StringPiece space(" ");
  const auto pos = skipped.rfind(rev);
  if (pos != re2::StringPiece::npos &&
      skipped.find(space, pos + rev.length()) == re2::StringPiece::npos) {
    return true;
  }

  // USB paths can be confused with IPv4 Addresses because they can look
  // similar: n-n.n.n.n . Ignore replacement if previous char is `-`
  static const re2::StringPiece dash("-");
  return skipped.ends_with(dash);
}

// Helper macro: Non capturing group
#define NCG(x) "(?:" x ")"
// Helper macro: Optional non capturing group
#define OPT_NCG(x) NCG(x) "?"

//////////////////////////////////////////////////////////////////////////
// Patterns for URLs, or better IRIs, based on RFC 3987 with an artificial
// limitation on the scheme to increase precision. Otherwise anything
// like "ID:" would be considered an IRI.

#define UNRESERVED "[-a-z0-9._~]"
#define RESERVED NGC(GEN_DELIMS "|" SUB_DELIMS)
#define SUB_DELIMS "[!$&'()*+,;=]"
#define GEN_DELIMS "[:/?#[\\]@]"

#define DIGIT "[0-9]"
#define HEXDIG "[0-9a-f]"

#define PCT_ENCODED "%" HEXDIG HEXDIG

#define DEC_OCTET NCG("1[0-9][0-9]|2[0-4][0-9]|25[0-5]|[1-9][0-9]|[0-9]")

#define IPV4ADDRESS DEC_OCTET "\\." DEC_OCTET "\\." DEC_OCTET "\\." DEC_OCTET

#define H16 NCG(HEXDIG) "{1,4}"
#define LS32 NCG(H16 ":" H16 "|" IPV4ADDRESS)
#define WB "\\b"

// clang-format off
#define IPV6ADDRESS NCG( \
                                          WB NCG(H16 ":") "{6}" LS32 WB "|" \
                                        "::" NCG(H16 ":") "{5}" LS32 WB "|" \
  OPT_NCG( WB                      H16) "::" NCG(H16 ":") "{4}" LS32 WB "|" \
  OPT_NCG( WB NCG(H16 ":") "{0,1}" H16) "::" NCG(H16 ":") "{3}" LS32 WB "|" \
  OPT_NCG( WB NCG(H16 ":") "{0,2}" H16) "::" NCG(H16 ":") "{2}" LS32 WB "|" \
  OPT_NCG( WB NCG(H16 ":") "{0,3}" H16) "::" NCG(H16 ":")       LS32 WB "|" \
  OPT_NCG( WB NCG(H16 ":") "{0,4}" H16) "::"                    LS32 WB "|" \
  OPT_NCG( WB NCG(H16 ":") "{0,5}" H16) "::"                    H16  WB "|" \
  OPT_NCG( WB NCG(H16 ":") "{0,6}" H16) "::")
// clang-format on

#define IPVFUTURE                     \
  "v" HEXDIG                          \
  "+"                                 \
  "\\." NCG(UNRESERVED "|" SUB_DELIMS \
                       "|"            \
                       ":") "+"

#define IP_LITERAL "\\[" NCG(IPV6ADDRESS "|" IPVFUTURE) "\\]"

#define PORT DIGIT "*"

// This is a diversion of RFC 3987
#define SCHEME \
  NCG("http|https|ftp|chrome|chrome-extension|android|rtsp|file|isolated-app")

#define IPRIVATE            \
  "["                       \
  "\\x{E000}-\\x{F8FF}"     \
  "\\x{F0000}-\\x{FFFFD}"   \
  "\\x{100000}-\\x{10FFFD}" \
  "]"

#define UCSCHAR           \
  "["                     \
  "\\x{A0}-\\x{D7FF}"     \
  "\\x{F900}-\\x{FDCF}"   \
  "\\x{FDF0}-\\x{FFEF}"   \
  "\\x{10000}-\\x{1FFFD}" \
  "\\x{20000}-\\x{2FFFD}" \
  "\\x{30000}-\\x{3FFFD}" \
  "\\x{40000}-\\x{4FFFD}" \
  "\\x{50000}-\\x{5FFFD}" \
  "\\x{60000}-\\x{6FFFD}" \
  "\\x{70000}-\\x{7FFFD}" \
  "\\x{80000}-\\x{8FFFD}" \
  "\\x{90000}-\\x{9FFFD}" \
  "\\x{A0000}-\\x{AFFFD}" \
  "\\x{B0000}-\\x{BFFFD}" \
  "\\x{C0000}-\\x{CFFFD}" \
  "\\x{D0000}-\\x{DFFFD}" \
  "\\x{E1000}-\\x{EFFFD}" \
  "]"

#define IUNRESERVED  \
  NCG("[-a-z0-9._~]" \
      "|" UCSCHAR)

#define IPCHAR                                   \
  NCG(IUNRESERVED "|" PCT_ENCODED "|" SUB_DELIMS \
                  "|"                            \
                  "[:@]")
#define IFRAGMENT \
  NCG(IPCHAR      \
      "|"         \
      "[/?]")     \
  "*"
#define IQUERY            \
  NCG(IPCHAR "|" IPRIVATE \
             "|"          \
             "[/?]")      \
  "*"

#define ISEGMENT IPCHAR "*"
#define ISEGMENT_NZ IPCHAR "+"
#define ISEGMENT_NZ_NC                           \
  NCG(IUNRESERVED "|" PCT_ENCODED "|" SUB_DELIMS \
                  "|"                            \
                  "@")                           \
  "+"

#define IPATH_EMPTY ""
#define IPATH_ROOTLESS ISEGMENT_NZ NCG("/" ISEGMENT) "*"
#define IPATH_NOSCHEME ISEGMENT_NZ_NC NCG("/" ISEGMENT) "*"
#define IPATH_ABSOLUTE "/" OPT_NCG(ISEGMENT_NZ NCG("/" ISEGMENT) "*")
#define IPATH_ABEMPTY NCG("/" ISEGMENT) "*"

#define IPATH                                                                \
  NCG(IPATH_ABEMPTY "|" IPATH_ABSOLUTE "|" IPATH_NOSCHEME "|" IPATH_ROOTLESS \
                    "|" IPATH_EMPTY)

#define IREG_NAME NCG(IUNRESERVED "|" PCT_ENCODED "|" SUB_DELIMS) "*"

#define IHOST NCG(IP_LITERAL "|" IPV4ADDRESS "|" IREG_NAME)
#define IUSERINFO                                \
  NCG(IUNRESERVED "|" PCT_ENCODED "|" SUB_DELIMS \
                  "|"                            \
                  ":")                           \
  "*"
#define IAUTHORITY OPT_NCG(IUSERINFO "@") IHOST OPT_NCG(":" PORT)

#define IRELATIVE_PART                                                    \
  NCG("//" IAUTHORITY IPATH_ABEMPTY "|" IPATH_ABSOLUTE "|" IPATH_NOSCHEME \
      "|" IPATH_EMPTY)

#define IRELATIVE_REF IRELATIVE_PART OPT_NCG("?" IQUERY) OPT_NCG("#" IFRAGMENT)

// RFC 3987 requires IPATH_EMPTY here but it is omitted so that statements
// that end with "Android:" for example are not considered a URL.
#define IHIER_PART \
  NCG("//" IAUTHORITY IPATH_ABEMPTY "|" IPATH_ABSOLUTE "|" IPATH_ROOTLESS)

#define ABSOLUTE_IRI SCHEME ":" IHIER_PART OPT_NCG("?" IQUERY)

#define IRI SCHEME ":" IHIER_PART OPT_NCG("\\?" IQUERY) OPT_NCG("#" IFRAGMENT)

#define IRI_REFERENCE NCG(IRI "|" IRELATIVE_REF)

// TODO(battre): Use http://tools.ietf.org/html/rfc5322 to represent email
// addresses. Capture names as well ("First Lastname" <foo@bar.com>).

// The |kCustomPatternWithoutContext| array defines further patterns to match
// and redact. Each pattern consists of a single capturing group.
CustomPatternWithAlias kCustomPatternsWithoutContext[] = {
    {"URL", "(?i)(" IRI ")", PIIType::kURL},
    // Email Addresses need to come after URLs because they can be part
    // of a query parameter.
    {"email", "(?i)([0-9a-z._%+-]+@[a-z0-9.-]+\\.[a-z]{2,6})", PIIType::kEmail},
    // IP filter rules need to come after URLs so that they don't disturb the
    // URL pattern in case the IP address is part of a URL.
    {"IPv4", "(?i)(" IPV4ADDRESS ")", PIIType::kIPAddress},
    {"IPv6", "(?i)(" IPV6ADDRESS ")", PIIType::kIPAddress},
    // Universal Unique Identifiers (UUIDs).
    {"UUID",
     "(?i)([0-9a-zA-Z]{8}-[0-9a-zA-Z]{4}-[0-9a-zA-Z]{4}-[0-9a-zA-Z]{4}-"
     "[0-9a-zA-Z]{12})",
     PIIType::kStableIdentifier},
    // Eche UID which is a base64 conversion of a 32 bytes public key.
    {"UID",
     "((?:[A-Za-z0-9+/]{4}){10}(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=))",
     PIIType::kStableIdentifier},
};

// Like RE2's FindAndConsume, searches for the first occurrence of |pattern| in
// |input| and consumes the bytes until the end of the pattern matching. Unlike
// FindAndConsume, the bytes skipped before the match of |pattern| are stored
// in |skipped_input|. |args| needs to contain at least one element.
// Returns whether a match was found.
//
// Example: input = "aaabbbc", pattern = "(b+)" leads to skipped_input = "aaa",
// args[0] = "bbb", and the beginning input is moved to the right so that it
// only contains "c".
// Example: input = "aaabbbc", pattern = "(z+)" leads to input = "aaabbbc",
// the args values are not modified and skipped_input is not modified.
bool FindAndConsumeAndGetSkippedN(re2::StringPiece* input,
                                  const re2::RE2& pattern,
                                  re2::StringPiece* skipped_input,
                                  re2::StringPiece* args[],
                                  int argc) {
  re2::StringPiece old_input = *input;

  CHECK_GE(argc, 1);
  re2::RE2::Arg a0(argc > 0 ? args[0] : nullptr);
  re2::RE2::Arg a1(argc > 1 ? args[1] : nullptr);
  re2::RE2::Arg a2(argc > 2 ? args[2] : nullptr);
  re2::RE2::Arg a3(argc > 3 ? args[3] : nullptr);
  const re2::RE2::Arg* const wrapped_args[] = {&a0, &a1, &a2, &a3};
  CHECK_LE(argc, 4);

  bool result = re2::RE2::FindAndConsumeN(input, pattern, wrapped_args, argc);

  if (skipped_input && result) {
    size_t bytes_skipped = args[0]->data() - old_input.data();
    *skipped_input = re2::StringPiece(old_input.data(), bytes_skipped);
  }
  return result;
}

// All |match_groups| need to be of type re2::StringPiece*.
template <typename... Arg>
bool FindAndConsumeAndGetSkipped(re2::StringPiece* input,
                                 const re2::RE2& pattern,
                                 re2::StringPiece* skipped_input,
                                 Arg*... match_groups) {
  re2::StringPiece* args[] = {match_groups...};
  return FindAndConsumeAndGetSkippedN(input, pattern, skipped_input, args,
                                      std::size(args));
}

// The following MAC addresses will not be redacted as they are not specific
// to a device but have general meanings.
const char* const kUnredactedMacAddresses[] = {
    "00:00:00:00:00:00",  // ARP failure result MAC.
    "ff:ff:ff:ff:ff:ff",  // Broadcast MAC.
};
constexpr size_t kNumUnredactedMacs = std::size(kUnredactedMacAddresses);

constexpr char kFeedbackRedactionToolHistogramName[] = "Feedback.RedactionTool";

}  // namespace

RedactionTool::RedactionTool(const char* const* first_party_extension_ids)
    : first_party_extension_ids_(first_party_extension_ids) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // Identity-map these, so we don't mangle them.
  for (const char* mac : kUnredactedMacAddresses) {
    mac_addresses_[mac] = mac;
  }
}

RedactionTool::~RedactionTool() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::map<PIIType, std::set<std::string>> RedactionTool::Detect(
    const std::string& input) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AssertLongCPUWorkAllowed();

  std::map<PIIType, std::set<std::string>> detected;

  RedactMACAddresses(input, &detected);
  // This function will add to |detected| only on Chrome OS as Android app
  // storage paths are only detected for Chrome OS.
  RedactAndroidAppStoragePaths(input, &detected);
  DetectWithCustomPatterns(input, &detected);
  // Do hashes last since they may appear in URLs and they also prevent us from
  // properly recognizing the Android storage paths.
  RedactHashes(input, &detected);
  return detected;
}

std::string RedactionTool::Redact(const std::string& input) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return RedactAndKeepSelected(input, /*pii_types_to_keep=*/{});
}

std::string RedactionTool::RedactAndKeepSelected(
    const std::string& input,
    const std::set<PIIType>& pii_types_to_keep) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AssertLongCPUWorkAllowed();

  // Copy |input| so we can modify it.
  std::string redacted = input;

  if (pii_types_to_keep.find(PIIType::kMACAddress) == pii_types_to_keep.end()) {
    redacted = RedactMACAddresses(std::move(redacted), nullptr);
  }
  if (pii_types_to_keep.find(PIIType::kAndroidAppStoragePath) ==
      pii_types_to_keep.end()) {
    redacted = RedactAndroidAppStoragePaths(std::move(redacted), nullptr);
  }

  redacted = RedactAndKeepSelectedCustomPatterns(std::move(redacted),
                                                 pii_types_to_keep);

  // Do hashes last since they may appear in URLs and they also prevent us
  // from properly recognizing the Android storage paths.
  if (pii_types_to_keep.find(PIIType::kStableIdentifier) ==
      pii_types_to_keep.end()) {
    // URLs and Android storage paths will be partially redacted (only hashes)
    // if |pii_types_to_keep| contains PIIType::kURL or
    // PIIType::kAndroidAppStoragePath and not PIIType::kStableIdentifier.
    redacted = RedactHashes(std::move(redacted), nullptr);
  }
  return redacted;
}

RE2* RedactionTool::GetRegExp(const std::string& pattern) {
  if (regexp_cache_.find(pattern) == regexp_cache_.end()) {
    RE2::Options options;
    // set_multiline of pcre is not supported by RE2, yet.
    options.set_dot_nl(true);  // Dot matches a new line.
    std::unique_ptr<RE2> re = std::make_unique<RE2>(pattern, options);
    DCHECK_EQ(re2::RE2::NoError, re->error_code()) << "Failed to parse:\n"
                                                   << pattern << "\n"
                                                   << re->error();
    regexp_cache_[pattern] = std::move(re);
  }
  return regexp_cache_[pattern].get();
}

std::string RedactionTool::RedactMACAddresses(
    const std::string& input,
    std::map<PIIType, std::set<std::string>>* detected) {
  // This regular expression finds the next MAC address. It splits the data into
  // an OUI (Organizationally Unique Identifier) part and a NIC (Network
  // Interface Controller) specific part. We also match on dash and underscore
  // because we have seen instances of both of those occurring.

  RE2* mac_re = GetRegExp(
      "([0-9a-fA-F][0-9a-fA-F][:\\-_]"
      "[0-9a-fA-F][0-9a-fA-F][:\\-_]"
      "[0-9a-fA-F][0-9a-fA-F])[:\\-_]("
      "[0-9a-fA-F][0-9a-fA-F][:\\-_]"
      "[0-9a-fA-F][0-9a-fA-F][:\\-_]"
      "[0-9a-fA-F][0-9a-fA-F])");

  std::string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  re2::StringPiece text(input);
  re2::StringPiece skipped, oui, nic;
  static const char kMacSeparatorChars[] = "-_";
  while (FindAndConsumeAndGetSkipped(&text, *mac_re, &skipped, &oui, &nic)) {
    // Look up the MAC address in the hash. Force the separator to be a colon
    // so that the same MAC with a different format will match in all cases.
    std::string oui_string = base::ToLowerASCII(std::string(oui));
    base::ReplaceChars(oui_string, kMacSeparatorChars, ":", &oui_string);
    std::string nic_string = base::ToLowerASCII(std::string(nic));
    base::ReplaceChars(nic_string, kMacSeparatorChars, ":", &nic_string);
    std::string mac = oui_string + ":" + nic_string;
    std::string replacement_mac = mac_addresses_[mac];
    if (replacement_mac.empty()) {
      // If not found, build up a replacement MAC address by generating a new
      // NIC part.
      int mac_id = mac_addresses_.size() - kNumUnredactedMacs;
      replacement_mac = base::StringPrintf("(MAC OUI=%s IFACE=%d)",
                                           oui_string.c_str(), mac_id);
      mac_addresses_[mac] = replacement_mac;
    }
    if (detected != nullptr) {
      (*detected)[PIIType::kMACAddress].insert(mac);
    }
    skipped.AppendToString(&result);
    result += replacement_mac;
  }

  text.AppendToString(&result);

  UMA_HISTOGRAM_ENUMERATION(kFeedbackRedactionToolHistogramName,
                            PIIType::kMACAddress);

  return result;
}

std::string RedactionTool::RedactHashes(
    const std::string& input,
    std::map<PIIType, std::set<std::string>>* detected) {
  // This will match hexadecimal strings from length 32 to 64 that have a word
  // boundary at each end. We then check to make sure they are one of our valid
  // hash lengths before replacing.
  // NOTE: There are some occurrences in the dump data (specifically modetest)
  // where relevant data is formatted with 32 hex chars on a line. In this case,
  // it is preceded by at least 3 whitespace chars, so check for that and in
  // that case do not redact.
  RE2* hash_re = GetRegExp(R"((\s*)\b([0-9a-fA-F]{4})([0-9a-fA-F]{28,60})\b)");

  std::string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  re2::StringPiece text(input);
  re2::StringPiece skipped, pre_whitespace, hash_prefix, hash_suffix;
  while (FindAndConsumeAndGetSkipped(&text, *hash_re, &skipped, &pre_whitespace,
                                     &hash_prefix, &hash_suffix)) {
    skipped.AppendToString(&result);
    pre_whitespace.AppendToString(&result);

    // Check if it's a valid length for our hashes or if we need to skip due to
    // the whitespace check.
    size_t hash_length = 4 + hash_suffix.length();
    if ((hash_length != 32 && hash_length != 40 && hash_length != 64) ||
        (hash_length == 32 && pre_whitespace.length() >= 3)) {
      // This is not a hash string, skip it.
      hash_prefix.AppendToString(&result);
      hash_suffix.AppendToString(&result);
      continue;
    }

    // Look up the hash value address in the map of replacements.
    std::string hash_prefix_string =
        base::ToLowerASCII(std::string(hash_prefix));
    std::string hash =
        hash_prefix_string + base::ToLowerASCII(std::string(hash_suffix));
    std::string replacement_hash = hashes_[hash];
    if (replacement_hash.empty()) {
      // If not found, build up a replacement value.
      replacement_hash = base::StringPrintf(
          "(HASH:%s %zd)", hash_prefix_string.c_str(), hashes_.size());
      hashes_[hash] = replacement_hash;
    }
    if (detected != nullptr) {
      (*detected)[PIIType::kStableIdentifier].insert(hash);
    }

    result += replacement_hash;
  }

  text.AppendToString(&result);

  UMA_HISTOGRAM_ENUMERATION(kFeedbackRedactionToolHistogramName,
                            PIIType::kStableIdentifier);

  return result;
}

std::string RedactionTool::RedactAndroidAppStoragePaths(
    const std::string& input,
    std::map<PIIType, std::set<std::string>>* detected) {
  // We only use this on Chrome OS and there's differences in the API for
  // FilePath on Windows which prevents this from compiling, so only enable this
  // code for Chrome OS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string result;
  result.reserve(input.size());

  // This is for redacting Android data paths included in 'android_app_storage'
  // and 'audit_log' output. <app_specific_path> in the following data paths
  // will be redacted.
  // - /data/data/<package_name>/<app_specific_path>
  // - /data/app/<package_name>/<app_specific_path>
  // - /data/user_de/<number>/<package_name>/<app_specific_path>
  // These data paths are preceded by "/home/root/<user_hash>/android-data" in
  // 'android_app_storage' output, and preceded by "path=" or "exe=" in
  // 'audit_log' output.
  RE2* path_re =
      GetRegExp(R"((?m)((path=|exe=|/home/root/[\da-f]+/android-data))"
                R"(/data/(data|app|user_de/\d+)/[^/\n]+)(/[^\n\s]+))");

  // Keep consuming, building up a result string as we go.
  re2::StringPiece text(input);
  re2::StringPiece skipped;
  re2::StringPiece path_prefix;   // path before app_specific;
  re2::StringPiece pre_data;      // (path=|exe=|/home/root/<hash>/android-data)
  re2::StringPiece post_data;     // (data|app|user_de/\d+)
  re2::StringPiece app_specific;  // (/[^\n\s]+)
  while (FindAndConsumeAndGetSkipped(&text, *path_re, &skipped, &path_prefix,
                                     &pre_data, &post_data, &app_specific)) {
    // We can record these parts as-is.
    skipped.AppendToString(&result);
    path_prefix.AppendToString(&result);

    // |app_specific| has to be redacted. First, convert it into components,
    // and then redact each component as follows:
    // - If the component has a non-ASCII character, change it to '*'.
    // - Otherwise, remove all the characters in the component but the first
    //   one.
    // - If the original component has 2 or more bytes, add '_'.
    const base::FilePath path(app_specific.as_string());
    std::vector<std::string> components = path.GetComponents();
    DCHECK(!components.empty());

    auto it = components.begin() + 1;  // ignore the leading slash
    for (; it != components.end(); ++it) {
      const auto& component = *it;
      DCHECK(!component.empty());
      result += '/';
      result += (base::IsStringASCII(component) ? component[0] : '*');
      if (component.length() > 1) {
        result += '_';
      }
    }
    if (detected != nullptr) {
      (*detected)[PIIType::kAndroidAppStoragePath].insert(
          app_specific.as_string());
    }
  }

  text.AppendToString(&result);

  UMA_HISTOGRAM_ENUMERATION(kFeedbackRedactionToolHistogramName,
                            PIIType::kAndroidAppStoragePath);

  return result;
#else
  return input;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

std::string RedactionTool::RedactAndKeepSelectedCustomPatterns(
    std::string input,
    const std::set<PIIType>& pii_types_to_keep) {
  for (const auto& pattern : kCustomPatternsWithContext) {
    if (pii_types_to_keep.find(pattern.pii_type) == pii_types_to_keep.end()) {
      input = RedactCustomPatternWithContext(input, pattern, nullptr);
    }
  }
  for (const auto& pattern : kCustomPatternsWithoutContext) {
    if (pii_types_to_keep.find(pattern.pii_type) == pii_types_to_keep.end()) {
      input = RedactCustomPatternWithoutContext(input, pattern, nullptr);
    }
  }
  return input;
}

void RedactionTool::DetectWithCustomPatterns(
    std::string input,
    std::map<PIIType, std::set<std::string>>* detected) {
  for (const auto& pattern : kCustomPatternsWithContext) {
    RedactCustomPatternWithContext(input, pattern, detected);
  }
  for (const auto& pattern : kCustomPatternsWithoutContext) {
    RedactCustomPatternWithoutContext(input, pattern, detected);
  }
}

std::string RedactionTool::RedactCustomPatternWithContext(
    const std::string& input,
    const CustomPatternWithAlias& pattern,
    std::map<PIIType, std::set<std::string>>* detected) {
  RE2* re = GetRegExp(pattern.pattern);
  DCHECK_EQ(3, re->NumberOfCapturingGroups());
  std::map<std::string, std::string>* identifier_space =
      &custom_patterns_with_context_[pattern.alias];

  std::string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  re2::StringPiece text(input);
  re2::StringPiece skipped;
  re2::StringPiece pre_match, pre_matched_id, matched_id, post_matched_id;
  while (FindAndConsumeAndGetSkipped(&text, *re, &skipped, &pre_matched_id,
                                     &matched_id, &post_matched_id)) {
    std::string matched_id_as_string(matched_id);
    std::string replacement_id;
    if (identifier_space->count(matched_id_as_string) == 0) {
      // The weird NumberToString trick is because Windows does not like
      // to deal with %zu and a size_t in printf, nor does it support %llu.
      replacement_id = base::StringPrintf(
          "(%s: %s)", pattern.alias,
          base::NumberToString(identifier_space->size() + 1).c_str());
      (*identifier_space)[matched_id_as_string] = replacement_id;
    } else {
      replacement_id = (*identifier_space)[matched_id_as_string];
    }
    if (detected != nullptr) {
      (*detected)[pattern.pii_type].insert(matched_id_as_string);
    }
    skipped.AppendToString(&result);
    pre_matched_id.AppendToString(&result);
    result += replacement_id;
    post_matched_id.AppendToString(&result);
  }
  text.AppendToString(&result);

  UMA_HISTOGRAM_ENUMERATION(kFeedbackRedactionToolHistogramName,
                            pattern.pii_type);

  return result;
}

// This takes a |url| argument and returns true if the URL is exempt from
// redaction, returns false otherwise.
bool IsUrlExempt(re2::StringPiece url,
                 const char* const* first_party_extension_ids) {
  // We do not exempt anything with a query parameter.
  if (url.contains("?")) {
    return false;
  }

  // Last part of an SELinux context is misdetected as a URL.
  // e.g. "u:object_r:system_data_file:s0:c512,c768"
  if (url.starts_with("file:s0")) {
    return true;
  }

  // Check for chrome:// URLs that are exempt.
  if (url.starts_with("chrome://")) {
    // We allow everything in chrome://resources/.
    if (url.starts_with("chrome://resources/")) {
      return true;
    }

    // We allow chrome://*/crisper.js.
    if (url.ends_with("/crisper.js")) {
      return true;
    }

    return false;
  }

  if (!first_party_extension_ids) {
    return false;
  }

  // Exempt URLs of the format chrome-extension://<first-party-id>/*.js
  if (!url.starts_with("chrome-extension://")) {
    return false;
  }

  // These must end with a .js extension.
  if (!url.ends_with(".js")) {
    return false;
  }

  int i = 0;
  const char* test_id = first_party_extension_ids[i];
  const re2::StringPiece url_sub =
      url.substr(sizeof("chrome-extension://") - 1);
  while (test_id) {
    if (url_sub.starts_with(test_id)) {
      return true;
    }
    test_id = first_party_extension_ids[++i];
  }
  return false;
}

std::string RedactionTool::RedactCustomPatternWithoutContext(
    const std::string& input,
    const CustomPatternWithAlias& pattern,
    std::map<PIIType, std::set<std::string>>* detected) {
  RE2* re = GetRegExp(pattern.pattern);
  DCHECK_EQ(1, re->NumberOfCapturingGroups());

  std::map<std::string, std::string>* identifier_space =
      &custom_patterns_without_context_[pattern.alias];

  std::string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  re2::StringPiece text(input);
  re2::StringPiece skipped;
  re2::StringPiece matched_id;
  while (FindAndConsumeAndGetSkipped(&text, *re, &skipped, &matched_id)) {
    if (IsUrlExempt(matched_id, first_party_extension_ids_)) {
      skipped.AppendToString(&result);
      matched_id.AppendToString(&result);
      continue;
    }
    std::string matched_id_as_string(matched_id);
    std::string replacement_id;
    if (identifier_space->count(matched_id_as_string) == 0) {
      replacement_id = MaybeScrubIPAddress(matched_id_as_string);
      if (replacement_id != matched_id_as_string) {
        // Double-check overly opportunistic IPv4 address matching.
        if ((strcmp("IPv4", pattern.alias) == 0) &&
            ShouldSkipIPAddress(skipped)) {
          skipped.AppendToString(&result);
          matched_id.AppendToString(&result);
          continue;
        }

        // The weird NumberToString trick is because Windows does not like
        // to deal with %zu and a size_t in printf, nor does it support %llu.
        replacement_id = base::StringPrintf(
            "(%s: %s)",
            replacement_id.empty() ? pattern.alias : replacement_id.c_str(),
            base::NumberToString(identifier_space->size() + 1).c_str());
        (*identifier_space)[matched_id_as_string] = replacement_id;
        if (detected != nullptr) {
          (*detected)[pattern.pii_type].insert(matched_id_as_string);
        }
      }
    } else {
      replacement_id = (*identifier_space)[matched_id_as_string];
      if (detected != nullptr) {
        (*detected)[pattern.pii_type].insert(matched_id_as_string);
      }
    }

    skipped.AppendToString(&result);
    result += replacement_id;
  }
  text.AppendToString(&result);

  UMA_HISTOGRAM_ENUMERATION(kFeedbackRedactionToolHistogramName,
                            pattern.pii_type);

  return result;
}

RedactionToolContainer::RedactionToolContainer(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const char* const* first_party_extension_ids)
    : redactor_(new RedactionTool(first_party_extension_ids)),
      task_runner_(task_runner) {}

RedactionToolContainer::~RedactionToolContainer() {
  task_runner_->DeleteSoon(FROM_HERE, std::move(redactor_));
}

RedactionTool* RedactionToolContainer::Get() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return redactor_.get();
}

}  // namespace redaction
