<?php
$max_age = $_GET['max-age'];
if (!(empty($max_age))) {
  header('Cache-Control: max-age=' . $max_age);
}

header("Access-Control-Allow-Origin: http://127.0.0.1:8000");

$name = 'abe.png';
$fp = fopen($name, 'rb');
header("Content-Type: image/png");
header("Content-Length: " . filesize($name));

fpassthru($fp);
exit;
?>
