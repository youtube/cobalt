<?php
// TODO(iclelland): Generate this sample token during the build. The token
//   below will expire in 2033, but it would be better to always have a token which
//   is guaranteed to be valid when the tests are run.
// NOTE: The token must match the UNAUTHENTICATED_ORIGIN used in the test.
//   Generate this token with the command:
// generate_token.py http://example.test:8000 FrobulateDeprecation --expire-timestamp=2000000000
header("Origin-Trial: AtHDu5WEwzV/VLWdacwA3ntxuSyMdzBNC0cGuSV5hEPh1EqKt3PUsHW70/R5t3kUO1nlpd4edKHvwoMWeXYg3AoAAABfeyJvcmlnaW4iOiAiaHR0cDovL2V4YW1wbGUudGVzdDo4MDAwIiwgImZlYXR1cmUiOiAiRnJvYnVsYXRlRGVwcmVjYXRpb24iLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=");
?>
<!DOCTYPE html>
<meta charset="utf-8">
<title>Test Sample API when deprecation trial is enabled, in insecure context</title>
<script src="../resources/testharness.js"></script>
<script src="../resources/testharnessreport.js"></script>
<script src="../resources/get-host-info.js"></script>
<script src="resources/origintrials.js"></script>
<script>

  if (window.location.origin != get_host_info().UNAUTHENTICATED_ORIGIN) {
    window.location = get_host_info().UNAUTHENTICATED_ORIGIN +
      window.location.pathname;
  } else {
    // The trial is enabled by the token above in the header.
    expect_success_deprecation();
  }
</script>
