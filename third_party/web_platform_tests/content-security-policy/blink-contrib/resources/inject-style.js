// Cobalt: document.write() not supported.
if (document.write) {
  document.write("<style>#test1 { display: none; }</style>");
} else {
  var s = document.createElement('style');
  s.textContent = "#test1 { display: none; }";
  document.body.appendChild(s);
}

var s = document.createElement('style');
s.textContent = "#test2 { display: none; }";
document.body.appendChild(s);
