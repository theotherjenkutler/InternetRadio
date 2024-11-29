//
// ToolkitWiFi.cpp

#include "ToolkitWiFi.h"
#include "websocket.h"
#include "postfile.h"
#include "../ToolkitSPIFFS/ToolkitSPIFFS.h"
#include "../parsingTools.h"

ToolkitWiFi::ToolkitWiFi()
{
    _server_running = false;
    _server = new WiFiServer(HTTP_PORT,MAX_CONNECTIONS);
    _default_index = NULL;
    _default_index_size = 0;
    _dns_server_running = false;
    _dnsServer = NULL;
    _num_clients = 0;
    memset(_client_list, 0, sizeof(ToolkitWiFi_Client) * MAX_CLIENTS);
    _icecast_is_sending = false;
    _mp3data_is_ready = false;
    _mp3_data_func = NULL;
    _mp3_data = NULL;
    _mp3_data_length = 0;
    _ws_live_changes_func = NULL;
}

ToolkitWiFi::~ToolkitWiFi()
{
    // because we use it as a singleton that exists forever
    // we don't cleanup :)
    // cleanup would include:
    // _client_list .. close and delete all
    // _dnsServer .. stop and delete
    // _server .. stop and delete
}

uint16_t ToolkitWiFi::begin(uint16_t timeout_in_seconds)
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
        _server->begin();
        _server_running = true;
    }

    return result;
}

void ToolkitWiFi::run()
{
    if (_dns_server_running) {
        _dnsServer->processNextRequest(); // forwards all URLs to this one
    }
    if (!_server_running) { return; }
    acceptNewClients();
    checkClientList();
}

void ToolkitWiFi::setDefaultIndexPage(const char *buffer, size_t size)
{
    _default_index = buffer;
    _default_index_size = size;
}

//------------------------------------------------------------------
//
// MP3 data stream
//

void ToolkitWiFi::setMP3DataStreamFunction(uint8_t*(*func)(size_t*))
{
    _mp3_data_func = func;
}

//
// MP3 Stream Data through ..

void ToolkitWiFi::markMP3StreamDataSent()
{
    if (_mp3_data_func) {
        _mp3_data = _mp3_data_func(&_mp3_data_length);
        _mp3data_is_ready = (NULL!=_mp3_data);
    }
}

// TODO: cleanup floating prototype!!!
boolean icy_start_stream(WiFiClient *client);

boolean ToolkitWiFi::startIcecastBroadcast() {
    // connect to the remote icecast server
    WiFiClient *client = new WiFiClient();
    if (!client) {
        Serial.println("Cannot create new WiFiClient!");
        return false;
    }
    const char *url = SettingItem::findString("remote_icecast_url");
    uint16_t port = SettingItem::findUInt("remote_icecast_port",8000);
    Serial.printf("ICY connecting .. %s:%u\n", url, port);
    if (!client->connect(url, port))
    {
        Serial.println("Failed to connect to remote icecast server!");
        delete(client);
        return false;
    }
    // send headers and wait for OK reply
    _icecast_is_sending = icy_start_stream(client);
    if (!_icecast_is_sending) {
        client->stop();
        delete(client);
    } else {
        // if we connect, then add the client as a ToolkitWiFi_Client
        ToolkitWiFi_Client *twfc = getAnEmptyClient();
        if (!twfc) {
            Serial.println("ToolkitWiFi client list is full!!");
            client->stop();
            delete(client);
            return false;
        }
        twfc->client = client;
        twfc->type = ToolkitWiFi_Client::TYPE_MP3ICECAST;
    }
    return _icecast_is_sending;
}
// WE CAN ALSO keep a direct link to this client
// so that we can check to see if it disconnects
boolean ToolkitWiFi::isIcecastBroadcastStillConnected()
{
    return _icecast_is_sending;
}

//------------------------------------------------------------------
//
// Manage the connections/client list
//

