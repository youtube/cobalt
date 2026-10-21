<?php

    $fileName = "incrementer.wasm";
    if (isset($_GET['name'])) {
       $fileName = $_GET['name'];
    }
    $last_modified = "Tue, 18 Dec 2018 23:15:53 GMT";
    if (isset($_GET['last-modified'])) {
       $last_modified = $_GET['last-modified'];
    }

    header("Content-Type: application/wasm");
    header("Last-Modified: ".$last_modified);

    if (isset($_GET['cors'])) {
      header("Access-Control-Allow-Origin: *");
    }
    require($fileName);
?>
