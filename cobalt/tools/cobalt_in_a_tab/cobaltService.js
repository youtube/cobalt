let CobaltService = class CobaltService{
    constructor(){
        this.portAddress = "http://localhost:4444";
        this.lockscreenshot = false;
    };

    fetchRequest(address, data, resolveFunction = (json) => { return json.value; }) {
        return new Promise((resolve, reject) => {
            fetch(address, data, )
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
        let address = `${this.portAddress}/session`;
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

    getElement(sessionId){
        let address = `${this.portAddress}/session/${sessionId}/element`;
        let data = {
            method: "POST",
            body: JSON.stringify({
                using: "css selector",
                value: "body"
            })
        }
        return this.fetchRequest(address, data);
    }

    //it will only request a screenshot after the last screenshot
    //request has been resolved. This prevents the network queue from
    //being flooded with requests and becoming unresponsive.
    getScreenshot(sessionId){
        let address = `${this.portAddress}/session/${sessionId}/screenshot`;
        let data = {method: "GET"};

        return this.fetchRequest(address, data);
    }

    getStatus(){
        let address = `${this.portAddress}/status`;
        let data = {method: "GET"};
        return this.fetchRequest(address, data);
    }

    getSessions(){
        let address = `${this.portAddress}/sessions`;
        let data = {method: "GET"};
        return this.fetchRequest(address, data);
    }

    sendKeystrokes(sessionId, keys){
        fetch(`${this.portAddress}/session/${sessionId}/keys`, {
            method: "POST",
            body: JSON.stringify({value: keys})
        });
    }

    sendClick(sessionId, data){
        fetch(`${this.portAddress}/session/${sessionId}/click`, {
            method: "POST",
            body: JSON.stringify(data)
        });
    }

    sendMouseMove(sessionId, data){
        fetch(`${this.portAddress}/session/${sessionId}/moveto`, {
            method: "POST",
            body: JSON.stringify(data)
        })
    }
}

const cobaltService = new CobaltService()
export default cobaltService;