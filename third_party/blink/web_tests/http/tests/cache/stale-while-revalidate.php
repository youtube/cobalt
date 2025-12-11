<?php
# Origin Trial header generated via:
# generate_token.py http://127.0.0.1:8000 StaleWhileRevalidate --expire-timestamp=2000000000
header('Origin-Trial: ', 'Ar+YcsxZApSlJwPStNXfjSZnlQi2VhVaelBM+U9oP761uOB73mfsBwYDg1VUYGn1GDIXnjzEp6PR8PMi2dRPPgIAAABceyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiU3RhbGVXaGlsZVJldmFsaWRhdGUiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=');
header('Content-Type: text/html');
?>
<!DOCTYPE html>
<meta charset="utf-8">
<title>Tests Stale While Revalidate Works when origin trial is enabled</title>
<script src="../resources/testharness.js"></script>
<script src="../resources/testharnessreport.js"></script>
<body>
<script>
var last_modified;
var last_modified_count = 0;

// The script will call report via a uniquely generated ID on the subresource.
// If it is a cache hit the ID will be the same and the test will pass.
function report(mod) {
  console.log('report ' + mod);
  if (!last_modified) {
    last_modified = mod;
    last_modified_count = 1;
  } else if (last_modified == mod) {
    last_modified_count++;
  }
}

async_test(t => {
  window.onload = t.step_func(() => {
    step_timeout(() => {
      var script = document.createElement("script");
      script.src = "resources/stale-script.php";
      document.body.appendChild(script);
      script.onload = t.step_func_done(() =>{
        assert_true(last_modified_count == 2);
      });
    }, 0);
  });
}, 'Cache returns stale resource');

var script = document.createElement("script");
script.src = "resources/stale-script.php";
document.body.appendChild(script);
</script>
</body>
