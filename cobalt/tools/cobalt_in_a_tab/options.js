let ip_list = document.getElementById('ip_list');
let ip_data = null;

// Make list of ip endpoints dynamically.
let current_ips = chrome.storage.sync.get('ip_endpoints', function(data) {
  ip_data = data.ip_endpoints;
  for (let i = 0; i < data.ip_endpoints.length; i++){
    let item = data.ip_endpoints[i];
    let ip_item = document.createElement('div');
    ip_item.classList.add("row");
    ip_item.id = item.endpoint;

    let ip_title = document.createElement('p');
    ip_title.innerText += item.device;
    ip_title.classList.add("listItem");

    let ip_endpoint = document.createElement('p');
    ip_endpoint.innerText += item.endpoint;
    ip_endpoint.classList.add("listItem");

    let delete_button = document.createElement('input');
    delete_button.type = "button";
    delete_button.value = "-";
    delete_button.classList.add("listItem");
    delete_button.onclick = function(){
      ip_data.splice(i, 1);
      chrome.storage.sync.set({ip_endpoints: ip_data});
      window.location.reload();
    };

    ip_list.appendChild(ip_item);

    ip_item.appendChild(ip_title);
    ip_item.appendChild(ip_endpoint);
    ip_item.appendChild(delete_button);
  }
});

document.getElementById("addIP").onclick = function () {
  let add_device = document.getElementById("inputDevice").value;
  let add_endpoint = "http://" + document.getElementById("inputIP").value;
  let item = {
    device: add_device,
    endpoint: add_endpoint
  };
  ip_data.push(item);
  chrome.storage.sync.set({ip_endpoints: ip_data});
  window.location.reload();
};