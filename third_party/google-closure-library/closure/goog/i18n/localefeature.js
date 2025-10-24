/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */
goog.module('goog.i18n.LocaleFeature');
goog.module.declareLegacyNamespace();

/**
 * @fileoverview Provides flag for using ECMAScript 402 features vs.
 * native JavaScript Closure implementations for I18N purposes.
 */

/**
 * @define {boolean} ECMASCRIPT_INTL_OPT_OUT
 * A global flag that an application can set to avoid using native
 * ECMAScript Intl implementation in any browser or Android implementations.
 * This may be necessary for applications that cannot use the regular
 * setting of goog.LOCALE or that must provide the Javascript data and
 * to create formatted output exactly the same on both client and server.
 *
 * Default value is false. Applications can set this to true so
 * compilation will opt out of the native mode.
 */
exports.ECMASCRIPT_INTL_OPT_OUT =
    goog.define('goog.i18n.ECMASCRIPT_INTL_OPT_OUT', false);

/**
 * @define {boolean} ECMASCRIPT_COMMON_LOCALES
 * A set of locales supported by all modern browsers in ECMASCRIPT Intl.
 * Common across all of the modern browsers and Android implementations
 * available in 2019 and later.
 */
exports.ECMASCRIPT_COMMON_LOCALES_2019 =
    (goog.LOCALE == 'am' || goog.LOCALE == 'ar' || goog.LOCALE == 'bg' ||
     goog.LOCALE == 'bn' || goog.LOCALE == 'ca' || goog.LOCALE == 'cs' ||
     goog.LOCALE == 'da' || goog.LOCALE == 'de' || goog.LOCALE == 'el' ||
     goog.LOCALE == 'en' || goog.LOCALE == 'es' || goog.LOCALE == 'et' ||
     goog.LOCALE == 'fa' || goog.LOCALE == 'fi' || goog.LOCALE == 'fil' ||
     goog.LOCALE == 'fr' || goog.LOCALE == 'gu' || goog.LOCALE == 'he' ||
     goog.LOCALE == 'hi' || goog.LOCALE == 'hr' || goog.LOCALE == 'hu' ||
     goog.LOCALE == 'id' || goog.LOCALE == 'it' || goog.LOCALE == 'ja' ||
     goog.LOCALE == 'kn' || goog.LOCALE == 'ko' || goog.LOCALE == 'lt' ||
     goog.LOCALE == 'lv' || goog.LOCALE == 'ml' || goog.LOCALE == 'mr' ||
     goog.LOCALE == 'ms' || goog.LOCALE == 'nl' || goog.LOCALE == 'pl' ||
     goog.LOCALE == 'ro' || goog.LOCALE == 'ru' || goog.LOCALE == 'sk' ||
     goog.LOCALE == 'sl' || goog.LOCALE == 'sr' || goog.LOCALE == 'sv' ||
     goog.LOCALE == 'sw' || goog.LOCALE == 'ta' || goog.LOCALE == 'te' ||
     goog.LOCALE == 'th' || goog.LOCALE == 'tr' || goog.LOCALE == 'uk' ||
     goog.LOCALE == 'vi' || goog.LOCALE == 'en_GB' || goog.LOCALE == 'en-GB' ||
     goog.LOCALE == 'es_419' || goog.LOCALE == 'es-419' ||
     goog.LOCALE == 'pt_BR' || goog.LOCALE == 'pt-BR' ||
     goog.LOCALE == 'pt_PT' || goog.LOCALE == 'pt-PT' ||
     goog.LOCALE == 'zh_CN' || goog.LOCALE == 'zh-CN' ||
     goog.LOCALE == 'zh_TW' || goog.LOCALE == 'zh-TW');

/**
 * @define {boolean} USE_ECMASCRIPT_I18N_2020 Evaluated to select
 * ECMAScript Intl object (when true) or JavaScript implementation (false) for
 * I18N purposes. It depends on browser implementation in January 2020.
 */
exports.USE_ECMASCRIPT_I18N_2020 =
    (goog.FEATURESET_YEAR >= 2020 && exports.ECMASCRIPT_COMMON_LOCALES_2019 &&
     !exports.ECMASCRIPT_INTL_OPT_OUT);

