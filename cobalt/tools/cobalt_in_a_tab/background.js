// Local machine is the first "Default" IP.
chrome.runtime.onInstalled.addListener(function() {
    chrome.storage.sync.set({
        ip_endpoints: [{
            device: 'Local',
            endpoint: 'http://localhost'
        }],
    });
});
