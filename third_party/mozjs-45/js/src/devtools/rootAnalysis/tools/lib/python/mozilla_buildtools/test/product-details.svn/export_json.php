#!/usr/bin/env php
<?php
   /**
    * command line program.
    * From the various datastructure, generates some json files which are going
    * to be used by other Mozilla applications
    *
    * @author Release management <release-mgmt@mozilla.com>
    */
define('JSONDIR', dirname(__FILE__).'/json/');


/**
 * Write a JSON file
 *
 * @param string $filename the name of the json file
 * @param Array  $data     the datastructure which is going to be exported in
 * the file
 *
 * @return void
 */
function writefile($filename, $data)
{
    file_put_contents(JSONDIR.$filename, json_encode($data, JSON_PRETTY_PRINT));
}

/** Locale Details */
require_once 'localeDetails.class.php';
$ld = new localeDetails();
writefile('languages.json', $ld->languages);

/** Product Details */
// Firefox Versions
require_once 'firefoxDetails.class.php';
$fx_constants = array('LATEST_FIREFOX_VERSION',
                      'LATEST_FIREFOX_DEVEL_VERSION',
                      'LATEST_FIREFOX_RELEASED_DEVEL_VERSION',
                      'LATEST_FIREFOX_OLDER_VERSION',
                      'FIREFOX_AURORA',
                      'FIREFOX_ESR',
                      'FIREFOX_ESR_NEXT'
                      );
$versiondata = array();
foreach ($fx_constants as $fx_c) {
    $versiondata[$fx_c] = constant($fx_c);
}
writefile('firefox_versions.json', $versiondata);

/**
 * Generate the list of filesize. Values are useless but some code might
 * expect it
 * { 'Windows': { 'filesize': '9.0' }, 'OS X': { ... etc ... } }
 *
 * @param Array $builds the declaration array
 *
 * @return Array the datastructure with the filesize inside
*/
function fillFileSize($builds)
{

    /* Create the filesize data structure which is going to be
     * duplicated
    */
    $filesize = array(
        'Windows' => array('filesize' => 0),
        'OS X'    => array('filesize' => 0),
        'Linux'   => array('filesize' => 0),
    );

    /*
    *  For each locales and version, insert the filesize
    */
    $newBuild=array();
    foreach ($builds as $locale => $build) {
        foreach ($build as $version) {
            $newBuild[$locale][$version] = $filesize;
        }
    }

    return $newBuild;
}

// Firefox Primary Builds
$fxd = new firefoxDetails();
$builds = array('primary_builds', 'beta_builds');
foreach ($builds as $build) {
    $buildArray = fillFileSize($fxd->$build);
    writefile("firefox_{$build}.json", $buildArray);
}

// Firefox History
require_once 'history/firefoxHistory.class.php';
$fxh = new firefoxHistory();
$releases = array('major_releases', 'stability_releases', 'development_releases');
foreach ($releases as $release) {
    writefile("firefox_history_{$release}.json", $fxh->$release);
}

// Thunderbird Versions
require_once 'thunderbirdDetails.class.php';
$tb_constants = array('LATEST_THUNDERBIRD_VERSION',
                      );
$versiondata = array();
foreach ($tb_constants as $tb_c) {
    $versiondata[$tb_c] = constant($tb_c);
}
writefile('thunderbird_versions.json', $versiondata);

// Firefox Primary Builds
$tbd = new thunderbirdDetails();
$builds = array('primary_builds', 'beta_builds');
foreach ($builds as $build) {
    writefile("thunderbird_{$build}.json", $tbd->$build);
}

// Thunderbird History
require_once 'history/thunderbirdHistory.class.php';
$tbh = new thunderbirdHistory();
$releases = array('major_releases', 'stability_releases', 'development_releases');
foreach ($releases as $release) {
    writefile("thunderbird_history_{$release}.json", $tbh->$release);
}

// Mobile Details
require_once 'mobileDetails.class.php';
$mobile = array(
    'version' => mobileDetails::latest_version,
    'beta_version' => mobileDetails::beta_version,
    'alpha_version' => mobileDetails::alpha_version,
    'builds'  => mobileDetails::primary_builds(false),
    'beta_builds' => mobileDetails::beta_builds(false),
    'alpha_builds' => mobileDetails::alpha_builds(false),
);
writefile('mobile_details.json', $mobile);

// Mobile History
require_once 'history/mobileHistory.class.php';
$mobh = new mobileHistory();
$releases = array('major_releases', 'stability_releases', 'development_releases');
foreach ($releases as $release) {
    writefile("mobile_history_{$release}.json", $mobh->$release);
}

// Region details
require_once 'regionDetails.class.php';

if (!file_exists(JSONDIR."regions")) {
    mkdir(JSONDIR."regions");
}

$rd = new regionDetails();
foreach ($ld->languages as $lang => $names) {
    $names = $rd->getRegionNames($lang);
    if (!empty($names)) {
        writefile("regions/$lang.json", $names);
    }
}