/**
 * @define {boolean} USE_ECMASCRIPT_I18N_2021 Evaluated to select
 * ECMAScript Intl object (when true) or JavaScript implementation (false) for
 * I18N purposes. It depends on browser implementation in January 2021.
 */
exports.USE_ECMASCRIPT_I18N_2021 =
    (goog.FEATURESET_YEAR >= 2021 && exports.ECMASCRIPT_COMMON_LOCALES_2019 &&
     !exports.ECMASCRIPT_INTL_OPT_OUT);

/**
 * @define {boolean} USE_ECMASCRIPT_I18N_2022 Evaluated to select
 * ECMAScript Intl object (when true) or JavaScript implementation (false) for
 * I18N purposes. It depends on browser implementation in January 2022.
 */
exports.USE_ECMASCRIPT_I18N_2022 =
    (goog.FEATURESET_YEAR >= 2022 && exports.ECMASCRIPT_COMMON_LOCALES_2019 &&
     !exports.ECMASCRIPT_INTL_OPT_OUT);

/**
 * @define {boolean} USE_ECMASCRIPT_I18N_RDTF is evaluated to enable
 * ECMAScript support for Intl.RelativeTimeFormat support in
 * browsers based on the locale. Browsers that are considered include:
 * Chrome, Firefox, Edge, and Safari.
 * As of January 2021, RelativeTimeFormat is supported in Chrome,
 * Edge, Firefox, and Safari.
 *
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/RelativeTimeFormat
 */
exports.USE_ECMASCRIPT_I18N_RDTF = exports.USE_ECMASCRIPT_I18N_2021;

/**
 * @define {boolean} USE_ECMASCRIPT_I18N_NUMFORMAT is evaluted to enable
 * ECMAScript support for Intl.NumberFormat support in
 * browsers based on the locale. As of January 2021, NumberFormat is
 * supported in Chrome, Edge, Firefox, and Safari.
 *
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/NumberFormat
 */
exports.USE_ECMASCRIPT_I18N_NUMFORMAT = exports.USE_ECMASCRIPT_I18N_2021;

/**
 * @define {boolean} USE_ECMASCRIPT_I18N_PLURALRULES is evaluated to enable
 * ECMAScript support for Intl.PluralRules support in
 * browsers based on the locale. Browsers that are considered include:
 * Chrome, Firefox, Edge, and Safari.
 * PluralRules are supported in Chrome, Edge, Firefox, and Safari.
 *
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/PluralRules/
 */
exports.USE_ECMASCRIPT_I18N_PLURALRULES = exports.USE_ECMASCRIPT_I18N_2020;

/**
 * @define {boolean} USE_ECMASCRIPT_I18N_DATETIMEF is evaluated to enable
 * ECMAScript support for Intl.DateTimeFormat support in
 * browsers based on the locale. Browsers that are considered include:
 * Chrome, Firefox 85 and above, Edge, and Safari.
 */
exports.USE_ECMASCRIPT_I18N_DATETIMEF = exports.USE_ECMASCRIPT_I18N_2021;

/**
 * The locales natively supported in ListFormat by all modern browsers.
 * @const
 * @type {!Array<string>} ECMASCRIPT_LISTFORMAT_LOCALES
 */
