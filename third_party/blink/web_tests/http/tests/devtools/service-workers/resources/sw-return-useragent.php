<html>
  <head>
    <title>repeat-fetch-service-worker.js with useragent</title>
    <script>
function installSW() {
  navigator.serviceWorker.register('repeat-fetch-service-worker.js');
}
    </script>
  </head>
  <body onload="installSW()">
    <p><?php echo $_SERVER['HTTP_USER_AGENT'] ?></p>
  </body>
</html>
