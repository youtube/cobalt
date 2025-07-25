<?php
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST');
header('Access-Control-Allow-Headers: content-type');
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
  echo 'replied to options with Access-Control-Allow headers';
} else {
  echo 'post data: ' . file_get_contents('php://input');
}
?>
