// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <algorithm>
#include <iterator>
#include <map>

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#elif defined(OS_POSIX)
#include <fcntl.h>
#if !defined(OS_ANDROID)
#include <ifaddrs.h>
#endif
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/i18n/time_formatting.h"
#include "base/json/string_escape.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_split.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/sys_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/time.h"
#include "base/utf_offset_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_canon_ip.h"
#include "googleurl/src/url_parse.h"
#include "grit/net_resources.h"
#include "net/base/dns_util.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "net/base/net_module.h"
#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif
#include "net/http/http_content_disposition.h"
#include "unicode/datefmt.h"
#include "unicode/regex.h"
#include "unicode/ucnv.h"
#include "unicode/uidna.h"
#include "unicode/ulocdata.h"
#include "unicode/uniset.h"
#include "unicode/uscript.h"
#include "unicode/uset.h"

using base::Time;

namespace net {

namespace {

// what we prepend to get a file URL
static const FilePath::CharType kFileURLPrefix[] =
    FILE_PATH_LITERAL("file:///");

// The general list of blocked ports. Will be blocked unless a specific
// protocol overrides it. (Ex: ftp can use ports 20 and 21)
static const int kRestrictedPorts[] = {
  1,    // tcpmux
  7,    // echo
  9,    // discard
  11,   // systat
  13,   // daytime
  15,   // netstat
  17,   // qotd
  19,   // chargen
  20,   // ftp data
  21,   // ftp access
  22,   // ssh
  23,   // telnet
  25,   // smtp
  37,   // time
  42,   // name
  43,   // nicname
  53,   // domain
  77,   // priv-rjs
  79,   // finger
  87,   // ttylink
  95,   // supdup
  101,  // hostriame
  102,  // iso-tsap
  103,  // gppitnp
  104,  // acr-nema
  109,  // pop2
  110,  // pop3
  111,  // sunrpc
  113,  // auth
  115,  // sftp
  117,  // uucp-path
  119,  // nntp
  123,  // NTP
  135,  // loc-srv /epmap
  139,  // netbios
  143,  // imap2
  179,  // BGP
  389,  // ldap
  465,  // smtp+ssl
  512,  // print / exec
  513,  // login
  514,  // shell
  515,  // printer
  526,  // tempo
  530,  // courier
  531,  // chat
  532,  // netnews
  540,  // uucp
  556,  // remotefs
  563,  // nntp+ssl
  587,  // stmp?
  601,  // ??
  636,  // ldap+ssl
  993,  // ldap+ssl
  995,  // pop3+ssl
  2049, // nfs
  3659, // apple-sasl / PasswordServer
  4045, // lockd
  6000, // X11
  6665, // Alternate IRC [Apple addition]
  6666, // Alternate IRC [Apple addition]
  6667, // Standard IRC [Apple addition]
  6668, // Alternate IRC [Apple addition]
  6669, // Alternate IRC [Apple addition]
  0xFFFF, // Used to block all invalid port numbers (see
          // third_party/WebKit/Source/WebCore/platform/KURLGoogle.cpp, port())
};

// FTP overrides the following restricted ports.
static const int kAllowedFtpPorts[] = {
  21,   // ftp data
  22,   // ssh
};

std::string::size_type CountTrailingChars(
    const std::string& input,
    const std::string::value_type trailing_chars[]) {
  const size_t last_good_char = input.find_last_not_of(trailing_chars);
  return (last_good_char == std::string::npos) ?
      input.length() : (input.length() - last_good_char - 1);
}

// Similar to Base64Decode. Decodes a Q-encoded string to a sequence
// of bytes. If input is invalid, return false.
bool QPDecode(const std::string& input, std::string* output) {
  std::string temp;
  temp.reserve(input.size());
  for (std::string::const_iterator it = input.begin(); it != input.end();
       ++it) {
    if (*it == '_') {
      temp.push_back(' ');
    } else if (*it == '=') {
      if ((input.end() - it < 3) ||
          !IsHexDigit(static_cast<unsigned char>(*(it + 1))) ||
          !IsHexDigit(static_cast<unsigned char>(*(it + 2))))
        return false;
      unsigned char ch = HexDigitToInt(*(it + 1)) * 16 +
                         HexDigitToInt(*(it + 2));
      temp.push_back(static_cast<char>(ch));
      ++it;
      ++it;
    } else if (0x20 < *it && *it < 0x7F) {
      // In a Q-encoded word, only printable ASCII characters
      // represent themselves. Besides, space, '=', '_' and '?' are
      // not allowed, but they're already filtered out.
      DCHECK_NE('=', *it);
      DCHECK_NE('?', *it);
      DCHECK_NE('_', *it);
      temp.push_back(*it);
    } else {
      return false;
    }
  }
  output->swap(temp);
  return true;
}

enum RFC2047EncodingType {Q_ENCODING, B_ENCODING};
bool DecodeBQEncoding(const std::string& part,
                      RFC2047EncodingType enc_type,
                      const std::string& charset,
                      std::string* output) {
  std::string decoded;
  if (!((enc_type == B_ENCODING) ?
      base::Base64Decode(part, &decoded) : QPDecode(part, &decoded)))
    return false;

  if (decoded.empty()) {
    output->clear();
    return true;
  }

  UErrorCode err = U_ZERO_ERROR;
  UConverter* converter(ucnv_open(charset.c_str(), &err));
  if (U_FAILURE(err))
    return false;

  // A single byte in a legacy encoding can be expanded to 3 bytes in UTF-8.
  // A 'two-byte character' in a legacy encoding can be expanded to 4 bytes
  // in UTF-8. Therefore, the expansion ratio is 3 at most. Add one for a
  // trailing '\0'.
  size_t output_length = decoded.length() * 3 + 1;
  char* buf = WriteInto(output, output_length);
  output_length = ucnv_toAlgorithmic(UCNV_UTF8, converter, buf, output_length,
                                     decoded.data(), decoded.length(), &err);
  ucnv_close(converter);
  if (U_FAILURE(err))
    return false;
  output->resize(output_length);
  return true;
}

bool DecodeWord(const std::string& encoded_word,
                const std::string& referrer_charset,
                bool* is_rfc2047,
                std::string* output) {
  *is_rfc2047 = false;
  output->clear();
  if (encoded_word.empty())
    return true;

  if (!IsStringASCII(encoded_word)) {
    // Try UTF-8, referrer_charset and the native OS default charset in turn.
    if (IsStringUTF8(encoded_word)) {
      *output = encoded_word;
    } else {
      string16 utf16_output;
      if (!referrer_charset.empty() &&
          base::CodepageToUTF16(encoded_word, referrer_charset.c_str(),
                                base::OnStringConversionError::FAIL,
                                &utf16_output)) {
        *output = UTF16ToUTF8(utf16_output);
      } else {
        *output = WideToUTF8(base::SysNativeMBToWide(encoded_word));
      }
    }

    return true;
  }

  // RFC 2047 : one of encoding methods supported by Firefox and relatively
  // widely used by web servers.
  // =?charset?<E>?<encoded string>?= where '<E>' is either 'B' or 'Q'.
  // We don't care about the length restriction (72 bytes) because
  // many web servers generate encoded words longer than the limit.
  std::string tmp;
  *is_rfc2047 = true;
  int part_index = 0;
  std::string charset;
  StringTokenizer t(encoded_word, "?");
  RFC2047EncodingType enc_type = Q_ENCODING;
  while (*is_rfc2047 && t.GetNext()) {
    std::string part = t.token();
    switch (part_index) {
      case 0:
        if (part != "=") {
          *is_rfc2047 = false;
          break;
        }
        ++part_index;
        break;
      case 1:
        // Do we need charset validity check here?
        charset = part;
        ++part_index;
        break;
      case 2:
        if (part.size() > 1 ||
            part.find_first_of("bBqQ") == std::string::npos) {
          *is_rfc2047 = false;
          break;
        }
        if (part[0] == 'b' || part[0] == 'B') {
          enc_type = B_ENCODING;
        }
        ++part_index;
        break;
      case 3:
        *is_rfc2047 = DecodeBQEncoding(part, enc_type, charset, &tmp);
        if (!*is_rfc2047) {
          // Last minute failure. Invalid B/Q encoding. Rather than
          // passing it through, return now.
          return false;
        }
        ++part_index;
        break;
      case 4:
        if (part != "=") {
          // Another last minute failure !
          // Likely to be a case of two encoded-words in a row or
          // an encoded word followed by a non-encoded word. We can be
          // generous, but it does not help much in terms of compatibility,
          // I believe. Return immediately.
          *is_rfc2047 = false;
          return false;
        }
        ++part_index;
        break;
      default:
        *is_rfc2047 = false;
        return false;
    }
  }

  if (*is_rfc2047) {
    if (*(encoded_word.end() - 1) == '=') {
      output->swap(tmp);
      return true;
    }
    // encoded_word ending prematurelly with '?' or extra '?'
    *is_rfc2047 = false;
    return false;
  }

  // We're not handling 'especial' characters quoted with '\', but
  // it should be Ok because we're not an email client but a
  // web browser.

  // What IE6/7 does: %-escaped UTF-8.
  tmp = UnescapeURLComponent(encoded_word, UnescapeRule::SPACES);
  if (IsStringUTF8(tmp)) {
    output->swap(tmp);
    return true;
    // We can try either the OS default charset or 'origin charset' here,
    // As far as I can tell, IE does not support it. However, I've seen
    // web servers emit %-escaped string in a legacy encoding (usually
    // origin charset).
    // TODO(jungshik) : Test IE further and consider adding a fallback here.
  }
  return false;
}

// Does some simple normalization of scripts so we can allow certain scripts
// to exist together.
// TODO(brettw) bug 880223: we should allow some other languages to be
// oombined such as Chinese and Latin. We will probably need a more
// complicated system of language pairs to have more fine-grained control.
UScriptCode NormalizeScript(UScriptCode code) {
  switch (code) {
    case USCRIPT_KATAKANA:
    case USCRIPT_HIRAGANA:
    case USCRIPT_KATAKANA_OR_HIRAGANA:
    case USCRIPT_HANGUL:  // This one is arguable.
      return USCRIPT_HAN;
    default:
      return code;
  }
}

bool IsIDNComponentInSingleScript(const char16* str, int str_len) {
  UScriptCode first_script = USCRIPT_INVALID_CODE;
  bool is_first = true;

  int i = 0;
  while (i < str_len) {
    unsigned code_point;
    U16_NEXT(str, i, str_len, code_point);

    UErrorCode err = U_ZERO_ERROR;
    UScriptCode cur_script = uscript_getScript(code_point, &err);
    if (err != U_ZERO_ERROR)
      return false;  // Report mixed on error.
    cur_script = NormalizeScript(cur_script);

    // TODO(brettw) We may have to check for USCRIPT_INHERENT as well.
    if (is_first && cur_script != USCRIPT_COMMON) {
      first_script = cur_script;
      is_first = false;
    } else {
      if (cur_script != USCRIPT_COMMON && cur_script != first_script)
        return false;
    }
  }
  return true;
}

// Check if the script of a language can be 'safely' mixed with
// Latin letters in the ASCII range.
bool IsCompatibleWithASCIILetters(const std::string& lang) {
  // For now, just list Chinese, Japanese and Korean (positive list).
  // An alternative is negative-listing (languages using Greek and
  // Cyrillic letters), but it can be more dangerous.
  return !lang.substr(0, 2).compare("zh") ||
         !lang.substr(0, 2).compare("ja") ||
         !lang.substr(0, 2).compare("ko");
}

typedef std::map<std::string, icu::UnicodeSet*> LangToExemplarSetMap;

class LangToExemplarSet {
 public:
  static LangToExemplarSet* GetInstance() {
    return Singleton<LangToExemplarSet>::get();
  }

