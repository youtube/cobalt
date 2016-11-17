// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File utilities that use the ICU library go in this file.

#include "base/i18n/file_util_icu.h"

#include "base/file_path.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/sys_string_conversions.h"
#include "build/build_config.h"
#include "unicode/coll.h"
#include "unicode/uniset.h"

namespace {

class IllegalCharacters {
 public:
  static IllegalCharacters* GetInstance() {
    return Singleton<IllegalCharacters>::get();
  }

  bool contains(UChar32 ucs4) {
    return !!set->contains(ucs4);
  }

  bool containsNone(const string16 &s) {
    return !!set->containsNone(icu::UnicodeString(s.c_str(), s.size()));
  }

 private:
  friend class Singleton<IllegalCharacters>;
  friend struct DefaultSingletonTraits<IllegalCharacters>;

  IllegalCharacters();
  ~IllegalCharacters() { }

  scoped_ptr<icu::UnicodeSet> set;

  DISALLOW_COPY_AND_ASSIGN(IllegalCharacters);
};

IllegalCharacters::IllegalCharacters() {
  UErrorCode status = U_ZERO_ERROR;
  // Control characters, formatting characters, non-characters, and
  // some printable ASCII characters regarded as dangerous ('"*/:<>?\\').
  // See  http://blogs.msdn.com/michkap/archive/2006/11/03/941420.aspx
  // and http://msdn2.microsoft.com/en-us/library/Aa365247.aspx
  // TODO(jungshik): Revisit the set. ZWJ and ZWNJ are excluded because they
  // are legitimate in Arabic and some S/SE Asian scripts. However, when used
  // elsewhere, they can be confusing/problematic.
  // Also, consider wrapping the set with our Singleton class to create and
  // freeze it only once. Note that there's a trade-off between memory and
  // speed.
#if defined(WCHAR_T_IS_UTF16)
  set.reset(new icu::UnicodeSet(icu::UnicodeString(
      L"[[\"*/:<>?\\\\|][:Cc:][:Cf:] - [\u200c\u200d]]"), status));
#else
  set.reset(new icu::UnicodeSet(UNICODE_STRING_SIMPLE(
      "[[\"*/:<>?\\\\|][:Cc:][:Cf:] - [\\u200c\\u200d]]").unescape(),
      status));
#endif
  DCHECK(U_SUCCESS(status));
  // Add non-characters. If this becomes a performance bottleneck by
  // any chance, do not add these to |set| and change IsFilenameLegal()
  // to check |ucs4 & 0xFFFEu == 0xFFFEu|, in addiition to calling
  // containsNone().
  set->add(0xFDD0, 0xFDEF);
  for (int i = 0; i <= 0x10; ++i) {
    int plane_base = 0x10000 * i;
    set->add(plane_base + 0xFFFE, plane_base + 0xFFFF);
  }
  set->freeze();
}

class LocaleAwareComparator {
 public:
  static LocaleAwareComparator* GetInstance() {
    return Singleton<LocaleAwareComparator,
                     LeakySingletonTraits<LocaleAwareComparator> >::get();
  }

  // Note: A similar function is available in l10n_util.
  // We cannot use it because base should not depend on l10n_util.
  // TODO(yuzo): Move some of l10n_util to base.
  int Compare(const string16& a, const string16& b) {
    // We are not sure if Collator::compare is thread-safe.
    // Use an AutoLock just in case.
    base::AutoLock auto_lock(lock_);

    UErrorCode error_code = U_ZERO_ERROR;
    UCollationResult result = collator_->compare(
        static_cast<const UChar*>(a.c_str()),
        static_cast<int>(a.length()),
        static_cast<const UChar*>(b.c_str()),
        static_cast<int>(b.length()),
        error_code);
    DCHECK(U_SUCCESS(error_code));
    return result;
  }

 private:
  friend struct DefaultSingletonTraits<LocaleAwareComparator>;

