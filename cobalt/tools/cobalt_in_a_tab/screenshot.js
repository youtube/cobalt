import cobaltService from "./cobaltService.js";

// The unique ID needed for almost every webdriver command.
let sessionId = null;
// A container holding all the screens sending requests for frames.
let screenList = [];
// The number of screens requesting frames from Cobalt.
// This must be greater than 1, but can be optimized for performance.
// On flakey networks, more may be necessary.
// 3-4 are recommended for stable behavior on a reliable, fast network.
let numScreens = 4;
// The screen that is currently displayed to the user.
let visibleScreen = null;
// The current position of the mouse relative to the screens.
let mousePosition = {xPos: 0, yPos:0};
// The ID of cobalt's body element.
// Used to send mouse positioning instructions.
let bodyElementId = null;
// A cancelable event that sends mouse position to Cobalt.
let mouseInterval = null;
// The div that contains all the image frames.
let parentContainer = null;

window.onload = async function(){
    await cobaltService.initialize();
    // Create image tags with the appropriate css.
    parentContainer = document.getElementById("screenContainer");
    if (numScreens < 1) return;
    for (let i = 0; i < numScreens; i++){
        // Create an image and append it to DOM.
        let newScreen = document.createElement("img");
        newScreen.id = "screen" + i;
        newScreen.classList.add("screen");
        parentContainer.appendChild(newScreen);
        screenList.push(newScreen);
    }

    // Grab a current session.
    let sessions = await cobaltService.getSessions();
    if(sessions.length > 0){
        sessionId = sessions[0];
    }
    else{
        // Create a new session if none are already created.
        sessionId = await cobaltService.makeSession();
    }
    // Get Webdriver body element for mouse positioning.
    bodyElementId = await cobaltService.getElement(sessionId);

    cobaltService.startScreencast(sessionId).then((response) => {
        cobaltService.setScreencastPort(response);
        subscribeToMouseEvents();
        window.addEventListener("keydown", sendKeyPress);
        window.addEventListener("beforeunload", function(e){
            cobaltService.stopScreencast(sessionId);
            cobaltService.deleteSession(sessionId);
        }, false);

        // Start requesting screenshots.
        for (let i = 0; i < screenList.length; i++) {
            assignScreenshotURLAndId(screenList[i]);
            // No screenshots are currently displayed.
            screenList[i].currentImageId = -1;
            visibleScreen = screenList[0];
        }
    });

    // Assign image behavior.
    for (let i = 0; i < screenList.length; i++) {
        screenList[i].onload = () => { imageLoaded(screenList[i]) };
        // Reload on error.
        // TODO - differentiate between types of errors.
        screenList[i].onerror = () => { assignScreenshotURLAndId(screenList[i]) };
    }
}

function assignScreenshotURLAndId(screen){
    let nextImageId = cobaltService.createNextScreenshotId();
    screen.nextImageId = nextImageId;
    screen.src = cobaltService.getScreenshotURL() + nextImageId;
}

function imageLoaded(screen){
    // The image will only become visible if it's more recent than the last image.
    if(screen.nextImageId > visibleScreen.currentImageId){
        visibleScreen.classList.remove("visibleScreen");
        screen.classList.add("visibleScreen");
        visibleScreen = screen;
    }
    screen.currentImageId = screen.nextImageId;
    assignScreenshotURLAndId(screen);
}

function sendClick(event){
    let data = {
        button: event.button
    };
    cobaltService.sendClick(sessionId, data);
}

function sendScroll(event){
    // Prevent default browser action like scrolling on mouseKey.
    event.preventDefault();
}

function saveMousePos(event){
    mousePosition = {
        xPos: event.clientX,
        yPos: event.clientY
    };
}

// Requests that get stalled send outdated mouse positions,
// so only send one at a time. This will help ensure no mouse
// movements are delayed.
var mouseLock = false;
function sendMousePos(){
    if (mouseLock) return;
    let viewportOffset = parentContainer.getBoundingClientRect();

    let data = {
        element: bodyElementId,
        xoffset: mousePosition.xPos - viewportOffset.left,
        yoffset: mousePosition.yPos - viewportOffset.top
    };
    mouseLock = true;
    cobaltService.sendMouseMove(sessionId, data)
    .then((response) => {
        mouseLock = false;
    });
}

function subscribeToMouseEvents(){
    window.addEventListener('mousemove', saveMousePos);
    window.addEventListener('click', sendClick);
    window.addEventListener('scroll', sendScroll);
    mouseInterval = setInterval(sendMousePos, 200);
}