exports.ECMASCRIPT_LISTFORMAT_LOCALES = [
  'am',         'ar',         'ar-001',     'ar-AE',      'ar-BH',
  'ar-DJ',      'ar-DZ',      'ar-EG',      'ar-EH',      'ar-ER',
  'ar-IL',      'ar-IQ',      'ar-JO',      'ar-KM',      'ar-KW',
  'ar-LB',      'ar-LY',      'ar-MA',      'ar-MR',      'ar-OM',
  'ar-PS',      'ar-QA',      'ar-SA',      'ar-SD',      'ar-SO',
  'ar-SS',      'ar-SY',      'ar-TD',      'ar-TN',      'ar-YE',
  'bg',         'bg-BG',      'bn',         'bn-BD',      'bn-IN',
  'bs-Cyrl',    'bs-Cyrl-BA', 'ca',         'ca-AD',      'ca-ES',
  'ca-FR',      'ca-IT',      'cs',         'cs-CZ',      'da',
  'da-DK',      'da-GL',      'de',         'de-AT',      'de-BE',
  'de-CH',      'de-DE',      'de-IT',      'de-LI',      'de-LU',
  'el',         'el-CY',      'el-GR',      'en',         'en-001',
  'en-150',     'en-AE',      'en-AG',      'en-AI',      'en-AS',
  'en-AT',      'en-AU',      'en-BB',      'en-BE',      'en-BI',
  'en-BM',      'en-BS',      'en-BW',      'en-BZ',      'en-CA',
  'en-CC',      'en-CH',      'en-CK',      'en-CM',      'en-CX',
  'en-CY',      'en-DE',      'en-DG',      'en-DK',      'en-DM',
  'en-ER',      'en-FI',      'en-FJ',      'en-FK',      'en-FM',
  'en-GB',      'en-GD',      'en-GG',      'en-GH',      'en-GI',
  'en-GM',      'en-GU',      'en-GY',      'en-HK',      'en-IE',
  'en-IL',      'en-IM',      'en-IN',      'en-IO',      'en-JE',
  'en-JM',      'en-KE',      'en-KI',      'en-KN',      'en-KY',
  'en-LC',      'en-LR',      'en-LS',      'en-MG',      'en-MH',
  'en-MO',      'en-MP',      'en-MS',      'en-MT',      'en-MU',
  'en-MW',      'en-MY',      'en-NA',      'en-NF',      'en-NG',
  'en-NL',      'en-NR',      'en-NU',      'en-NZ',      'en-PG',
  'en-PH',      'en-PK',      'en-PN',      'en-PR',      'en-PW',
  'en-RW',      'en-SB',      'en-SC',      'en-SD',      'en-SE',
  'en-SG',      'en-SH',      'en-SI',      'en-SL',      'en-SS',
  'en-SX',      'en-SZ',      'en-TC',      'en-TK',      'en-TO',
  'en-TT',      'en-TV',      'en-TZ',      'en-UG',      'en-UM',
  'en-US',      'en-VC',      'en-VG',      'en-VI',      'en-VU',
  'en-WS',      'en-ZA',      'en-ZM',      'en-ZW',      'es',
  'es-419',     'es-AR',      'es-BO',      'es-BR',      'es-BZ',
  'es-CL',      'es-CO',      'es-CR',      'es-CU',      'es-DO',
  'es-EA',      'es-EC',      'es-ES',      'es-GQ',      'es-GT',
  'es-HN',      'es-IC',      'es-MX',      'es-NI',      'es-PA',
  'es-PE',      'es-PH',      'es-PR',      'es-PY',      'es-SV',
  'es-US',      'es-UY',      'es-VE',      'et',         'et-EE',
  'fa',         'fa-AF',      'fa-IR',      'fi',         'fi-FI',
  'fil',        'fil-PH',     'fr',         'fr-BE',      'fr-BF',
  'fr-BI',      'fr-BJ',      'fr-BL',      'fr-CA',      'fr-CD',
  'fr-CF',      'fr-CG',      'fr-CH',      'fr-CI',      'fr-CM',
  'fr-DJ',      'fr-DZ',      'fr-FR',      'fr-GA',      'fr-GF',
  'fr-GN',      'fr-GP',      'fr-GQ',      'fr-HT',      'fr-KM',
  'fr-LU',      'fr-MA',      'fr-MC',      'fr-MF',      'fr-MG',
  'fr-ML',      'fr-MQ',      'fr-MR',      'fr-MU',      'fr-NC',
  'fr-NE',      'fr-PF',      'fr-PM',      'fr-RE',      'fr-RW',
  'fr-SC',      'fr-SN',      'fr-SY',      'fr-TD',      'fr-TG',
  'fr-TN',      'fr-VU',      'fr-WF',      'fr-YT',      'gu',
  'gu-IN',      'he',         'he-IL',      'hi',         'hi-IN',
  'hr',         'hr-BA',      'hr-HR',      'hu',         'hu-HU',
  'id',         'id-ID',      'it',         'it-CH',      'it-IT',
  'it-SM',      'it-VA',      'ja',         'ja-JP',      'kn',
  'kn-IN',      'ko',         'ko-KP',      'ko-KR',      'lt',
  'lt-LT',      'lv',         'lv-LV',      'ml',         'ml-IN',
  'mr',         'mr-IN',      'ms',         'ms-BN',      'ms-ID',
  'ms-MY',      'ms-SG',      'nb',         'nl',         'nl-AW',
  'nl-BE',      'nl-BQ',      'nl-CW',      'nl-NL',      'nl-SR',
  'nl-SX',      'no',         'pl',         'pl-PL',      'pt',
  'pt-AO',      'pt-BR',      'pt-CH',      'pt-CV',      'pt-GQ',
  'pt-GW',      'pt-LU',      'pt-MO',      'pt-MZ',      'pt-PT',
  'pt-ST',      'pt-TL',      'ro',         'ro-MD',      'ro-RO',
  'ru',         'ru-BY',      'ru-KG',      'ru-KZ',      'ru-MD',
  'ru-RU',      'ru-UA',      'sk',         'sk-SK',      'sl',
  'sl-SI',      'sr',         'sr-Cyrl',    'sr-Cyrl-BA', 'sr-Cyrl-ME',
  'sr-Cyrl-RS', 'sr-Cyrl-XK', 'sr-Latn',    'sr-Latn-BA', 'sr-Latn-ME',
  'sr-Latn-RS', 'sr-Latn-XK', 'sv',         'sv-AX',      'sv-FI',
  'sv-SE',      'sw',         'sw-CD',      'sw-KE',      'sw-TZ',
  'sw-UG',      'ta',         'ta-IN',      'ta-LK',      'ta-MY',
  'ta-SG',      'te',         'te-IN',      'th',         'th-TH',
  'tr',         'tr-CY',      'tr-TR',      'uk',         'uk-UA',
  'vi',         'vi-VN',      'zh',         'zh-Hans',    'zh-Hans-CN',
  'zh-Hans-HK', 'zh-Hans-MO', 'zh-Hans-SG', 'zh-Hant',    'zh-Hant-HK',
  'zh-Hant-MO', 'zh-Hant-TW'
];

