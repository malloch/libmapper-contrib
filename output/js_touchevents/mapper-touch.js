
// adapted from
// https://stackoverflow.com/questions/29018151/how-do-i-programmatically-create-a-touchevent-in-chrome-41

window.onload = init;

var socket = null;
var connected = false;
var connectionInterval = 30000;

/* eventType is 'touchstart', 'touchmove', 'touchcancel', 'touchend' */
function sendTouchEvent(x, y, element, eventType) {
    const touchObj = new Touch({identifier: Date.now(),
                                target: element,
                                clientX: x,
                                clientY: y,
                                radiusX: 2.5,
                                radiusY: 2.5,
                                rotationAngle: 10,
                                force: 0.5,});

    const touchEvent = new TouchEvent(eventType, {cancelable: true,
                                                  bubbles: true,
                                                  touches: [touchObj],
                                                  targetTouches: [],
                                                  changedTouches: [touchObj],
                                                  shiftKey: true,});

    element.dispatchEvent(touchEvent);
}

const myElement = document.getElementById('foo')

sendTouchEvent(150, 150, myElement, 'touchstart');
sendTouchEvent(220, 200, myElement, 'touchmove');
sendTouchEvent(220, 200, myElement, 'touchend');

function init() {
    console.log('initializing mapper-touch bridge');

    function openWebSocket() {
        console.log("  Websocket: Trying to connect...");

            // Create a new WebSocket.
        socket = new WebSocket('ws://192.168.1.120/Dal200');
        socket.onopen = function(event) {
            console.log("  Websocket: Connection established");
            socket.send("touch connected.");
            connected = true;
        }
        socket.onclose = function(event) {
            console.log("  Websocket: Connection dropped, will try to "
                        "reconnect in", connectionInterval/1000, "seconds");
            connected = false;
        }
        socket.onmessage = function(event) {
            let data = null;
            if (event.data)
                data = JSON.parse(event.data);
            if (!data)
                return;
            if (data.touches) {
                for (var i in data.touches) {
                    let = data.touches[i];
                    sendTouchEvent(t.x, t.y, myElement, t.type);
                }
            }
        }
    }

    // open webSocket
    openWebSocket();

    // check the websocket periodically
    setInterval(function() {
        console.log("checking websocket...");
        if (connected == true) {
            console.log("   Socket ok.");
            return;
        }
        openWebSocket();
    }, connectionInterval);
}
