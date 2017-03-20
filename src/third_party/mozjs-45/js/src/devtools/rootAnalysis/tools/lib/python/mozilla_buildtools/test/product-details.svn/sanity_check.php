#!/usr/bin/env php
<?php
// Including them is already a sanity check

include("FIREFOX_AURORA.php");
include("FIREFOX_ESR.php");
include("LATEST_FIREFOX_DEVEL_VERSION.php");
include("LATEST_FIREFOX_RELEASED_DEVEL_VERSION.php");
include("LATEST_FIREFOX_RELEASED_VERSION.php");
include("LATEST_FIREFOX_VERSION.php");
include("LATEST_THUNDERBIRD_VERSION.php");
include("mobile_alpha_version.php");
include("mobile_beta_version.php");
include("mobile_latest_version.php");

include("history/firefoxHistory.class.php");
include("history/mobileHistory.class.php");
include("history/thunderbirdHistory.class.php");

function checkHistory($releases) {
        foreach ($releases as $version => $date_release) {
                $date_parsed = date_parse($date_release);
                if ($date_parsed['error_count'] != 0){
                        echo "Wrong date $date_release for version $version\n";
			print_r($date_parsed);
			exit(1);
                }
		if (strlen($version) == 0) {
			echo "Empty version\n";
			exit(2);
		}
		if (strpos($version, ".") === false) {
			echo "No '.' in $version. That should not happen\n";
			exit(3);
		}
        }
}

$fx = new firefoxHistory();
checkHistory($fx->major_releases);
checkHistory($fx->stability_releases);
checkHistory($fx->development_releases);

$fennec = new mobileHistory();
checkHistory($fennec->major_releases);
checkHistory($fennec->stability_releases);
checkHistory($fennec->development_releases);

$tb = new thunderbirdHistory();
checkHistory($tb->major_releases);
checkHistory($tb->stability_releases);
checkHistory($tb->development_releases);


?>