 private:
  LangToExemplarSetMap map;
  LangToExemplarSet() { }
  ~LangToExemplarSet() {
    STLDeleteContainerPairSecondPointers(map.begin(), map.end());
  }

  friend class Singleton<LangToExemplarSet>;
  friend struct DefaultSingletonTraits<LangToExemplarSet>;
  friend bool GetExemplarSetForLang(const std::string&, icu::UnicodeSet**);
  friend void SetExemplarSetForLang(const std::string&, icu::UnicodeSet*);

  DISALLOW_COPY_AND_ASSIGN(LangToExemplarSet);
};

bool GetExemplarSetForLang(const std::string& lang,
                           icu::UnicodeSet** lang_set) {
  const LangToExemplarSetMap& map = LangToExemplarSet::GetInstance()->map;
  LangToExemplarSetMap::const_iterator pos = map.find(lang);
  if (pos != map.end()) {
    *lang_set = pos->second;
    return true;
  }
  return false;
}

void SetExemplarSetForLang(const std::string& lang,
                           icu::UnicodeSet* lang_set) {
  LangToExemplarSetMap& map = LangToExemplarSet::GetInstance()->map;
  map.insert(std::make_pair(lang, lang_set));
}

static base::LazyInstance<base::Lock>::Leaky
    g_lang_set_lock = LAZY_INSTANCE_INITIALIZER;

// Returns true if all the characters in component_characters are used by
// the language |lang|.
bool IsComponentCoveredByLang(const icu::UnicodeSet& component_characters,
                              const std::string& lang) {
  CR_DEFINE_STATIC_LOCAL(
      const icu::UnicodeSet, kASCIILetters, ('a', 'z'));
  icu::UnicodeSet* lang_set;
  // We're called from both the UI thread and the history thread.
  {
    base::AutoLock lock(g_lang_set_lock.Get());
    if (!GetExemplarSetForLang(lang, &lang_set)) {
      UErrorCode status = U_ZERO_ERROR;
      ULocaleData* uld = ulocdata_open(lang.c_str(), &status);
      // TODO(jungshik) Turn this check on when the ICU data file is
      // rebuilt with the minimal subset of locale data for languages
      // to which Chrome is not localized but which we offer in the list
      // of languages selectable for Accept-Languages. With the rebuilt ICU
      // data, ulocdata_open never should fall back to the default locale.
      // (issue 2078)
      // DCHECK(U_SUCCESS(status) && status != U_USING_DEFAULT_WARNING);
      if (U_SUCCESS(status) && status != U_USING_DEFAULT_WARNING) {
        lang_set = reinterpret_cast<icu::UnicodeSet *>(
            ulocdata_getExemplarSet(uld, NULL, 0,
                                    ULOCDATA_ES_STANDARD, &status));
        // If |lang| is compatible with ASCII Latin letters, add them.
        if (IsCompatibleWithASCIILetters(lang))
          lang_set->addAll(kASCIILetters);
      } else {
        lang_set = new icu::UnicodeSet(1, 0);
      }
      lang_set->freeze();
      SetExemplarSetForLang(lang, lang_set);
      ulocdata_close(uld);
    }
  }
  return !lang_set->isEmpty() && lang_set->containsAll(component_characters);
}

// Returns true if the given Unicode host component is safe to display to the
// user.
bool IsIDNComponentSafe(const char16* str,
                        int str_len,
                        const std::string& languages) {
  // Most common cases (non-IDN) do not reach here so that we don't
  // need a fast return path.
  // TODO(jungshik) : Check if there's any character inappropriate
  // (although allowed) for domain names.
  // See http://www.unicode.org/reports/tr39/#IDN_Security_Profiles and
  // http://www.unicode.org/reports/tr39/data/xidmodifications.txt
  // For now, we borrow the list from Mozilla and tweaked it slightly.
  // (e.g. Characters like U+00A0, U+3000, U+3002 are omitted because
  //  they're gonna be canonicalized to U+0020 and full stop before
  //  reaching here.)
  // The original list is available at
  // http://kb.mozillazine.org/Network.IDN.blacklist_chars and
  // at http://mxr.mozilla.org/seamonkey/source/modules/libpref/src/init/all.js#703

  UErrorCode status = U_ZERO_ERROR;
#ifdef U_WCHAR_IS_UTF16
  icu::UnicodeSet dangerous_characters(icu::UnicodeString(
      L"[[\\ \u00bc\u00bd\u01c3\u0337\u0338"
      L"\u05c3\u05f4\u06d4\u0702\u115f\u1160][\u2000-\u200b]"
      L"[\u2024\u2027\u2028\u2029\u2039\u203a\u2044\u205f]"
      L"[\u2154-\u2156][\u2159-\u215b][\u215f\u2215\u23ae"
      L"\u29f6\u29f8\u2afb\u2afd][\u2ff0-\u2ffb][\u3014"
      L"\u3015\u3033\u3164\u321d\u321e\u33ae\u33af\u33c6\u33df\ufe14"
      L"\ufe15\ufe3f\ufe5d\ufe5e\ufeff\uff0e\uff06\uff61\uffa0\ufff9]"
      L"[\ufffa-\ufffd]]"), status);
  DCHECK(U_SUCCESS(status));
  icu::RegexMatcher dangerous_patterns(icu::UnicodeString(
      // Lone katakana no, so, or n
      L"[^\\p{Katakana}][\u30ce\u30f3\u30bd][^\\p{Katakana}]"
      // Repeating Japanese accent characters
      L"|[\u3099\u309a\u309b\u309c][\u3099\u309a\u309b\u309c]"),
      0, status);
#else
  icu::UnicodeSet dangerous_characters(icu::UnicodeString(
      "[[\\u0020\\u00bc\\u00bd\\u01c3\\u0337\\u0338"
      "\\u05c3\\u05f4\\u06d4\\u0702\\u115f\\u1160][\\u2000-\\u200b]"
      "[\\u2024\\u2027\\u2028\\u2029\\u2039\\u203a\\u2044\\u205f]"
      "[\\u2154-\\u2156][\\u2159-\\u215b][\\u215f\\u2215\\u23ae"
      "\\u29f6\\u29f8\\u2afb\\u2afd][\\u2ff0-\\u2ffb][\\u3014"
      "\\u3015\\u3033\\u3164\\u321d\\u321e\\u33ae\\u33af\\u33c6\\u33df\\ufe14"
      "\\ufe15\\ufe3f\\ufe5d\\ufe5e\\ufeff\\uff0e\\uff06\\uff61\\uffa0\\ufff9]"
      "[\\ufffa-\\ufffd]]", -1, US_INV), status);
  DCHECK(U_SUCCESS(status));
  icu::RegexMatcher dangerous_patterns(icu::UnicodeString(
      // Lone katakana no, so, or n
      "[^\\p{Katakana}][\\u30ce\\u30f3\u30bd][^\\p{Katakana}]"
      // Repeating Japanese accent characters
      "|[\\u3099\\u309a\\u309b\\u309c][\\u3099\\u309a\\u309b\\u309c]"),
      0, status);
#endif
  DCHECK(U_SUCCESS(status));
  icu::UnicodeSet component_characters;
  icu::UnicodeString component_string(str, str_len);
  component_characters.addAll(component_string);
  if (dangerous_characters.containsSome(component_characters))
    return false;

  DCHECK(U_SUCCESS(status));
  dangerous_patterns.reset(component_string);
  if (dangerous_patterns.find())
    return false;

  // If the language list is empty, the result is completely determined
  // by whether a component is a single script or not. This will block
  // even "safe" script mixing cases like <Chinese, Latin-ASCII> that are
  // allowed with |languages| (while it blocks Chinese + Latin letters with
  // an accent as should be the case), but we want to err on the safe side
  // when |languages| is empty.
  if (languages.empty())
    return IsIDNComponentInSingleScript(str, str_len);

  // |common_characters| is made up of  ASCII numbers, hyphen, plus and
  // underscore that are used across scripts and allowed in domain names.
  // (sync'd with characters allowed in url_canon_host with square
  // brackets excluded.) See kHostCharLookup[] array in url_canon_host.cc.
  icu::UnicodeSet common_characters(UNICODE_STRING_SIMPLE("[[0-9]\\-_+\\ ]"),
                                    status);
  DCHECK(U_SUCCESS(status));
  // Subtract common characters because they're always allowed so that
  // we just have to check if a language-specific set contains
  // the remainder.
  component_characters.removeAll(common_characters);

  StringTokenizer t(languages, ",");
  while (t.GetNext()) {
    if (IsComponentCoveredByLang(component_characters, t.token()))
      return true;
  }
  return false;
}

// Converts one component of a host (between dots) to IDN if safe. The result
// will be APPENDED to the given output string and will be the same as the input
// if it is not IDN or the IDN is unsafe to display.  Returns whether any
// conversion was performed.
bool IDNToUnicodeOneComponent(const char16* comp,
                              size_t comp_len,
                              const std::string& languages,
                              string16* out) {
  DCHECK(out);
  if (comp_len == 0)
    return false;

  // Only transform if the input can be an IDN component.
  static const char16 kIdnPrefix[] = {'x', 'n', '-', '-'};
  if ((comp_len > arraysize(kIdnPrefix)) &&
      !memcmp(comp, kIdnPrefix, arraysize(kIdnPrefix) * sizeof(char16))) {
    // Repeatedly expand the output string until it's big enough.  It looks like
    // ICU will return the required size of the buffer, but that's not
    // documented, so we'll just grow by 2x. This should be rare and is not on a
    // critical path.
    size_t original_length = out->length();
    for (int extra_space = 64; ; extra_space *= 2) {
      UErrorCode status = U_ZERO_ERROR;
      out->resize(out->length() + extra_space);
      int output_chars = uidna_IDNToUnicode(comp,
          static_cast<int32_t>(comp_len), &(*out)[original_length], extra_space,
          UIDNA_DEFAULT, NULL, &status);
      if (status == U_ZERO_ERROR) {
        // Converted successfully.
        out->resize(original_length + output_chars);
        if (IsIDNComponentSafe(out->data() + original_length, output_chars,
                               languages))
          return true;
      }

      if (status != U_BUFFER_OVERFLOW_ERROR)
        break;
    }
    // Failed, revert back to original string.
    out->resize(original_length);
  }

  // We get here with no IDN or on error, in which case we just append the
  // literal input.
  out->append(comp, comp_len);
  return false;
}

// Clamps the offsets in |offsets_for_adjustment| to the length of |str|.
void LimitOffsets(const string16& str,
                  std::vector<size_t>* offsets_for_adjustment) {
  if (offsets_for_adjustment) {
    std::for_each(offsets_for_adjustment->begin(),
                  offsets_for_adjustment->end(),
                  LimitOffset<string16>(str.length()));
  }
}

// TODO(brettw) bug 734373: check the scripts for each host component and
// don't un-IDN-ize if there is more than one. Alternatively, only IDN for
// scripts that the user has installed. For now, just put the entire
// path through IDN. Maybe this feature can be implemented in ICU itself?
//
// We may want to skip this step in the case of file URLs to allow unicode
// UNC hostnames regardless of encodings.
string16 IDNToUnicodeWithOffsets(const std::string& host,
                                 const std::string& languages,
                                 std::vector<size_t>* offsets_for_adjustment) {
  // Convert the ASCII input to a string16 for ICU.
  string16 input16;
  input16.reserve(host.length());
  input16.insert(input16.end(), host.begin(), host.end());

  // Do each component of the host separately, since we enforce script matching
  // on a per-component basis.
  string16 out16;
  {
    OffsetAdjuster offset_adjuster(offsets_for_adjustment);
    for (size_t component_start = 0, component_end;
         component_start < input16.length();
         component_start = component_end + 1) {
      // Find the end of the component.
      component_end = input16.find('.', component_start);
      if (component_end == string16::npos)
        component_end = input16.length();  // For getting the last component.
      size_t component_length = component_end - component_start;
      size_t new_component_start = out16.length();
      bool converted_idn = false;
      if (component_end > component_start) {
        // Add the substring that we just found.
        converted_idn = IDNToUnicodeOneComponent(
            input16.data() + component_start, component_length, languages,
            &out16);
      }
      size_t new_component_length = out16.length() - new_component_start;

      if (converted_idn && offsets_for_adjustment) {
        offset_adjuster.Add(OffsetAdjuster::Adjustment(component_start,
            component_length, new_component_length));
      }

      // Need to add the dot we just found (if we found one).
      if (component_end < input16.length())
        out16.push_back('.');
    }
  }

  LimitOffsets(out16, offsets_for_adjustment);
  return out16;
}

// Transforms |original_offsets| by subtracting |component_begin| from all
// offsets.  Any offset which was not at least this large to begin with is set
// to std::string::npos.
std::vector<size_t> OffsetsIntoComponent(
    const std::vector<size_t>& original_offsets,
    size_t component_begin) {
  DCHECK_NE(std::string::npos, component_begin);
  std::vector<size_t> offsets_into_component(original_offsets);
  for (std::vector<size_t>::iterator i(offsets_into_component.begin());
       i != offsets_into_component.end(); ++i) {
    if (*i != std::string::npos)
      *i = (*i < component_begin) ? std::string::npos : (*i - component_begin);
  }
  return offsets_into_component;
}

// Called after we transform a component and append it to an output string.
// Maps |transformed_offsets|, which represent offsets into the transformed
// component itself, into appropriate offsets for the output string, by adding
// |output_component_begin| to each.  Determines which offsets need mapping by
// checking to see which of the |original_offsets| were within the designated
// original component, using its provided endpoints.
void AdjustForComponentTransform(
    const std::vector<size_t>& original_offsets,
    size_t original_component_begin,
    size_t original_component_end,
    const std::vector<size_t>& transformed_offsets,
    size_t output_component_begin,
    std::vector<size_t>* offsets_for_adjustment) {
  if (!offsets_for_adjustment)
    return;

  DCHECK_NE(std::string::npos, original_component_begin);
  DCHECK_NE(std::string::npos, original_component_end);
  DCHECK_NE(string16::npos, output_component_begin);
  size_t offsets_size = offsets_for_adjustment->size();
  DCHECK_EQ(offsets_size, original_offsets.size());
  DCHECK_EQ(offsets_size, transformed_offsets.size());
  for (size_t i = 0; i < offsets_size; ++i) {
    size_t original_offset = original_offsets[i];
    if ((original_offset >= original_component_begin) &&
        (original_offset < original_component_end)) {
      size_t transformed_offset = transformed_offsets[i];
      (*offsets_for_adjustment)[i] = (transformed_offset == string16::npos) ?
          string16::npos : (output_component_begin + transformed_offset);
    }
  }
}

// If |component| is valid, its begin is incremented by |delta|.
void AdjustComponent(int delta, url_parse::Component* component) {
  if (!component->is_valid())
    return;

  DCHECK(delta >= 0 || component->begin >= -delta);
  component->begin += delta;
}

// Adjusts all the components of |parsed| by |delta|, except for the scheme.
void AdjustComponents(int delta, url_parse::Parsed* parsed) {
  AdjustComponent(delta, &(parsed->username));
  AdjustComponent(delta, &(parsed->password));
  AdjustComponent(delta, &(parsed->host));
  AdjustComponent(delta, &(parsed->port));
  AdjustComponent(delta, &(parsed->path));
  AdjustComponent(delta, &(parsed->query));
  AdjustComponent(delta, &(parsed->ref));
}

// Helper for FormatUrlWithOffsets().
string16 FormatViewSourceUrl(const GURL& url,
                             const std::vector<size_t>& original_offsets,
                             const std::string& languages,
                             FormatUrlTypes format_types,
                             UnescapeRule::Type unescape_rules,
                             url_parse::Parsed* new_parsed,
                             size_t* prefix_end,
                             std::vector<size_t>* offsets_for_adjustment) {
  DCHECK(new_parsed);
  const char kViewSource[] = "view-source:";
  const size_t kViewSourceLength = arraysize(kViewSource) - 1;
  std::vector<size_t> offsets_into_url(
      OffsetsIntoComponent(original_offsets, kViewSourceLength));

  GURL real_url(url.possibly_invalid_spec().substr(kViewSourceLength));
  string16 result(ASCIIToUTF16(kViewSource) +
      FormatUrlWithOffsets(real_url, languages, format_types, unescape_rules,
                           new_parsed, prefix_end, &offsets_into_url));

  // Adjust position values.
  if (new_parsed->scheme.is_nonempty()) {
    // Assume "view-source:real-scheme" as a scheme.
    new_parsed->scheme.len += kViewSourceLength;
  } else {
    new_parsed->scheme.begin = 0;
    new_parsed->scheme.len = kViewSourceLength - 1;
  }
  AdjustComponents(kViewSourceLength, new_parsed);
  if (prefix_end)
    *prefix_end += kViewSourceLength;
  AdjustForComponentTransform(original_offsets, kViewSourceLength,
      url.possibly_invalid_spec().length(), offsets_into_url, kViewSourceLength,
      offsets_for_adjustment);
  LimitOffsets(result, offsets_for_adjustment);
  return result;
}

class AppendComponentTransform {
 public:
  AppendComponentTransform() {}
  virtual ~AppendComponentTransform() {}

