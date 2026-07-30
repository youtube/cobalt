<?php
header("Cross-Origin-Opener-Policy: same-origin");
header("Cross-Origin-Embedder-Policy: require-corp");
header("Permissions-Policy: cross-origin-isolated=(self), direct-sockets=(self), local-network=(self), loopback-network=(self), direct-sockets-multicast=(self)");
header("Origin-Agent-Cluster: ?0");
?>

<!DOCTYPE html>
<html lang="en">
  <head>
    <title>Direct socket test page with isolated context</title>
    <meta charset="utf-8">
  </head>
  <body>
  </body>
</html>
