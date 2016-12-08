<?php

    require_once dirname(__FILE__).'/localeDetails.class.php';

    /**
     * We wanted to keep this class as modular as possible, but realized we needed a
     * way to localize the content, so we used the existing ___() function for
     * mozilla websites.  If other projects use this class without that function (or
     * the need for localization), it should still work, so this is a dummy function
     * that just returns what it is given.
     *
     * This function is commented out because other sites may not define
     * it before they include this file.  If your site needs a dummy function, copy
     * this one to another file and use it:
     *
        if (!function_exists('___')) {
            function ___($input) {
                return $input;
            }
        }
     */

    /**
     * Provide a common interface to get to the firefox and thunderbird classes, as well as
     * holding some shared functions between them.  You shouldn't be calling any functions
     * in this class directly - only classes that extend this class should be calling them,
     * since they require $this->primary_builds and $this->beta_builds to exist.
     *
     * If we move to php5 and can access object's arrays statically, we can make a lot of
     * these functions static.
     *
     * @author Wil Clouser <clouserw@mozilla.com>
     *
     */
    class productDetails {

        /**
         * Hold a copy of the localeDetails class
         * @var object
         */
        var $localeDetails;
        var $download_base_url_direct;
        var $download_base_url_transition;
        var $has_transition_download_page = array();

        /**
         * Constructor:  Setup class variables.
         */
        function productDetails() {
            $this->localeDetails = new localeDetails();
            $this->download_base_url_direct = 'https://download.mozilla.org/';
            $this->download_base_url_transition = '/products/download.html';
            $this->has_transition_download_page = array('en-US');
        }

        /**
         * Looks up the size of a file from the locale and platform.
         *
         * @param string locale
         * @param string platform
         * @return float file size
         */
        function getFilesizeForLocaleAndPlatform($locale, $platform) {
            $_current_version = $this->getNewestVersionForLocale($locale);

            $_size = 0;

            if (isset($this->primary_builds[$locale][$_current_version][$platform]) && array_key_exists('filesize', $this->primary_builds[$locale][$_current_version][$platform])) {
                $_size = $this->primary_builds[$locale][$_current_version][$platform]['filesize'];
            }

            if ($_size == 0 && array_key_exists('filesize', $this->beta_builds[$locale][$_current_version][$platform])) {
                $_size = $this->beta_builds[$locale][$_current_version][$platform]['filesize'];
            }

            $_rounded = round($_size, 1);

            if (!in_array($locale, array('as', 'bn-BD', 'bn-IN', 'en-GB', 'en-US', 'gu-IN', 'ga-IE', 'he', 'hi-IN', 'ja', 'kn', 'ml', 'mr', 'or', 'pa-IN', 'si', 'ta', 'ta-LK', 'te', 'th', 'zh-CN', 'zh-TW'))) {
                return ((strpos($_rounded, '.'))) ? number_format($_rounded, 1, ',', ' ') : $_rounded;
            } else {
                return $_rounded;
            }
        }

        /**
         * Retrieve the most current version of the product for a specified locale.  If a
         * version is not found in the primary builds, this will look in the beta
         * builds as well.  This determines the "newest" version for a locale by
         * looking at the order of the array(!).  The newest version should be after
         * the older version.
         *
         * @param string locale (eg. de or en-US)
         * @return string most current version of the product for the specified locale
         */
        function getNewestVersionForLocale($locale) {

            $version = $this->getNewestVersionForLocaleFromBuildArray($locale, $this->primary_builds);

            if (empty($version)) {
                $version = $this->getNewestVersionForLocaleFromBuildArray($locale, $this->beta_builds);
            }

            return $version;
        }

        /**
         * Retrieve the most current version of the product for a specified locale from a
         * specified build array.  This differs from getNewestVersionForLocale() becuase
         * in some cases (eg. a single locale has both a primary and beta build) we need to
         * retrieve the newest version but restrict it to a single build array.  This function
         * will NOT return a devel version for a locale.
         *
         * @param string locale (eg. de or en-US)
         * @param array build array
         * @return string most current version of the product for the specified locale
         */
        function getNewestVersionForLocaleFromBuildArray($locale, $build_array) {

            // We have to do a foreach() loop here because we might have a version number
            // that only contains a null value.  In that case, we want to offer the next most recent
            // version
            if (isset($build_array[$locale]) && is_array($build_array[$locale]) && !empty($build_array[$locale])) {
                foreach (array_reverse($build_array[$locale]) as $version => $platforms) {

                    if ($version == LATEST_FIREFOX_DEVEL_VERSION || $version == FIREFOX_AURORA || $version == FIREFOX_ESR ) {
                        continue;
                    }

                    if ($platforms !== null) {
                        return $version;
                    }
                }
            }

            return '';
        }

        /**
         * Retrieve the most current ESR version of the product for a specified locale.  If a
         * version is not found in the primary builds, this will look in the beta
         * builds as well.  This determines the "newest" version for a locale by
         * looking at the order of the array(!).  The newest version should be after
         * the older version.
         *
         * @param string locale (eg. de or en-US)
         * @return string most current version of the product for the specified locale
         */
        function getNewestESRVersionForLocale($locale) {

            $version = $this->getNewestESRVersionForLocaleFromBuildArray($locale, $this->primary_builds);

            if (empty($version)) {
                $version = $this->getNewestESRVersionForLocaleFromBuildArray($locale, $this->beta_builds);
            }

            return $version;
        }

        /**
         * Retrieve the most current ESR version of the product for a specified locale from a
         * specified build array.  This differs from getNewestVersionForLocale() becuase
         * in some cases (eg. a single locale has both a primary and beta build) we need to
         * retrieve the newest version but restrict it to a single build array.  This function
         * will NOT return a devel version for a locale.
         *
         * @param string locale (eg. de or en-US)
         * @param array build array
         * @return string most current version of the product for the specified locale
         */
        function getNewestESRVersionForLocaleFromBuildArray($locale, $build_array) {

            // We have to do a foreach() loop here because we might have a version number
            // that only contains a null value.  In that case, we want to offer the next most recent
            // version
            if (isset($build_array[$locale]) && is_array($build_array[$locale]) && !empty($build_array[$locale])) {
                foreach (array_reverse($build_array[$locale]) as $version => $platforms) {

                    if ($version == LATEST_FIREFOX_DEVEL_VERSION || $version == FIREFOX_AURORA || $version == LATEST_FIREFOX_VERSION ) {
                        continue;
                    }

                    if ($platforms !== null) {
                        return $version;
                    }
                }
            }

            return '';
        }

        /**
         * Retrieve the development version of the product for a specified locale from a
         * specified build array.  This function is essentially just checking if a development build exists
         * (i.e. a version equivalent to the LATEST_FIREFOX_DEVEL_VERSION) for a locale.  This function has no
         * no fallback and returns an empty string if there is no devel version for that locale.
         *
         * @param string locale (eg. de or en-US)
         * @param array build array
         * @return string most current version of the product for the specified locale or empty
         */
        function getDevelVersionForLocaleFromBuildArray($locale, $build_array) {

            if (isset($build_array[$locale][LATEST_FIREFOX_DEVEL_VERSION]) &&
                is_array($build_array[$locale][LATEST_FIREFOX_DEVEL_VERSION]) &&
                !empty($build_array[$locale][LATEST_FIREFOX_DEVEL_VERSION])) {
                    return LATEST_FIREFOX_DEVEL_VERSION;
            }

            return '';
        }

        /**
         * Retrieves the development version of the product we have in our build arrays.  If the locale
         * is not found in the primary_builds array, it will look in beta_builds.
         *
         * @param string locale (eg. de or en-US)
         * @return string oldest version of product for the specified locale
         */
        function getDevelVersionForLocale($locale) {

            $version = $this->getDevelVersionForLocaleFromBuildArray($locale, $this->primary_builds);

            if (empty($version)) {
                $version = $this->getDevelVersionForLocaleFromBuildArray($locale, $this->beta_builds);
            }

            return $version;
        }

        /**
         * Retrieve the Aurora Channel version of the product for a specified locale from a
         * specified build array.  This function is essentially just checking if a development build exists
         * (i.e. a version equivalent to the FIREFOX_AURORA) for a locale.  This function has
         * no fallback and returns an empty string if there is no devel version for that locale.
         *
         * @param string locale (eg. de or en-US)
         * @param array build array
         * @return string most current version of the product for the specified locale or empty
         */
        function getAuroraVersionForLocaleFromBuildArray($locale, $build_array) {

            if (isset($build_array[$locale][FIREFOX_AURORA]) &&
                is_array($build_array[$locale][FIREFOX_AURORA]) &&
                !empty($build_array[$locale][FIREFOX_AURORA])) {
                    return FIREFOX_AURORA;
            }

            return '';
        }

        /**
         * Retrieves the Aurora channel version of the product we have in our build arrays.  If the locale
         * is not found in the primary_builds array, it will look in beta_builds.
         *
         * @param string locale (eg. de or en-US)
         * @return string Aurora Channel version of product for the specified locale
         */
        function getAuroraVersionForLocale($locale) {

            $version = $this->getAuroraVersionForLocaleFromBuildArray($locale, $this->primary_builds);

            if (empty($version)) {
                $version = $this->getAuroraVersionForLocaleFromBuildArray($locale, $this->beta_builds);
            }

            return $version;
        }




        /**
         * Retrieves the oldest version of the product we have in our build arrays.  If the locale
         * is not found in the primary_builds array, it will look in beta_builds.  This is useful
         * to find an older version of a product.
         *
         * @param string locale (eg. de or en-US)
         * @return string oldest version of product for the specified locale
         */
        function getOldestVersionForLocale($locale) {

            $version = $this->getOldestVersionForLocaleFromBuildArray($locale, $this->primary_builds);

            if (empty($version)) {
                $version = $this->getOldestVersionForLocaleFromBuildArray($locale, $this->beta_builds);
            }

            return $version;
        }

        /**
         * Retrieves the oldest version of the product we have in our build arrays.  This function
         * accepts a build array as a second arguement that restricts it's searching to only that
         * array. This will NOT return a development version even if no other versions exist.
         *
         * @param string locale (eg. de or en-US)
         * @param array build array
         * @return string oldest version of product for the specified locale
         */
        function getOldestVersionForLocaleFromBuildArray($locale, $build_array) {

            // We have to do a foreach() loop here because we might have a version number
            // that only contains a null value.  In that case, we want to offer the next most recent
            // version
            if (isset($build_array[$locale]) && is_array($build_array[$locale]) && !empty($build_array[$locale])) {
                foreach ($build_array[$locale] as $version => $platforms) {

                    if ($version == LATEST_FIREFOX_DEVEL_VERSION || $version == FIREFOX_AURORA ) {
                        continue;
                    }

                    if ($platforms !== null) {
                        return $version;
                    }
                }
            }

            return '';
        }

        /**
         * Return an HTML block.  This function depends on the ___() function being
         * defined and an appropriate .lang file loaded.  See
         * _getDownloadBlockListHtmlForLocaleAndPlatform() for additional layout
         * options
         *
         * @param string locale (eg. de or en-US)
         * @param array optional parameters to adjust return value:
         *      include_js: boolean, include <script> code for offering the best download. Default: true
         *      js_replace_links: boolean, should we replace the direct download
         *          links with /products/download.html instead? (moz.com uses this,
         *          moz-eu.org does not.  Even if set, the JS won't replace the link
         *          for IE browsers (details in the JS).  Default: false
         *      ancillary_links: boolean, include extra links after download button. Default: true
         *      locale_link_override: string, a locale string to use for the ancillary links.  This is very much a corner
         *          case, to be used when drawing multiple download blocks (with getDownloadBlockForLocales).  When this
         *          is set, the ancillary links will use this locale instead of the last locale block drawn. Default: none
         *      devel_version: boolean, return a development release.  Default: false
         *
         * @return string HTML block
         */
        function getDownloadBlockForLocale($locale, $options=array()) {
            $_include_js               = array_key_exists('include_js', $options) ?  $options['include_js'] : true;
            $_js_replace_links         = array_key_exists('js_replace_links', $options) ?  $options['js_replace_links'] : false;
            $_include_ancillary_links  = array_key_exists('ancillary_links', $options) ? $options['ancillary_links'] : true;
            $_locale_link_override     = array_key_exists('locale_link_override', $options) ? $options['locale_link_override'] : $locale;
            $_download_parent_override = array_key_exists('download_parent_override', $options) ? $options['download_parent_override'] : 'main-feature';
            $_devel_version            = array_key_exists('devel_version', $options) ? $options['devel_version'] : false;
            $_aurora_version           = array_key_exists('aurora_version', $options) ? $options['aurora_version'] : false;
            $_android_version          = array_key_exists('android_version', $options) ? $options['android_version'] : false;

            $_ancillary_links = $_include_ancillary_links ? $this->getAncillaryLinksForLocale($_locale_link_override, $options) : '';

            if ($_aurora_version) {
                $_current_version = $this->getAuroraVersionForLocaleFromBuildArray($locale, $this->primary_builds);
            } elseif ($_devel_version) {
                $_current_version = $this->getDevelVersionForLocaleFromBuildArray($locale, $this->primary_builds);
            } else {
                $_current_version = $this->getNewestVersionForLocale($locale);
            }

            // if we have no builds at all, let's default to en-US so as to display download boxes on pages
            if (empty($this->primary_builds[$locale][$_current_version]) && empty($this->beta_builds[$locale][$_current_version])) {
                $locale = 'en-US';
            }

            $_in_primary = empty($this->primary_builds[$locale][$_current_version]) ? array('') : $this->primary_builds[$locale][$_current_version];
            $_in_beta    = empty($this->beta_builds[$locale][$_current_version]) ? array('') : $this->beta_builds[$locale][$_current_version];
            $_platforms  = array('Windows', 'Linux', 'OS X');

            foreach ($_platforms as $_os_var) {

                if (array_key_exists($_os_var, $_in_primary) && !isset($_in_primary[$_os_var]['unavailable']) ) {
                    $_os_support[$_os_var] = true;
                } elseif (array_key_exists($_os_var, $_in_beta) && !isset($_in_beta[$_os_var]['unavailable']) ) {
                    $_os_support[$_os_var] = true;
                } else {
                    $_os_support[$_os_var] = false;
                }

                if ($_os_support[$_os_var] == true) {
                    // Special case for OS X and Japanese...
                    $_t_locale = ($locale == 'ja') ? 'ja-JP-mac' : $locale;
                    $_temp[$_os_var] = $this->_getDownloadBlockListHtmlForLocaleAndPlatform($_t_locale, $_os_var, $options);
                } else {
                    $_temp[$_os_var] = $this->_getDownloadBlockListHtmlForLocaleAndPlatform('en-US', $_os_var, $options);
                }
            }

            $_li_windows = $_temp['Windows'];
            $_li_linux   = $_temp['Linux'];
            $_li_osx     = $_temp['OS X'];

            $_li_android = "";
            if ($_android_version) {
                $_li_android = $this->_getDownloadBlockListHtmlForLocaleAndPlatform('en-US', 'Android', $options);
            }


            if ($_include_js) {
                $_js_replace_links = $_js_replace_links ? 'true' : 'false';
                $_js_include = <<<JS_INCLUDE
                <script type="text/javascript">// <![CDATA[
                    var download_parent_override = '{$_download_parent_override}';

                    if ({$_js_replace_links} && ('function' == typeof window.replaceDownloadLinksForId)) {
                        replaceDownloadLinksForId(download_parent_override);
                    }
                    if ('function' == typeof window.offerBestDownloadLink) {
                        offerBestDownloadLink(download_parent_override);
                    }
                // ]]></script>
JS_INCLUDE;

            } else {
                $_js_include = '';
            }

            $_return = <<<HTML_RETURN

            <ul class="home-download">
{$_li_windows}
{$_li_linux}
{$_li_osx}
{$_li_android}
            </ul>

{$_ancillary_links}

{$_js_include}
HTML_RETURN;

            return $this->tweakString($_return, $options);
        }

        /**
         * Return an <li> HTML block (to drop directly into an existing <ul>.  This function depends on the ___() function being
         * defined and an appropriate .lang file loaded.  This is a protected function
         * for use by getDownloadBlockForLocale()
         *
         * @access protected
         * @param string locale (eg. de or en-US)
         * @param string platform (Windows, Linux, or OS X)
         * @param array optional parameters to adjust return value:
         *      product: string, what is the name of the product? Default: firefox
         *      extra_link_attr: string, a complete string to add to the <a> tag on the primary
         *          download link.  Useful for urchinTracker's onclick="" stuff. Default: none
         *      layout: Determines what the returned HTML contains and what order. Default: none
         *      download_product: an L10n variable for the main button.  Your .lang
         *          file has to have whatever this value is in order to localize it!
         *
         * @return string HTML block
         */
    function _getDownloadBlockListHtmlForLocaleAndPlatform($locale, $platform, $options=array()) {
            $_product         = array_key_exists('product', $options) ?  $options['product'] : 'firefox';
            $_extra_link_attr = array_key_exists('extra_link_attr', $options) ? $options['extra_link_attr'] : '';
            $_layout          = array_key_exists('layout', $options) ? $options['layout'] : '';
            $_devel_version   = array_key_exists('devel_version', $options) ? $options['devel_version'] : false;
            $_aurora_version   = array_key_exists('aurora_version', $options) ? $options['aurora_version'] : false;

            if ($_devel_version) {
                $_current_version = $this->getDevelVersionForLocaleFromBuildArray($locale, $this->primary_builds);
            } else {
                $_current_version = $this->getNewestVersionForLocale($locale);
            }

            $_download_product            = array_key_exists('download_product', $options) ?  ___($options['download_product']) : ___('Download Now - Free');
            $_other_systems_and_languages = ___('Other Systems and Languages');
            $_language_name               = $this->localeDetails->getNativeNameForLocale($locale);
            $_windows_name                = ___('Windows');
            $_linux_name                  = ___('Linux');
            $_osx_name                    = ___('Mac OS X');
            $_mb                          = ___('MB');
            $_megabytes                   = ___('MegaBytes');

            if (in_array($locale, $this->has_transition_download_page)) {
               $_download_base_url = $this->download_base_url_transition;
            } else {
               $_download_base_url = $this->download_base_url_direct;
            }

            switch ($platform) {
                case 'Windows':
                    $_os_class     = 'os_windows';
                    $_os_shortname = 'win';
                    $_os_name      = ___('Windows');
                    $_os_file_ext  = 'win32.installer.exe';
                    break;
                case 'Linux':
                    $_os_class     = 'os_linux';
                    $_os_shortname = 'linux';
                    $_os_name      = ___('Linux');
                    $_os_file_ext  = 'linux-i686.tar.bz2';
                    break;
                case 'OS X':
                    $_os_class     = 'os_osx';
                    $_os_shortname = 'osx';
                    $_os_name      = ___('Mac OS X');
                    $_os_file_ext  = 'mac.dmg';
                    break;
                case 'Android':
                    $_os_class     = 'os_android';
                    $_os_shortname = 'android';
                    $_os_name      = ___('Android');
                    $_os_store_link = 'https://play.google.com/store/apps/details?id=org.mozilla.' . $_product;
                    break;
                default:
                    return;
            }

            switch ($_layout) {
                case 'subpage':

                    $_download_title = $options['download_title'];

                    $_product_version = $_aurora_version ? "aurora" : "";
                    $_download_link = isset($_os_store_link) ? $_os_store_link : $this->_getDownloadLink($_download_base_url, $_product, $_product_version, $_current_version, $_os_shortname, $locale);
                    $_download_link_direct = isset($_os_store_link) ? $_os_store_link : $this->_getDownloadLink($this->download_base_url_direct, $_product, $_product_version, $_current_version, $_os_shortname, $locale);

                    $_return = <<<LI_SIDEBAR
                    <li class="{$_os_class}">
                        <a class="download-link download-{$_product}" onclick="init_download('{$_download_link_direct}');" href="{$_download_link}" {$_extra_link_attr}><span class="download-content">
                            <span class="download-title">{$_download_product}</span>
                            {$_download_title}
                        </span></a>
                    </li>
LI_SIDEBAR;
                    break;

                case 'sidebar':

                    $_download_title = $options['download_title'];

                    $_return = <<<LI_SIDEBAR
                    <li class="{$_os_class}">
                        <h3>{$_download_title}</h3>
                        <div>{$_current_version}, {$_language_name}, {$_os_name}</div>
                        <a class="download-link download-{$_product}" onclick="init_download('{$this->download_base_url_direct}?product={$_product}-{$_current_version}&amp;os={$_os_shortname}&amp;lang={$locale}');" href="{$_download_base_url}?product={$_product}-{$_current_version}&amp;os={$_os_shortname}&amp;lang={$locale}" {$_extra_link_attr}>
                            <span class="{$_os_class} download-link-text">{$_download_product}</span>
                        </a>
                    </li>
LI_SIDEBAR;
                    break;

                case 'main':
                default:
                    $_return = <<<LI_MAIN
                    <li class="{$_os_class}">
                        <a class="download-link download-{$_product}" onclick="init_download('{$this->download_base_url_direct}?product={$_product}-{$_current_version}&amp;os={$_os_shortname}&amp;lang={$locale}');" href="{$_download_base_url}?product={$_product}-{$_current_version}&amp;os={$_os_shortname}&amp;lang={$locale}" {$_extra_link_attr}>
                            <span>
                                <strong>{$_download_product}</strong>
                                <em>{$_os_name} ({$_current_version}, {$_language_name}</em>
                            </span>
                        </a>
                    </li>
LI_MAIN;
                    break;
            }

            return $_return;
    }

        /**
         * A convenience function for showing download links for multiple locales.
         * This will only display one set of ancillary links and js calls
         *
         * @param array locales
         * @param array options (more detail in getDownloadBlockForLocale())
         * @return string HTML block
         */
        function getDownloadBlockForLocales($locales, $options=array()) {

            $_original_ancillary_link_value = array_key_exists('ancillary_links', $options) ? $options['ancillary_links'] : null;
            $_original_include_js_value     = array_key_exists('include_js', $options) ? $options['include_js'] : null;
            $_return = '';
            $_start = 1;

            foreach ($locales as $locale) {

                // If we're on the last locale, pass in the original value of the
                // ancillary link option.  Otherwise, it's false.
                if ($_start >= count($locales)) {
                    if ($_original_ancillary_link_value !== null) {
                        $options['ancillary_links'] = $_original_ancillary_link_value;
                    } else {
                        unset($options['ancillary_links']);
                    }
                    if ($_original_include_js_value !== null) {
                        $options['include_js'] = $_original_include_js_value;
                    } else {
                        unset($options['include_js']);
                    }
                } else {
                    $options['ancillary_links'] = false;
                    $options['include_js']      = false;
                }

                $_return .= $this->getDownloadBlockForLocale($locale, $options);
                $_start++;
            }

            return $_return;
        }

        /**
         * Return a <table> with the links to the newest versions of all locales in the given
         * build array
         *
         * @access protected
         * @param array A build array (eg. $this->primary_builds)
         * @param array optional parameters to adjust return value:
         *      product: string, what is the name of the product? Default: firefox
         *      include_js: boolean, include <script> code for offering the best download. Default: true
         *      latest_version: string, what is the newest version of the product?  Used for bolding. Default: 0
         *      product_version: string, either 'newest', 'devel', or 'oldest'. Default: newest
         *          newest: return the latest version available
         *          oldest: return the oldest version available
         *          devel: return a version equal to LATEST_FIREFOX_DEVEL_VERSION
         *          aurora: return a version equal to FIREFOX_AURORA
         *      force_output: boolean, even if there are no rows in the table, do you want output? Default: false
         *
         * @return string HTML block
         */
        function _getDownloadTableFromBuildArray($build_array, $options=array()) {

            $_product         = array_key_exists('product', $options) ?  $options['product'] : 'firefox';
            $_include_js      = array_key_exists('include_js', $options) ?  $options['include_js'] : true;
            $_latest_version  = array_key_exists('latest_version', $options) ?  $options['latest_version'] : 0;
            $_product_version = array_key_exists('product_version', $options) ?  $options['product_version'] : 'newest';
            $_product_version = in_array($_product_version, array('newest', 'devel', 'oldest', 'aurora', 'esr')) ? $_product_version : 'newest';
            $_hide_empty_rows = array_key_exists('hide_empty_rows', $options) ?  $options['hide_empty_rows'] : true;
            $_force_output    = array_key_exists('force_output', $options) ?  $options['force_output'] : false;
            $_table_summary   = array_key_exists('download_table_summary', $options) ? $options['download_table_summary'] : ___('Firefox builds available for download.');
            $_beta_locale     = array_key_exists('beta', $options) ?  $options['beta'] : false;

            $_language          = ___('Language');
            $_version           = ___('Version');
            $_windows_name      = ___('Windows');
            $_linux_name        = ___('Linux');
            $_osx_name          = ___('Mac OS X');
            $_download          = ___('Download');
            $_not_yet_available = ___('Not Yet Available');

            $_zebra  = 'even';
            $_return = '';

            $build_array = $this->_sortBuildArrayByEnglishName($build_array);
            $nb_builds   = 0;
            $list_builds = '';

            foreach ($build_array as $locale => $versions) {
                $_english_language_name = $this->localeDetails->getEnglishNameForLocale($locale);
                $_native_language_name  = $this->localeDetails->getNativeNameForLocale($locale);

                switch ($_product_version) {
                    case 'oldest':
                        $_build_version = $this->getOldestVersionForLocaleFromBuildArray($locale, $build_array);
                        break;
                    case 'devel':
                        $_build_version = $this->getDevelVersionForLocaleFromBuildArray($locale, $build_array);
                        break;
                    case 'aurora':
                        $_build_version = $this->getAuroraVersionForLocaleFromBuildArray($locale, $build_array);
                        break;
                    case 'esr':
                        $_build_version = $this->getNewestESRVersionForLocaleFromBuildArray($locale, $build_array);
                        break;
                    case 'newest':
                    default:
                        $_build_version = $this->getNewestVersionForLocaleFromBuildArray($locale, $build_array);
                        break;
                }

                // A build version doesn't exist for this locale
                if ($_build_version === '') {
                    continue;
                }

                // We don't want older beta locales builds to be displayed
                // in parallel to the final version for the locale
                if ( ($_beta_locale == true) &&
                     ($_product_version == 'newest' || $_product_version == 'esr') &&
                     ($_build_version != $_latest_version) ) {
                    continue;
                }

                // Special case:  If the current version is the same as our oldest version,
                // we don't want to show it in the older table.
                if ( ($_product_version == 'oldest') && ($_build_version == $_latest_version) ) {
                    continue;
                }

                $_is_current_version = ($_build_version == $_latest_version) ? ' class="curVersion" ' : '';

                $_windows_link = $_linux_link = $_osx_link = '';

                if (in_array($locale, $this->has_transition_download_page)) {
                    $_download_base_url = $this->download_base_url_transition;
                    $_include_init_download = true;
                } else {
                    $_download_base_url = $this->download_base_url_direct;
                    $_include_init_download = false;
                }

                if (array_key_exists('Windows', $build_array[$locale][$_build_version])) {
                    if(!isset($build_array[$locale][$_build_version]['Windows']['unavailable'])) {
                        $_download_link = $this->_getDownloadLink($_download_base_url, $_product, $_product_version, $_build_version, 'win', $locale);
                        $_download_link_direct = $this->_getDownloadLink($this->download_base_url_direct, $_product, $_product_version, $_build_version, 'win', $locale);
                        $_onclick = ($_include_init_download) ?  "onclick=\"init_download('{$_download_link}');\"" : '';
                        $_windows_link = "<a {$_onclick} href=\"{$_download_link_direct}\" class=\"download-windows\">{$_download}</a>";
                    } else {
                        $_windows_link = $_not_yet_available;
                    }
                }

                if (array_key_exists('Linux', $build_array[$locale][$_build_version])) {
                    if(!isset($build_array[$locale][$_build_version]['Linux']['unavailable'])) {
                        $_download_link = $this->_getDownloadLink($_download_base_url, $_product, $_product_version, $_build_version, 'linux', $locale);
                        $_download_link_direct = $this->_getDownloadLink($this->download_base_url_direct, $_product, $_product_version, $_build_version, 'linux', $locale);
                        $_onclick = ($_include_init_download) ?  "onclick=\"init_download('{$_download_link}');\"" : '';
                        $_linux_link = "<a {$_onclick} href=\"{$_download_link_direct}\" class=\"download-linux\">{$_download}</a>";
                    } else {
                        $_linux_link = $_not_yet_available;
                    }
                }

                if (array_key_exists('OS X', $build_array[$locale][$_build_version])) {
                    if(!isset($build_array[$locale][$_build_version]['OS X']['unavailable'])) {
                        // Special case for OS X and Japanese...
                        $_t_locale = ($locale == 'ja') ? 'ja-JP-mac' : $locale;
                        $_download_link = $this->_getDownloadLink($_download_base_url, $_product, $_product_version, $_build_version, 'osx', $_t_locale);
                        $_download_link_direct = $this->_getDownloadLink($this->download_base_url_direct, $_product, $_product_version, $_build_version, 'osx', $_t_locale);
                        $_onclick = ($_include_init_download) ?  "onclick=\"init_download('{$_download_link}');\"" : '';
                        $_osx_link = "<a {$_onclick} href=\"{$_download_link_direct}\" class=\"download-osx\">{$_download}</a>";
                    } else {
                        $_osx_link = $_not_yet_available;
                    }
                }

                // This is a little bit ugly because we only show the download links if they exist.  Otherwise we have a 4span "not yet available" message


                if (is_null($build_array[$locale][$_build_version])) {
                    $_download_links = "<td colspan=\"4\" class=\"nya right\">{$_not_yet_available}</td>";
                } else {
                    $_download_links = "<td{$_is_current_version}>{$_build_version}</td>\n <td>{$_windows_link}</td>\n <td>{$_osx_link}</td>\n <td class=\"right\">{$_linux_link}</td>";
                    $nb_builds++;
                    $list_builds .= $locale.', ';
                }

                if (!($_hide_empty_rows && empty($_windows_link) && empty($_linux_link) && empty($_osx_link))) {
                    $_zebra = ($_zebra == 'even') ? 'odd' : 'even';
                    $_return .= <<<TABLE_ROW
                    <tr id="{$locale}" class="{$_zebra}">
                        <td class="left">{$_english_language_name}</td>
                        <td class="second" lang="{$locale}">{$_native_language_name}</td>
                        {$_download_links}
                    </tr>
TABLE_ROW;
                }
                unset($_windows_link, $_linux_link, $_osx_link, $_english_language_name, $_native_language_name, $_is_current_version);
            }

            if($_include_js) {
                $_js_include = <<<JS_INCLUDE
                <script type="text/javascript">// <![CDATA[
                    if ('function' == typeof window.replaceDownloadLinksForId) {
                        replaceDownloadLinksForId('downloads');
                    }
                // ]]></script>
JS_INCLUDE;
            } else {
                $_js_include = '';
            }

            if (!$_force_output && empty($_return)) {
                return '';
            }

            $_return = <<<TABLE_LAYOUT
    <table class="downloads dalvay-table" summary="{$_table_summary}">
        <thead>
            <tr>
            <th colspan="2" class="top-left">{$_language}</th>
            <th>{$_version}</th>
            <th><span class="download-windows">{$_windows_name}</span></th>
            <th><span class="download-osx">{$_osx_name}</span></th>
            <th class="top-right"><span class="download-linux">{$_linux_name}</span></th>
            </tr>
        </thead>
        <tfoot>
            <tr>
                <td class="bottom-left"></td>
                <td colspan="4"></td>
                <td class="bottom-right"></td>
            </tr>
        </tfoot>
        <tbody>
            {$_return}
        </tbody>
    </table>
    <!-- number of builds : {$nb_builds} -->
    <!-- list of builds : {$list_builds} -->


    {$_js_include}
TABLE_LAYOUT;

            return $_return;

        }

        /**
         * This is a post-logic pre-rendering hook.  It will allow you to make changes
         * to the output of any string.  It should be used as a last resort if other
         * options will let you make the same changes.  Original and replacement strings
         * are Perl compatible regular expressions.
         *
         * @param string the string to make changes to
         * @param array the full options array.  Any changes should be in $options['tweaks']
         *      and follow the following format:
         *              $options['tweaks'] = array('original' => 'replacement');
         *      Multiple tweaks are supported.
         *
         * @return string the modified string
         */
        function tweakString($string, $options=array()) {
            if (!array_key_exists('tweaks', $options) || count($options['tweaks']) == 0) {
                return $string;
            }

            $string = preg_replace(array_keys($options['tweaks']), array_values($options['tweaks']), $string);

            return $string;
        }

        /**
         * A complement to localeDetails::getLanguageArraySortedByEnglishName(), this
         * function will sort a build array by the English name.
         *
         * @return array sorted by English name
         */
        function _sortBuildArrayByEnglishName($build_array) {
            uksort($build_array, array($this, '_compareToLanguageArraySortedByEnglishName'));

            return $build_array;
        }

        /**
         * This function is used to resort an array by the keys in another array.
         * It's used by $this->_sortBuildArrayByEnglishName.  It has to be a separate
         * method because uksort() can only pass in 2 parameters and we needed to
         * access the languages array from localeDetails.
         *
         * @param string to compare first
         * @param string to compare second
         * @return int -1 if param1 < param2, else 1
         */
        function _compareToLanguageArraySortedByEnglishName($a, $b) {

            $_keys = array_keys($this->localeDetails->getLanguageArraySortedByEnglishName());

            $_position_a = array_search( $a, $_keys ) ;
            $_position_b = array_search( $b, $_keys ) ;

            return  $_position_a < $_position_b ? -1 : 1 ;
        }

        /**
         * This is a TEMPORARY functions while we migrate pages from mozilla-europe to mozilla.com.  This function
         * will return the language arrays into a format that the old download.js understands.
         *
         * There are currently two sets of behavior for our download buttons:
         *
         * 1) Locale and platform are detected by javascript, and the best download we can give is
         *    offered in the green download button.  This means someone visiting http://www.mozilla.com/de/
         *    with an English browser will be given an English download (even if the content of the page
         *    is in German).
         *
         * 2) Platform is detected by javascript and locale is determined by URL.  This means visiting
         *    http://www.mozilla.com/de/ will offer a German download, no matter what locale the visitor's
         *    browser is in.
         *
         * As of this writing, mozilla-europe.org is using method #2, mozilla.com is using method #1.  Our
         * goal is to get both sites on method #2, however, since mozilla.com only has en-US on it right now,
         * we'd only ever offer users an English download.  Until we get translated content for other locales,
         * we'll have to revert to option #1 on mozilla.com.  This function translates the php into js so we can
         * continue to provide that behavior.
         *
         * When this file is removed, grep for the following string:
         *          20070827_TEMP
         * Sections of code marked with that string are related directly to this file, and are not used
         * elsewhere (and can be removed).
         *
         *  -- clouserw
         */
        function getJavaScriptDownloadArray() {

            // Good thing this is going to be cached, because this is gonna be ugly...
            $_firefoxDetails = new firefoxDetails();
            $_thunderbirdDetails = new thunderbirdDetails();
            $_localeDetails = new localeDetails();

            // Building the array in php is going to be easier, so we'll do that first
            $_php_array = array();

            foreach ($_localeDetails->languages as $locale => $names) {
                $locale_array = explode('-',strtolower($locale));
                $_language_code = $locale_array[0];
                $_region_code = isset($locale_array[1]) ? $locale_array[1] : '-';

                $_newest_firefox = $_firefoxDetails->getNewestVersionForLocale($locale);
                $_oldest_firefox = $_firefoxDetails->getOldestVersionForLocale($locale);
                $_beta_firefox   = $_firefoxDetails->getDevelVersionForLocale($locale);
                $_aurora_firefox = $_firefoxDetails->getAuroraVersionForLocale($locale);

                $_newest_thunderbird = $_thunderbirdDetails->getNewestVersionForLocale($locale);
                $_oldest_thunderbird = $_thunderbirdDetails->getOldestVersionForLocale($locale);
                $_beta_thunderbird = $_thunderbirdDetails->getDevelVersionForLocale($locale);

                $_oldest_firefox = ($_oldest_firefox == $_newest_firefox) ? '' : $_oldest_firefox;
                $_oldest_thunderbird = ($_oldest_thunderbird == $_newest_thunderbird) ?  '' : $_oldest_thunderbird;

                $_php_array[$_language_code][$_region_code] = array(
                    'fx'        => $_newest_firefox,
                    'fxold'     => $_oldest_firefox,
                    'fxbeta'    => $_beta_firefox,
                    'fxaurora'  => $_aurora_firefox,
                    'tb'        => $_newest_thunderbird,
                    'tbold'     => $_oldest_thunderbird,
                    'tbbeta'    => $_beta_thunderbird,
                    'name'      => $_localeDetails->getEnglishNameForLocale($locale),
                    'localName' => $_localeDetails->getNativeNameForLocale($locale),
                );

            }

        $_final = '';
        $_final_region = '';

        foreach ($_php_array as $locale => $val) {
            $_final .= (empty($_final)) ? '' : ",\n";

            $_final .= '"'.$locale.'": {';

            foreach ($val as $region => $data) {
                $_fx       = empty($data['fx'])       ? 'null' : '"'.$data['fx'].'"';
                $_fxold    = empty($data['fxold'])    ? 'null' : '"'.$data['fxold'].'"';
                $_fxbeta   = empty($data['fxbeta'])   ? 'null' : '"'.$data['fxbeta'].'"';
                $_fxaurora = empty($data['fxaurora']) ? 'null' : '"'.$data['fxaurora'].'"';
                $_tb       = empty($data['tb'])       ? 'null' : '"'.$data['tb'].'"';
                $_tbold    = empty($data['tbold'])    ? 'null' : '"'.$data['tbold'].'"';
                $_tbbeta   = empty($data['tbbeta'])   ? 'null' : '"'.$data['tbbeta'].'"';

                $_final_region .= (empty($_final_region)) ? '' : ",\n";
                $_final_region .= <<<A_FINAL
 \t"{$region}": { fx: {$_fx},\tfxold: {$_fxold},\tfxbeta: {$_fxbeta},\tfxaurora: {$_fxaurora},\ttb: {$_tb},\ttbold: {$_tbold},\ttbbeta: {$_tbbeta},\tname: "{$data['name']}",\tlocalName: "{$data['localName']}" }
A_FINAL;
            }
            $_final .= $_final_region.' }';
            $_final_region = '';
        }

            return "var gLanguages = {\n{$_final}\n};";
        }

        /**
         * This is a temporary function for echoing out a <noscript> block under our download buttons.  This is
         * in php so it can automatically generate the versions.
         *          20070827_TEMP
         *
         */
        function getNoScriptBlockForLocale($locale, $options=array()) {
            $_product          = array_key_exists('product', $options) ?  $options['product'] : 'firefox';
            $_download_product = array_key_exists('download_product', $options) ?  ___($options['download_product']) : ___('Download Now - Free');
            $_devel_version    = array_key_exists('devel_version', $options) ? $options['devel_version'] : false;

            if ($_devel_version) {
                $_current_version = $this->getDevelVersionForLocale($locale);
            } else {
                $_current_version = $this->getNewestVersionForLocale($locale);
            }

            $_other_systems_and_languages = ___('Other Systems and Languages');
            $_windows_name                = ___('Windows');
            $_linux_name                  = ___('Linux');
            $_osx_name                    = ___('Mac OS X');
            $_mb                          = ___('MB');
            $_megabytes                   = ___('MegaBytes');
            $_native_language_name        = $this->localeDetails->getNativeNameForLocale($locale);

            $_windows_filesize = $this->getFilesizeForLocaleAndPlatform($locale, 'Windows');
            $_linux_filesize   = $this->getFilesizeForLocaleAndPlatform($locale, 'Linux');
            $_osx_filesize     = $this->getFilesizeForLocaleAndPlatform($locale, 'OS X');

            if (array_key_exists('Windows', $this->primary_builds[$locale][$_current_version]) || array_key_exists('Windows', $this->beta_builds[$locale][$_current_version])) {
                $_li_windows = <<<LI_WINDOWS
                <li><a href="https://download.mozilla.org/?product={$_product}-{$_current_version}&amp;os=win&amp;lang={$locale}">{$_windows_name}</a></li>
LI_WINDOWS;
            }
            if (array_key_exists('Linux', $this->primary_builds[$locale][$_current_version]) || array_key_exists('Linux', $this->beta_builds[$locale][$_current_version])) {
                $_li_linux = <<<LI_LINUX
                <li><a href="https://download.mozilla.org/?product={$_product}-{$_current_version}&amp;os=linux&amp;lang={$locale}">{$_linux_name}</a></li>
LI_LINUX;
            }
            if (array_key_exists('OS X', $this->primary_builds[$locale][$_current_version]) || array_key_exists('OS X', $this->beta_builds[$locale][$_current_version])) {

                // Special case for OS X and Japanese...
                $_t_locale = ($locale == 'ja') ? 'ja-JP-mac' : $locale;

                $_li_osx = <<<LI_OSX
                <li><a href="https://download.mozilla.org/?product={$_product}-{$_current_version}&amp;os=osx&amp;lang={$locale}">{$_osx_name}</a></li>
LI_OSX;
            }

            $_return = <<<HTML_RETURN

            <noscript>
                <div class="download download-noscript">
                <h3>{$_download_product} <span>({$_native_language_name} | <a href="http://www.mozilla.com/{$locale}/{$_product}/all.html">{$_other_systems_and_languages}</a>)</span></h3>
                <ul>
                    {$_li_windows}
                    {$_li_linux}
                    {$_li_osx}
                </ul>
                </div>
            </noscript>
HTML_RETURN;

            return $this->tweakString($_return, $options);

        }

        function _getDownloadLink($base_url, $product, $product_version, $build_version, $os, $locale) {
            if ($product_version == 'aurora') {
                switch ($os) {
                    case 'win':
                        $os_file_ext = 'win32.installer.exe';
                    break;
                    case 'linux':
                        $os_file_ext = 'linux-i686.tar.bz2';
                    break;
                    case 'osx':
                        $os_file_ext = 'mac.dmg';
                    break;
                }
                $base_url = 'http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-aurora-l10n';
                if ($locale == 'en-US') {
                    $base_url = 'http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-mozilla-aurora';
                }
                if ($locale == 'ja' && $os  == 'osx') {
                    $locale = 'ja-JP-mac';
                }
                return "{$base_url}/{$product}-{$build_version}.{$locale}.{$os_file_ext}";
            }

            return "{$base_url}?product={$product}-{$build_version}&amp;os={$os}&amp;lang={$locale}";
        }

    }
?>
