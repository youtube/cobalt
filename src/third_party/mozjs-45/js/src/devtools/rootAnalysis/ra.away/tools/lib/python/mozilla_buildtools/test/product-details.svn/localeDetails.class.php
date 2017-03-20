<?php

    /**
     * Holds locale data for all the locales we currently support
     * If we switch to php5, all of these functions can become static with some tweaking.
     *
     * @author Wil Clouser <clouserw@mozilla.com>
     */
    class localeDetails {

        /**
         * An array of English and native names of all the locales we support.
         *
         * @var array
         */
        var $languages = array(
            'af'         => array( 'English' => 'Afrikaans',                 'native' => 'Afrikaans'),
            'ach'        => array( 'English' => 'Acholi',                    'native' => 'Acholi'),
            'ak'         => array( 'English' => 'Akan',                      'native' => 'Akan'), // unverified native name
            'am-et'      => array( 'English' => 'Amharic',                   'native' => 'አማርኛ'),
            'an'         => array( 'English' => 'Aragonese',                 'native' => 'aragonés'),
            'ar'         => array( 'English' => 'Arabic',                    'native' => 'عربي'),
            'as'         => array( 'English' => 'Assamese',                  'native' => 'অসমীয়া'),
            'ast'        => array( 'English' => 'Asturian',                  'native' => 'Asturianu'),
            'az'         => array( 'English' => 'Azerbaijani',               'native' => 'Azərbaycanca'),
            'be'         => array( 'English' => 'Belarusian',                'native' => 'Беларуская'),
            'bg'         => array( 'English' => 'Bulgarian',                 'native' => 'Български'),
            'bn-BD'      => array( 'English' => 'Bengali (Bangladesh)',      'native' => 'বাংলা (বাংলাদেশ)'),
            'bn-IN'      => array( 'English' => 'Bengali (India)',           'native' => 'বাংলা (ভারত)'),
            'br'         => array( 'English' => 'Breton',                    'native' => 'Brezhoneg'),
            'bs'         => array( 'English' => 'Bosnian',                   'native' => 'Bosanski'),
            'ca'         => array( 'English' => 'Catalan',                   'native' => 'Català'),
            'ca-valencia'=> array( 'English' => 'Catalan (Valencian)',       'native' => 'català (valencià)'), // not iso-639-1. a=l10n-drivers
            'cs'         => array( 'English' => 'Czech',                     'native' => 'Čeština'),
            'csb'        => array( 'English' => 'Kashubian',                 'native' => 'Kaszëbsczi'),
            'cy'         => array( 'English' => 'Welsh',                     'native' => 'Cymraeg'),
            'da'         => array( 'English' => 'Danish',                    'native' => 'Dansk'),
            'dbg'        => array( 'English' => 'Debug Robot',               'native' => 'Ḓḗƀŭɠ Řǿƀǿŧ'), # Used for site debugging
            'de'         => array( 'English' => 'German',                    'native' => 'Deutsch'),
            'de-AT'      => array( 'English' => 'German (Austria)',          'native' => 'Deutsch (Österreich)'),
            'de-CH'      => array( 'English' => 'German (Switzerland)',      'native' => 'Deutsch (Schweiz)'),
            'de-DE'      => array( 'English' => 'German (Germany)',          'native' => 'Deutsch (Deutschland)'),
            'dsb'        => array( 'English' => 'Lower Sorbian',             'native' => 'Dolnoserbšćina'), // iso-639-2
            'el'         => array( 'English' => 'Greek',                     'native' => 'Ελληνικά'),
            'en-AU'      => array( 'English' => 'English (Australian)',      'native' => 'English (Australian)'),
            'en-CA'      => array( 'English' => 'English (Canadian)',        'native' => 'English (Canadian)'),
            'en-GB'      => array( 'English' => 'English (British)',         'native' => 'English (British)'),
            'en-NZ'      => array( 'English' => 'English (New Zealand)',     'native' => 'English (New Zealand)'),
            'en-US'      => array( 'English' => 'English (US)',              'native' => 'English (US)'),
            'en-ZA'      => array( 'English' => 'English (South African)',   'native' => 'English (South African)'),
            'eo'         => array( 'English' => 'Esperanto',                 'native' => 'Esperanto'),
            'es'         => array( 'English' => 'Spanish',                   'native' => 'Español'),
            'es-AR'      => array( 'English' => 'Spanish (Argentina)',       'native' => 'Español (de Argentina)'),
            'es-CL'      => array( 'English' => 'Spanish (Chile)',           'native' => 'Español (de Chile)'),
            'es-ES'      => array( 'English' => 'Spanish (Spain)',           'native' => 'Español (de España)'),
            'es-MX'      => array( 'English' => 'Spanish (Mexico)',          'native' => 'Español (de México)'),
            'et'         => array( 'English' => 'Estonian',                  'native' => 'Eesti keel'),
            'eu'         => array( 'English' => 'Basque',                    'native' => 'Euskara'),
            'fa'         => array( 'English' => 'Persian',                   'native' => 'فارسی'),
            'ff'         => array( 'English' => 'Fulah',                     'native' => 'Pulaar-Fulfulde'),
            'fi'         => array( 'English' => 'Finnish',                   'native' => 'suomi'),
            'fj-FJ'      => array( 'English' => 'Fijian',                    'native' => 'Vosa vaka-Viti'),
            'fr'         => array( 'English' => 'French',                    'native' => 'Français'),
            'fur-IT'     => array( 'English' => 'Friulian',                  'native' => 'Furlan'),
            'fy-NL'      => array( 'English' => 'Frisian',                   'native' => 'Frysk'),
            'ga'         => array( 'English' => 'Irish',                     'native' => 'Gaeilge'),
            'ga-IE'      => array( 'English' => 'Irish',                     'native' => 'Gaeilge'),
            'gd'         => array( 'English' => 'Gaelic (Scotland)',         'native' => 'Gàidhlig'),
            'gl'         => array( 'English' => 'Galician',                  'native' => 'Galego'),
            'gu'         => array( 'English' => 'Gujarati',                  'native' => 'ગુજરાતી'),
            'gu-IN'      => array( 'English' => 'Gujarati (India)',          'native' => 'ગુજરાતી (ભારત)'),
            'he'         => array( 'English' => 'Hebrew',                    'native' => 'עברית'),
            'hi'         => array( 'English' => 'Hindi',                     'native' => 'हिन्दी'),
            'hi-IN'      => array( 'English' => 'Hindi (India)',             'native' => 'हिन्दी (भारत)'),
            'hr'         => array( 'English' => 'Croatian',                  'native' => 'Hrvatski'),
            'hsb'        => array( 'English' => 'Upper Sorbian',             'native' => 'Hornjoserbsce'),
            'hu'         => array( 'English' => 'Hungarian',                 'native' => 'magyar'),
            'hy-AM'      => array( 'English' => 'Armenian',                  'native' => 'Հայերեն'),
            'id'         => array( 'English' => 'Indonesian',                'native' => 'Bahasa Indonesia'),
            'is'         => array( 'English' => 'Icelandic',                 'native' => 'íslenska'),
            'it'         => array( 'English' => 'Italian',                   'native' => 'Italiano'),
            'ja'         => array( 'English' => 'Japanese',                  'native' => '日本語'),
            'ja-JP-mac'  => array( 'English' => 'Japanese',                  'native' => '日本語'), // not iso-639-1
            'ka'         => array( 'English' => 'Georgian',                  'native' => 'ქართული'),
            'kk'         => array( 'English' => 'Kazakh',                    'native' => 'Қазақ'),
            'km'         => array( 'English' => 'Khmer',                     'native' => 'ខ្មែរ'),
            'kn'         => array( 'English' => 'Kannada',                   'native' => 'ಕನ್ನಡ'),
            'ko'         => array( 'English' => 'Korean',                    'native' => '한국어'),
            'ku'         => array( 'English' => 'Kurdish',                   'native' => 'Kurdî'),
            'la'         => array( 'English' => 'Latin',                     'native' => 'Latina'),
            'lg'         => array( 'English' => 'Luganda',                   'native' => 'Luganda'),
            'lij'        => array( 'English' => 'Ligurian',                  'native' => 'Ligure'),
            'lo'         => array( 'English' => 'Lao',                       'native' => 'ພາສາລາວ'),
            'lt'         => array( 'English' => 'Lithuanian',                'native' => 'lietuvių kalba'),
            'lv'         => array( 'English' => 'Latvian',                   'native' => 'Latviešu'),
            'mai'        => array( 'English' => 'Maithili',                  'native' => 'मैथिली মৈথিলী'),
            'mg'         => array( 'English' => 'Malagasy',                  'native' => 'Malagasy'),
            'mi'         => array( 'English' => 'Maori (Aotearoa)',          'native' => 'Māori (Aotearoa)'),
            'mk'         => array( 'English' => 'Macedonian',                'native' => 'Македонски'),
            'ml'         => array( 'English' => 'Malayalam',                 'native' => 'മലയാളം'),
            'mn'         => array( 'English' => 'Mongolian',                 'native' => 'Монгол'),
            'mr'         => array( 'English' => 'Marathi',                   'native' => 'मराठी'),
            'ms'         => array( 'English' => 'Malay',                     'native' => 'Melayu'),
            'my'         => array( 'English' => 'Burmese',                   'native' => 'မြန်မာဘာသာ'),
            'nb-NO'      => array( 'English' => 'Norwegian (Bokmål)',        'native' => 'Norsk bokmål'),
            'ne-NP'      => array( 'English' => 'Nepali',                    'native' => 'नेपाली'),
            'nn-NO'      => array( 'English' => 'Norwegian (Nynorsk)',       'native' => 'Norsk nynorsk'),
            'nl'         => array( 'English' => 'Dutch',                     'native' => 'Nederlands'),
            'nr'         => array( 'English' => 'Ndebele, South',            'native' => 'isiNdebele'),
            'nso'        => array( 'English' => 'Northern Sotho',            'native' => 'Sepedi'),
            'oc'         => array( 'English' => 'Occitan (Lengadocian)',     'native' => 'occitan (lengadocian)'),
            'or'         => array( 'English' => 'Oriya',                     'native' => 'ଓଡ଼ିଆ'),
            'pa'         => array( 'English' => 'Punjabi',                   'native' => 'ਪੰਜਾਬੀ'),
            'pa-IN'      => array( 'English' => 'Punjabi (India)',           'native' => 'ਪੰਜਾਬੀ (ਭਾਰਤ)'),
            'pl'         => array( 'English' => 'Polish',                    'native' => 'Polski'),
            'pt-BR'      => array( 'English' => 'Portuguese (Brazilian)',    'native' => 'Português (do Brasil)'),
            'pt-PT'      => array( 'English' => 'Portuguese (Portugal)',     'native' => 'Português (Europeu)'),
            'ro'         => array( 'English' => 'Romanian',                  'native' => 'română'),
            'rm'         => array( 'English' => 'Romansh',                   'native' => 'rumantsch'),
            'ru'         => array( 'English' => 'Russian',                   'native' => 'Русский'),
            'rw'         => array( 'English' => 'Kinyarwanda',               'native' => 'Ikinyarwanda'),
            'sa'         => array( 'English' => 'Sanskrit',                  'native' => 'संस्कृत'),
            'sah'        => array( 'English' => 'Sakha',                     'native' => 'Сахалыы'),
            'si'         => array( 'English' => 'Sinhala',                   'native' => 'සිංහල'),
            'sk'         => array( 'English' => 'Slovak',                    'native' => 'slovenčina'),
            'sl'         => array( 'English' => 'Slovenian',                 'native' => 'Slovenščina'),
            'son'        => array( 'English' => 'Songhai',                   'native' => 'Soŋay'),
            'sq'         => array( 'English' => 'Albanian',                  'native' => 'Shqip'),
            'sr'         => array( 'English' => 'Serbian',                   'native' => 'Српски'),
            'sr-Cyrl'    => array( 'English' => 'Serbian',                   'native' => 'Српски'), // follows RFC 4646
            'sr-Latn'    => array( 'English' => 'Serbian',                   'native' => 'Srpski'), // follows RFC 4646
            'ss'         => array( 'English' => 'Siswati',                   'native' => 'siSwati'),
            'st'         => array( 'English' => 'Southern Sotho',            'native' => 'Sesotho'),
            'sv-SE'      => array( 'English' => 'Swedish',                   'native' => 'Svenska'),
            'sw'         => array( 'English' => 'Swahili',                   'native' => 'Kiswahili'),
            'ta'         => array( 'English' => 'Tamil',                     'native' => 'தமிழ்'),
            'ta-IN'      => array( 'English' => 'Tamil (India)',             'native' => 'தமிழ் (இந்தியா)'),
            'ta-LK'      => array( 'English' => 'Tamil (Sri Lanka)',         'native' => 'தமிழ் (இலங்கை)'),
            'te'         => array( 'English' => 'Telugu',                    'native' => 'తెలుగు'),
            'th'         => array( 'English' => 'Thai',                      'native' => 'ไทย'),
            'tn'         => array( 'English' => 'Tswana',                    'native' => 'Setswana'),
            'tr'         => array( 'English' => 'Turkish',                   'native' => 'Türkçe'),
            'ts'         => array( 'English' => 'Tsonga',                    'native' => 'Xitsonga'),
            'tt-RU'      => array( 'English' => 'Tatar',                     'native' => 'Tatarça'),
            'uk'         => array( 'English' => 'Ukrainian',                 'native' => 'Українська'),
            'ur'         => array( 'English' => 'Urdu',                      'native' => 'اُردو'),
            'uz'         => array( 'English' => 'Uzbek',                     'native' => 'Oʻzbek tili'),
            've'         => array( 'English' => 'Venda',                     'native' => 'Tshivenḓa'),
            'vi'         => array( 'English' => 'Vietnamese',                'native' => 'Tiếng Việt'),
            'wo'         => array( 'English' => 'Wolof',                     'native' => 'Wolof'),
            'x-testing'  => array( 'English' => 'Testing',                   'native' => 'Ŧḗşŧīƞɠ'),
            'xh'         => array( 'English' => 'Xhosa',                     'native' => 'isiXhosa'),
            'zh-CN'      => array( 'English' => 'Chinese (Simplified)',      'native' => '中文 (简体)'),
            'zh-TW'      => array( 'English' => 'Chinese (Traditional)',     'native' => '正體中文 (繁體)'),
            'zu'         => array( 'English' => 'Zulu',                      'native' => 'isiZulu')
        );

        /**
         * An array of regions/continents and their locales. Specific countries in regions are not documented here.
         * Primary locales refers to locales that are used in the region according to United Nations Standard Country
         * or Area Codes for Statistical Use.  Secondary refers to locales that lack a specific geography or have a small
         * population that uses it in the region.
         *
         * WARNING: This is out of date and unmaintained.
         *
         * @var array
         */
        var $locations = array(
            'Northern America' => array(
                    'primary'   => array('en-CA','en-GB','en-US','es-AR','es-ES','es-MX','fr'),
                    'secondary' => array('da','eo','la','nb-NO','nn-NO')
                ),
            'Central America' => array(
                    'primary'   => array('en-GB','en-US','es-AR','es-ES','es-MX','fr','nl'),
                    'secondary' => array('eo','la')
                ),
            'Southern America' => array(
                    'primary'   => array('en-GB','en-US','es-AR','es-ES','es-MX','fr','nl','pt-BR','pt-PT'),
                    'secondary' => array('eo','la')
                ),
            'Northern Europe' => array(
                    'primary'   => array('cy','da','en-GB','en-US','et','fi','ga-IE','is','lt','lv','nb-NO','nn-NO','sv-SE','tt-RU'),
                    'secondary' => array('eo','la')
                ),
            'Western Europe' => array(
                    'primary'   => array('br-FR','de','de-AT','de-CH','de-DE','fr','fy-NL','gl','nl'),
                    'secondary' => array('en-GB','en-US','eo','hr','hsb','it','la','sl')
                ),
            'Eastern Europe' => array(
                    'primary'   => array('be','bg','cs','hu','pl','ro','ru','sk','tt-RU','uk'),
                    'secondary' => array('en-GB','en-US','eo','la','tr')
                ),
            'Southern Europe' => array(
                    'primary'   => array('ast-ES','eu','ca','ca-valencia','el','es-ES','hu','it','la','mk','oc','pt-PT','sq','sr','sr-Latn','tr'),
                    'secondary' => array('de','en-GB','en-US','eo','fur-IT')
                ),
            'Africa' => array(
                    'primary'   => array('af','ak','ar','en-US','en-GB','en-ZA','es-ES','fr','mg','nr','nso','pt-PT','rw','ss','st','tn','ts','ve','wo','xh','zu'),
                    'secondary' => array('eo','la')
                ),
            'Western Asia' => array(
                    'primary'   => array('ar','en-GB','en-US','el','he','hy-AM','ka','kk','ku','tr'),
                    'secondary' => array('eo','la')
                ),
            'South-Central Asia' => array(
                    'primary'   => array('as','bn-BD','bn-IN','en-GB','en-US','fa','gu-IN','hi','hi-IN','kk','kn','ml','mr','ne-NP','pa-IN','ru','si','te','tt-RU','ur'),
                    'secondary' => array('eo','la')
                ),
            'Eastern Asia' => array(
                    'primary'   => array('en-GB','en-US','ja','ko','mn','zh-CN','zh-TW'),
                    'secondary' => array('eo','la')
                ),
            'South Eastern Asia' => array(
                    'primary'   => array('en-GB','en-US','es-AR','es-ES','fr','id','pt-PT','ta-IN','ta-LK','th','vi'),
                    'secondary' => array('eo','la')
                ),
            'Oceania' => array(
                    'primary'   => array('en-AU','en-GB','en-US','en-NZ','fr','ja','mi'),
                    'secondary' => array('eo','la')
                )
        );

        /**
         * Locations by continent
         *
         * @var array
         */
        var $continents = array(
            'Africa' => array(
                'Africa'
                ),
            'Asia' => array(
                'Western Asia', 'South-Central Asia', 'Eastern Asia', 'South Eastern Asia'
                ),
            'Australia & Oceania' => array(
                'Oceania'
                ),
            'Europe' => array(
                'Northern Europe', 'Western Europe', 'Eastern Europe', 'Southern Europe'
                ),
            'North America' => array(
                'Northern America', 'Central America'
                ),
            'South America' => array(
                'Southern America'
                )
        );

        /**
         * An array of languages that are displayed rtl.  This is a separate array
         * because there are so few languages.
         *
         * @var array
         */
        var $rtl_languages = array( 'ar', 'fa', 'fa-IR', 'he', 'ur' );

        /**
         * This is a function for getting the direction a language is displayed in.
         * If the locale doesn't exist, a blank string is returned.
         *
         * @param string locale to lookup
         * @return string 'rtl' or 'ltr' for valid locales, or a blank string for unknowns
         */
        function getDirectionForLocale($locale) {
            if (array_key_exists($locale, $this->languages)) {
                if (in_array($locale, $this->rtl_languages)) {
                    return 'rtl';
                } else {
                    return 'ltr';
                }
            }

            return '';
        }

        /**
         * This is a function for getting a language's native name from a
         * locale.  If the name is not available, a blank string is returned.
         *
         * @param string locale to lookup
         * @return string native name for locale
         */
        function getNativeNameForLocale($locale) {
            if (array_key_exists($locale, $this->languages)) {
                return $this->languages[$locale]['native'];
            }
        }

        /**
         * This is a function for getting a language's English name from a
         * locale.  If the name is not available, a blank string is returned.
         *
         * @param string locale to lookup
         * @return string native name for locale
         */
        function getEnglishNameForLocale($locale) {
            if (array_key_exists($locale, $this->languages)) {
                return $this->languages[$locale]['English'];
            }
        }

        /**
         * This is a function for getting all the locations for a locale (as defined by the array above).
         *
         * @param string locale to lookup
         * @return array of locations in the format:
         *  array(
         *      'primary' => array()
         *      'secondary' => array()
         *      )
         */
        function getLocationsForLocale($locale) {
            $_result = array('primary' => array(), 'secondary' => array());

            foreach ($this->locations as $region => $k) {
                if (in_array($locale, $k['primary'])) {
                    $_result['primary'][] = $region;
                }
                if (in_array($locale, $k['secondary'])) {
                    $_result['secondary'][] = $region;
                }
            }

            return $_result;
        }

        /**
         * This function will sort this classes languages array so it's sorted by
         * English name instead of locale code.
         *
         * @return array sorted by English name
         */
        function getLanguageArraySortedByEnglishName() {
            $_temp = $this->languages;

            uasort($_temp, create_function('$a,$b', 'return strcasecmp($a["English"],$b["English"]);'));

            return $_temp;
        }

    }

?>
