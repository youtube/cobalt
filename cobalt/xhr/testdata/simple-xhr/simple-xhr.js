var xhr = new XMLHttpRequest();
xhr.onreadystatechange = function() {
  var cobalt = document.createElement('span');
  if(xhr.readyState == 1) {
    cobalt.textContent = "...Opened";
  }
  else if (xhr.readyState == 2) {
    cobalt.textContent = "...HeadersReceived";
  }
  else if (xhr.readyState == 3) {
    cobalt.textContent = "...Loading";
  }
  else if (xhr.readyState == 4) {
    cobalt.textContent = "...Loaded.";
    var headers = document.createElement('span');
    headers.textContent = "Headers: " + xhr.getAllResponseHeaders();
    document.body.appendChild(headers);
  }
  document.body.appendChild(cobalt);
}

xhr.open("GET", "https://www.youtube.com/tv");
xhr.send();
