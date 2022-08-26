const xhr = new XMLHttpRequest();
// Script should fail and stop execution at this step due to SecurityViolation.
xhr.open("GET", "http://www.google.com");
xhr.send();

self.postMessage("Should not be reached!");