  virtual string16 Execute(
      const std::string& component_text,
      std::vector<size_t>* offsets_into_component) const = 0;

  // NOTE: No DISALLOW_COPY_AND_ASSIGN here, since gcc < 4.3.0 requires an
  // accessible copy constructor in order to call AppendFormattedComponent()
  // with an inline temporary (see http://gcc.gnu.org/bugs/#cxx%5Frvalbind ).
};

class HostComponentTransform : public AppendComponentTransform {
 public:
  explicit HostComponentTransform(const std::string& languages)
      : languages_(languages) {
  }

 private:
  virtual string16 Execute(
      const std::string& component_text,
      std::vector<size_t>* offsets_into_component) const {
    return IDNToUnicodeWithOffsets(component_text, languages_,
                                   offsets_into_component);
  }

  const std::string& languages_;
};

class NonHostComponentTransform : public AppendComponentTransform {
 public:
  explicit NonHostComponentTransform(UnescapeRule::Type unescape_rules)
      : unescape_rules_(unescape_rules) {
  }

 private:
  virtual string16 Execute(
      const std::string& component_text,
      std::vector<size_t>* offsets_into_component) const {
    return (unescape_rules_ == UnescapeRule::NONE) ?
        UTF8ToUTF16AndAdjustOffsets(component_text, offsets_into_component) :
        UnescapeAndDecodeUTF8URLComponentWithOffsets(component_text,
            unescape_rules_, offsets_into_component);
  }

