<?php
require_once 'portabilityLayer.php';
header("Access-Control-Allow-Origin: *");

if (!sys_get_temp_dir()) {
    echo "FAIL: No temp dir was returned.\n";
    exit();
}

$tmpFile = sys_get_temp_dir() . "/" . $_GET['filename'];

$stat = stat($tmpFile);
if (!$stat) {
    echo "FAIL: stat() call failed.\n";
    exit();
}

$mtime = $stat['mtime'] + 2;

if (!touch($tmpFile, $mtime)) {
    echo "FAIL: touch() call failed.\n";
}
?>
