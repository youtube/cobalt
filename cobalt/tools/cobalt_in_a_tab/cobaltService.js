let CobaltService = class CobaltService{
    constructor(){}

    async initialize(){
        this.cobaltIP = await this.getCobaltIP();
        this.webdriverAddress = this.cobaltIP + ":4444";

        this.screencastAddress = "";
        this.imageId = 0;
    }

    async getCobaltIP(){
        return new Promise((resolve) => {
            chrome.storage.sync.get('selected_ip', function(data) {
                resolve(data.selected_ip);
            });
        });
    }

    setScreencastPort(port){
        this.screencastAddress = this.cobaltIP + ":" + port;
    }

    // Boilerplate code. It makes a request to |address| with |data|
    // and returns json.value (or something else if you define resolveFunction)
    fetchRequest(address, data, resolveFunction = (json) => { return json.value; }) {
        return new Promise((resolve, reject) => {
            fetch(address, data)
            .then((response) => {return response.json();})
            .then((json) => {
                resolve(resolveFunction(json));
            })
            .catch((error) => {
                console.log(error);
                reject(error);
            });
        })
    }

    makeSession(){
        let address = `${this.webdriverAddress}/session`;
        let dataObject = {
            method: "POST",
            body: JSON.stringify({
                "capabilities": {
                    "alwaysMatch": {
                        "browserName": "cobalt",
                        "platformName": "linux"
                    },
                    "firstMatch": [{}]},
                "desiredCapabilities": {
                    "javascriptEnabled": true,
                    "browserName": "cobalt",
                    "platform": "LINUX"
                }
            })
        };
        let resolveFunction = function (json) {
            console.log("session id returned:");
            console.log(json);
            return json.sessionId;
        }
        return this.fetchRequest(address, dataObject, resolveFunction);
    }

    deleteSession(sessionId){
        let address = `${this.webdriverAddress}/session/${sessionId}`;
        let dataObject = {
            method: "DELETE"
        };
        return this.fetchRequest(address, dataObject);
    }

    getElement(sessionId){
        let address = `${this.webdriverAddress}/session/${sessionId}/element`;
        let data = {
            method: "POST",
            body: JSON.stringify({
                using: "css selector",
                value: "body"
            })
        }
        return this.fetchRequest(address, data);
    }

    getStatus(){
        let address = `${this.webdriverAddress}/status`;
        let data = {method: "GET"};
        return this.fetchRequest(address, data);
    }

    getSessions(){
        let address = `${this.webdriverAddress}/sessions`;
        let data = {method: "GET"};
        return this.fetchRequest(address, data);
    }

    sendKeystrokes(sessionId, keys){
        fetch(`${this.webdriverAddress}/session/${sessionId}/keys`, {
            method: "POST",
            body: JSON.stringify({value: keys})
        });
    }

    sendClick(sessionId, data){
        fetch(`${this.webdriverAddress}/session/${sessionId}/click`, {
            method: "POST",
            body: JSON.stringify(data)
        });
    }

    sendMouseMove(sessionId, data){
        let address = `${this.webdriverAddress}/session/${sessionId}/moveto`;
        let headers = {
            method: "POST",
            body: JSON.stringify(data)
        };
        return this.fetchRequest(address, headers);
    }

    startScreencast(sessionId){
        let address = `${this.webdriverAddress}/session/${sessionId}/startscreencast`;
        let data = {method: "GET"};
        return this.fetchRequest(address, data);
    }

    stopScreencast(sessionId){
        let address = `${this.webdriverAddress}/session/${sessionId}/stopscreencast`;
        let data = {method: "GET"};
        return this.fetchRequest(address, data);
    }

    //combine this call to a call with getNextScreenshotId()
    getScreenshotURL(){
        return `${this.screencastAddress}/screenshot/`;
    }

    createNextScreenshotId(){
        return this.imageId++;
    }
}

const cobaltService = new CobaltService()
export default cobaltService;