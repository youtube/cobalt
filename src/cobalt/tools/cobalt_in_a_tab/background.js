chrome.browserAction.onClicked.addListener(function(){
    //open a new tab
    createProperties = {
        url: chrome.extension.getURL('cobaltView.html')
    };
    chrome.tabs.create(createProperties, function(tab){
    });
});