//
// ToolkitWiFi_Server.cpp

#include "ToolkitWiFi_Server.h"
#include "websocket.h"
#include "http_file.h"
#include "icy_stream.h"
#include "../ToolkitFiles/ToolkitFiles.h"
#include "../parsingTools.h"

//
// Everything is static except the constructor
// It's sort of like a singleton

static boolean _server_running = false;
static WiFiServer *_server = NULL;
    // a TCP listener on port 80, 30 clients
    // I am betting that max clients is really max listener stack size
    // since the clients get passed once they are accepted.
    // NOTE: a WiFiServer is a NetworkServer

static const char *_default_index = NULL;
static size_t _default_index_size = 0;

static boolean _dns_server_running = false;
static DNSServer *_dnsServer = NULL;

        // this is a network connected() state
        // true if we are connected to the icecast server
        // false if we disconnect
static boolean _icecast_is_sending = false;
static boolean _mp3data_is_ready = false;
static uint8_t *(*_mp3_data_func)(size_t*) = NULL;
static uint8_t *_mp3_data = NULL;
static size_t _mp3_data_length = 0;

static void (*_ws_live_changes_func)(const char*,const char*) = NULL;

ToolkitWiFi_Server::ToolkitWiFi_Server()
{
    // nothing to do
}

ToolkitWiFi_Server::~ToolkitWiFi_Server()
{
    // because we use it as a singleton that exists forever
    // we don't cleanup :)
    // cleanup would include:
    // _client_list .. close and delete all
    // _dnsServer .. stop and delete
    // _server .. stop and delete
}

uint16_t ToolkitWiFi_Server::begin(uint16_t timeout_in_seconds)
{
    uint16_t result = WIFI_ALL_OKAY;

    //
    // (1) Autoconnect to a local router in station mode
    Serial.print("Connecting to SSID ");
    Serial.println(SettingItem::findString("wifi_router_SSID"));

    WiFi.mode(WIFI_STA);
    delay(200);
    WiFi.hostname(SettingItem::findString("wifi_toolkit_hostname"));
    WiFi.begin(SettingItem::findString("wifi_router_SSID"),
        SettingItem::findString("wifi_router_password"));

    uint16_t timeout = timeout_in_seconds * 2;
    while (timeout && (WiFi.status() != WL_CONNECTED)) {
        delay(500);
        Serial.print(".");
        timeout--;
    }

    if (WiFi.status() != WL_CONNECTED) {
        result = result | WIFI_TIMEOUT_ON_STATION;
        Serial.println("\nWiFi failed to connect to local router!");
    } else {
         Serial.println("\nWiFi connected to local router");
         Serial.println("IP address: ");  Serial.println(WiFi.localIP());
         // WiFi.localIP().toString().c_str()
         const char *ip = WiFi.localIP().toString().c_str();
         SettingItem::updateOrAdd("wifi_router_ip", ip);
    }

    //
    // (2) Create a soft access point
    const char *default_apssid = "WaveFarmToolkit";
    const char *apssid = SettingItem::findString("wifi_toolkit_AP_SSID");
    if (NULL == apssid) { apssid = default_apssid; }

    // ssid,pswd,channel,hidden,maxconnections
    if (!WiFi.softAP(apssid,"",AP_CHANNEL,false,MAX_CONNECTIONS)) {
        result = result | WIFI_FAIL_ON_ACCESS_POINT;
        Serial.println("WiFi failed to start soft Access Point!");
    } else {
        delay(500); // wait until we have a valid IP
        Serial.printf("Soft AP SSID: %s, IP address: ", apssid);
        Serial.println(WiFi.softAPIP());
        const char *ip = WiFi.softAPIP().toString().c_str();
        SettingItem::updateOrAdd("wifi_toolkit_AP_IP", ip);
    }

    //
    // (3) dnsRedirect
    if (0 == (result & WIFI_FAIL_ON_ACCESS_POINT)) {
        _dnsServer = new DNSServer();   // default to captive-portal mode
        _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
        // Setup the DNS server redirecting all the domains to the apIP
        if (_dnsServer->start(DNS_PORT, F("*"), WiFi.softAPIP())) {
            Serial.println("DNS Server started on Access Point.");
        } else {
            Serial.println("DNS Server failed to start!");
        }
        _dns_server_running = true;
    }

    //
    // (4) Start the server
    if (WIFI_ALL_FAILED != result) {
        _server = new WiFiServer(HTTP_PORT,MAX_CONNECTIONS);
        _server->begin();
        _server_running = true;
    }

    return result;
}

