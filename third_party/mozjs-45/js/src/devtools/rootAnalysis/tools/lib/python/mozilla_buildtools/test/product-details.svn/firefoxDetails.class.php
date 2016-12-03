<?php

// LATEST_FIREFOX_RELEASED_VERSION and LATEST_FIREFOX_RELEASED_DEVEL_VERSION
// are only used by SUMO. Only update when builds are truly released.
require_once "LATEST_FIREFOX_VERSION.php";
require_once "LATEST_FIREFOX_RELEASED_VERSION.php";
require_once "LATEST_FIREFOX_DEVEL_VERSION.php";
require_once "LATEST_FIREFOX_RELEASED_DEVEL_VERSION.php";
require_once "LATEST_FIREFOX_OLDER_VERSION.php";
require_once "FIREFOX_AURORA.php";
require_once "FIREFOX_ESR.php";
// About once per year, we have 2 cycles during which we have two ESR
// FIREFOX_ESR_NEXT is used during the two cycles. Once _NEXT replace the
// "old" ESR, this line should be commented and the value moved to FIREFOX_ESR
require_once "FIREFOX_ESR_NEXT.php";

require_once dirname(__FILE__).'/productDetails.class.php';

/**
 * Holds data related to the current version of Firefox.
 *
 * Q: We're releasing a new version of Firefox - what should I do?
 * A:
 *      There is no single answer for this, because there are so many languages
 *      we support.  Use common sense.  A potential scenario is outlined below:
 *
 *      1) Update the LATEST_FIREFOX_VERSION define to the latest version
 *      2) For each language which has the new version, update the filesizes in
 *          the LATEST_FIREFOX_VERSION array
 *      3) For each language which does not have the new version, replace
 *          LATEST_FIREFOX_VERSION with the previous version for that language.
 *      4) Edit history/firefoxHistory.class.php to reflect the new version and
 *          date
 *
 *
 * @author Wil Clouser <clouserw@mozilla.com>
 *
 */
class firefoxDetails extends productDetails {

        /**
         * Array holding information about current available builds.  Filesize
         * is in megabytes. If you add a new language here, make sure it exists in
         * localeDetails::languages too.
         *
         *  If you don't want a download button to appear for a certain platform, just don't put that platform in the array
         *
         *  If you want "Not Yet Available" to appear for the locale, set the version to null.  If getDownloadBlockForLocale()
         *  is called, it will offer the most recent version that actually has a value.
         *
         * @var array
         */


        var $primary_builds = array(
            'ach'   => array(
                FIREFOX_AURORA,
                LATEST_FIREFOX_DEVEL_VERSION,
                LATEST_FIREFOX_VERSION,
                ),

            'af'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'an'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                ),

            'ar'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'as'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'ast'   => array(
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'az'    => array(
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                ),

            'be'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'bg'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'bn-BD' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'bn-IN' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'br'    => array(
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'bs'    => array(
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'ca'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'cs'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'csb'    => array(
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'cy'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'da'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'de'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'dsb'    => array(
                FIREFOX_AURORA,
                LATEST_FIREFOX_DEVEL_VERSION,
                LATEST_FIREFOX_VERSION,
                ),

            'el'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'en-GB' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'en-US' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'en-ZA' => array(
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'eo'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'es-AR' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'es-CL' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'es-ES' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'es-MX' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'et'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'eu'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'fa'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'ff'    => array(
                FIREFOX_AURORA,
                LATEST_FIREFOX_DEVEL_VERSION,
                LATEST_FIREFOX_VERSION,
                ),

            'fi'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'fr'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'fy-NL' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'ga-IE' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'gd'    => array(
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'gl'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'gu-IN' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'he'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'hi-IN' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'hr'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'hsb'    => array(
                FIREFOX_AURORA,
                LATEST_FIREFOX_DEVEL_VERSION,
                LATEST_FIREFOX_VERSION,
                ),

            'hu'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'hy-AM' => array(
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'id'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'is'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'it'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'ja'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'ka'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                ),

            'kk'    => array(
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                ),

            'km'    => array(
                FIREFOX_AURORA,
                LATEST_FIREFOX_DEVEL_VERSION,
                LATEST_FIREFOX_VERSION,
                ),

            'kn'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'ko'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'lij'   => array(
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                ),

            'lt'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'lv'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'mai'   => array(
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'mk'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'ml'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'mr'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'ms'    => array(
                FIREFOX_AURORA,
                LATEST_FIREFOX_DEVEL_VERSION,
                LATEST_FIREFOX_VERSION,
                ),

            'my'    => array(
                FIREFOX_AURORA,
                ),

            'nb-NO' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'nl'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'nn-NO' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'oc'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                // LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                // LATEST_FIREFOX_VERSION,
                ),

            'or'    => array(
                // LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'pa-IN' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'pl'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'pt-BR' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'pt-PT' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'rm'    => array(
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'ro'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'ru'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'si'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'sk'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'sl'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'sq'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'sr'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'sv-SE' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'sw'    => array(
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA         ,
                ),
            'ta'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_DEVEL_VERSION,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),
            'te'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'th'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'tr'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'uk'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'uz'    => array(
                FIREFOX_AURORA,
                ),

            'vi'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'xh'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                // FIREFOX_ESR,
                ),

            'zh-CN' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                ),

            'zh-TW' => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                LATEST_FIREFOX_VERSION,
                FIREFOX_ESR,
                FIREFOX_ESR_NEXT,
                            ),

        );

        /**
         * Array holding information about currently available beta builds.  Beta builds, in this case, means localizations that are still in beta.  If you're looking for
         * Firefox betas look for the LATEST_FIREFOX_DEVEL_VERSION in the $primary_builds array.
         *
         * @var array
         */
        var $beta_builds = array(


            'ku'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                LATEST_FIREFOX_DEVEL_VERSION,
                FIREFOX_AURORA,
                FIREFOX_ESR,
                ),

            'rm'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                ),


            'or'    => array(
                LATEST_FIREFOX_OLDER_VERSION,
                ),

            'sw'    => array(
                FIREFOX_AURORA,
                LATEST_FIREFOX_DEVEL_VERSION,
                ),


        );

