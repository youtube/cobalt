# Helper script to run an placeholder process on windows.
# This serves to keep windows containers running, so we can shell into it to
# administer it.

while ($true) {
  test-connection google.com -Count 1;
  sleep 120;
}