ToolkitWiFi_Client* ToolkitWiFi::getAnEmptyClient()
{
    if (_num_clients < MAX_CLIENTS) {
        for (uint16_t i = 0; i < MAX_CLIENTS; i++) {
            if (ToolkitWiFi_Client::TYPE_UNUSED == _client_list[i].type) {
                _num_clients++;
                //Serial.printf("Num clients = %u\n", _num_clients);
                _client_list[i].type = ToolkitWiFi_Client::TYPE_UNKNOWN;
                return &_client_list[i];
            }
        }
    }
    return NULL;
}

void ToolkitWiFi::closeClient(ToolkitWiFi_Client *twfc)
{
    twfc->client->stop();
    delete(twfc->client);
    twfc->client = NULL;
    if (ToolkitWiFi_Client::TYPE_MP3ICECAST==twfc->type) {
        _icecast_is_sending = false;
        Serial.println("ICY server disconnected.");
    }
    // if (ToolkitWiFi_Client::TYPE_MP3STREAM==twfc->type) {
    //     Serial.println("MP3 client closing");
    // }
    twfc->type = ToolkitWiFi_Client::TYPE_UNUSED;
    twfc->millis_last_used = 0;
    _num_clients--;
    //Serial.printf("Num clients (closing) = %u\n", _num_clients);
}

void ToolkitWiFi::setClientTimedClose(ToolkitWiFi_Client *twfc)
{
    twfc->millis_last_used = millis();  // milliseconds since Arduino started
    twfc->type = ToolkitWiFi_Client::TYPE_UNKNOWN;
}

boolean ToolkitWiFi::didClientTimeout(ToolkitWiFi_Client *twfc)
{
    if (0!=twfc->millis_last_used) {
        uint32_t alive = millis() - twfc->millis_last_used;
        if (alive > ToolkitWiFi_Client::CLOSE_TIMEOUT) {
            return true;
        }
    }
    return false;
}

//------------------------------------------------------------------
//
// ACCEPT
//

void ToolkitWiFi::acceptNewClients()
{ // we do it this way because _server->accept() is passing classes not pointers
    WiFiClient *client = NULL;
    do {
        WiFiClient clone = _server->accept();
        if (clone) {
            client = new WiFiClient(clone);
            // client is now outside the scope of do..while
            ToolkitWiFi_Client *twfc = getAnEmptyClient();
            if (twfc) {
                twfc->client = client;
            //    Serial.println("Accepted new client.");
            } else {
                client->stop();
                delete(client);
                client = NULL;
                Serial.println("Accept .. server is full.");
            }
        } else {
            client = NULL;
        }
    } while (client);
}

//------------------------------------------------------------------
//
// HANDLE OPEN CONNECTIONS
//

