<?php
require_once '../../resources/portabilityLayer.php';

clearstatcache();
$tmpFilename = sys_get_temp_dir() . "/" . $_GET['filename'];
if (file_exists($tmpFilename)) {
    header('HTTP/1.1 404 Not Found');
    exit();
} else {
    $tmpFile = fopen($tmpFilename, 'w');
    fclose($tmpFile);
    
    $filename = 'compass-no-cache.jpg';
    $filemtime = filemtime($filename);
    $filesize = filesize($filename);

    $etag = '"' . $filesize . '-' . $filemtime . '"';
    $last_modified = gmdate(DATE_RFC1123, $filemtime);
    $max_age = 12 * 31 * 24 * 60 * 60; //one year
    $expires = gmdate(DATE_RFC1123, time() + $max_age);

    $handle = fopen($filename, 'rb');
    $contents = fread($handle, $filesize);
    fclose($handle);

    header('Cache-Control: public, max-age=' . $max_age);
    header('Expires: ' . $expires);
    header('Content-Type: image/png');
    header('Content-Length: ' . $filesize);
    header('Etag: ' . $etag);
    header('Last-Modified: ' . $last_modified);
    echo $contents;
    exit();
}
?>