if (typeof KeyEvent === 'undefined') {
    var KeyEvent = {
        DOM_VK_CANCEL: 3,
        DOM_VK_HELP: 6,
        DOM_VK_BACK_SPACE: 8,
        DOM_VK_TAB: 9,
        DOM_VK_CLEAR: 12,
        DOM_VK_RETURN: 13,
        DOM_VK_ENTER: 14,
        DOM_VK_SHIFT: 16,
        DOM_VK_CONTROL: 17,
        DOM_VK_ALT: 18,
        DOM_VK_PAUSE: 19,
        DOM_VK_CAPS_LOCK: 20,
        DOM_VK_ESCAPE: 27,
        DOM_VK_SPACE: 32,
        DOM_VK_PAGE_UP: 33,
        DOM_VK_PAGE_DOWN: 34,
        DOM_VK_END: 35,
        DOM_VK_HOME: 36,
        DOM_VK_LEFT: 37,
        DOM_VK_UP: 38,
        DOM_VK_RIGHT: 39,
        DOM_VK_DOWN: 40,
        DOM_VK_PRINTSCREEN: 44,
        DOM_VK_INSERT: 45,
        DOM_VK_DELETE: 46,
        DOM_VK_0: 48,
        DOM_VK_1: 49,
        DOM_VK_2: 50,
        DOM_VK_3: 51,
        DOM_VK_4: 52,
        DOM_VK_5: 53,
        DOM_VK_6: 54,
        DOM_VK_7: 55,
        DOM_VK_8: 56,
        DOM_VK_9: 57,
        DOM_VK_SEMICOLON: 59,
        DOM_VK_EQUALS: 61,
        DOM_VK_A: 65,
        DOM_VK_B: 66,
        DOM_VK_C: 67,
        DOM_VK_D: 68,
        DOM_VK_E: 69,
        DOM_VK_F: 70,
        DOM_VK_G: 71,
        DOM_VK_H: 72,
        DOM_VK_I: 73,
        DOM_VK_J: 74,
        DOM_VK_K: 75,
        DOM_VK_L: 76,
        DOM_VK_M: 77,
        DOM_VK_N: 78,
        DOM_VK_O: 79,
        DOM_VK_P: 80,
        DOM_VK_Q: 81,
        DOM_VK_R: 82,
        DOM_VK_S: 83,
        DOM_VK_T: 84,
        DOM_VK_U: 85,
        DOM_VK_V: 86,
        DOM_VK_W: 87,
        DOM_VK_X: 88,
        DOM_VK_Y: 89,
        DOM_VK_Z: 90,
        DOM_VK_CONTEXT_MENU: 93,
        DOM_VK_NUMPAD0: 96,
        DOM_VK_NUMPAD1: 97,
        DOM_VK_NUMPAD2: 98,
        DOM_VK_NUMPAD3: 99,
        DOM_VK_NUMPAD4: 100,
        DOM_VK_NUMPAD5: 101,
        DOM_VK_NUMPAD6: 102,
        DOM_VK_NUMPAD7: 103,
        DOM_VK_NUMPAD8: 104,
        DOM_VK_NUMPAD9: 105,
        DOM_VK_MULTIPLY: 106,
        DOM_VK_ADD: 107,
        DOM_VK_SEPARATOR: 108,
        DOM_VK_SUBTRACT: 109,
        DOM_VK_DECIMAL: 110,
        DOM_VK_DIVIDE: 111,
        DOM_VK_F1: 112,
        DOM_VK_F2: 113,
        DOM_VK_F3: 114,
        DOM_VK_F4: 115,
        DOM_VK_F5: 116,
        DOM_VK_F6: 117,
        DOM_VK_F7: 118,
        DOM_VK_F8: 119,
        DOM_VK_F9: 120,
        DOM_VK_F10: 121,
        DOM_VK_F11: 122,
        DOM_VK_F12: 123,
        DOM_VK_F13: 124,
        DOM_VK_F14: 125,
        DOM_VK_F15: 126,
        DOM_VK_F16: 127,
        DOM_VK_F17: 128,
        DOM_VK_F18: 129,
        DOM_VK_F19: 130,
        DOM_VK_F20: 131,
        DOM_VK_F21: 132,
        DOM_VK_F22: 133,
        DOM_VK_F23: 134,
        DOM_VK_F24: 135,
        DOM_VK_NUM_LOCK: 144,
        DOM_VK_SCROLL_LOCK: 145,
        DOM_VK_COMMA: 188,
        DOM_VK_PERIOD: 190,
        DOM_VK_SLASH: 191,
        DOM_VK_BACK_QUOTE: 192,
        DOM_VK_OPEN_BRACKET: 219,
        DOM_VK_BACK_SLASH: 220,
        DOM_VK_CLOSE_BRACKET: 221,
        DOM_VK_QUOTE: 222,
        DOM_VK_META: 224
    };
}

function mapKeyCode(code){
    // We want to optimize this for all printable characters ideally.
    switch(code){
        case KeyEvent.DOM_VK_UP:
            return '\ue013';
        case KeyEvent.DOM_VK_DOWN:
            return '\ue015';
        case KeyEvent.DOM_VK_LEFT:
            return '\ue012';
        case KeyEvent.DOM_VK_RIGHT:
            return '\ue014';
        case KeyEvent.DOM_VK_ENTER:
            return '\ue007';
        case KeyEvent.DOM_VK_RETURN:
            return '\ue006';
        case KeyEvent.DOM_VK_ESCAPE:
            return '\ue00c';
        case KeyEvent.DOM_VK_BACK_SPACE:
            return '\ue003';
        case KeyEvent.DOM_VK_SPACE:
            return '\ue00d';
        case KeyEvent.DOM_VK_SHIFT:
            return '\ue008';
        default:
            return null;
    }
}

function sendKeyPress(event){
    // Prevent default browser action like scrolling on mouseKey.
    event.preventDefault();

    let uKeyCode = mapKeyCode(event.keyCode);
    if(uKeyCode === null) uKeyCode = event.key;
    cobaltService.sendKeystrokes(sessionId, [uKeyCode]);
}