        /**
         * Constructor.
         */
        function firefoxDetails() {
            parent::productDetails();
        }

        /**
         * Returns an HTML block with links for a certain locale
         *
         * @param string locale
         * @param array optional parameters to adjust return value:
         *      devel_version: boolean, return a development release.  Default: false
         * @return string HTML block
         */
        function getAncillaryLinksForLocale($locale, $options=array()) {
            $_devel_version            = array_key_exists('devel_version', $options) ? $options['devel_version'] : false;

            if ($_devel_version) {
                $_current_version = $this->getDevelVersionForLocaleFromBuildArray($locale, $this->primary_builds);
                $_all_page_link = 'all-beta.html';
            } else {
                $_current_version = $this->getNewestVersionForLocale($locale);
                $_all_page_link = 'all.html';
            }

            $_release_notes = ___('Release Notes');
            $_privacy_policy              = ___('Privacy');
            $_other_systems_and_languages = ___('Systems &amp; Languages');

            $_return = <<<HTML_RETURN
            <p class="download-other">
                <a class="ancillaryLink" href="http://www.mozilla.org/{$locale}/firefox/{$_all_page_link}">{$_other_systems_and_languages}</a> |
                <a class="ancillaryLink" href="http://www.mozilla.org/{$locale}/firefox/{$_current_version}/releasenotes/">{$_release_notes}</a> |
                <a class="ancillaryLink" href="http://www.mozilla.org/{$locale}/legal/privacy/firefox.html">{$_privacy_policy}</a>
            </p>
HTML_RETURN;

            return $_return;
        }

        /**
         * Overload parent function.  See parent for details.
         *
         */
        function getDownloadBlockForLocale($locale, $options=array()) {

            $options['product'] = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            # Used on the sidebar only
            $options['download_title'] = array_key_exists('download_title', $options) ? $options['download_title'] : ___('Free Download');
            $options['android_version'] = array_key_exists('android_version', $options) ? $options['android_version'] : true;

            return parent::getDownloadBlockForLocale($locale, $options);
        }

        /**
         * Convenience function to return a <table> of Firefox primary builds
         *
         * @param array options (more detail in getDownloadBlockForLocale())
         * @return string HTML block
         */
        function getDownloadTableForPrimaryBuilds($options=array()) {

            $options['product']        = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['latest_version'] = LATEST_FIREFOX_VERSION;
            $options['product_version'] = 'newest';
            $options['hide_empty_rows'] = true;


            $builds = array();

            foreach($this->primary_builds as $key => $val) {
                if( array_key_exists(LATEST_FIREFOX_VERSION, $val) ) {
                    $builds[$key] = $this->primary_builds[$key];
                }
            }


            return $this->tweakString($this->_getDownloadTableFromBuildArray($builds, $options), $options);
        }

