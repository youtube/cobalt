<?php
    $rw = $_SERVER["HTTP_WIDTH"];
    $expected_rw = $_GET["rw"];

    if ((isset($expected_rw) && $rw == $expected_rw) || (isset($rw) && !isset($expected_rw))) {
        $fn = fopen("compass.jpg", "r");
        fpassthru($fn);
        fclose($fn);
        exit;
    }
    header("HTTP/1.1 417 Expectation failed");
?>
