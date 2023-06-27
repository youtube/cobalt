// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/**
 *******************************************************************************
 * Copyright (C) 2001-2014, International Business Machines Corporation and    *
 * others. All Rights Reserved.                                                *
 *******************************************************************************
 *
 *******************************************************************************
 */
#include "third_party/icu/source/common/unicode/utypes.h"

#if !UCONFIG_NO_SERVICE

#if defined(STARBOARD)
#include "starboard/client_porting/poem/string_poem.h"
#endif  // defined(STARBOARD)
#include "third_party/icu/source/common/unicode/resbund.h"
#include "third_party/icu/source/common/uresimp.h"
#include "third_party/icu/source/common/cmemory.h"
#include "third_party/icu/source/common/servloc.h"
#include "third_party/icu/source/common/ustrfmt.h"
#include "third_party/icu/source/common/uhash.h"
#include "third_party/icu/source/common/charstr.h"
#include "third_party/icu/source/common/uassert.h"

#define UNDERSCORE_CHAR ((UChar)0x005f)
#define AT_SIGN_CHAR    ((UChar)64)
#define PERIOD_CHAR     ((UChar)46)

U_NAMESPACE_BEGIN

/*
 ******************************************************************
 */

SimpleLocaleKeyFactory::SimpleLocaleKeyFactory(UObject* objToAdopt,
                                               const UnicodeString& locale,
                                               int32_t kind,
                                               int32_t coverage)
  : LocaleKeyFactory(coverage)
  , _obj(objToAdopt)
  , _id(locale)
  , _kind(kind)
{
}

SimpleLocaleKeyFactory::SimpleLocaleKeyFactory(UObject* objToAdopt,
                                               const Locale& locale,
                                               int32_t kind,
                                               int32_t coverage)
  : LocaleKeyFactory(coverage)
  , _obj(objToAdopt)
  , _id()
  , _kind(kind)
{
    LocaleUtility::initNameFromLocale(locale, _id);
}

SimpleLocaleKeyFactory::~SimpleLocaleKeyFactory()
{
  delete _obj;
  _obj = NULL;
}

UObject*
SimpleLocaleKeyFactory::create(const ICUServiceKey& key, const ICUService* service, UErrorCode& status) const
{
    if (U_SUCCESS(status)) {
        const LocaleKey& lkey = (const LocaleKey&)key;
        if (_kind == LocaleKey::KIND_ANY || _kind == lkey.kind()) {
            UnicodeString keyID;
            lkey.currentID(keyID);
            if (_id == keyID) {
                return service->cloneInstance(_obj);
            }
        }
    }
    return NULL;
}

//UBool
//SimpleLocaleKeyFactory::isSupportedID(const UnicodeString& id, UErrorCode& /* status */) const
//{
//    return id == _id;
//}

void
SimpleLocaleKeyFactory::updateVisibleIDs(Hashtable& result, UErrorCode& status) const
{
    if (U_SUCCESS(status)) {
        if (_coverage & 0x1) {
            result.remove(_id);
        } else {
            result.put(_id, (void*)this, status);
        }
    }
}

#ifdef SERVICE_DEBUG
UnicodeString&
SimpleLocaleKeyFactory::debug(UnicodeString& result) const
{
    LocaleKeyFactory::debug(result);
    result.append((UnicodeString)", id: ");
    result.append(_id);
    result.append((UnicodeString)", kind: ");
    result.append(_kind);
    return result;
}

UnicodeString&
SimpleLocaleKeyFactory::debugClass(UnicodeString& result) const
{
    return result.append((UnicodeString)"SimpleLocaleKeyFactory");
}
#endif

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(SimpleLocaleKeyFactory)

U_NAMESPACE_END

/* !UCONFIG_NO_SERVICE */
#endif