        /**
         * Convenience function to return a <table> of Firefox primary builds
         * filtered by the specified keywords on the language names
         *
         * @param array options (more detail in getDownloadBlockForLocale())
         * @param string keywords; will be dropped in a regex expression
         * @return string HTML block
         */
        function getFilteredDownloadTableForPrimaryBuilds($options = array(), $keywords = '') {

            $options['product']         = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['hide_empty_rows'] = array_key_exists('hide_empty_rows', $options) ? $options['hide_empty_rows'] : true;
            $options['latest_version']  = array_key_exists('latest_version', $options) ? $options['latest_version'] : LATEST_FIREFOX_VERSION;

            $_builds = $this->_getFilteredBuilds($this->primary_builds, $keywords);
            return $this->tweakString($this->_getDownloadTableFromBuildArray($_builds, $options), $options);
        }

        /**
         * Convenience function to return a <table> of Firefox beta builds
         * filtered by the specified keywords on the language names
         *
         * @param array options (more detail in getDownloadBlockForLocale())
         * @param string keywords; will be dropped in a regex expression
         * @return string HTML block
         */
        function getFilteredDownloadTableForBetaBuilds($options = array(), $keywords = '') {

            $options['product']         = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['hide_empty_rows'] = array_key_exists('hide_empty_rows', $options) ? $options['hide_empty_rows'] : true;
            $options['latest_version']  = array_key_exists('latest_version', $options) ? $options['latest_version'] : LATEST_FIREFOX_VERSION;

            $_builds = $this->_getFilteredBuilds($this->beta_builds, $keywords);
            return $this->tweakString($this->_getDownloadTableFromBuildArray($_builds, $options), $options);
        }


        /**
         * Convenience function to return a <table> of Firefox primary ESR builds
         *
         * @param array options (more detail in getDownloadBlockForLocale())
         * @return string HTML block
         */
        function getDownloadTableForESRPrimaryBuilds($options=array()) {

            $options['product']        = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['latest_version'] = FIREFOX_ESR;
            $options['product_version'] = 'esr';
            $options['hide_empty_rows'] = true;


            $builds = array();

            foreach($this->primary_builds as $key => $val) {
                if( array_key_exists(FIREFOX_ESR, $val) ) {
                    $builds[$key] = $this->primary_builds[$key];
                }
            }


            return $this->tweakString($this->_getDownloadTableFromBuildArray($builds, $options), $options);
        }

        /**
         * Convenience function to return a <table> of Firefox ESR primary builds
         * filtered by the specified keywords on the language names
         *
         * @param array options (more detail in getDownloadBlockForLocale())
         * @param string keywords; will be dropped in a regex expression
         * @return string HTML block
         */
        function getFilteredDownloadTableForESRPrimaryBuilds($options = array(), $keywords = '') {

            $options['product']         = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['hide_empty_rows'] = array_key_exists('hide_empty_rows', $options) ? $options['hide_empty_rows'] : true;
            $options['latest_version']  = array_key_exists('latest_version', $options) ? $options['latest_version'] : FIREFOX_ESR;
            $options['product_version'] = 'esr';

            $_builds = $this->_getFilteredBuilds($this->primary_builds, $keywords);
            return $this->tweakString($this->_getDownloadTableFromBuildArray($_builds, $options), $options);
        }

        /**
         * Convenience function to return a <table> of Firefox ESR beta builds
         * filtered by the specified keywords on the language names
         *
         * @param array options (more detail in getDownloadBlockForLocale())
         * @param string keywords; will be dropped in a regex expression
         * @return string HTML block
         */
        function getFilteredDownloadTableForESRBetaBuilds($options = array(), $keywords = '') {

            $options['product']         = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['hide_empty_rows'] = array_key_exists('hide_empty_rows', $options) ? $options['hide_empty_rows'] : true;
            $options['latest_version']  = array_key_exists('latest_version', $options) ? $options['latest_version'] : FIREFOX_ESR;
            $options['product_version'] = 'esr';

            $_builds = $this->_getFilteredBuilds($this->beta_builds, $keywords);
            return $this->tweakString($this->_getDownloadTableFromBuildArray($_builds, $options), $options);
        }

        /**
         * Convenience function to return a <table> of Firefox Aurora builds
         * filtered by the specified keywords on the language names
         *
         * @param array options (more detail in getDownloadBlockForLocale())
         * @param string keywords; will be dropped in a regex expression
         * @return string HTML block
         */
        function getFilteredDownloadTableForDevelBuilds($options = array(), $keywords = '') {

            $options['product']         = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['hide_empty_rows'] = array_key_exists('hide_empty_rows', $options) ? $options['hide_empty_rows'] : true;
            $options['latest_version']  = LATEST_FIREFOX_DEVEL_VERSION;
            $options['product_version'] = 'devel';
            $options['tweaks'] = array('/<td class="curVersion" >(\d+\.\d+)b\d+<\/td>/' => '<td class="curVersion">$1</td>');

            $all_builds = $this->primary_builds + $this->beta_builds;

            $_builds = $this->_getFilteredBuilds($all_builds, $keywords);
            return $this->tweakString($this->_getDownloadTableFromBuildArray($_builds, $options), $options);
        }


