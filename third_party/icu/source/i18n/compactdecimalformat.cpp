// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

<<<<<<< HEAD
#include "starboard/client_porting/poem/stdlib_poem.h"
#include "starboard/client_porting/poem/string_poem.h"
#include "charstr.h"
#include "cstring.h"
#include "digitlst.h"
#include "mutex.h"
#include "unicode/compactdecimalformat.h"
#include "unicode/numsys.h"
#include "unicode/plurrule.h"
#include "unicode/ures.h"
#include "ucln_in.h"
#include "uhash.h"
#include "umutex.h"
#include "unicode/ures.h"
#include "uresimp.h"

// Maps locale name to CDFLocaleData struct.
static UHashtable* gCompactDecimalData = NULL;
static UMutex gCompactDecimalMetaLock = U_MUTEX_INITIALIZER;

U_NAMESPACE_BEGIN

static const int32_t MAX_DIGITS = 15;
static const char gOther[] = "other";
static const char gLatnTag[] = "latn";
static const char gNumberElementsTag[] = "NumberElements";
static const char gDecimalFormatTag[] = "decimalFormat";
static const char gPatternsShort[] = "patternsShort";
static const char gPatternsLong[] = "patternsLong";
static const char gRoot[] = "root";

static const UChar u_0 = 0x30;
static const UChar u_apos = 0x27;

static const UChar kZero[] = {u_0};

// Used to unescape single quotes.
enum QuoteState {
  OUTSIDE,
  INSIDE_EMPTY,
  INSIDE_FULL
};

enum FallbackFlags {
  ANY = 0,
  MUST = 1,
  NOT_ROOT = 2
  // Next one will be 4 then 6 etc.
};


// CDFUnit represents a prefix-suffix pair for a particular variant
// and log10 value.
struct CDFUnit : public UMemory {
  UnicodeString prefix;
  UnicodeString suffix;
  inline CDFUnit() : prefix(), suffix() {
    prefix.setToBogus();
  }
  inline ~CDFUnit() {}
  inline UBool isSet() const {
    return !prefix.isBogus();
  }
  inline void markAsSet() {
    prefix.remove();
  }
};

// CDFLocaleStyleData contains formatting data for a particular locale
// and style.
class CDFLocaleStyleData : public UMemory {
 public:
  // What to divide by for each log10 value when formatting. These values
  // will be powers of 10. For English, would be:
  // 1, 1, 1, 1000, 1000, 1000, 1000000, 1000000, 1000000, 1000000000 ...
  double divisors[MAX_DIGITS];
  // Maps plural variants to CDFUnit[MAX_DIGITS] arrays.
  // To format a number x,
  // first compute log10(x). Compute displayNum = (x / divisors[log10(x)]).
  // Compute the plural variant for displayNum
  // (e.g zero, one, two, few, many, other).
  // Compute cdfUnits = unitsByVariant[pluralVariant].
  // Prefix and suffix to use at cdfUnits[log10(x)]
  UHashtable* unitsByVariant;
  inline CDFLocaleStyleData() : unitsByVariant(NULL) {}
  ~CDFLocaleStyleData();
  // Init initializes this object.
  void Init(UErrorCode& status);
  inline UBool isBogus() const {
    return unitsByVariant == NULL;
  }
  void setToBogus();
 private:
  CDFLocaleStyleData(const CDFLocaleStyleData&);
  CDFLocaleStyleData& operator=(const CDFLocaleStyleData&);
};

// CDFLocaleData contains formatting data for a particular locale.
struct CDFLocaleData : public UMemory {
  CDFLocaleStyleData shortData;
  CDFLocaleStyleData longData;
  inline CDFLocaleData() : shortData(), longData() { }
  inline ~CDFLocaleData() { }
  // Init initializes this object.
  void Init(UErrorCode& status);
};

U_NAMESPACE_END
=======
// Allow implicit conversion from char16_t* to UnicodeString for this file:
// Helpful in toString methods and elsewhere.
#define UNISTR_FROM_STRING_EXPLICIT
>>>>>>> 047a7134fa7a3ed5d506179d439db144bf326e70

#include "unicode/compactdecimalformat.h"
#include "number_mapper.h"
#include "number_decimfmtprops.h"

using namespace icu;


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CompactDecimalFormat)


CompactDecimalFormat*
CompactDecimalFormat::createInstance(const Locale& inLocale, UNumberCompactStyle style,
                                     UErrorCode& status) {
    return new CompactDecimalFormat(inLocale, style, status);
}

CompactDecimalFormat::CompactDecimalFormat(const Locale& inLocale, UNumberCompactStyle style,
                                           UErrorCode& status)
        : DecimalFormat(new DecimalFormatSymbols(inLocale, status), status) {
    if (U_FAILURE(status)) return;
    // Minimal properties: let the non-shim code path do most of the logic for us.
    fields->properties.compactStyle = style;
    fields->properties.groupingSize = -2; // do not forward grouping information
    fields->properties.minimumGroupingDigits = 2;
    touch(status);
}

CompactDecimalFormat::CompactDecimalFormat(const CompactDecimalFormat& source) = default;

CompactDecimalFormat::~CompactDecimalFormat() = default;

CompactDecimalFormat& CompactDecimalFormat::operator=(const CompactDecimalFormat& rhs) {
    DecimalFormat::operator=(rhs);
    return *this;
}

CompactDecimalFormat* CompactDecimalFormat::clone() const {
    return new CompactDecimalFormat(*this);
}

void
CompactDecimalFormat::parse(
        const UnicodeString& /* text */,
        Formattable& /* result */,
        ParsePosition& /* parsePosition */) const {
}

void
CompactDecimalFormat::parse(
        const UnicodeString& /* text */,
        Formattable& /* result */,
        UErrorCode& status) const {
    status = U_UNSUPPORTED_ERROR;
}

CurrencyAmount*
CompactDecimalFormat::parseCurrency(
        const UnicodeString& /* text */,
        ParsePosition& /* pos */) const {
    return nullptr;
}


#endif /* #if !UCONFIG_NO_FORMATTING */
