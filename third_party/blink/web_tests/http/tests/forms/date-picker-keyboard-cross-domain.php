<?php
header("Origin-Agent-Cluster: ?0");
?>
<!DOCTYPE html>
<html>
<head>
  <script src="../../../resources/testharness.js"></script>
  <script src="../../../resources/testharnessreport.js"></script>
</head>
<body >
  <script>
    let t = async_test("Test input date popup's keyboard usability when inside cross-process iframe.");

    let iframe = document.createElement("iframe");
    iframe.src = "http://localhost:8000/forms/resources/date-picker-keyboard-cross-domain-iframe.php";

    const runTest = t.step_func((event) => {
      // The eventSender.keyDown() invocations in the iframe create extra window messages.
      // Ignore those.
      if (typeof(event.data) !== "string" || !event.data.includes("Date result:")) {
        return;
      }

      // iframe should be cross-origin (and cross-process)
      assert_equals(iframe.contentDocument, null);

      assert_equals(event.data, "Date result: 2019-12-13");
      t.done();
    }, "Check date of picker in cross-domain iframe.");

    window.onmessage = runTest;

    document.body.appendChild(iframe);
  </script>
</body>
</html>