        /**
         * Convenience function to return a <table> of Firefox Aurora builds
         * filtered by the specified keywords on the language names
         *
         * @param array options (more detail in getDownloadBlockForLocale())
         * @param string keywords; will be dropped in a regex expression
         * @return string HTML block
         */
        function getFilteredDownloadTableForAuroraBuilds($options = array(), $keywords = '') {

            $options['product']         = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['hide_empty_rows'] = array_key_exists('hide_empty_rows', $options) ? $options['hide_empty_rows'] : true;
            $options['latest_version']  = FIREFOX_AURORA;
            $options['product_version'] = 'aurora';
            $all_builds = $this->primary_builds + $this->beta_builds;

            $_builds = $this->_getFilteredBuilds($all_builds, $keywords);
            return $this->tweakString($this->_getDownloadTableFromBuildArray($_builds, $options), $options);
        }

        /**
         * Filters a build list by the specified keywords on the language names
         * (English and native)
         *
         * @param array builds array to filter.
         * @param string keywords; will be dropped in a regex expression
         * @return array the filtered builds array.
         */
        function _getFilteredBuilds($builds, $keywords) {

            $_builds = array();

            // filter the builds
            foreach ($builds as $_locale => $_build) {

                $_native   = $this->localeDetails->languages[$_locale]['native'];
                $_english  = $this->localeDetails->languages[$_locale]['English'];
                $_keywords = preg_quote($keywords, '/');
                $_pattern  = '/(?:^' . $_keywords . '|[ (]' . $_keywords . ')/i';

                // words are matched at the start of string or at the beginning of a word
                if (preg_match($_pattern, $_native) || preg_match($_pattern, $_english)) {
                    $_builds[$_locale] = $_build;
                }
            }

            return $_builds;
        }