// forward prototype
static void handleRequest(ToolkitWiFi_Client *twfc);

void ToolkitWiFi_Server::run()
{
    if (_dns_server_running) {
        _dnsServer->processNextRequest(); // forwards all URLs to this one
    }

    //
    // check to see if our HTTP/WS server is running
    if (!_server_running) { return; }

    //
    // Accept new clients
    WiFiClient client;
    while (client = _server->accept()) {
        ToolkitWiFi_Client::acceptNewClient(client);
    }

    //
    // Check the client list for active requests/streams
    ToolkitWiFi_Client::checkClientList(handleRequest);
    if (_mp3_data_func) { // data has been sent to all stream clients
        _mp3_data = _mp3_data_func(&_mp3_data_length);
        _mp3data_is_ready = (NULL!=_mp3_data);
    }
    // Even if we don't have any mp3 clients, we still want to pull
    // the data through so we don't get behind the encoder.
    // If we do have mp3 clients, then if we get behind, we should
    // eventually catchup (if the WiFi connection is fast enough)
    // or we may get over-written/mangled (in which case we will
    // hear a chirp on the mp3 stream, or it will drop out)
}

void ToolkitWiFi_Server::setDefaultIndexPage(const char *buffer, size_t size)
{
    _default_index = buffer;
    _default_index_size = size;
}

//------------------------------------------------------------------
//
// MP3 data stream
//

void ToolkitWiFi_Server::setMP3DataStreamFunction(uint8_t*(*func)(size_t*))
{
    _mp3_data_func = func;
}

// MUST STAY IN THIS CODE FILE!
static void icyCloseFunction()
{
    _icecast_is_sending = false;
}

boolean ToolkitWiFi_Server::startIcecastBroadcast() {
    ToolkitWiFi_Client *twfc = ToolkitWiFi_Client::getAnEmptyClient();
    if (!twfc) {
        Serial.println("ToolkitWiFi client list is full!!");
        return false;
    }

    // connect to the remote icecast server
    WiFiClient *client = new WiFiClient();
    if (!client) {
        Serial.println("Cannot create new WiFiClient!");
        twfc->closeClient();
        return false;
    }
    const char *url = SettingItem::findString("remote_icecast_url");
    uint16_t port = SettingItem::findUInt("remote_icecast_port",8000);
    Serial.printf("ICY connecting .. %s:%u\n", url, port);
    if (!client->connect(url, port))
    {
        Serial.println("Failed to connect to remote icecast server!");
        delete(client);
        twfc->closeClient();
        return false;
    }
    // send headers and wait for OK reply
    _icecast_is_sending = icy_start_stream(client);
    if (!_icecast_is_sending) {
        client->stop();
        delete(client);
        twfc->closeClient();
    } else {
        twfc->client = client;
        twfc->type = ToolkitWiFi_Client::TYPE_MP3ICECAST;
        twfc->closed = icyCloseFunction;
    }
    return _icecast_is_sending;
}

// WE CAN ALSO keep a direct link to this client
// so that we can check to see if it disconnects
boolean ToolkitWiFi_Server::isIcecastBroadcastStillConnected()
{
    return _icecast_is_sending;
}

//------------------------------------------------------------------
//
// HANDLE OPEN CONNECTIONS of various types ..
//      UNKNOWN (froma new connection)
//      WEBSOCKET   .. websocket to a local portal page
//      MP3STREAM   .. stream to a local portal page
//      MP3ICECAST  .. stream to an icecast server
//

//
// Buffer for incoming request packets

#define MAX_PACKET_SIZE 1024   // incoming packets
static char http_buffer[MAX_PACKET_SIZE];

//------------------------------------------------------------------
//
// Send infinite headers for a new outgoing MP3 stream
//

