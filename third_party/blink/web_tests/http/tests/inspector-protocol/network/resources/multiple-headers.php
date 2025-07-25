<?php
    // We need live headers, so make sure it's not cached.
    header('Cache-Control: no-cache, no-store, must-revalidate, max-age=0');
    header('Access-Control-Pragma: value1');
    header('Access-Control-Pragma: value2', false /* = $replace */);
?>
<html>
<body>
Hello, world!
</body>
</body>