        /**
         * Builds an unordered list of download links separated into sublists by geographic subregion
         *
         * This is used on the All-2.html page for selecting a localized build.
         *
         * @param string continent name or id.
         * @param array options (more detail in getDownloadBlockForLocale())
         * @return string HTML block
         */
        function getDownloadListForContinent($continent, $options = array()) {
            $_product         = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $_latest_version  = array_key_exists('latest_version', $options) ? $options['latest_version'] : LATEST_FIREFOX_VERSION;
            $_product_version = array_key_exists('product_version', $options) ?  $options['product_version'] : 'newest';
            $_product_version = in_array($_product_version, array('newest', 'devel', 'oldest')) ? $_product_version : 'newest';

            $_text_new_localized_builds_note = ___('beta translation; may still contain translation errors');

            // get continent sub-regions
            $_regions = array();

            foreach ($this->localeDetails->continents as $_continent => $_regions) {
                $_continent_id = strtolower($_continent);
                $_continent_id = preg_replace('/[^a-z ]/', '', $_continent_id);
                $_continent_id = str_replace(' ', '-', $_continent_id);
                $_continent_id = preg_replace('/-+/', '-', $_continent_id);

                // match continent by id or by full title
                if ($continent == $_continent || $continent == $_continent_id) {
                    break;
                }
            }

            // no sub-regions, return early
            if (empty($_regions)) {
                return '';
            }

            // add beta flag to beta builds
            $_beta_builds = array();
            foreach ($this->beta_builds as $_locale => $_build) {
                foreach ($_build as $_version => $_platforms) {
                    foreach ($_platforms as $_platform => $_details) {
                        $_details['beta'] = true;
                        $_platforms[$_platform] = $_details;
                    }
                    $_build[$_version] = $_platforms;
                }
                $_beta_builds[$_locale] = $_build;
            }

            // full builds array, will be filtered by sub-region later
            $_builds = array_merge($this->primary_builds, $_beta_builds);

            $_platform_titles = array(
                'Windows' => 'Windows',
                'OS X'    => 'Mac OS X',
                'Linux'   => 'Linux'
            );

            $_platform_classes = array(
                'Windows' => 'windows',
                'OS X'    => 'osx-uni',
                'Linux'   => 'linux'
            );

            $_platform_ids = array(
                'Windows' => 'win',
                'OS X'    => 'osx',
                'Linux'   => 'linux'
            );

            ob_start();

            foreach ($_regions as $_region) {

                // get build information for current region
                $_region_builds = array();

                foreach ($this->localeDetails->locations[$_region]['primary'] as $_locale) {
                    if (array_key_exists($_locale, $_builds)) {
                        $_region_builds[$_locale] = $_builds[$_locale];//[$_latest_version];
                    }
                }
                foreach ($this->localeDetails->locations[$_region]['secondary'] as $_locale) {
                    if (array_key_exists($_locale, $_builds)) {
                        $_region_builds[$_locale] = $_builds[$_locale];//[$_latest_version];
                    }
                }

                if (empty($_region_builds)) {
                    continue;
                }

                // sort build information
                $_region_builds = $this->_sortBuildArrayByEnglishName($_region_builds);

                $_region_id    = strtolower($_region);
                $_region_id    = preg_replace('/[^a-z ]/', '', $_region_id);
                $_region_id    = str_replace(' ', '-', $_region_id);
                $_region_id    = preg_replace('/-+/', '-', $_region_id);
                $_region_title = htmlspecialchars($_region);
                $_region_image = '/img/tignish/firefox/all-1/region-'.$_region_id.'.png';

                // display region header and image
                echo "<div class=\"region\">\n";
                echo "<img src=\"", $_region_image, "\" width=\"225\" height=\"225\" alt=\"", $_region_title, "\" class=\"region-map\" />\n";
                echo "<h2>Firefox ", $_latest_version, "</h2>\n";
                echo "<ul class=\"locales\">\n";

                // display locales for region
                $_count = 0;
                foreach ($_region_builds as $_locale => $_versions) {

                    switch ($_product_version) {
                        case 'oldest':
                            $_build_version = $this->getOldestVersionForLocaleFromBuildArray($_locale, $_region_builds);
                            break;
                        case 'devel':
                            $_build_version = $this->getDevelVersionForLocaleFromBuildArray($_locale, $_region_builds);
                            break;
                        case 'newest':
                        default:
                            $_build_version = $this->getNewestVersionForLocaleFromBuildArray($_locale, $_region_builds);
                            break;
                    }

                    if ($_build_version === '') {
                        continue;
                    }

                    $_build_info = $_versions[$_build_version];

                    echo "<li class=\"locale\">\n";
                    echo "<h3 lang=\"", $_locale, "\">", $this->localeDetails->languages[$_locale]['native'], "</h3>\n";
                    echo "<span class=\"english\">", $this->localeDetails->languages[$_locale]['English'], "</span>\n";

                    $_beta = false;
                    foreach ($_build_info as $_info) {
                        if (array_key_exists('beta', $_info) && $_info['beta']) {
                            $_beta = true;
                            break;
                        }
                    }

                    if ($_beta) {
                        echo "<span class=\"note\">", $_text_new_localized_builds_note, "</span>\n";
                    }

                    // display platform builds
                    echo "<ul>\n";
                    foreach ($_build_info as $_platform => $_info) {

                        // special case for OS X and Japanese
                        if ($_platform_ids[$_platform] == 'osx' && $_locale == 'ja') {
                            $_the_locale = 'ja-JP-mac';
                        } else {
                            $_the_locale = $_locale;
                        }

                        $_download_link = $this->download_base_url_direct.'?product='.$_product.'-'.$_build_version.'&amp;os='.$_platform_ids[$_platform].'&amp;lang='.$_the_locale;

                        echo "<li class=\"", $_platform_classes[$_platform], "\">";
                        echo "<a href=\"", $_download_link, "\">", $_platform_titles[$_platform], "</a>";
                        echo "</li>";
                    }
                    echo "</ul>\n";

                    echo "</li>\n";

                    $_count++; // counter for clearing list items

                    if ($_count % 3 === 0) {
                        echo "<li class=\"clear\"> </li>\n";
                        $_class = 'locale';
                    }
                }

                echo "</ul>\n";
                echo "</div>\n";
            }

            return ob_get_clean();
        }

