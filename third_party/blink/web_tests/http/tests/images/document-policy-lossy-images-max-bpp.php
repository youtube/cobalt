<?php
header("Document-Policy: lossy-images-max-bpp=0.5");
?>
<!DOCTYPE html>
<style>
body {
  font: 10px Ahem;
}
</style>
<head>
  <base href="resources/">
</head>
<body>
  <div width="700" height="500">
    <img src="Fisher-large.jpg" width="200"/>
    <img src="Fisher-small.jpg" width="200"/>
    <img src="pass-all.png" width="200"/>
    <img src="fail-strict.png" width="200"/>
    <img src="fail-all.png" width="200"/>
  </div>
</body>