  const UnescapeRule::Type unescape_rules_;
};

void AppendFormattedComponent(const std::string& spec,
                              const url_parse::Component& original_component,
                              const std::vector<size_t>& original_offsets,
                              const AppendComponentTransform& transform,
                              string16* output,
                              url_parse::Component* output_component,
                              std::vector<size_t>* offsets_for_adjustment) {
  DCHECK(output);
  if (original_component.is_nonempty()) {
    size_t original_component_begin =
        static_cast<size_t>(original_component.begin);
    size_t output_component_begin = output->length();
    if (output_component)
      output_component->begin = static_cast<int>(output_component_begin);

    std::vector<size_t> offsets_into_component =
        OffsetsIntoComponent(original_offsets, original_component_begin);
    output->append(transform.Execute(std::string(spec, original_component_begin,
        static_cast<size_t>(original_component.len)), &offsets_into_component));

    if (output_component) {
      output_component->len =
          static_cast<int>(output->length() - output_component_begin);
    }
    AdjustForComponentTransform(original_offsets, original_component_begin,
                                static_cast<size_t>(original_component.end()),
                                offsets_into_component, output_component_begin,
                                offsets_for_adjustment);
  } else if (output_component) {
    output_component->reset();
  }
}

void SanitizeGeneratedFileName(std::string& filename) {
  if (!filename.empty()) {
    // Remove "." from the beginning and end of the file name to avoid tricks
    // with hidden files, "..", and "."
    TrimString(filename, ".", &filename);
#if defined(OS_WIN)
    // Handle CreateFile() stripping trailing dots and spaces on filenames
    // http://support.microsoft.com/kb/115827
    std::string::size_type pos = filename.find_last_not_of(" .");
    if (pos == std::string::npos)
      filename.resize(0);
    else
      filename.resize(++pos);
#endif
    // Replace any path information by changing path separators with
    // underscores.
    ReplaceSubstringsAfterOffset(&filename, 0, "/", "_");
    ReplaceSubstringsAfterOffset(&filename, 0, "\\", "_");
  }
}

// Returns the filename determined from the last component of the path portion
// of the URL.  Returns an empty string if the URL doesn't have a path or is
// invalid. If the generated filename is not reliable,
// |should_overwrite_extension| will be set to true, in which case a better
// extension should be determined based on the content type.
std::string GetFileNameFromURL(const GURL& url,
                               const std::string& referrer_charset,
                               bool* should_overwrite_extension) {
  // about: and data: URLs don't have file names, but esp. data: URLs may
  // contain parts that look like ones (i.e., contain a slash).  Therefore we
  // don't attempt to divine a file name out of them.
  if (!url.is_valid() || url.SchemeIs("about") || url.SchemeIs("data"))
    return std::string();

  const std::string unescaped_url_filename = UnescapeURLComponent(
      url.ExtractFileName(),
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);

  // The URL's path should be escaped UTF-8, but may not be.
  std::string decoded_filename = unescaped_url_filename;
  if (!IsStringASCII(decoded_filename)) {
    bool ignore;
    // TODO(jshin): this is probably not robust enough. To be sure, we need
    // encoding detection.
    DecodeWord(unescaped_url_filename, referrer_charset, &ignore,
               &decoded_filename);
  }
  // If the URL contains a (possibly empty) query, assume it is a generator, and
  // allow the determined extension to be overwritten.
  *should_overwrite_extension = !decoded_filename.empty() && url.has_query();

  return decoded_filename;
}

#if defined(OS_WIN)
// Returns whether the specified extension is automatically integrated into the
// windows shell.
bool IsShellIntegratedExtension(const string16& extension) {
  string16 extension_lower = StringToLowerASCII(extension);

  static const wchar_t* const integrated_extensions[] = {
    // See <http://msdn.microsoft.com/en-us/library/ms811694.aspx>.
    L"local",
    // Right-clicking on shortcuts can be magical.
    L"lnk",
  };

  for (int i = 0; i < arraysize(integrated_extensions); ++i) {
    if (extension_lower == integrated_extensions[i])
      return true;
  }

  // See <http://www.juniper.net/security/auto/vulnerabilities/vuln2612.html>.
  // That vulnerability report is not exactly on point, but files become magical
  // if their end in a CLSID.  Here we block extensions that look like CLSIDs.
  if (!extension_lower.empty() && extension_lower[0] == L'{' &&
      extension_lower[extension_lower.length() - 1] == L'}')
    return true;

  return false;
}

// Returns whether the specified file name is a reserved name on windows.
// This includes names like "com2.zip" (which correspond to devices) and
// desktop.ini and thumbs.db which have special meaning to the windows shell.
bool IsReservedName(const string16& filename) {
  // This list is taken from the MSDN article "Naming a file"
  // http://msdn2.microsoft.com/en-us/library/aa365247(VS.85).aspx
  // I also added clock$ because GetSaveFileName seems to consider it as a
  // reserved name too.
  static const wchar_t* const known_devices[] = {
    L"con", L"prn", L"aux", L"nul", L"com1", L"com2", L"com3", L"com4", L"com5",
    L"com6", L"com7", L"com8", L"com9", L"lpt1", L"lpt2", L"lpt3", L"lpt4",
    L"lpt5", L"lpt6", L"lpt7", L"lpt8", L"lpt9", L"clock$"
  };
  string16 filename_lower = StringToLowerASCII(filename);

  for (int i = 0; i < arraysize(known_devices); ++i) {
    // Exact match.
    if (filename_lower == known_devices[i])
      return true;
    // Starts with "DEVICE.".
    if (filename_lower.find(string16(known_devices[i]) + L".") == 0)
      return true;
  }

  static const wchar_t* const magic_names[] = {
    // These file names are used by the "Customize folder" feature of the shell.
    L"desktop.ini",
    L"thumbs.db",
  };

  for (int i = 0; i < arraysize(magic_names); ++i) {
    if (filename_lower == magic_names[i])
      return true;
  }

  return false;
}
#endif  // OS_WIN

// Examines the current extension in |file_name| and modifies it if necessary in
// order to ensure the filename is safe.  If |file_name| doesn't contain an
// extension or if |ignore_extension| is true, then a new extension will be
// constructed based on the |mime_type|.
//
// We're addressing two things here:
//
// 1) Usability.  If there is no reliable file extension, we want to guess a
//    reasonable file extension based on the content type.
//
// 2) Shell integration.  Some file extensions automatically integrate with the
//    shell.  We block these extensions to prevent a malicious web site from
//    integrating with the user's shell.
void EnsureSafeExtension(const std::string& mime_type,
                         bool ignore_extension,
                         FilePath* file_name) {
  // See if our file name already contains an extension.
  FilePath::StringType extension = file_name->Extension();
  if (!extension.empty())
    extension.erase(extension.begin());  // Erase preceding '.'.

  if ((ignore_extension || extension.empty()) && !mime_type.empty()) {
    FilePath::StringType mime_extension;
    // The GetPreferredExtensionForMimeType call will end up going to disk.  Do
    // this on another thread to avoid slowing the IO thread.
    // http://crbug.com/61827
    // TODO(asanka): Remove this ScopedAllowIO once all callers have switched
    // over to IO safe threads.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    net::GetPreferredExtensionForMimeType(mime_type, &mime_extension);
    if (!mime_extension.empty())
      extension = mime_extension;
  }

#if defined(OS_WIN)
  static const FilePath::CharType default_extension[] =
      FILE_PATH_LITERAL("download");

  // Rename shell-integrated extensions.
  // TODO(asanka): Consider stripping out the bad extension and replacing it
  // with the preferred extension for the MIME type if one is available.
  if (IsShellIntegratedExtension(extension))
    extension.assign(default_extension);
#endif

  *file_name = file_name->ReplaceExtension(extension);
}

}  // namespace

const FormatUrlType kFormatUrlOmitNothing                     = 0;
const FormatUrlType kFormatUrlOmitUsernamePassword            = 1 << 0;
const FormatUrlType kFormatUrlOmitHTTP                        = 1 << 1;
const FormatUrlType kFormatUrlOmitTrailingSlashOnBareHostname = 1 << 2;
const FormatUrlType kFormatUrlOmitAll = kFormatUrlOmitUsernamePassword |
    kFormatUrlOmitHTTP | kFormatUrlOmitTrailingSlashOnBareHostname;

static base::LazyInstance<std::multiset<int> >::Leaky
    g_explicitly_allowed_ports = LAZY_INSTANCE_INITIALIZER;

size_t GetCountOfExplicitlyAllowedPorts() {
  return g_explicitly_allowed_ports.Get().size();
}

GURL FilePathToFileURL(const FilePath& path) {
  // Produce a URL like "file:///C:/foo" for a regular file, or
  // "file://///server/path" for UNC. The URL canonicalizer will fix up the
  // latter case to be the canonical UNC form: "file://server/path"
  FilePath::StringType url_string(kFileURLPrefix);
  url_string.append(path.value());

  // Now do replacement of some characters. Since we assume the input is a
  // literal filename, anything the URL parser might consider special should
  // be escaped here.

  // must be the first substitution since others will introduce percents as the
  // escape character
  ReplaceSubstringsAfterOffset(&url_string, 0,
      FILE_PATH_LITERAL("%"), FILE_PATH_LITERAL("%25"));

  // semicolon is supposed to be some kind of separator according to RFC 2396
  ReplaceSubstringsAfterOffset(&url_string, 0,
      FILE_PATH_LITERAL(";"), FILE_PATH_LITERAL("%3B"));

  ReplaceSubstringsAfterOffset(&url_string, 0,
      FILE_PATH_LITERAL("#"), FILE_PATH_LITERAL("%23"));

  ReplaceSubstringsAfterOffset(&url_string, 0,
      FILE_PATH_LITERAL("?"), FILE_PATH_LITERAL("%3F"));

#if defined(OS_POSIX)
  ReplaceSubstringsAfterOffset(&url_string, 0,
      FILE_PATH_LITERAL("\\"), FILE_PATH_LITERAL("%5C"));
#endif

  return GURL(url_string);
}

std::string GetSpecificHeader(const std::string& headers,
                              const std::string& name) {
  // We want to grab the Value from the "Key: Value" pairs in the headers,
  // which should look like this (no leading spaces, \n-separated) (we format
  // them this way in url_request_inet.cc):
  //    HTTP/1.1 200 OK\n
  //    ETag: "6d0b8-947-24f35ec0"\n
  //    Content-Length: 2375\n
  //    Content-Type: text/html; charset=UTF-8\n
  //    Last-Modified: Sun, 03 Sep 2006 04:34:43 GMT\n
  if (headers.empty())
    return std::string();

  std::string match('\n' + name + ':');

  std::string::const_iterator begin =
      std::search(headers.begin(), headers.end(), match.begin(), match.end(),
             base::CaseInsensitiveCompareASCII<char>());

  if (begin == headers.end())
    return std::string();

  begin += match.length();

  std::string ret;
  TrimWhitespace(std::string(begin, std::find(begin, headers.end(), '\n')),
                 TRIM_ALL, &ret);
  return ret;
}

bool DecodeCharset(const std::string& input,
                   std::string* decoded_charset,
                   std::string* value) {
  StringTokenizer t(input, "'");
  t.set_options(StringTokenizer::RETURN_DELIMS);
  std::string temp_charset;
  std::string temp_value;
  int numDelimsSeen = 0;
  while (t.GetNext()) {
    if (t.token_is_delim()) {
      ++numDelimsSeen;
      continue;
    } else {
      switch (numDelimsSeen) {
        case 0:
          temp_charset = t.token();
          break;
        case 1:
          // Language is ignored.
          break;
        case 2:
          temp_value = t.token();
          break;
        default:
          return false;
      }
    }
  }
  if (numDelimsSeen != 2)
    return false;
  if (temp_charset.empty() || temp_value.empty())
    return false;
  decoded_charset->swap(temp_charset);
  value->swap(temp_value);
  return true;
}

bool DecodeFilenameValue(const std::string& input,
                         const std::string& referrer_charset,
                         std::string* output) {
  std::string tmp;
  // Tokenize with whitespace characters.
  StringTokenizer t(input, " \t\n\r");
  t.set_options(StringTokenizer::RETURN_DELIMS);
  bool is_previous_token_rfc2047 = true;
  while (t.GetNext()) {
    if (t.token_is_delim()) {
      // If the previous non-delimeter token is not RFC2047-encoded,
      // put in a space in its place. Otheriwse, skip over it.
      if (!is_previous_token_rfc2047) {
        tmp.push_back(' ');
      }
      continue;
    }
    // We don't support a single multibyte character split into
    // adjacent encoded words. Some broken mail clients emit headers
    // with that problem, but most web servers usually encode a filename
    // in a single encoded-word. Firefox/Thunderbird do not support
    // it, either.
    std::string decoded;
    if (!DecodeWord(t.token(), referrer_charset, &is_previous_token_rfc2047,
                    &decoded))
      return false;
    tmp.append(decoded);
  }
  output->swap(tmp);
  return true;
}

bool DecodeExtValue(const std::string& param_value, std::string* decoded) {
  if (param_value.find('"') != std::string::npos)
    return false;

  std::string charset;
  std::string value;
  if (!DecodeCharset(param_value, &charset, &value))
    return false;

  // RFC 5987 value should be ASCII-only.
  if (!IsStringASCII(value)) {
    decoded->clear();
    return true;
  }

  std::string unescaped = UnescapeURLComponent(value,
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);

  return base::ConvertToUtf8AndNormalize(unescaped, charset, decoded);
}

string16 IDNToUnicode(const std::string& host,
                      const std::string& languages) {
  return IDNToUnicodeWithOffsets(host, languages, NULL);
}

std::string CanonicalizeHost(const std::string& host,
                             url_canon::CanonHostInfo* host_info) {
  // Try to canonicalize the host.
  const url_parse::Component raw_host_component(
      0, static_cast<int>(host.length()));
  std::string canon_host;
  url_canon::StdStringCanonOutput canon_host_output(&canon_host);
  url_canon::CanonicalizeHostVerbose(host.c_str(), raw_host_component,
                                     &canon_host_output, host_info);

  if (host_info->out_host.is_nonempty() &&
      host_info->family != url_canon::CanonHostInfo::BROKEN) {
    // Success!  Assert that there's no extra garbage.
    canon_host_output.Complete();
    DCHECK_EQ(host_info->out_host.len, static_cast<int>(canon_host.length()));
  } else {
    // Empty host, or canonicalization failed.  We'll return empty.
    canon_host.clear();
  }

  return canon_host;
}

std::string GetDirectoryListingHeader(const string16& title) {
  static const base::StringPiece header(
      NetModule::GetResource(IDR_DIR_HEADER_HTML));
  // This can be null in unit tests.
  DLOG_IF(WARNING, header.empty()) <<
      "Missing resource: directory listing header";

  std::string result;
  if (!header.empty())
    result.assign(header.data(), header.size());

  result.append("<script>start(");
  base::JsonDoubleQuote(title, true, &result);
  result.append(");</script>\n");

  return result;
}

inline bool IsHostCharAlpha(char c) {
  // We can just check lowercase because uppercase characters have already been
  // normalized.
  return (c >= 'a') && (c <= 'z');
}

inline bool IsHostCharDigit(char c) {
  return (c >= '0') && (c <= '9');
}

bool IsCanonicalizedHostCompliant(const std::string& host,
                                  const std::string& desired_tld) {
  if (host.empty())
    return false;

  bool in_component = false;
  bool most_recent_component_started_alpha = false;
  bool last_char_was_hyphen_or_underscore = false;

  for (std::string::const_iterator i(host.begin()); i != host.end(); ++i) {
    const char c = *i;
    if (!in_component) {
      most_recent_component_started_alpha = IsHostCharAlpha(c);
      if (!most_recent_component_started_alpha && !IsHostCharDigit(c) &&
          (c != '-'))
        return false;
      in_component = true;
    } else {
      if (c == '.') {
        if (last_char_was_hyphen_or_underscore)
          return false;
        in_component = false;
      } else if (IsHostCharAlpha(c) || IsHostCharDigit(c)) {
        last_char_was_hyphen_or_underscore = false;
      } else if ((c == '-') || (c == '_')) {
        last_char_was_hyphen_or_underscore = true;
      } else {
        return false;
      }
    }
  }

  return most_recent_component_started_alpha ||
      (!desired_tld.empty() && IsHostCharAlpha(desired_tld[0]));
}

std::string GetDirectoryListingEntry(const string16& name,
                                     const std::string& raw_bytes,
                                     bool is_dir,
                                     int64 size,
                                     Time modified) {
  std::string result;
  result.append("<script>addRow(");
  base::JsonDoubleQuote(name, true, &result);
  result.append(",");
  if (raw_bytes.empty()) {
    base::JsonDoubleQuote(EscapePath(UTF16ToUTF8(name)),
                                   true, &result);
  } else {
    base::JsonDoubleQuote(EscapePath(raw_bytes), true, &result);
  }
  if (is_dir) {
    result.append(",1,");
  } else {
    result.append(",0,");
  }

  // Negative size means unknown or not applicable (e.g. directory).
  string16 size_string;
  if (size >= 0)
    size_string = FormatBytesUnlocalized(size);
  base::JsonDoubleQuote(size_string, true, &result);

  result.append(",");

  string16 modified_str;
  // |modified| can be NULL in FTP listings.
  if (!modified.is_null()) {
    modified_str = base::TimeFormatShortDateAndTime(modified);
  }
  base::JsonDoubleQuote(modified_str, true, &result);

  result.append(");</script>\n");

  return result;
}

string16 StripWWW(const string16& text) {
  const string16 www(ASCIIToUTF16("www."));
  return StartsWith(text, www, true) ? text.substr(www.length()) : text;
}

void GenerateSafeFileName(const std::string& mime_type,
                          bool ignore_extension,
                          FilePath* file_path) {
  // Make sure we get the right file extension
  EnsureSafeExtension(mime_type, ignore_extension, file_path);

#if defined(OS_WIN)
  // Prepend "_" to the file name if it's a reserved name
  FilePath::StringType leaf_name = file_path->BaseName().value();
  DCHECK(!leaf_name.empty());
  if (IsReservedName(leaf_name)) {
    leaf_name = FilePath::StringType(FILE_PATH_LITERAL("_")) + leaf_name;
    *file_path = file_path->DirName();
    if (file_path->value() == FilePath::kCurrentDirectory) {
      *file_path = FilePath(leaf_name);
    } else {
      *file_path = file_path->Append(leaf_name);
    }
  }
#endif
}

string16 GetSuggestedFilename(const GURL& url,
                              const std::string& content_disposition,
                              const std::string& referrer_charset,
                              const std::string& suggested_name,
                              const std::string& mime_type,
                              const std::string& default_name) {
  // TODO: this function to be updated to match the httpbis recommendations.
  // Talk to abarth for the latest news.

  // We don't translate this fallback string, "download". If localization is
  // needed, the caller should provide localized fallback in |default_name|.
  static const char* kFinalFallbackName = "download";
  std::string filename;  // In UTF-8
  bool overwrite_extension = false;

  // Try to extract a filename from content-disposition first.
  if (!content_disposition.empty()) {
    HttpContentDisposition header(content_disposition, referrer_charset);
    filename = header.filename();
  }

  // Then try to use the suggested name.
  if (filename.empty() && !suggested_name.empty())
    filename = suggested_name;

  // Now try extracting the filename from the URL.  GetFileNameFromURL() only
  // looks at the last component of the URL and doesn't return the hostname as a
  // failover.
  if (filename.empty())
    filename = GetFileNameFromURL(url, referrer_charset, &overwrite_extension);

  // Finally try the URL hostname, but only if there's no default specified in
  // |default_name|.  Some schemes (e.g.: file:, about:, data:) do not have a
  // host name.
  if (filename.empty() && default_name.empty() &&
      url.is_valid() && !url.host().empty()) {
    // TODO(jungshik) : Decode a 'punycoded' IDN hostname. (bug 1264451)
    filename = url.host();
  }

#if defined(OS_WIN)
  std::string::size_type trimmed_trailing_character_count =
      CountTrailingChars(filename, " .");
#endif
  SanitizeGeneratedFileName(filename);
  // Sanitization can cause the filename to disappear (e.g.: if the filename
  // consisted entirely of spaces and '.'s), in which case we use the default.
  if (filename.empty()) {
#if defined(OS_WIN)
    trimmed_trailing_character_count = 0;
#endif
    overwrite_extension = false;
    if (default_name.empty())
      filename = kFinalFallbackName;
  }

#if defined(OS_WIN)
  string16 path = UTF8ToUTF16(filename.empty() ? default_name : filename);
  // On Windows we want to preserve or replace all characters including
  // whitespace to prevent file extension obfuscation on trusted websites
  // e.g. Gmail might think evil.exe. is safe, so we don't want it to become
  // evil.exe when we download it
  string16::size_type path_length_before_trim = path.length();
  TrimWhitespace(path, TRIM_TRAILING, &path);
  trimmed_trailing_character_count += path_length_before_trim - path.length();
  file_util::ReplaceIllegalCharactersInPath(&path, '-');
  path.append(trimmed_trailing_character_count, '-');
  FilePath result(path);
  GenerateSafeFileName(mime_type, overwrite_extension, &result);
  return result.value();
#else
  std::string path = filename.empty() ? default_name : filename;
  file_util::ReplaceIllegalCharactersInPath(&path, '-');
  FilePath result(path);
  GenerateSafeFileName(mime_type, overwrite_extension, &result);
  return UTF8ToUTF16(result.value());
#endif
}

FilePath GenerateFileName(const GURL& url,
                          const std::string& content_disposition,
                          const std::string& referrer_charset,
                          const std::string& suggested_name,
                          const std::string& mime_type,
                          const std::string& default_file_name) {
  string16 file_name = GetSuggestedFilename(url,
                                            content_disposition,
                                            referrer_charset,
                                            suggested_name,
                                            mime_type,
                                            default_file_name);

#if defined(OS_WIN)
  FilePath generated_name(file_name);
#else
  FilePath generated_name(base::SysWideToNativeMB(UTF16ToWide(file_name)));
#endif

#if defined(OS_CHROMEOS)
  // When doing file manager operations on ChromeOS, the file paths get
  // normalized in WebKit layer, so let's ensure downloaded files have
  // normalized names. Otherwise, we won't be able to handle files with NFD
  // utf8 encoded characters in name.
  file_util::NormalizeFileNameEncoding(&generated_name);
#endif

  DCHECK(!generated_name.empty());

  return generated_name;
}

bool IsPortAllowedByDefault(int port) {
  int array_size = arraysize(kRestrictedPorts);
  for (int i = 0; i < array_size; i++) {
    if (kRestrictedPorts[i] == port) {
      return false;
    }
  }
  return true;
}

bool IsPortAllowedByFtp(int port) {
  int array_size = arraysize(kAllowedFtpPorts);
  for (int i = 0; i < array_size; i++) {
    if (kAllowedFtpPorts[i] == port) {
        return true;
    }
  }
  // Port not explicitly allowed by FTP, so return the default restrictions.
  return IsPortAllowedByDefault(port);
}

bool IsPortAllowedByOverride(int port) {
  if (g_explicitly_allowed_ports.Get().empty())
    return false;

  return g_explicitly_allowed_ports.Get().count(port) > 0;
}

int SetNonBlocking(int fd) {
#if defined(OS_WIN)
  unsigned long no_block = 1;
  return ioctlsocket(fd, FIONBIO, &no_block);
#elif defined(OS_POSIX)
  int flags = fcntl(fd, F_GETFL, 0);
  if (-1 == flags)
    return flags;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

bool ParseHostAndPort(std::string::const_iterator host_and_port_begin,
                      std::string::const_iterator host_and_port_end,
                      std::string* host,
                      int* port) {
  if (host_and_port_begin >= host_and_port_end)
    return false;

  // When using url_parse, we use char*.
  const char* auth_begin = &(*host_and_port_begin);
  int auth_len = host_and_port_end - host_and_port_begin;

  url_parse::Component auth_component(0, auth_len);
  url_parse::Component username_component;
  url_parse::Component password_component;
  url_parse::Component hostname_component;
  url_parse::Component port_component;

  url_parse::ParseAuthority(auth_begin, auth_component, &username_component,
      &password_component, &hostname_component, &port_component);

  // There shouldn't be a username/password.
  if (username_component.is_valid() || password_component.is_valid())
    return false;

  if (!hostname_component.is_nonempty())
    return false;  // Failed parsing.

  int parsed_port_number = -1;
  if (port_component.is_nonempty()) {
    parsed_port_number = url_parse::ParsePort(auth_begin, port_component);

    // If parsing failed, port_number will be either PORT_INVALID or
    // PORT_UNSPECIFIED, both of which are negative.
    if (parsed_port_number < 0)
      return false;  // Failed parsing the port number.
  }

  if (port_component.len == 0)
    return false;  // Reject inputs like "foo:"

  // Pass results back to caller.
  host->assign(auth_begin + hostname_component.begin, hostname_component.len);
  *port = parsed_port_number;

  return true;  // Success.
}

bool ParseHostAndPort(const std::string& host_and_port,
                      std::string* host,
                      int* port) {
  return ParseHostAndPort(
      host_and_port.begin(), host_and_port.end(), host, port);
}

std::string GetHostAndPort(const GURL& url) {
  // For IPv6 literals, GURL::host() already includes the brackets so it is
  // safe to just append a colon.
  return base::StringPrintf("%s:%d", url.host().c_str(),
                            url.EffectiveIntPort());
}

std::string GetHostAndOptionalPort(const GURL& url) {
  // For IPv6 literals, GURL::host() already includes the brackets
  // so it is safe to just append a colon.
  if (url.has_port())
    return base::StringPrintf("%s:%s", url.host().c_str(), url.port().c_str());
  return url.host();
}

std::string NetAddressToString(const struct addrinfo* net_address) {
  return NetAddressToString(net_address->ai_addr, net_address->ai_addrlen);
}

std::string NetAddressToString(const struct sockaddr* net_address,
                               socklen_t address_len) {
#if defined(OS_WIN)
  EnsureWinsockInit();
#endif

  // This buffer is large enough to fit the biggest IPv6 string.
  char buffer[INET6_ADDRSTRLEN];

  int result = getnameinfo(net_address, address_len, buffer, sizeof(buffer),
                           NULL, 0, NI_NUMERICHOST);

  if (result != 0) {
    DVLOG(1) << "getnameinfo() failed with " << result << ": "
             << gai_strerror(result);
    buffer[0] = '\0';
  }
  return std::string(buffer);
}

std::string NetAddressToStringWithPort(const struct addrinfo* net_address) {
  return NetAddressToStringWithPort(
      net_address->ai_addr, net_address->ai_addrlen);
}
std::string NetAddressToStringWithPort(const struct sockaddr* net_address,
                                       socklen_t address_len) {
  std::string ip_address_string = NetAddressToString(net_address, address_len);
  if (ip_address_string.empty())
    return std::string();  // Failed.

  int port = GetPortFromSockaddr(net_address, address_len);

  if (ip_address_string.find(':') != std::string::npos) {
    // Surround with square brackets to avoid ambiguity.
    return base::StringPrintf("[%s]:%d", ip_address_string.c_str(), port);
  }

  return base::StringPrintf("%s:%d", ip_address_string.c_str(), port);
}

std::string GetHostName() {
#if defined(OS_WIN)
  EnsureWinsockInit();
#endif

  // Host names are limited to 255 bytes.
  char buffer[256];
  int result = gethostname(buffer, sizeof(buffer));
  if (result != 0) {
    DVLOG(1) << "gethostname() failed with " << result;
    buffer[0] = '\0';
  }
  return std::string(buffer);
}

void GetIdentityFromURL(const GURL& url,
                        string16* username,
                        string16* password) {
  UnescapeRule::Type flags =
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS;
  *username = UnescapeAndDecodeUTF8URLComponent(url.username(), flags, NULL);
  *password = UnescapeAndDecodeUTF8URLComponent(url.password(), flags, NULL);
}

std::string GetHostOrSpecFromURL(const GURL& url) {
  return url.has_host() ? TrimEndingDot(url.host()) : url.spec();
}

void AppendFormattedHost(const GURL& url,
                         const std::string& languages,
                         string16* output) {
  std::vector<size_t> offsets;
  AppendFormattedComponent(url.possibly_invalid_spec(),
      url.parsed_for_possibly_invalid_spec().host, offsets,
      HostComponentTransform(languages), output, NULL, NULL);
}

string16 FormatUrlWithOffsets(const GURL& url,
                              const std::string& languages,
                              FormatUrlTypes format_types,
                              UnescapeRule::Type unescape_rules,
                              url_parse::Parsed* new_parsed,
                              size_t* prefix_end,
                              std::vector<size_t>* offsets_for_adjustment) {
  url_parse::Parsed parsed_temp;
  if (!new_parsed)
    new_parsed = &parsed_temp;
  else
    *new_parsed = url_parse::Parsed();
  std::vector<size_t> original_offsets;
  if (offsets_for_adjustment)
    original_offsets = *offsets_for_adjustment;

  // Special handling for view-source:.  Don't use chrome::kViewSourceScheme
  // because this library shouldn't depend on chrome.
  const char* const kViewSource = "view-source";
  // Reject "view-source:view-source:..." to avoid deep recursion.
  const char* const kViewSourceTwice = "view-source:view-source:";
  if (url.SchemeIs(kViewSource) &&
      !StartsWithASCII(url.possibly_invalid_spec(), kViewSourceTwice, false)) {
    return FormatViewSourceUrl(url, original_offsets, languages, format_types,
        unescape_rules, new_parsed, prefix_end, offsets_for_adjustment);
  }

  // We handle both valid and invalid URLs (this will give us the spec
  // regardless of validity).
  const std::string& spec = url.possibly_invalid_spec();
  const url_parse::Parsed& parsed = url.parsed_for_possibly_invalid_spec();

  // Scheme & separators.  These are ASCII.
  string16 url_string;
  url_string.insert(url_string.end(), spec.begin(),
      spec.begin() + parsed.CountCharactersBefore(url_parse::Parsed::USERNAME,
                                                  true));
  const char kHTTP[] = "http://";
  const char kFTP[] = "ftp.";
  // URLFixerUpper::FixupURL() treats "ftp.foo.com" as ftp://ftp.foo.com.  This
  // means that if we trim "http://" off a URL whose host starts with "ftp." and
  // the user inputs this into any field subject to fixup (which is basically
  // all input fields), the meaning would be changed.  (In fact, often the
  // formatted URL is directly pre-filled into an input field.)  For this reason
  // we avoid stripping "http://" in this case.
  bool omit_http = (format_types & kFormatUrlOmitHTTP) &&
      EqualsASCII(url_string, kHTTP) &&
      !StartsWithASCII(url.host(), kFTP, true);
  new_parsed->scheme = parsed.scheme;

  // Username & password.
  if ((format_types & kFormatUrlOmitUsernamePassword) != 0) {
    // Remove the username and password fields. We don't want to display those
    // to the user since they can be used for attacks,
    // e.g. "http://google.com:search@evil.ru/"
    new_parsed->username.reset();
    new_parsed->password.reset();
    // Update the offsets based on removed username and/or password.
    if (offsets_for_adjustment && !offsets_for_adjustment->empty() &&
        (parsed.username.is_nonempty() || parsed.password.is_nonempty())) {
      OffsetAdjuster offset_adjuster(offsets_for_adjustment);
      if (parsed.username.is_nonempty() && parsed.password.is_nonempty()) {
        // The seeming off-by-one and off-by-two in these first two lines are to
        // account for the ':' after the username and '@' after the password.
        offset_adjuster.Add(OffsetAdjuster::Adjustment(
            static_cast<size_t>(parsed.username.begin),
            static_cast<size_t>(parsed.username.len + parsed.password.len + 2),
            0));
      } else {
        const url_parse::Component* nonempty_component =
            parsed.username.is_nonempty() ? &parsed.username : &parsed.password;
        // The seeming off-by-one in below is to account for the '@' after the
        // username/password.
        offset_adjuster.Add(OffsetAdjuster::Adjustment(
            static_cast<size_t>(nonempty_component->begin),
            static_cast<size_t>(nonempty_component->len + 1), 0));
      }
    }
  } else {
    AppendFormattedComponent(spec, parsed.username, original_offsets,
        NonHostComponentTransform(unescape_rules), &url_string,
        &new_parsed->username, offsets_for_adjustment);
    if (parsed.password.is_valid()) {
      size_t colon = parsed.username.end();
      DCHECK_EQ(static_cast<size_t>(parsed.password.begin - 1), colon);
      std::vector<size_t>::const_iterator colon_iter =
          std::find(original_offsets.begin(), original_offsets.end(), colon);
      if (colon_iter != original_offsets.end()) {
        (*offsets_for_adjustment)[colon_iter - original_offsets.begin()] =
            url_string.length();
      }
      url_string.push_back(':');
    }
    AppendFormattedComponent(spec, parsed.password, original_offsets,
        NonHostComponentTransform(unescape_rules), &url_string,
        &new_parsed->password, offsets_for_adjustment);
    if (parsed.username.is_valid() || parsed.password.is_valid()) {
      size_t at_sign = (parsed.password.is_valid() ?
          parsed.password : parsed.username).end();
      DCHECK_EQ(static_cast<size_t>(parsed.host.begin - 1), at_sign);
      std::vector<size_t>::const_iterator at_sign_iter =
          std::find(original_offsets.begin(), original_offsets.end(), at_sign);
      if (at_sign_iter != original_offsets.end()) {
        (*offsets_for_adjustment)[at_sign_iter - original_offsets.begin()] =
            url_string.length();
      }
      url_string.push_back('@');
    }
  }
  if (prefix_end)
    *prefix_end = static_cast<size_t>(url_string.length());

  // Host.
  AppendFormattedComponent(spec, parsed.host, original_offsets,
      HostComponentTransform(languages), &url_string, &new_parsed->host,
      offsets_for_adjustment);

  // Port.
  if (parsed.port.is_nonempty()) {
    url_string.push_back(':');
    new_parsed->port.begin = url_string.length();
    url_string.insert(url_string.end(),
                      spec.begin() + parsed.port.begin,
                      spec.begin() + parsed.port.end());
    new_parsed->port.len = url_string.length() - new_parsed->port.begin;
  } else {
    new_parsed->port.reset();
  }

  // Path & query.  Both get the same general unescape & convert treatment.
  if (!(format_types & kFormatUrlOmitTrailingSlashOnBareHostname) ||
      !CanStripTrailingSlash(url)) {
    AppendFormattedComponent(spec, parsed.path, original_offsets,
        NonHostComponentTransform(unescape_rules), &url_string,
        &new_parsed->path, offsets_for_adjustment);
  }
  if (parsed.query.is_valid())
    url_string.push_back('?');
  AppendFormattedComponent(spec, parsed.query, original_offsets,
      NonHostComponentTransform(unescape_rules), &url_string,
      &new_parsed->query, offsets_for_adjustment);

  // Ref.  This is valid, unescaped UTF-8, so we can just convert.
  if (parsed.ref.is_valid()) {
    url_string.push_back('#');
    size_t original_ref_begin = static_cast<size_t>(parsed.ref.begin);
    size_t output_ref_begin = url_string.length();
    new_parsed->ref.begin = static_cast<int>(output_ref_begin);

    std::vector<size_t> offsets_into_ref(
        OffsetsIntoComponent(original_offsets, original_ref_begin));
    if (parsed.ref.len > 0) {
      url_string.append(UTF8ToUTF16AndAdjustOffsets(
          spec.substr(original_ref_begin, static_cast<size_t>(parsed.ref.len)),
          &offsets_into_ref));
    }

    new_parsed->ref.len =
        static_cast<int>(url_string.length() - new_parsed->ref.begin);
    AdjustForComponentTransform(original_offsets, original_ref_begin,
        static_cast<size_t>(parsed.ref.end()), offsets_into_ref,
        output_ref_begin, offsets_for_adjustment);
  }

  // If we need to strip out http do it after the fact. This way we don't need
  // to worry about how offset_for_adjustment is interpreted.
  if (omit_http && StartsWith(url_string, ASCIIToUTF16(kHTTP), true)) {
    const size_t kHTTPSize = arraysize(kHTTP) - 1;
    url_string = url_string.substr(kHTTPSize);
    if (offsets_for_adjustment && !offsets_for_adjustment->empty()) {
      OffsetAdjuster offset_adjuster(offsets_for_adjustment);
      offset_adjuster.Add(OffsetAdjuster::Adjustment(0, kHTTPSize, 0));
    }
    if (prefix_end)
      *prefix_end -= kHTTPSize;

    // Adjust new_parsed.
    DCHECK(new_parsed->scheme.is_valid());
    int delta = -(new_parsed->scheme.len + 3);  // +3 for ://.
    new_parsed->scheme.reset();
    AdjustComponents(delta, new_parsed);
  }

  LimitOffsets(url_string, offsets_for_adjustment);
  return url_string;
}

string16 FormatUrl(const GURL& url,
                   const std::string& languages,
                   FormatUrlTypes format_types,
                   UnescapeRule::Type unescape_rules,
                   url_parse::Parsed* new_parsed,
                   size_t* prefix_end,
                   size_t* offset_for_adjustment) {
  std::vector<size_t> offsets;
  if (offset_for_adjustment)
    offsets.push_back(*offset_for_adjustment);
  string16 result = FormatUrlWithOffsets(url, languages, format_types,
      unescape_rules, new_parsed, prefix_end, &offsets);
  if (offset_for_adjustment)
    *offset_for_adjustment = offsets[0];
  return result;
}

bool CanStripTrailingSlash(const GURL& url) {
  // Omit the path only for standard, non-file URLs with nothing but "/" after
  // the hostname.
  return url.IsStandard() && !url.SchemeIsFile() &&
      !url.SchemeIsFileSystem() && !url.has_query() && !url.has_ref()
      && url.path() == "/";
}

GURL SimplifyUrlForRequest(const GURL& url) {
  DCHECK(url.is_valid());
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

// Specifies a comma separated list of port numbers that should be accepted
// despite bans. If the string is invalid no allowed ports are stored.
void SetExplicitlyAllowedPorts(const std::string& allowed_ports) {
  if (allowed_ports.empty())
    return;

  std::multiset<int> ports;
  size_t last = 0;
  size_t size = allowed_ports.size();
  // The comma delimiter.
  const std::string::value_type kComma = ',';

  // Overflow is still possible for evil user inputs.
  for (size_t i = 0; i <= size; ++i) {
    // The string should be composed of only digits and commas.
    if (i != size && !IsAsciiDigit(allowed_ports[i]) &&
        (allowed_ports[i] != kComma))
      return;
    if (i == size || allowed_ports[i] == kComma) {
      if (i > last) {
        int port;
        base::StringToInt(base::StringPiece(allowed_ports.begin() + last,
                                            allowed_ports.begin() + i),
                          &port);
        ports.insert(port);
      }
      last = i + 1;
    }
  }
  g_explicitly_allowed_ports.Get() = ports;
}

ScopedPortException::ScopedPortException(int port) : port_(port) {
  g_explicitly_allowed_ports.Get().insert(port);
}

ScopedPortException::~ScopedPortException() {
  std::multiset<int>::iterator it =
      g_explicitly_allowed_ports.Get().find(port_);
  if (it != g_explicitly_allowed_ports.Get().end())
    g_explicitly_allowed_ports.Get().erase(it);
  else
    NOTREACHED();
}

enum IPv6SupportStatus {
  IPV6_CANNOT_CREATE_SOCKETS,
  IPV6_CAN_CREATE_SOCKETS,
  IPV6_GETIFADDRS_FAILED,
  IPV6_GLOBAL_ADDRESS_MISSING,
  IPV6_GLOBAL_ADDRESS_PRESENT,
  IPV6_INTERFACE_ARRAY_TOO_SHORT,
  IPV6_SUPPORT_MAX  // Bounding values for enumeration.
};

static void IPv6SupportResults(IPv6SupportStatus result) {
  static bool run_once = false;
  if (!run_once) {
    run_once = true;
    UMA_HISTOGRAM_ENUMERATION("Net.IPv6Status", result, IPV6_SUPPORT_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Net.IPv6Status_retest", result,
                              IPV6_SUPPORT_MAX);
  }
}

// TODO(jar): The following is a simple estimate of IPv6 support.  We may need
// to do a test resolution, and a test connection, to REALLY verify support.
// static
bool IPv6Supported() {
#if defined(OS_ANDROID)
  // TODO: We should fully implement IPv6 probe once 'getifaddrs' API available;
  // Another approach is implementing the similar feature by
  // java.net.NetworkInterface through JNI.
  NOTIMPLEMENTED();
  // so we don't get a 'defined but not used' warning/err
  IPv6SupportResults(IPV6_GETIFADDRS_FAILED);
  return true;
#elif defined(OS_POSIX)
  int test_socket = socket(AF_INET6, SOCK_STREAM, 0);
  if (test_socket == -1) {
    IPv6SupportResults(IPV6_CANNOT_CREATE_SOCKETS);
    return false;
  }
  close(test_socket);

  // Check to see if any interface has a IPv6 address.
  struct ifaddrs* interface_addr = NULL;
  int rv = getifaddrs(&interface_addr);
  if (rv != 0) {
     IPv6SupportResults(IPV6_GETIFADDRS_FAILED);
     return true;  // Don't yet block IPv6.
  }

  bool found_ipv6 = false;
  for (struct ifaddrs* interface = interface_addr;
       interface != NULL;
       interface = interface->ifa_next) {
    if (!(IFF_UP & interface->ifa_flags))
      continue;
    if (IFF_LOOPBACK & interface->ifa_flags)
      continue;
    struct sockaddr* addr = interface->ifa_addr;
    if (!addr)
      continue;
    if (addr->sa_family != AF_INET6)
      continue;
    // Safe cast since this is AF_INET6.
    struct sockaddr_in6* addr_in6 =
        reinterpret_cast<struct sockaddr_in6*>(addr);
    struct in6_addr* sin6_addr = &addr_in6->sin6_addr;
    if (IN6_IS_ADDR_LOOPBACK(sin6_addr) || IN6_IS_ADDR_LINKLOCAL(sin6_addr))
      continue;
    found_ipv6 = true;
    break;
  }
  freeifaddrs(interface_addr);
  if (!found_ipv6) {
    IPv6SupportResults(IPV6_GLOBAL_ADDRESS_MISSING);
    return false;
  }

  IPv6SupportResults(IPV6_GLOBAL_ADDRESS_PRESENT);
  return true;
#elif defined(OS_WIN)
  EnsureWinsockInit();
  SOCKET test_socket = socket(AF_INET6, SOCK_STREAM, 0);
  if (test_socket == INVALID_SOCKET) {
    IPv6SupportResults(IPV6_CANNOT_CREATE_SOCKETS);
    return false;
  }
  closesocket(test_socket);

  // Check to see if any interface has a IPv6 address.
  // The GetAdaptersAddresses MSDN page recommends using a size of 15000 to
  // avoid reallocation.
  ULONG adapters_size = 15000;
  scoped_ptr_malloc<IP_ADAPTER_ADDRESSES> adapters;
  ULONG error;
  int num_tries = 0;
  do {
    adapters.reset(
        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(adapters_size)));
    // Return only unicast addresses.
    error = GetAdaptersAddresses(AF_UNSPEC,
                                 GAA_FLAG_SKIP_ANYCAST |
                                 GAA_FLAG_SKIP_MULTICAST |
                                 GAA_FLAG_SKIP_DNS_SERVER |
                                 GAA_FLAG_SKIP_FRIENDLY_NAME,
                                 NULL, adapters.get(), &adapters_size);
    num_tries++;
  } while (error == ERROR_BUFFER_OVERFLOW && num_tries <= 3);
  if (error == ERROR_NO_DATA) {
    IPv6SupportResults(IPV6_GLOBAL_ADDRESS_MISSING);
    return false;
  }
  if (error != ERROR_SUCCESS) {
    IPv6SupportResults(IPV6_GETIFADDRS_FAILED);
    return true;  // Don't yet block IPv6.
  }

  PIP_ADAPTER_ADDRESSES adapter;
  for (adapter = adapters.get(); adapter; adapter = adapter->Next) {
    if (adapter->OperStatus != IfOperStatusUp)
      continue;
    if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
      continue;
    PIP_ADAPTER_UNICAST_ADDRESS unicast_address;
    for (unicast_address = adapter->FirstUnicastAddress;
         unicast_address;
         unicast_address = unicast_address->Next) {
      if (unicast_address->Address.lpSockaddr->sa_family != AF_INET6)
        continue;
      // Safe cast since this is AF_INET6.
      struct sockaddr_in6* addr_in6 = reinterpret_cast<struct sockaddr_in6*>(
          unicast_address->Address.lpSockaddr);
      struct in6_addr* sin6_addr = &addr_in6->sin6_addr;
      if (IN6_IS_ADDR_LOOPBACK(sin6_addr) || IN6_IS_ADDR_LINKLOCAL(sin6_addr))
        continue;
      IPv6SupportResults(IPV6_GLOBAL_ADDRESS_PRESENT);
      return true;
    }
  }

  IPv6SupportResults(IPV6_GLOBAL_ADDRESS_MISSING);
  return false;
#else
  NOTIMPLEMENTED();
  return true;
#endif  // defined(various platforms)
}

bool HaveOnlyLoopbackAddresses() {
#if defined(OS_POSIX) && !defined(OS_ANDROID)
  struct ifaddrs* interface_addr = NULL;
  int rv = getifaddrs(&interface_addr);
  if (rv != 0) {
    DVLOG(1) << "getifaddrs() failed with errno = " << errno;
    return false;
  }

  bool result = true;
  for (struct ifaddrs* interface = interface_addr;
       interface != NULL;
       interface = interface->ifa_next) {
    if (!(IFF_UP & interface->ifa_flags))
      continue;
    if (IFF_LOOPBACK & interface->ifa_flags)
      continue;
    const struct sockaddr* addr = interface->ifa_addr;
    if (!addr)
      continue;
    if (addr->sa_family == AF_INET6) {
      // Safe cast since this is AF_INET6.
      const struct sockaddr_in6* addr_in6 =
          reinterpret_cast<const struct sockaddr_in6*>(addr);
      const struct in6_addr* sin6_addr = &addr_in6->sin6_addr;
      if (IN6_IS_ADDR_LOOPBACK(sin6_addr) || IN6_IS_ADDR_LINKLOCAL(sin6_addr))
        continue;
    }
    if (addr->sa_family != AF_INET6 && addr->sa_family != AF_INET)
      continue;

    result = false;
    break;
  }
  freeifaddrs(interface_addr);
  return result;
#elif defined(OS_WIN)
  // TODO(wtc): implement with the GetAdaptersAddresses function.
  NOTIMPLEMENTED();
  return false;
#else
  NOTIMPLEMENTED();
  return false;
#endif  // defined(various platforms)
}

bool ParseIPLiteralToNumber(const std::string& ip_literal,
                            IPAddressNumber* ip_number) {
  // |ip_literal| could be either a IPv4 or an IPv6 literal. If it contains
  // a colon however, it must be an IPv6 address.
  if (ip_literal.find(':') != std::string::npos) {
    // GURL expects IPv6 hostnames to be surrounded with brackets.
    std::string host_brackets = "[" + ip_literal + "]";
    url_parse::Component host_comp(0, host_brackets.size());

    // Try parsing the hostname as an IPv6 literal.
    ip_number->resize(16);  // 128 bits.
    return url_canon::IPv6AddressToNumber(host_brackets.data(),
                                          host_comp,
                                          &(*ip_number)[0]);
  }

  // Otherwise the string is an IPv4 address.
  ip_number->resize(4);  // 32 bits.
  url_parse::Component host_comp(0, ip_literal.size());
  int num_components;
  url_canon::CanonHostInfo::Family family = url_canon::IPv4AddressToNumber(
      ip_literal.data(), host_comp, &(*ip_number)[0], &num_components);
  return family == url_canon::CanonHostInfo::IPV4;
}

IPAddressNumber ConvertIPv4NumberToIPv6Number(
    const IPAddressNumber& ipv4_number) {
  DCHECK(ipv4_number.size() == 4);

  // IPv4-mapped addresses are formed by:
  // <80 bits of zeros>  + <16 bits of ones> + <32-bit IPv4 address>.
  IPAddressNumber ipv6_number;
  ipv6_number.reserve(16);
  ipv6_number.insert(ipv6_number.end(), 10, 0);
  ipv6_number.push_back(0xFF);
  ipv6_number.push_back(0xFF);
  ipv6_number.insert(ipv6_number.end(), ipv4_number.begin(), ipv4_number.end());
  return ipv6_number;
}

bool ParseCIDRBlock(const std::string& cidr_literal,
                    IPAddressNumber* ip_number,
                    size_t* prefix_length_in_bits) {
  // We expect CIDR notation to match one of these two templates:
  //   <IPv4-literal> "/" <number of bits>
  //   <IPv6-literal> "/" <number of bits>

  std::vector<std::string> parts;
  base::SplitString(cidr_literal, '/', &parts);
  if (parts.size() != 2)
    return false;

  // Parse the IP address.
  if (!ParseIPLiteralToNumber(parts[0], ip_number))
    return false;

  // Parse the prefix length.
  int number_of_bits = -1;
  if (!base::StringToInt(parts[1], &number_of_bits))
    return false;

  // Make sure the prefix length is in a valid range.
  if (number_of_bits < 0 ||
      number_of_bits > static_cast<int>(ip_number->size() * 8))
    return false;

  *prefix_length_in_bits = static_cast<size_t>(number_of_bits);
  return true;
}

bool IPNumberMatchesPrefix(const IPAddressNumber& ip_number,
                           const IPAddressNumber& ip_prefix,
                           size_t prefix_length_in_bits) {
  // Both the input IP address and the prefix IP address should be
  // either IPv4 or IPv6.
  DCHECK(ip_number.size() == 4 || ip_number.size() == 16);
  DCHECK(ip_prefix.size() == 4 || ip_prefix.size() == 16);

  DCHECK_LE(prefix_length_in_bits, ip_prefix.size() * 8);

  // In case we have an IPv6 / IPv4 mismatch, convert the IPv4 addresses to
  // IPv6 addresses in order to do the comparison.
  if (ip_number.size() != ip_prefix.size()) {
    if (ip_number.size() == 4) {
      return IPNumberMatchesPrefix(ConvertIPv4NumberToIPv6Number(ip_number),
                                   ip_prefix, prefix_length_in_bits);
    }
    return IPNumberMatchesPrefix(ip_number,
                                 ConvertIPv4NumberToIPv6Number(ip_prefix),
                                 96 + prefix_length_in_bits);
  }

  // Otherwise we are comparing two IPv4 addresses, or two IPv6 addresses.
  // Compare all the bytes that fall entirely within the prefix.
  int num_entire_bytes_in_prefix = prefix_length_in_bits / 8;
  for (int i = 0; i < num_entire_bytes_in_prefix; ++i) {
    if (ip_number[i] != ip_prefix[i])
      return false;
  }

  // In case the prefix was not a multiple of 8, there will be 1 byte
  // which is only partially masked.
  int remaining_bits = prefix_length_in_bits % 8;
  if (remaining_bits != 0) {
    unsigned char mask = 0xFF << (8 - remaining_bits);
    int i = num_entire_bytes_in_prefix;
    if ((ip_number[i] & mask) != (ip_prefix[i] & mask))
      return false;
  }

  return true;
}

const uint16* GetPortFieldFromSockaddr(const struct sockaddr* address,
                                       socklen_t address_len) {
  if (address->sa_family == AF_INET) {
    DCHECK_LE(sizeof(sockaddr_in), static_cast<size_t>(address_len));
    const struct sockaddr_in* sockaddr =
        reinterpret_cast<const struct sockaddr_in*>(address);
    return &sockaddr->sin_port;
  } else if (address->sa_family == AF_INET6) {
    DCHECK_LE(sizeof(sockaddr_in6), static_cast<size_t>(address_len));
    const struct sockaddr_in6* sockaddr =
        reinterpret_cast<const struct sockaddr_in6*>(address);
    return &sockaddr->sin6_port;
  } else {
    NOTREACHED();
    return NULL;
  }
}

int GetPortFromSockaddr(const struct sockaddr* address, socklen_t address_len) {
  const uint16* port_field = GetPortFieldFromSockaddr(address, address_len);
  if (!port_field)
    return -1;
  return base::NetToHost16(*port_field);
}

bool IsLocalhost(const std::string& host) {
  if (host == "localhost" ||
      host == "localhost.localdomain" ||
      host == "localhost6" ||
      host == "localhost6.localdomain6")
    return true;

  IPAddressNumber ip_number;
  if (ParseIPLiteralToNumber(host, &ip_number)) {
    size_t size = ip_number.size();
    switch (size) {
      case kIPv4AddressSize: {
        IPAddressNumber localhost_prefix;
        localhost_prefix.push_back(127);
        for (int i = 0; i < 3; ++i) {
          localhost_prefix.push_back(0);
        }
        return IPNumberMatchesPrefix(ip_number, localhost_prefix, 8);
      }

      case kIPv6AddressSize: {
        struct in6_addr sin6_addr;
        memcpy(&sin6_addr, &ip_number[0], kIPv6AddressSize);
        return !!IN6_IS_ADDR_LOOPBACK(&sin6_addr);
      }

      default:
        NOTREACHED();
    }
  }

  return false;
}

NetworkInterface::NetworkInterface() {
}

NetworkInterface::NetworkInterface(const std::string& name,
                                   const IPAddressNumber& address)
    : name(name), address(address) {
}

NetworkInterface::~NetworkInterface() {
}

}  // namespace net
