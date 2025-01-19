//
// toolkit.js

//----------------------------------------------------
// UI functions .. called by UI elements
//

function dealWithNumber(elem, precision)
{
    let n = parseFloat(elem.value);
    let min = parseFloat(elem.min);
    let max = parseFloat(elem.max);
    if (n < min) {
        n = min;
    } else if (n > max) {
        n = max;
    }
    elem.value = n.toFixed(precision);
}

function dealWithPopup(elem)
{
    // nothing to do ..
}

function dealWithCheckbox(elem)
{
    if (elem.checked) {
        elem.value = "1";
    } else {
        elem.value = "0";
    }
}

function setting(id, precision)
{
    let elem = document.getElementById(id);
    if ("number"==typeof(precision)) {
        dealWithNumber(elem, precision);
    } /*else if ("SELECT"===elem.nodeName) {
        dealWithPopup(elem);
    } */else if (("INPUT"===elem.nodeName)&&("checkbox"===elem.type)) {
        dealWithCheckbox(elem);
    }
    elem.value = elem.value.replace(/\s+/g, '');
    sendToToolkit(id, elem.value);
}

function focusText(id)
{
    let elem = document.getElementById(id);
    elem.type = "text";

}

function blurPassword(id)
{
    let elem = document.getElementById(id);
    elem.type = "password";
}

//----------------------------------------------------
// Functions to set UI elemen values from a setting
//

function updateUI(id, value, precision)
{
    let elem = document.getElementById(id);
    if (elem) {
        elem.value = value;
        if ("number"==typeof(precision)) {
            dealWithNumber(elem, precision);
        }
        if (("INPUT"===elem.nodeName)&&("checkbox"===elem.type)) {
            elem.checked = ("1"==elem.value);
        }
    }
}

//----------------------------------------------------
// For listening to whatever is streaming through
// the toolkit.
//

function startAudio()
{
    let elem = document.getElementById("audio_stream");
    if (elem) {
        elem.type = "audio/mpeg";
        elem.src = "audio.mp3";
        elem.play();
        console.log("Start Audio");
    }
}

//----------------------------------------------------
// Functions to deal with the Web-Socket stuff
//

function sendToToolkit(id, value)
{
    console.log("sendToToolKit: " + id + ", " + value);
    toolkitSocket.send(id + " " + value + "\n");
}

function sendToolkitCommand(command)
{
    console.log("sendToolkitCommand: " + command);
    toolkitSocket.send(command + "\n");
}

function getPrecision(id)
{
    let precision = false;
    switch (id) {
        case "listen_icecast_port" :
            precision = 0;
            break;
        case "listen_volume" :
            precision = 2;
            break;
        case "remote_icecast_port" :
            precision = 0;
            break;
        case "manual_gain_level" :
            precision = 1;
            break;
        case "agc_maximum_gain" :
            precision = 1;
            break;
    }
    return precision;
}

function receiveFromToolkit(packet)
{
    let messages = packet.split("\n");
    for (let i = 0; i < messages.length; i++) {
        let fields = messages[i].split(" ");
        // we may receive the settings as <name> = <value>
        // OR <name> <value>
        // for the password, value may not exist
        let value = "";
        if (fields[1]) {
            if ('=' == fields[1]) {
                if (fields[2]) {
                    value = fields[2];
                }
            } else {
                value = fields[1];
            }
        }
        let precision = getPrecision(fields[0]);
        updateUI(fields[0],value,precision);
    }
}

//
// Create and open the web-socket with an input handler function

class toolkitSocket
{
    static uri;
    static socket;
    static pending;

    static open() {
        this.uri = "ws://" + window.location.host;
        this.socket = new WebSocket(this.uri); // WS on same URL as html page
        this.socket.binaryType = "arraybuffer";

        this.socket.addEventListener("open", (event) => {
            console.log("Socket is open.");
        });

        this.socket.addEventListener("error", (event) => {
            console.log("Socket error .. ");
            console.log(event);
        });

        this.socket.addEventListener("message", (event) => {
            if (typeof event.data === "string") {
                console.log("data is string ..");
                console.log(event.data);
                receiveFromToolkit(event.data);
            } else if (event.data instanceof Blob) {
                console.log("data is Blob");
            } else if (event.data instanceof ArrayBuffer) {
                console.log("data is ArrayBuffer");
            } else {
                console.log("Unknown message type!");
            }
        });

        this.socket.addEventListener("close", (event) => {
            console.log("Socket has closed");
        });
    } // end of static setup

    static close() {
        this.socket.close();
    }

    static isConnected() {
        return (this.socket.readyState < WebSocket.CLOSING);
    }

    static send(message) {
        if (this.isConnected()) {
            this.socket.send(message);
        } else {
            this.open();
            this.pending = message;
            setTimeout(function() {
                this.socket.send(this.pending);
            }.bind(this), 500);
        }
    }

    static {
        setTimeout(function() {
            this.open();
        }.bind(this), 500);

        window.addEventListener("focus", function(event)
        {
            console.log("window focus handler");
            if (!toolkitSocket.isConnected()) {
                setTimeout(function() {
                    toolkitSocket.open();
                }, 500);
            }
        });

        document.addEventListener("visibilitychange", () => {
            if (document.visibilityState === "hidden") {
                // We are now in the background
                // close the WS connection
                if (toolkitSocket.isConnected()) {
                    toolkitSocket.close();
                }
            } else {
                // We have just come into the foreground
                // reconnect to the WS
                setTimeout(function() {
                    toolkitSocket.open();
                }, 500);
            }
        });
    }
}
//
// test code
/*
updateUI("bitrate","96");
updateUI("manual_gain_level", "6", 1);
updateUI("agc_not_manual",1);
updateUI("startup_auto_mode","transmitter");

receiveFromToolkit("wifi_router_SSID TestSSID\nwifi_router_password secret\n");
receiveFromToolkit("listen_volume 0.6\n");
*/
//
// END OF toolkit.js