/**
 * @define {boolean} ECMASCRIPT_LISTFORMAT_COMMON_LOCALES_2022 is true if
 * goog.LOCALE is one of the locales below that are supported by
 * modern browsers (Chrome, Firefox, Edge, Safari) as of January 2022.
 */
exports.ECMASCRIPT_LISTFORMAT_COMMON_LOCALES_2022 =
    (goog.LOCALE === 'am' || goog.LOCALE === 'ar' || goog.LOCALE === 'ar-001' ||
     goog.LOCALE === 'ar-AE' || goog.LOCALE === 'ar-BH' ||
     goog.LOCALE === 'ar-DJ' || goog.LOCALE === 'ar-DZ' ||
     goog.LOCALE === 'ar-EG' || goog.LOCALE === 'ar-EH' ||
     goog.LOCALE === 'ar-ER' || goog.LOCALE === 'ar-IL' ||
     goog.LOCALE === 'ar-IQ' || goog.LOCALE === 'ar-JO' ||
     goog.LOCALE === 'ar-KM' || goog.LOCALE === 'ar-KW' ||
     goog.LOCALE === 'ar-LB' || goog.LOCALE === 'ar-LY' ||
     goog.LOCALE === 'ar-MA' || goog.LOCALE === 'ar-MR' ||
     goog.LOCALE === 'ar-OM' || goog.LOCALE === 'ar-PS' ||
     goog.LOCALE === 'ar-QA' || goog.LOCALE === 'ar-SA' ||
     goog.LOCALE === 'ar-SD' || goog.LOCALE === 'ar-SO' ||
     goog.LOCALE === 'ar-SS' || goog.LOCALE === 'ar-SY' ||
     goog.LOCALE === 'ar-TD' || goog.LOCALE === 'ar-TN' ||
     goog.LOCALE === 'ar-YE' || goog.LOCALE === 'bg' ||
     goog.LOCALE === 'bg-BG' || goog.LOCALE === 'bn' ||
     goog.LOCALE === 'bn-BD' || goog.LOCALE === 'bn-IN' ||
     goog.LOCALE === 'bs-Cyrl' || goog.LOCALE === 'bs-Cyrl-BA' ||
     goog.LOCALE === 'ca' || goog.LOCALE === 'ca-AD' ||
     goog.LOCALE === 'ca-ES' || goog.LOCALE === 'ca-FR' ||
     goog.LOCALE === 'ca-IT' || goog.LOCALE === 'cs' ||
     goog.LOCALE === 'cs-CZ' || goog.LOCALE === 'da' ||
     goog.LOCALE === 'da-DK' || goog.LOCALE === 'da-GL' ||
     goog.LOCALE === 'de' || goog.LOCALE === 'de-AT' ||
     goog.LOCALE === 'de-BE' || goog.LOCALE === 'de-CH' ||
     goog.LOCALE === 'de-DE' || goog.LOCALE === 'de-IT' ||
     goog.LOCALE === 'de-LI' || goog.LOCALE === 'de-LU' ||
     goog.LOCALE === 'el' || goog.LOCALE === 'el-CY' ||
     goog.LOCALE === 'el-GR' || goog.LOCALE === 'en' ||
     goog.LOCALE === 'en-001' || goog.LOCALE === 'en-150' ||
     goog.LOCALE === 'en-AE' || goog.LOCALE === 'en-AG' ||
     goog.LOCALE === 'en-AI' || goog.LOCALE === 'en-AS' ||
     goog.LOCALE === 'en-AT' || goog.LOCALE === 'en-AU' ||
     goog.LOCALE === 'en-BB' || goog.LOCALE === 'en-BE' ||
     goog.LOCALE === 'en-BI' || goog.LOCALE === 'en-BM' ||
     goog.LOCALE === 'en-BS' || goog.LOCALE === 'en-BW' ||
     goog.LOCALE === 'en-BZ' || goog.LOCALE === 'en-CA' ||
     goog.LOCALE === 'en-CC' || goog.LOCALE === 'en-CH' ||
     goog.LOCALE === 'en-CK' || goog.LOCALE === 'en-CM' ||
     goog.LOCALE === 'en-CX' || goog.LOCALE === 'en-CY' ||
     goog.LOCALE === 'en-DE' || goog.LOCALE === 'en-DG' ||
     goog.LOCALE === 'en-DK' || goog.LOCALE === 'en-DM' ||
     goog.LOCALE === 'en-ER' || goog.LOCALE === 'en-FI' ||
     goog.LOCALE === 'en-FJ' || goog.LOCALE === 'en-FK' ||
     goog.LOCALE === 'en-FM' || goog.LOCALE === 'en-GB' ||
     goog.LOCALE === 'en-GD' || goog.LOCALE === 'en-GG' ||
     goog.LOCALE === 'en-GH' || goog.LOCALE === 'en-GI' ||
     goog.LOCALE === 'en-GM' || goog.LOCALE === 'en-GU' ||
     goog.LOCALE === 'en-GY' || goog.LOCALE === 'en-HK' ||
     goog.LOCALE === 'en-IE' || goog.LOCALE === 'en-IL' ||
     goog.LOCALE === 'en-IM' || goog.LOCALE === 'en-IN' ||
     goog.LOCALE === 'en-IO' || goog.LOCALE === 'en-JE' ||
     goog.LOCALE === 'en-JM' || goog.LOCALE === 'en-KE' ||
     goog.LOCALE === 'en-KI' || goog.LOCALE === 'en-KN' ||
     goog.LOCALE === 'en-KY' || goog.LOCALE === 'en-LC' ||
     goog.LOCALE === 'en-LR' || goog.LOCALE === 'en-LS' ||
     goog.LOCALE === 'en-MG' || goog.LOCALE === 'en-MH' ||
     goog.LOCALE === 'en-MO' || goog.LOCALE === 'en-MP' ||
     goog.LOCALE === 'en-MS' || goog.LOCALE === 'en-MT' ||
     goog.LOCALE === 'en-MU' || goog.LOCALE === 'en-MW' ||
     goog.LOCALE === 'en-MY' || goog.LOCALE === 'en-NA' ||
     goog.LOCALE === 'en-NF' || goog.LOCALE === 'en-NG' ||
     goog.LOCALE === 'en-NL' || goog.LOCALE === 'en-NR' ||
     goog.LOCALE === 'en-NU' || goog.LOCALE === 'en-NZ' ||
     goog.LOCALE === 'en-PG' || goog.LOCALE === 'en-PH' ||
     goog.LOCALE === 'en-PK' || goog.LOCALE === 'en-PN' ||
     goog.LOCALE === 'en-PR' || goog.LOCALE === 'en-PW' ||
     goog.LOCALE === 'en-RW' || goog.LOCALE === 'en-SB' ||
     goog.LOCALE === 'en-SC' || goog.LOCALE === 'en-SD' ||
     goog.LOCALE === 'en-SE' || goog.LOCALE === 'en-SG' ||
     goog.LOCALE === 'en-SH' || goog.LOCALE === 'en-SI' ||
     goog.LOCALE === 'en-SL' || goog.LOCALE === 'en-SS' ||
     goog.LOCALE === 'en-SX' || goog.LOCALE === 'en-SZ' ||
     goog.LOCALE === 'en-TC' || goog.LOCALE === 'en-TK' ||
     goog.LOCALE === 'en-TO' || goog.LOCALE === 'en-TT' ||
     goog.LOCALE === 'en-TV' || goog.LOCALE === 'en-TZ' ||
     goog.LOCALE === 'en-UG' || goog.LOCALE === 'en-UM' ||
     goog.LOCALE === 'en-US' || goog.LOCALE === 'en-VC' ||
     goog.LOCALE === 'en-VG' || goog.LOCALE === 'en-VI' ||
     goog.LOCALE === 'en-VU' || goog.LOCALE === 'en-WS' ||
     goog.LOCALE === 'en-ZA' || goog.LOCALE === 'en-ZM' ||
     goog.LOCALE === 'en-ZW' || goog.LOCALE === 'es' ||
     goog.LOCALE === 'es-419' || goog.LOCALE === 'es-AR' ||
     goog.LOCALE === 'es-BO' || goog.LOCALE === 'es-BR' ||
     goog.LOCALE === 'es-BZ' || goog.LOCALE === 'es-CL' ||
     goog.LOCALE === 'es-CO' || goog.LOCALE === 'es-CR' ||
     goog.LOCALE === 'es-CU' || goog.LOCALE === 'es-DO' ||
     goog.LOCALE === 'es-EA' || goog.LOCALE === 'es-EC' ||
     goog.LOCALE === 'es-ES' || goog.LOCALE === 'es-GQ' ||
     goog.LOCALE === 'es-GT' || goog.LOCALE === 'es-HN' ||
     goog.LOCALE === 'es-IC' || goog.LOCALE === 'es-MX' ||
     goog.LOCALE === 'es-NI' || goog.LOCALE === 'es-PA' ||
     goog.LOCALE === 'es-PE' || goog.LOCALE === 'es-PH' ||
     goog.LOCALE === 'es-PR' || goog.LOCALE === 'es-PY' ||
     goog.LOCALE === 'es-SV' || goog.LOCALE === 'es-US' ||
     goog.LOCALE === 'es-UY' || goog.LOCALE === 'es-VE' ||
     goog.LOCALE === 'et' || goog.LOCALE === 'et-EE' || goog.LOCALE === 'fa' ||
     goog.LOCALE === 'fa-AF' || goog.LOCALE === 'fa-IR' ||
     goog.LOCALE === 'fi' || goog.LOCALE === 'fi-FI' || goog.LOCALE === 'fil' ||
     goog.LOCALE === 'fil-PH' || goog.LOCALE === 'fr' ||
     goog.LOCALE === 'fr-BE' || goog.LOCALE === 'fr-BF' ||
     goog.LOCALE === 'fr-BI' || goog.LOCALE === 'fr-BJ' ||
     goog.LOCALE === 'fr-BL' || goog.LOCALE === 'fr-CA' ||
     goog.LOCALE === 'fr-CD' || goog.LOCALE === 'fr-CF' ||
     goog.LOCALE === 'fr-CG' || goog.LOCALE === 'fr-CH' ||
     goog.LOCALE === 'fr-CI' || goog.LOCALE === 'fr-CM' ||
     goog.LOCALE === 'fr-DJ' || goog.LOCALE === 'fr-DZ' ||
     goog.LOCALE === 'fr-FR' || goog.LOCALE === 'fr-GA' ||
     goog.LOCALE === 'fr-GF' || goog.LOCALE === 'fr-GN' ||
     goog.LOCALE === 'fr-GP' || goog.LOCALE === 'fr-GQ' ||
     goog.LOCALE === 'fr-HT' || goog.LOCALE === 'fr-KM' ||
     goog.LOCALE === 'fr-LU' || goog.LOCALE === 'fr-MA' ||
     goog.LOCALE === 'fr-MC' || goog.LOCALE === 'fr-MF' ||
     goog.LOCALE === 'fr-MG' || goog.LOCALE === 'fr-ML' ||
     goog.LOCALE === 'fr-MQ' || goog.LOCALE === 'fr-MR' ||
     goog.LOCALE === 'fr-MU' || goog.LOCALE === 'fr-NC' ||
     goog.LOCALE === 'fr-NE' || goog.LOCALE === 'fr-PF' ||
     goog.LOCALE === 'fr-PM' || goog.LOCALE === 'fr-RE' ||
     goog.LOCALE === 'fr-RW' || goog.LOCALE === 'fr-SC' ||
     goog.LOCALE === 'fr-SN' || goog.LOCALE === 'fr-SY' ||
     goog.LOCALE === 'fr-TD' || goog.LOCALE === 'fr-TG' ||
     goog.LOCALE === 'fr-TN' || goog.LOCALE === 'fr-VU' ||
     goog.LOCALE === 'fr-WF' || goog.LOCALE === 'fr-YT' ||
     goog.LOCALE === 'gu' || goog.LOCALE === 'gu-IN' || goog.LOCALE === 'he' ||
     goog.LOCALE === 'he-IL' || goog.LOCALE === 'hi' ||
     goog.LOCALE === 'hi-IN' || goog.LOCALE === 'hr' ||
     goog.LOCALE === 'hr-BA' || goog.LOCALE === 'hr-HR' ||
     goog.LOCALE === 'hu' || goog.LOCALE === 'hu-HU' || goog.LOCALE === 'id' ||
     goog.LOCALE === 'id-ID' || goog.LOCALE === 'it' ||
     goog.LOCALE === 'it-CH' || goog.LOCALE === 'it-IT' ||
     goog.LOCALE === 'it-SM' || goog.LOCALE === 'it-VA' ||
     goog.LOCALE === 'ja' || goog.LOCALE === 'ja-JP' || goog.LOCALE === 'kn' ||
     goog.LOCALE === 'kn-IN' || goog.LOCALE === 'ko' ||
     goog.LOCALE === 'ko-KP' || goog.LOCALE === 'ko-KR' ||
     goog.LOCALE === 'lt' || goog.LOCALE === 'lt-LT' || goog.LOCALE === 'lv' ||
     goog.LOCALE === 'lv-LV' || goog.LOCALE === 'ml' ||
     goog.LOCALE === 'ml-IN' || goog.LOCALE === 'mr' ||
     goog.LOCALE === 'mr-IN' || goog.LOCALE === 'ms' ||
     goog.LOCALE === 'ms-BN' || goog.LOCALE === 'ms-ID' ||
     goog.LOCALE === 'ms-MY' || goog.LOCALE === 'ms-SG' ||
     goog.LOCALE === 'nb' || goog.LOCALE === 'nl' || goog.LOCALE === 'nl-AW' ||
     goog.LOCALE === 'nl-BE' || goog.LOCALE === 'nl-BQ' ||
     goog.LOCALE === 'nl-CW' || goog.LOCALE === 'nl-NL' ||
     goog.LOCALE === 'nl-SR' || goog.LOCALE === 'nl-SX' ||
     goog.LOCALE === 'no' || goog.LOCALE === 'pl' || goog.LOCALE === 'pl-PL' ||
     goog.LOCALE === 'pt' || goog.LOCALE === 'pt-AO' ||
     goog.LOCALE === 'pt-BR' || goog.LOCALE === 'pt-CH' ||
     goog.LOCALE === 'pt-CV' || goog.LOCALE === 'pt-GQ' ||
     goog.LOCALE === 'pt-GW' || goog.LOCALE === 'pt-LU' ||
     goog.LOCALE === 'pt-MO' || goog.LOCALE === 'pt-MZ' ||
     goog.LOCALE === 'pt-PT' || goog.LOCALE === 'pt-ST' ||
     goog.LOCALE === 'pt-TL' || goog.LOCALE === 'ro' ||
     goog.LOCALE === 'ro-MD' || goog.LOCALE === 'ro-RO' ||
     goog.LOCALE === 'ru' || goog.LOCALE === 'ru-BY' ||
     goog.LOCALE === 'ru-KG' || goog.LOCALE === 'ru-KZ' ||
     goog.LOCALE === 'ru-MD' || goog.LOCALE === 'ru-RU' ||
     goog.LOCALE === 'ru-UA' || goog.LOCALE === 'sk' ||
     goog.LOCALE === 'sk-SK' || goog.LOCALE === 'sl' ||
     goog.LOCALE === 'sl-SI' || goog.LOCALE === 'sr' ||
     goog.LOCALE === 'sr-Cyrl' || goog.LOCALE === 'sr-Cyrl-BA' ||
     goog.LOCALE === 'sr-Cyrl-ME' || goog.LOCALE === 'sr-Cyrl-RS' ||
     goog.LOCALE === 'sr-Cyrl-XK' || goog.LOCALE === 'sr-Latn' ||
     goog.LOCALE === 'sr-Latn-BA' || goog.LOCALE === 'sr-Latn-ME' ||
     goog.LOCALE === 'sr-Latn-RS' || goog.LOCALE === 'sr-Latn-XK' ||
     goog.LOCALE === 'sv' || goog.LOCALE === 'sv-AX' ||
     goog.LOCALE === 'sv-FI' || goog.LOCALE === 'sv-SE' ||
     goog.LOCALE === 'sw' || goog.LOCALE === 'sw-CD' ||
     goog.LOCALE === 'sw-KE' || goog.LOCALE === 'sw-TZ' ||
     goog.LOCALE === 'sw-UG' || goog.LOCALE === 'ta' ||
     goog.LOCALE === 'ta-IN' || goog.LOCALE === 'ta-LK' ||
     goog.LOCALE === 'ta-MY' || goog.LOCALE === 'ta-SG' ||
     goog.LOCALE === 'te' || goog.LOCALE === 'te-IN' || goog.LOCALE === 'th' ||
     goog.LOCALE === 'th-TH' || goog.LOCALE === 'tr' ||
     goog.LOCALE === 'tr-CY' || goog.LOCALE === 'tr-TR' ||
     goog.LOCALE === 'uk' || goog.LOCALE === 'uk-UA' || goog.LOCALE === 'vi' ||
     goog.LOCALE === 'vi-VN' || goog.LOCALE === 'zh' ||
     goog.LOCALE === 'zh-Hans' || goog.LOCALE === 'zh-Hans-CN' ||
     goog.LOCALE === 'zh-Hans-HK' || goog.LOCALE === 'zh-Hans-MO' ||
     goog.LOCALE === 'zh-Hans-SG' || goog.LOCALE === 'zh-Hant' ||
     goog.LOCALE === 'zh-Hant-HK' || goog.LOCALE === 'zh-Hant-MO' ||
     goog.LOCALE === 'zh-Hant-TW');