        function getDownloadTablesByContinent($options = array()) {
            $options['product']         = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['hide_empty_rows'] = array_key_exists('hide_empty_rows', $options) ? $options['hide_empty_rows'] : true;
            $options['latest_version']  = array_key_exists('latest_version', $options) ? $options['latest_version'] : LATEST_FIREFOX_VERSION;

            $_text_fully_localized_versions  = ___('Fully Localized Versions');
            $_text_new_localized_builds      = ___('New Localized Builds');
            $_text_new_localized_builds_note = ___('These localized builds are in beta and may contain translation errors.');

            $_text_summaries = array(
                'africa' => array(
                    'stable' => ___('African Firefox builds available for download.'),
                    'beta'   => ___('African beta localizations of Firefox available for download.')
                    ),
                'asia' => array(
                    'stable' => ___('Asian Firefox builds available for download.'),
                    'beta'   => ___('Asian beta localizations Firefox available for download.')
                    ),
                'australia-oceania' => array(
                    'stable' => ___('Australian and Oceanian Firefox builds available for download.'),
                    'beta'   => ___('Australian and Oceanian beta localizations of Firefox available for download.')
                    ),
                'europe' => array(
                    'stable' => ___('European Firefox builds available for download.'),
                    'beta'   => ___('European beta localizations of Firefox available for download.')
                    ),
                'north-america' => array(
                    'stable' => ___('North American Firefox builds available for download.'),
                    'beta'   => ___('North American beta localizations of Firefox available for download.')
                    ),
                'south-america' => array(
                    'stable' => ___('South American Firefox builds available for download.'),
                    'beta'   => ___('South American beta localizations of Firefox available for download.')
                    )
                );

            // build list of locales-by-continent
            $_continents = array();
            foreach ($this->localeDetails->continents as $_continent => $_locations) {
                $_continents[$_continent] = array();
                foreach ($_locations as $_location) {
                    if (array_key_exists($_location, $this->localeDetails->locations)) {
                        foreach ($this->localeDetails->locations[$_location]['primary'] as $_locale) {
                            $_continents[$_continent][] = $_locale;
                        }
                        foreach ($this->localeDetails->locations[$_location]['secondary'] as $_locale) {
                            $_continents[$_continent][] = $_locale;
                        }
                    }
                }
                $_continents[$_continent] = array_unique($_continents[$_continent]);
            }

            ob_start();

            $_count = 0;
            foreach ($_continents as $_continent => $_locales) {
                $_count++;

                $_continent_title = htmlspecialchars($_continent);

                $_continent_id = strtolower($_continent);
                $_continent_id = preg_replace('/[^a-z ]/', '', $_continent_id);
                $_continent_id = str_replace(' ', '-', $_continent_id);
                $_continent_id = preg_replace('/-+/', '-', $_continent_id);

                if ($_count % 2 == 0) {
                    $_expander_class = 'expander';
                } else {
                    $_expander_class = 'expander expander-odd';
                }

                echo "<div class=\"{$_expander_class}\" id=\"{$_continent_id}\">\n";
                echo "<h3 class=\"expander-header\">{$_continent_title}</h3>\n";
                echo "<div class=\"expander-content\">\n";

                // display locales for the current continent
                echo "<div class=\"full-languages\">\n";
                echo "<h4>{$_text_fully_localized_versions}</h4>";

                $_builds = array();

                // filter the builds by locale
                foreach ($_locales as $_locale) {
                    if (array_key_exists($_locale, $this->primary_builds)) {
                        $_builds[$_locale] = $this->primary_builds[$_locale];
                    }
                }

                $options['download_table_summary'] = $_text_summaries[$_continent_id]['stable'];

                echo $this->tweakString($this->_getDownloadTableFromBuildArray($_builds, $options), $options);
                echo "</div>";
                // done displaying locales

                // display beta locales for the current continent (if they exist)
                $options['beta'] = true;
                $options['download_table_summary'] = $_text_summaries[$_continent_id]['beta'];

                $_builds = array();

                // filter the builds by locale
                foreach ($_locales as $_locale) {
                    if (array_key_exists($_locale, $this->beta_builds)) {
                        $_builds[$_locale] = $this->beta_builds[$_locale];
                    }
                }

                $_beta_table = $this->tweakString($this->_getDownloadTableFromBuildArray($_builds, $options), $options);
                if (!empty($_beta_table)) {
                    echo "<div class=\"beta-languages\">\n";
                    echo "<h4>{$_text_new_localized_builds}</h4>\n";
                    echo "<p>{$_text_new_localized_builds_note}</p>\n";
                    echo $_beta_table;
                    echo "</div>\n";
                }
                // done displaying beta locales

                $_primary_builds = $this->_getJavaScriptBuildArray($this->primary_builds, array('continent' => $_continent));
                $_beta_builds    = $this->_getJavaScriptBuildArray($this->beta_builds,    array('continent' => $_continent));
                $_data           = '[' . $_primary_builds . ', ' . $_beta_builds . ']';
                $_id             = "'" . escapeForJavaScript($_continent_id) . "'";
                $_version        = "'" . LATEST_FIREFOX_VERSION . "'";
                echo "<script type=\"text/javascript\">// <![CDATA[\n";
                echo "var platform_switcher_{$_count} = new Mozilla.PlatformSwitcher({$_id}, {$_data}, {$_version});\n";
                echo "// ]]></script>\n";

                echo "</div>";
                echo "</div>";
            }

            return $this->tweakString(ob_get_clean(), $options);
        }