// Used by handleUnknownRequest
static void http_send_infinite(ToolkitWiFi_Client *twfc, bool okay, const char *type) {
//  const char nocache[] =
//      "Cache-Control: no-cache, no-store, must-revalidate\nPragma: no-cache\nExpires: 0\n";
    const char nocache[] =
        "Cache-Control: no-cache, no-store\nPragma: no-cache\nExpires: 0\n";
    int response = 200;
    if (!okay) response = 404;
	twfc->client->printf(
        "HTTP/1.1 %d OK\nContent-Type: %s\n%s\n",
        response, type, nocache);
}

//------------------------------------------------------------------
//
// HANDLE GET REQUEST
//

static void handleGetRequest(ToolkitWiFi_Client *twfc, const char *path)
{
    http_handleGetRequest(twfc, path,
        _default_index, _default_index_size);
}

//------------------------------------------------------------------
//
// HANDLE POST REQUEST
//

// We want to parse the incoming buffer .. which will contain the headers
// and the first part of the data.
// Then we want to load in more chunks of data until we reach the
// end boundary.

// The header and start boundary should fit inside 1024 bytes
// then we can push the rest through as a stream of 1024 bytes at a time

static void handlePostRequest(ToolkitWiFi_Client *twfc,
    const char *buffer, size_t size)
{
    http_handlePostRequest(twfc,http_buffer, MAX_PACKET_SIZE);
}

//------------------------------------------------------------------
//
// HANDLE Websocket Message
//

void ToolkitWiFi_Server::setWSLiveChangesFunction(
    void(*func)(const char*,const char*))
{
    _ws_live_changes_func = func;
}

void ToolkitWiFi_Server::handleWSLiveChanges(
    const char *name, const char *value)
{
    if (NULL!=_ws_live_changes_func) {
        _ws_live_changes_func(name, value);
    }
}

static void handleWebSocketMessage(ToolkitWiFi_Client *twfc)
{
    // if bytes are available, read the data
    // parse incoming, send response
    // messages should be of the form <setting_name> = <value>
    size_t avail = twfc->client->available();
    if (avail <= 0) { return; }
    if (avail > (MAX_PACKET_SIZE-1)) {
        avail = MAX_PACKET_SIZE - 1;
        Serial.println("WS - INCOMING PACKET IS TOO BIG!");
    }
    twfc->client->readBytes(http_buffer, avail);

    if (!websocket_handleIncoming(twfc, http_buffer, avail)) {
        twfc->closeClient();
    }
}

//------------------------------------------------------------------
//
// HANDLE Outgoing MP3 Stream .. send to icecast and local listeners
//

// USES http_buffer which is defined in this file
static void handleOutgoingMP3Stream(ToolkitWiFi_Client *twfc)
{
    // if we have MP3 data in the buffer then send it out
    if (_mp3data_is_ready) {
        // write() will try to send the data up to 10 times
        // then it will fail if there's something wrong with the
        // TCP connection.
        size_t actual = twfc->client->write(_mp3_data, _mp3_data_length);
        if (actual != _mp3_data_length) {
            Serial.printf("Error sending mp3 stream .. %u bytes sent out of %u\n",
                actual, _mp3_data_length);
        }
    }
    if (ToolkitWiFi_Client::TYPE_MP3ICECAST==twfc->type) {
        // check for messages from the server and clear
        // the input buffer.
        // static char http_buffer[MAX_PACKET_SIZE];
        size_t avail = twfc->client->available();
        if (avail > 0) {
            if (avail > (MAX_PACKET_SIZE-1)) {
                avail = MAX_PACKET_SIZE-1;
            }
            twfc->client->readBytes(http_buffer, avail);
            http_buffer[avail] = 0;
            Serial.printf("ICY SERVER SAYS: %s\n", http_buffer);
        }
    }
}

//------------------------------------------------------------------
//
// PARSE HTTP HEADERS and figure out what type of request we have
//

// GET /favicon.ico HTTP/1.1    -> reply with 404
// GET /generate_204 HTTP/1.1   -> reply with index.html
// ignore POST requests         -> reply with 200 OK ??
// GET /toolkit.mp3 HTTP/1.1    -> reply with infinite header
// WS request has a Sec-WebSocket-Key header -> reply with WS handshake

enum {
    RESPONSE_IGNORE,
    RESPONSE_WEBSOCKET,
    RESPONSE_INDEX,
    RESPONSE_MP3,
    RESPONSE_FILE,
    RESPONSE_POST
};

