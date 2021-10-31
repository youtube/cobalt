let body = document.getElementById('popup_body');

// Dynamically create list of device endpoints.
let current_ips = chrome.storage.sync.get('ip_endpoints', function(data) {
  for (let i = 0; i < data.ip_endpoints.length; i++){
    let item = data.ip_endpoints[i];
    let ip_item = document.createElement('input');
    item.device !== "" ? ip_item.value = item.device : ip_item.value = item.endpoint;
    ip_item.type = "button";
    ip_item.classList.add("buttonListItem");
    ip_item.onclick = function(){
      chrome.storage.sync.set({selected_ip: item.endpoint});
      //open a new tab
      createProperties = {
        url: chrome.extension.getURL('cobaltView.html')
      };
      chrome.tabs.create(createProperties, function(tab){
      });
    };

    body.appendChild(ip_item);
  }
});