        /**
         * Gets a JavaScript structure for localized Firefox builds. NOTE: ***This requires the escapeForJavaScript() function
         * but it isn't defined in this library!  Since this is a function for a specific part of mozilla.com I'm leaving
         * the function out of the library but if this becomes used elsewhere we should copy it in.  If you need the function
         * you can find it at moz.com/includes/functions.inc.php.***
         *
         * This stucture is used for JavaScript searching on the 'All Languages'
         * page and for JavaScript platform switching on the same page.
         *
         * Structured as:
         * [
         *  [
         *   $locale_id,
         *   $english,
         *   $native,
         *   { 'Windows': { 'filesize': '9.0' }, 'OS X': { ... etc ... } }
         *  ],
         *  ... etc ...
         * ]
         *
         * @param array options
         * @return string JavaScript structure.
         */
        function _getJavaScriptBuildArray($build_array, $options = array()) {

            $options['latest_version'] = array_key_exists('latest_version', $options) ? $options['latest_version'] : LATEST_FIREFOX_VERSION;

            $_languages = array();

            if (array_key_exists('continent', $options)) {
                // if continent is specified, get build info for the specified continent only
                $_build_array = array();
                if (array_key_exists($options['continent'], $this->localeDetails->continents)) {
                    foreach ($this->localeDetails->continents[$options['continent']] as $_location) {
                        if (array_key_exists($_location, $this->localeDetails->locations)) {
                            foreach ($this->localeDetails->locations[$_location]['primary'] as $_locale) {
                                if (array_key_exists($_locale, $build_array)) {
                                    $_build_array[$_locale] = $build_array[$_locale];
                                }
                            }
                            foreach ($this->localeDetails->locations[$_location]['secondary'] as $_locale) {
                                if (array_key_exists($_locale, $build_array)) {
                                    $_build_array[$_locale] = $build_array[$_locale];
                                }
                            }
                        }
                    }
                }
            } else {
                // no continent specified, use all build info
                $_build_array = $build_array;
            }

            $_build_array = $this->_sortBuildArrayByEnglishName($_build_array);

            foreach ($_build_array as $_locale => $_build_info) {
                $_build_info     = $_build_info[$options['latest_version']];
                $_locale_details = $this->localeDetails->languages[$_locale];

                // skip languages that don't have a FF3 release
                if (empty($_build_info)) {
                    continue;
                }

                $_language = array();

                // [0] locale-id field
                $_language[] = "'" . escapeForJavaScript($_locale) . "'";

                // [1] English language version field
                $_language[] = "'" . escapeForJavaScript($_locale_details['English']) . "'";

                // [2] localized version field
                $_language[] = "'" . escapeForJavaScript($_locale_details['native']) . "'";

                $_builds = '{';

                foreach ($_build_info as $_system => $_build) {
                    $_builds .= "'" . escapeForJavaScript($_system) . "': ";
                    $_builds .= '{';
                    foreach ($_build as $_name => $_value) {
                        $_builds .= "'" . escapeForJavaScript($_name) . "': ";
                        $_builds .= "'" . escapeForJavaScript($_value) . "', ";
                    }
                    $_builds  = substr($_builds, 0, -2);
                    $_builds .= '}, ';
                }

                $_builds  = substr($_builds, 0, -2);
                $_builds .= '}';

                // [3] builds structure field
                $_language[] = $_builds;

                $_language = '[' . implode(', ', $_language) . ']';
                $_languages[] = $_language;
            }

            $_javascript = "[" . implode(",\n ", $_languages) . "]\n";
            return $_javascript;
        }

        /**
         * Convenience function to gets JavaScript array structure for all builds (primary and beta)
         *
         * @param array options
         * @return string JavaScript structure.
         */
        function getJavaScriptArrayForAllBuilds($options = array()) {

            // add beta flag to beta builds
            $_beta_builds = array();
            foreach ($this->beta_builds as $_locale => $_build) {
                foreach ($_build as $_version => $_platforms) {
                    foreach ($_platforms as $_platform => $_details) {
                        $_details['beta'] = true;
                        $_platforms[$_platform] = $_details;
                    }
                    $_build[$_version] = $_platforms;
                }
                $_beta_builds[$_locale] = $_build;
            }

            $_builds = array_merge($this->primary_builds, $_beta_builds);

            $options['latest_version'] = array_key_exists('latest_version', $options) ? $options['latest_version'] : LATEST_FIREFOX_VERSION;

            return $this->_getJavaScriptBuildArray($_builds, $options);
        }

        /**
         * Convenience function to return a <table> of Beta development (not beta locales) builds
         *
         * @param array options (more detail in getDownloadBlockForLocale())
         * @return string HTML block
         */
        function getDownloadTableForDevelBuilds($options=array()) {

            $options['product']         = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['latest_version']  = LATEST_FIREFOX_DEVEL_VERSION;
            $options['product_version'] = 'devel';
            $options['tweaks'] = array('/<td class="curVersion" >(\d+\.\d+)b\d+<\/td>/' => '<td class="curVersion">$1</td>');

            $all_builds = $this->primary_builds + $this->beta_builds;
            return $this->tweakString($this->_getDownloadTableFromBuildArray($all_builds, $options), $options);
        }