void ToolkitWiFi::checkClientList()
{
    for (uint16_t i = 0; i < MAX_CLIENTS; i++) {
        ToolkitWiFi_Client *twfc = &_client_list[i];
        if (NULL != twfc->client) {
            if (!twfc->client->connected()) {
                closeClient(twfc);
                //Serial.println("Client closed itself");
            } else if (didClientTimeout(twfc)) {
                closeClient(twfc);
                //Serial.println("Client timeout close!");
            } else {
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
            } // end of connected()
        } // end of if(client)
    } // end of for()
    markMP3StreamDataSent(); // data has been sent to all stream clients
    // Even if we don't have any mp3 clients, we still want to pull
    // the data through so we don't get behind the encoder.
    // If we do have mp3 clients, then if we get behind, we should
    // eventually catchup (if the WiFi connection is fast enough)
    // or we may get over-written/mangled (in which case we will
    // hear a chirp on the mp3 stream, or it will drop out)
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
#define MAX_PACKET_SIZE 1024   // incoming packets
static char http_buffer[MAX_PACKET_SIZE];

enum {
    RESPONSE_IGNORE,
    RESPONSE_WEBSOCKET,
    RESPONSE_INDEX,
    RESPONSE_MP3,
    RESPONSE_FILE,
    RESPONSE_POST
};

// parse the header to figure out what type we are
// Look for GET <filename>
// if not GET -> ignore
// if web-socket handshake -> WS stuff
// if /favicon.ico -> ignore OR just let it go through as a file
// if / or /generate_204 -> send index.html
// if /*.mp3 -> send infinite header and setup as MP3 stream
// else load and send file or 404

static int whatisit(char *buffer, size_t size, char **path)
{
    buffer[size] = 0;

    // const char *req_end = findTwoEndlines(buffer);
    // if (NULL != req_end) {
    //     uint32_t remaining = size - (req_end - buffer);
    //     Serial.printf("Request buffer has %u bytes remaining\n", remaining);
    // }

//    Serial.println("=====================");
//    Serial.println(buffer);
//    Serial.println("=====================");

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

static void http_send_infinite(ToolkitWiFi_Client *twfc, bool okay, const char *type) {
//    const char nocache[] = "Cache-Control: no-cache, no-store, must-revalidate\nPragma: no-cache\nExpires: 0\n";
    const char nocache[] = "Cache-Control: no-cache, no-store\nPragma: no-cache\nExpires: 0\n";
    int response = 200;
    if (!okay) response = 404;
	twfc->client->printf(
        "HTTP/1.1 %d OK\nContent-Type: %s\n%s\n",
        response, type, nocache);
}

//------------------------------------------------------------------
//
// HANDLE Unknown Request Type
//

void ToolkitWiFi::handleUnknownRequest(ToolkitWiFi_Client *twfc)
{
    // if bytes are available, read the data into a largish internal buffer
    // add a NULL character to the end of the buffer so we can use c-string

    size_t avail = twfc->client->available();
    if (avail <= 0) { return; }
    if (avail > (MAX_PACKET_SIZE-1)) {
        avail = MAX_PACKET_SIZE - 1;
        Serial.println("INCOMING PACKET IS TOO BIG!");
    }

    const char indexfile[] = "/index.html";
    char *path = NULL;
    twfc->client->readBytes(http_buffer, avail);
    switch (whatisit(http_buffer, avail, &path)) {
        case RESPONSE_WEBSOCKET :
        {//   Serial.println("Request for Web Socket");
            twfc->type = ToolkitWiFi_Client::TYPE_WEBSOCKET;
            const char *ws_key = websocket_getClientKey();
            uint32_t reply_length;
            char *reply = websocket_handshake(ws_key, &reply_length);
            twfc->client->write(reply,reply_length);
            free(reply);
            websocket_sendSettings(twfc);
        }   break;
        case RESPONSE_MP3 :
            Serial.println("Request for MP3 Stream");
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
            closeClient(twfc);
            break;
    }

}

//------------------------------------------------------------------
//
// HANDLE GET REQUEST
//

// (1) send a file and header packet with a mime-type
static void http_send(ToolkitWiFi_Client *twfc, const char *data, size_t size, bool okay, const char *type) {
    const char nocache[] = "Cache-Control: no-cache, no-store, must-revalidate\nPragma: no-cache\nExpires: 0\n";
    int response = 200;
    if (!okay) response = 404;
	twfc->client->printf("HTTP/1.1 %d OK\nContent-Length: %u\nContent-Type: %s; charset=UTF-8\n%s\n",
        response, size, type, nocache);
	twfc->client->write(data,size);
}

// (2) MIME TYPES .. based on file extension
typedef struct {
    char ext[8];
    char type[16];
} mime_item;

static mime_item mime_list[] = {
    { "txt",  "text/plain" },
    { "htm",  "text/html" },
    { "html", "text/html" },
    { "js",   "text/javascript" },
    { "css",  "text/css" },
    { "png",  "image/png" },
    { "jpg",  "image/jpeg" },
    { "jpeg", "image/jpeg "},
    { "", ""}
};

static char *mime_type(const char *name)
{
    char *dot = strrchr(name, '.');
    if ((NULL != dot) && (0 != dot[1])) {
        dot++;  // point at first char after the dot
        mime_item *mi = mime_list;
        while (mi->ext[0]) {
            if (0 == strcmp(dot, mi->ext)) {
                return mi->type;
            }
            mi++;
        }
    }
    return mime_list[0].type;
}

enum {
    FILE_IS_ANY     = 0,
    FILE_IS_ROOT,
    FILE_IS_INDEX,
    FILE_IS_FAVICON,
    FILE_IS_UPLOAD
};

static uint32_t match_filename(const char *path)
{
    if (0==strcmp("/favicon.ico", path)) {
        return FILE_IS_FAVICON;
    } else if (0==strcmp("/upload", path)) {
        return FILE_IS_UPLOAD;
    } else if (0==strcmp("/upload.html", path)) {
        return FILE_IS_UPLOAD;
    } else if (0==strcmp("/index.html", path)) {
        return FILE_IS_INDEX;
    } else if (0==strcmp("/", path)) {
        return FILE_IS_ROOT;
    }
    return FILE_IS_ANY;
}

void ToolkitWiFi::handleGetRequest(ToolkitWiFi_Client *twfc, const char *path)
{
    // fetch the file from *path, set the mime-type and content headers
        // the spiffs read will pull the file into an internal 4kB buffer
        // then send those bytes directly to the client
    size_t size = 0;
    char *data = NULL;
    uint32_t type = match_filename(path);

    // upload needs to set data and size directly
    // index if it doesn't exist needs to set data and size directly
    // favicon should keep it's path
    // any, should change to index if it is not found

    if (FILE_IS_ROOT==type) {
        path = "/index.html";
        type = FILE_IS_INDEX;
    }
    
    if (FILE_IS_ANY==type) {
        if (!ToolkitSPIFFS::fileExists(path)) {
            path = "/index.html";
            type = FILE_IS_INDEX;
        }
    }

    if (FILE_IS_INDEX==type) {
        if(!ToolkitSPIFFS::fileExists(path)) {
            type = FILE_IS_UPLOAD;
        }
    }

    if (FILE_IS_UPLOAD==type) {
        data = (char *) _default_index;
        size = _default_index_size;
        path = "/upload.html";  // so we get the correct mime type
    } else {
        data = ToolkitSPIFFS::fileRead(path, &size);
    }

    if (data && size) {
        const char *mime = mime_type(path);
        http_send(twfc, data, size, true, mime);
    } else {
        const char unknown[] = "404 Can't find the file!\n";
        http_send(twfc, unknown, strlen(unknown), false, "text/html");
    }
    setClientTimedClose(twfc);
}

//------------------------------------------------------------------
//
// HANDLE POST REQUEST
//

//
// We want to parse the incoming buffer .. which will contain the headers
// and the first part of the data.
// Then we want to load in more chunks of data until we reach the
// end boundary.

// The header and start boundary should fit inside 1024 bytes
// then we can push the rest through as a stream of N bytes at a time?

void ToolkitWiFi::handlePostRequest(ToolkitWiFi_Client *twfc, const char *buffer,
    size_t size)
{
    const char *content = NULL;
    size_t remaining = 0;
    const char *filename = NULL;

    filename = postfile_findContent(buffer, &content, &remaining);

    if ((NULL==filename) || (0==filename[1])) {
        closeClient(twfc);
        return;
    }

    File f = ToolkitSPIFFS::fileOpen(filename, FILE_WRITE);
    if (!f) {
        Serial.printf("Error creating file %s\n", filename);
        return;
    }

    boolean keepgoing = true;

    if (remaining) {
        keepgoing = postfile_addContent(&f, content, remaining);
    }

    int32_t tryagain = 4;

    // It works, but it's terrible code .. :)

    while (keepgoing && (0!=tryagain)) {
        // delay and try to load more bytes
        vTaskDelay(portTICK_PERIOD_MS * 20);
        size_t avail;
        while ((avail = twfc->client->available()) > 0) {
            if (avail > MAX_PACKET_SIZE) {
                avail = MAX_PACKET_SIZE;
            }
            twfc->client->readBytes(http_buffer, avail);
            // we still want to read any bytes after the file data
            // so we keep readBytes(..) from the client, but
            // only write to the file if keepgoing==true
            if (keepgoing) {
                keepgoing = postfile_addContent(&f, http_buffer, avail);
            }
        }

        tryagain--;
        if (tryagain <= 0) {
            Serial.println("INCOMING POST DATA TIMEDOUT WAITING FOR PACKET!");
        }
    }

    f.close();

//    twfc->client->printf("HTTP/1.1 201 OK\n\n");
    const char okay[] = "200 File was uploaded!\n";
    http_send(twfc, okay, strlen(okay), true, "text/html");

    setClientTimedClose(twfc);
}

//------------------------------------------------------------------
//
// HANDLE Websocket Message
//

void ToolkitWiFi::setWSLiveChangesFunction(void(*func)(const char*,const char*))
{
    _ws_live_changes_func = func;
}

void ToolkitWiFi::websocket_handleLiveChanges(const char *name,
    const char *value)
{
    if (NULL!=_ws_live_changes_func) {
        _ws_live_changes_func(name, value);
    }
}

void ToolkitWiFi::websocket_echoToOthers(ToolkitWiFi_Client *twfc,
    const char *name, const char *value)
{
    for (uint16_t i = 0; i < MAX_CLIENTS; i++) {
        ToolkitWiFi_Client *t = &_client_list[i];
        if (t != twfc) {
            if (NULL != t->client) {
                if (!t->client->connected()) {
                    closeClient(t);
                    //    Serial.println("Client closed itself");
                } else if (ToolkitWiFi_Client::TYPE_WEBSOCKET==t->type) {
                    static char s[64];
                    sprintf(s,"%s %s\n", name, value);
                    websocket_sendString(t, s);
                }
            }
        }
    } // end of for()
}

boolean ToolkitWiFi::websocket_handleIncoming(
    ToolkitWiFi_Client *twfc,
    const char *buffer, size_t size)
{
    static websocket_frame_info wfi;
    int remaining = size;

    uint32_t framesize = websocket_parse_frame(&wfi, buffer, remaining);
    while ((framesize) && (remaining > 0)) {
        if (8 == wfi.opcode) {
            framesize = 0;
            remaining = 0;
            // Serial.println("WebSocket has closed");
            return false;
        } else {
            if (1 == wfi.opcode) { // text
                if (framesize <= 126) {
                    const char *msg = &buffer[2+4]; // masks are 4 bytes
                    size_t msg_length = wfi.dataSize;
                    char *m = (char *) msg;
                    m[msg_length-1] = 0;
                    // Serial.println(m);

                    const char *name = NULL;
                    const char *value = NULL;
                    SettingItem::parseSetting(m, &name, &value);
                    if (name) {
                        if ('$' == name[0]) { // command
                            //Serial.println(name);
                            if (0==strcmp("$RESET", name)) {
                                ESP.restart();
                            }
                        } else { // setting
                            SettingItem::updateOrAdd(name, value);
                            ToolkitSPIFFS::saveSettings();
                            websocket_echoToOthers(twfc, name, value);
                            websocket_handleLiveChanges(name, value);
                        }
                    }
                }
            }
            buffer = buffer + framesize;
            remaining = remaining - framesize;
            framesize = websocket_parse_frame(&wfi, buffer, remaining);
        }
    } // end while()
    return true;
}

void ToolkitWiFi::handleWebSocketMessage(ToolkitWiFi_Client *twfc)
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
        closeClient(twfc);
    }
}

//------------------------------------------------------------------
//
// HANDLE Outgoing MP3 Stream .. send to icecast and local listeners
//

void ToolkitWiFi::handleOutgoingMP3Stream(ToolkitWiFi_Client *twfc)
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

//
// END OF ToolkitWiFi.cpp
