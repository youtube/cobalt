<?php
header_remove("X-Powered-By");

$redirect = (bool) $_GET["redirect"];
$cached = (bool) $_GET["cached"];
$chunked = (bool) $_GET["chunked"];
$size = (int) $_GET["size"];
$gzip = (bool) $_GET["gzip"];
$flush_header_with_x_bytes = (int) $_GET["flush_header_with_x_bytes"];
$wait_after_headers_packet = (int) $_GET["wait_after_headers_packet"];
$flush_every_x_bytes = ((int) $_GET["flush_every"])?:1;
$wait_every_x_bytes = ((int) $_GET["wait_every_x_bytes"])?:0xFFFFF;
$wait_duration_every_x_bytes = ((int) $_GET["wait_duration_every_x_bytes"])?:50;

$sent_data_size = 0;

if ($redirect) {
    unset($_GET["redirect"]);
    header("Location: ?" . http_build_query($_GET));
    exit;
}

// This is done because it should force netstack to handle data as it comes.
header("Content-Type: application/json");

if ($cached) {
    header("HTTP/1.0 304 Not Modified");
    exit;
}

if ($gzip)
    ob_start("ob_gzhandler");
else
    ob_start();

if (!$chunked)
    header("Content-Length: " . $size);

if ($flush_header_with_x_bytes) {
    send_data($flush_header_with_x_bytes);
    full_flush();
}

if ($wait_after_headers_packet)
    usleep($wait_after_headers_packet * 1000);

while ($sent_data_size < $size) {
    $flush_size = $flush_every_x_bytes - ($sent_data_size % $flush_every_x_bytes);
    $wait_size = $wait_every_x_bytes - ($sent_data_size % $wait_every_x_bytes);

    $send_size = min($flish_size, $wait_size);
    if (!$send_size)
        $send_size = max($flish_size, $wait_size);

    send_data($send_size);
    if ($sent_data_size % $flush_every_x_bytes === 0)
        full_flush();
    if ($sent_data_size % $wait_every_x_bytes === 0)
        usleep($wait_duration_every_x_bytes * 1000);
}

function send_data($size)
{
    global $sent_data_size;
    echo str_repeat("a", $size);
    $sent_data_size += $size;
}

function full_flush()
{
    ob_flush();
    flush();
}