        /**
         * Convenience function to return a <table> of Firefox beta builds for beta locales
         *
         * @param array options (more detail in getDownloadBlockForLocale())
         * @return string HTML block
         */
        function getDownloadTableForUpcomingLocales($options=array()) {

            $options['product']         = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['latest_version']  = LATEST_FIREFOX_DEVEL_VERSION;
            $options['product_version'] = 'devel';

            $builds = array();

            foreach($this->beta_builds as $key => $val) {
                if( array_key_exists(LATEST_FIREFOX_DEVEL_VERSION, $val) ) {
                    $builds[$key] = $this->beta_builds[$key];
                }
            }

            foreach($this->primary_builds as $key => $val) {
                if( array_key_exists(LATEST_FIREFOX_DEVEL_VERSION, $val) && !array_key_exists(LATEST_FIREFOX_VERSION, $val) ) {
                    $builds[$key] = $this->primary_builds[$key];
                }
            }

            return $this->_getDownloadTableFromBuildArray($builds, $options);
        }

        /**
         * Convenience function to return a <table> of Firefox Aurora builds
         *
         * @param array options (more detail in getDownloadBlockForLocale())
         * @return string HTML block
         */
        function getDownloadTableForAuroraBuilds($options=array()) {

            $options['product']         = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['latest_version']  = FIREFOX_AURORA;
            $options['product_version'] = 'aurora';
            $all_builds = $this->primary_builds + $this->beta_builds;
            return $this->tweakString($this->_getDownloadTableFromBuildArray($all_builds, $options), $options);
        }

        /**
         * Convenience function to return a <table> of Firefox beta builds
         *
         * @param array options (more detail in getDownloadBlockForLocale())
         * @return string HTML block
         */
        function getDownloadTableForBetaBuilds($options=array()) {

            $options['product']        = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['latest_version'] = LATEST_FIREFOX_VERSION;
            $options['beta']           = true;

            $_text_new_localized_builds_summary = ___('Firefox localized beta builds available for download.');

            if (!array_key_exists('download_table_summary', $options)) {
                $options['download_table_summary'] = $_text_new_localized_builds_summary;
            }

            return $this->tweakString($this->_getDownloadTableFromBuildArray($this->beta_builds, $options), $options);
        }

        /**
         * Convenience function to return a <table> of Firefox ESR beta locales builds
         *
         * @param array options (more detail in getDownloadBlockForLocale())
         * @return string HTML block
         */
        function getDownloadTableForESRBetaBuilds($options=array()) {

            $options['product']         = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['latest_version']  = FIREFOX_ESR;
            $options['product_version'] = 'esr';
            $options['beta']            = true;

            $_text_new_localized_builds_summary = ___('Firefox localized beta builds available for download.');

            if (!array_key_exists('download_table_summary', $options)) {
                $options['download_table_summary'] = $_text_new_localized_builds_summary;
            }

            return $this->tweakString($this->_getDownloadTableFromBuildArray($this->beta_builds, $options), $options);
        }

        /**
         * Return a <table> with the links to the older versions of all locales with
         * primary builds.  We keep links to a single previous version.
         *
         * @return string HTML block
         */
        function getDownloadTableForOlderPrimaryBuilds($options=array()) {

            $options['product']         = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['latest_version']  = LATEST_FIREFOX_VERSION;
            $options['product_version'] = 'oldest';

            return $this->tweakString($this->_getDownloadTableFromBuildArray($this->primary_builds, $options), $options);
        }

        /**
         * Return a <table> with the links to the older versions of all locales with
         * beta builds.  We keep links to a single previous version.
         *
         * @return string HTML block
         */
        function getDownloadTableForOlderBetaBuilds($options=array()) {

            $options['product']         = array_key_exists('product', $options) ? $options['product'] : 'firefox';
            $options['latest_version']  = LATEST_FIREFOX_VERSION;
            $options['product_version'] = 'oldest';

            return $this->tweakString($this->_getDownloadTableFromBuildArray($this->beta_builds, $options), $options);
        }

        /**
         * TEMPORARY CODE
         *          20070827_TEMP
         * Overload parent function.  See parent for details.
         *
         */
        function getNoScriptBlockForLocale($locale, $options=array()) {

            $options['product'] = array_key_exists('product', $options) ? $options['product'] : 'firefox';

            return parent::getNoScriptBlockForLocale($locale, $options);
        }

}
?>