/**
 * @define {boolean} USE_ECMASCRIPT_I18N_LISTFORMAT is evaluated to enable
 * ECMAScript support for Intl.ListFormat support in browsers based on the
 * locale. As of January 2022, ListFormat is supported by Chrome, Edge,
 * Firefox, and Safari.
 *
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/ListFormat
 */
exports.USE_ECMASCRIPT_I18N_LISTFORMAT =
    (goog.FEATURESET_YEAR >= 2022 &&
     exports.ECMASCRIPT_LISTFORMAT_COMMON_LOCALES_2022 &&
     !exports.ECMASCRIPT_INTL_OPT_OUT);

/**
 * @define {boolean} USE_ECMASCRIPT_I18N_DATEINTERVALFORMAT is evaluated to
 *     enable
 * ECMAScript support for Intl.DateFormat support in browsers based on the
 * locale. As of January 2022, DateFormat formatRange is supported by Chrome,
 * Edge, Firefox, and Safari.
 *
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/ListFormat
 */
exports.USE_ECMASCRIPT_I18N_DATEINTERVALFORMAT =
    (goog.FEATURESET_YEAR >= 2022 &&
     exports.ECMASCRIPT_LISTFORMAT_COMMON_LOCALES_2022 &&
     !exports.ECMASCRIPT_INTL_OPT_OUT);