// parse the header to figure out what type we are
// Look for GET <filename> or POST
// if web-socket handshake -> WS stuff
// if /favicon.ico -> ignore OR just let it go through as a file
// if / or /generate_204 -> send index.html
// if /*.mp3 -> send infinite header and setup as MP3 stream
// else load and send file or 404

static int whatisit(char *buffer, size_t size, char **path)
{
    buffer[size] = 0;

    // Serial.println("=====================");
    // Serial.println(buffer);
    // Serial.println("=====================");

    char *get = strtok(buffer, " ");
    if (NULL==get) {
        return RESPONSE_IGNORE;
    } else if (0 == strcmp("POST", get)) {
        buffer[4] = ' ';    // replace the strtok \0 with a space character
        return RESPONSE_POST;
    } else if (0 != strcmp("GET", get)) {
        return RESPONSE_IGNORE;
    }

    size_t getsize = strlen(get) + 1;
    buffer = get + getsize;
    size = size - getsize;

    const char *ws_key = websocket_isWSHeader(buffer, size);
    if (ws_key) { // web socket open request
        return RESPONSE_WEBSOCKET;
    }

    *path = strtok(NULL, " ");
    if (NULL==path) {
        return RESPONSE_IGNORE;
    }

    char *dot = strrchr(*path, '.');
    if (dot && dot[1]) {
        if (0 == strcmp("mp3", &dot[1])) {
            return RESPONSE_MP3;
        }
    }

    // all missing files will default to index.html
    // this deals with all the different captive portal requests
    return RESPONSE_FILE;
}

//------------------------------------------------------------------
//
// HANDLE Unknown Request Type
//

// USES http_buffer which is defined in this file!
static void handleUnknownRequest(ToolkitWiFi_Client *twfc)
{
    // if bytes are available, read the data into a largish internal buffer
    // add a NULL character to the end of the buffer so we can use c-string

    size_t avail = twfc->client->available();
    if (avail <= 0) { return; }
    if (avail > (MAX_PACKET_SIZE-1)) {
        avail = MAX_PACKET_SIZE - 1;
        //Serial.println("INCOMING PACKET IS TOO BIG!");
    }

    twfc->millis_last_used = 0; // reset the timer when a new request comes in

    const char indexfile[] = "/index.html";
    char *path = NULL;
    twfc->client->readBytes(http_buffer, avail);
    switch (whatisit(http_buffer, avail, &path)) {
        case RESPONSE_WEBSOCKET :
        {   // Serial.println("Request for Web Socket");
            twfc->type = ToolkitWiFi_Client::TYPE_WEBSOCKET;
            const char *ws_key = websocket_getClientKey();
            uint32_t reply_length;
            char *reply = websocket_handshake(ws_key, &reply_length);
            twfc->client->write(reply,reply_length);
            free(reply);
            websocket_sendSettings(twfc);
        }   break;
        case RESPONSE_MP3 :
            // Serial.println("Request for MP3 Stream");
            twfc->type = ToolkitWiFi_Client::TYPE_MP3STREAM;
            http_send_infinite(twfc, true, "audio/mpeg");
            break;
        case RESPONSE_INDEX :
            // Serial.println("Request for HTML Index");
            path = (char *) indexfile;
        case RESPONSE_FILE :
            Serial.printf("Request for file %s\n", path);
            handleGetRequest(twfc, (char *) path);
            break;
        case RESPONSE_POST :
            handlePostRequest(twfc, http_buffer, avail);
            break;
        case RESPONSE_IGNORE :
            // Serial.println("Request for Ignore");
            twfc->closeClient();
            break;
    }
}

//------------------------------------------------------------------
//
// Handle Requests .. direct traffic
//

static void handleRequest(ToolkitWiFi_Client *twfc)
{
    switch (twfc->type) {
        case ToolkitWiFi_Client::TYPE_UNKNOWN :
            handleUnknownRequest(twfc);
            break;
        case ToolkitWiFi_Client::TYPE_WEBSOCKET :
            handleWebSocketMessage(twfc);
            break;
        case ToolkitWiFi_Client::TYPE_MP3STREAM :
        case ToolkitWiFi_Client::TYPE_MP3ICECAST :
            handleOutgoingMP3Stream(twfc);
            break;
    } // end of switch()
}

//
// END OF ToolkitWiFi_Server.cpp
