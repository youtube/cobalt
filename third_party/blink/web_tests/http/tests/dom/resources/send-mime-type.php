<?php
header("HTTP/1.0 200 OK");
if (isset($_GET['m']) && $_GET['m'] != "")
	header("Content-Type: " . $_GET['m']);
echo "<h1>Hello</h1>"
?>