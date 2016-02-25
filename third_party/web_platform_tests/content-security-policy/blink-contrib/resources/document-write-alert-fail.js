// Cobalt: document.write() not supported.
if (document.write) {
  document.write("<script>test(function () { assert_unreached('FAIL inline script from document.write ran') });</script>");
} else {
  var s = document.createElement('script');
  s.textContent = "test(function () { assert_unreached('FAIL inline script from document.write ran') });";
  document.body.appendChild(s);
}