  LocaleAwareComparator() {
    UErrorCode error_code = U_ZERO_ERROR;
    // Use the default collator. The default locale should have been properly
    // set by the time this constructor is called.
    collator_.reset(icu::Collator::createInstance(error_code));
    DCHECK(U_SUCCESS(error_code));
    // Make it case-sensitive.
    collator_->setStrength(icu::Collator::TERTIARY);
    // Note: We do not set UCOL_NORMALIZATION_MODE attribute. In other words, we
    // do not pay performance penalty to guarantee sort order correctness for
    // non-FCD (http://unicode.org/notes/tn5/#FCD) file names. This should be a
    // reasonable tradeoff because such file names should be rare and the sort
    // order doesn't change much anyway.
  }

  scoped_ptr<icu::Collator> collator_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(LocaleAwareComparator);
};

}  // namespace

namespace file_util {

bool IsFilenameLegal(const string16& file_name) {
  return IllegalCharacters::GetInstance()->containsNone(file_name);
}

void ReplaceIllegalCharactersInPath(FilePath::StringType* file_name,
                                    char replace_char) {
  DCHECK(file_name);

  DCHECK(!(IllegalCharacters::GetInstance()->contains(replace_char)));

  // Remove leading and trailing whitespace.
  TrimWhitespace(*file_name, TRIM_ALL, file_name);

  IllegalCharacters* illegal = IllegalCharacters::GetInstance();
  int cursor = 0;  // The ICU macros expect an int.
  while (cursor < static_cast<int>(file_name->size())) {
    int char_begin = cursor;
    uint32 code_point;
#if defined(OS_MACOSX)
    // Mac uses UTF-8 encoding for filenames.
    U8_NEXT(file_name->data(), cursor, static_cast<int>(file_name->length()),
            code_point);
#elif defined(OS_WIN)
    // Windows uses UTF-16 encoding for filenames.
    U16_NEXT(file_name->data(), cursor, static_cast<int>(file_name->length()),
             code_point);
#elif defined(OS_POSIX) || defined(OS_STARBOARD)
    // Linux doesn't actually define an encoding. It basically allows anything
    // except for a few special ASCII characters.
    unsigned char cur_char = static_cast<unsigned char>((*file_name)[cursor++]);
    if (cur_char >= 0x80)
      continue;
    code_point = cur_char;
#else
    NOTREACHED();
#endif

    if (illegal->contains(code_point)) {
      file_name->replace(char_begin, cursor - char_begin, 1, replace_char);
      // We just made the potentially multi-byte/word char into one that only
      // takes one byte/word, so need to adjust the cursor to point to the next
      // character again.
      cursor = char_begin + 1;
    }
  }
}

bool LocaleAwareCompareFilenames(const FilePath& a, const FilePath& b) {
#if defined(OS_WIN)
  return LocaleAwareComparator::GetInstance()->Compare(a.value().c_str(),
                                                       b.value().c_str()) < 0;

#elif defined(OS_POSIX) || defined(OS_STARBOARD)
  // On linux, the file system encoding is not defined. We assume
  // SysNativeMBToWide takes care of it.
  //
  // ICU's collator can take strings in OS native encoding. But we convert the
  // strings to UTF-16 ourselves to ensure conversion consistency.
  // TODO(yuzo): Perhaps we should define SysNativeMBToUTF16?
  return LocaleAwareComparator::GetInstance()->Compare(
      WideToUTF16(base::SysNativeMBToWide(a.value().c_str())),
      WideToUTF16(base::SysNativeMBToWide(b.value().c_str()))) < 0;
#else
  #error Not implemented on your system
#endif
}

void NormalizeFileNameEncoding(FilePath* file_name) {
#if defined(OS_CHROMEOS)
  std::string normalized_str;
  if (base::ConvertToUtf8AndNormalize(file_name->BaseName().value(),
                                      base::kCodepageUTF8,
                                      &normalized_str)) {
    *file_name = file_name->DirName().Append(FilePath(normalized_str));
  }
#endif
}

}  // namespace
