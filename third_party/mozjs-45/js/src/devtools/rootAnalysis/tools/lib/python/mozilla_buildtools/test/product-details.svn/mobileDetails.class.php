<?php

    require_once 'localeDetails.class.php';
    require_once 'mobile_alpha_version.php';
    require_once 'mobile_beta_version.php';
    require_once 'mobile_latest_version.php';

    /**
     * Holds data related to the current versions of Firefox Mobile.
     *
     * @author Alex Buchanan <abuchanan@mozilla.com>
     */
    class mobileDetails {
        // XY.0 for the first release, XY.0.Z for the next releases
        const latest_version = latest_version_;
        const beta_version = beta_version_;
        const alpha_version = alpha_version_;
        const maemo = 'maemo';
        const android = 'android';
        const windows = 'win32';
        const mac = 'mac';
        const linux = 'linux-i686';

        static $platform_names = array(
          self::android => 'Android',
          self::maemo => 'Maemo',
        );

        static $single_locales = array(
          'ar',
          'be',
          'ca',
          'el',
          'es-AR',
          'eu',
          'fa',
          'fy-NL',
          'ga-IE',
          'gl',
//          'he',
          'hu',
          'ja',
//          'ko',
          'lt',
          'nn-NO',
          'pa-IN',
          'pt-BR',
          'ro',
          'sk',
          'sl',
          'sr',
          'tr',
          'uk',
          'vi',
          'zh-CN',
          'zh-TW',
        );

        // supported locales
        static $multi_locales = array(
          'cs',
          'de',
          'en-US',
          'es-ES',
          'fi',
          'fr',
          'it',
//          'nb-NO',
          'nl',
          'pl',
          'pt-PT',
          'ru',
        );

        static $beta_single_locales = array(
        );

        static $beta_multi_locales = array(
          'cs',
          'de',
          'en-US',
          'es-ES',
          'fi',
          'fr',
          'it',
          'ja',
//          'nb-NO',
          'nl',
          'pl',
          'pt-PT',
          'ru',
        );

        static $alpha_single_locales = array();
        static $alpha_multi_locales = array(
          'en-US',
        );

        static $desktop_platforms = array(
            self::windows => 'http://ftp.mozilla.org/pub/mozilla.org/mobile/releases/4.0.1/win32-i686/fennec-4.0.1.en-US.win32.zip',
            self::mac => 'http://ftp.mozilla.org/pub/mozilla.org/mobile/releases/4.0.1/macosx-i686/fennec-4.0.1.en-US.mac.dmg',
            self::linux => 'http://ftp.mozilla.org/pub/mozilla.org/mobile/releases/4.0.1/linux-i686/fennec-4.0.1.en-US.linux-i686.tar.bz2',
        );

        static function all_locales() {
            $all_locales = array_merge(self::$multi_locales, self::$single_locales);
            sort($all_locales);
            return $all_locales;
        }

        static function beta_locales() {
            $all_locales = array_merge(self::$beta_multi_locales, self::$beta_single_locales);
            sort($all_locales);
            return $all_locales;
        }

        static function alpha_locales() {
            $all_locales = array_merge(self::$alpha_multi_locales, self::$alpha_single_locales);
            sort($all_locales);
            return $all_locales;
        }

        // @return array - locale names and download links for primary builds
        static function primary_builds($clean = true) {
            $localeDetails = new localeDetails;
            $version = self::latest_version;

            $ret = array();
            foreach (self::all_locales() as $locale) {
                $ret[] = array(
                    'locale' => array(
                        'code' => $locale,
                        'english' => $localeDetails->getEnglishNameForLocale($locale),
                        'native' => $localeDetails->getNativeNameForLocale($locale),
                    ),
                    'download' => array(
                        self::maemo => self::download_url($locale, self::maemo, $version, $clean),
                        self::windows => self::download_url($locale, self::windows, $version, $clean),
                        self::mac => self::download_url($locale, self::mac, $version, $clean),
                        self::linux => self::download_url($locale, self::linux, $version, $clean),
                    ),
                );
            }
            return $ret;
        }

        static function beta_builds($clean = true) {
            $localeDetails = new localeDetails;
            $version = self::beta_version;

            $ret = array();
            foreach (self::beta_locales() as $locale) {
                $ret[] = array(
                    'locale' => array(
                        'code' => $locale,
                        'english' => $localeDetails->getEnglishNameForLocale($locale),
                        'native' => $localeDetails->getNativeNameForLocale($locale),
                    ),
                    'download' => array(
                        self::maemo => self::download_url($locale, self::maemo, $version, $clean),
                        self::android => self::download_url($locale, self::android, $version, $clean),
                    ),
                );
            }
            return $ret;
        }

        static function alpha_builds($clean = true) {
            $localeDetails = new localeDetails;
            $version = self::alpha_version;

            $ret = array();
            foreach (self::alpha_locales() as $locale) {
                $ret[] = array(
                    'locale' => array(
                        'code' => $locale,
                        'english' => $localeDetails->getEnglishNameForLocale($locale),
                        'native' => $localeDetails->getNativeNameForLocale($locale),
                    ),
                    'download' => array(
                        self::maemo => self::download_url($locale, self::maemo, $version, $clean),
                        self::android => self::download_url($locale, self::android, $version, $clean),
                    ),
                );
            }
            return $ret;
        }

        // @return string - URL of release notes for a specific version
        static function release_notes_url($version = self::latest_version) {
            return '/mobile/'.$version.'/releasenotes';
        }


        // @return string - download URL for specific build (locale + platform + version)
        static function download_url($locale = 'en-US', $platform = self::maemo, $version = self::latest_version, $clean = true) {
            $locale = self::_check_locale($locale);
            if ($locale == 'ja' && $platform == self::mac)
                $locale = 'ja-JP-mac';

            if (array_key_exists($platform, self::$desktop_platforms)) {
                $url = self::$desktop_platforms[$platform];
            } elseif ($platform == 'android') {
                if($version == self::beta_version) {
                    $url = 'market://details?id=org.mozilla.firefox_beta';
                }
                else {
                    $url = 'market://details?id=org.mozilla.firefox';
                }
            } else {
                $lang_param = 'multi';
                if ($version == self::latest_version && in_array($locale, self::$single_locales))
                    $lang_param = $locale;
                elseif ($version == self::beta_version && in_array($locale, self::$beta_single_locales))
                    $lang_param = $locale;
                if ($version == self::alpha_version && in_array($locale, self::$alpha_single_locales))
                    $lang_param = $locale;

                $url = 'https://download.mozilla.org/?product=firefox-mobile-'.$version.'&os='.$platform.'&lang='.$lang_param;
            }

            if ($clean)
                $url = htmlentities($url);

            return $url;
        }

        static function android_web_link($link) {
            return "https://market.android.com/" . ltrim($link, 'market://');
        }

        // @return string - given a locale code, check that we have a build in that locale.  if not, fallback to en-US
        static function _check_locale($locale) {
            return in_array($locale, self::all_locales()) ? $locale : 'en-US';
        }
    }
