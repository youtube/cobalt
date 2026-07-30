# content/browser/security

This directory contains security-relevant code for the browser process.
* `cpsp`: This contains ChildProcessSecurityPolicy code, which is the reference
  monitor that enforces Site Isolation and other permission grants for child
  processes.
* The remaining directories contain the browser-process implementations of
  security policies of the web platform (e.g., COOP, DIP, etc).

