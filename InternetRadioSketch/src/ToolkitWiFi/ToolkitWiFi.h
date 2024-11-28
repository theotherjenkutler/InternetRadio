//
// ToolkitWiFi.h

//
// This is the local connection, "captive portal" and
// HTTP+WS server code.
// The server can be accessed via the local router IP
// and via the WiFi access point.
//

#ifndef ToolkitWiFi_H
#define ToolkitWiFi_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>

//
// Server client

class ToolkitWiFi_Client
{
    public:
        enum {
            TYPE_UNUSED     = 0,
            TYPE_UNKNOWN    = 1,
            TYPE_GET        = 2,
            TYPE_WEBSOCKET  = 3,
            TYPE_MP3STREAM  = 4,    // a local network listener
            TYPE_MP3ICECAST = 5     // broadcast to a remote icecast server
        };

        enum {
            CLOSE_TIMEOUT = 1000   // milliseconds
        };

        WiFiClient *client;
        uint32_t type;
        uint32_t millis_last_used;
};

//
// The server object

class ToolkitWiFi
{
    public:
        ToolkitWiFi();
        ~ToolkitWiFi();

        enum {
            WIFI_ALL_OKAY               = 0x00,
            WIFI_TIMEOUT_ON_STATION     = 0x01,
            WIFI_FAIL_ON_ACCESS_POINT   = 0x02,
            WIFI_ALL_FAILED             = 0x03
        };

        enum {
            DNS_PORT        = 53,
            HTTP_PORT       = 80
        };

        enum {
            MAX_CONNECTIONS = 30,
            MAX_CLIENTS     = 30,
            AP_CHANNEL      = 10
        };

        uint16_t begin(uint16_t timeout_in_seconds=60);
        void run();

        void setDefaultIndexPage(const char *buffer, size_t size);

        void setMP3DataStreamFunction(uint8_t*(*func)(size_t*));

        boolean startIcecastBroadcast();
        boolean isIcecastBroadcastStillConnected();

        void setWSLiveChangesFunction(void(*func)(const char*,const char*));

    private:
        boolean _server_running;
        WiFiServer *_server;   // a TCP listener on port 80, 30 clients
            // I am betting that max clients is really max listener stack size
            // since the clients get passed once they are accepted.
            // NOTE: a WiFiServer is a NetworkServer
        const char *_default_index;
        size_t _default_index_size;

        boolean _dns_server_running;
        DNSServer *_dnsServer;

        uint16_t _num_clients;
        ToolkitWiFi_Client _client_list[MAX_CLIENTS];

        // this is a network connected() state
        // true if we are connected to the icecast server
        // false if we disconnect
        boolean _icecast_is_sending;

        boolean _mp3data_is_ready;
        uint8_t *(*_mp3_data_func)(size_t*);
        uint8_t *_mp3_data = NULL;
        size_t _mp3_data_length = 0;

        void (*_ws_live_changes_func)(const char*,const char*);


        ToolkitWiFi_Client *getAnEmptyClient();
        void closeClient(ToolkitWiFi_Client *twfc);
        void setClientTimedClose(ToolkitWiFi_Client *twfc);
        boolean didClientTimeout(ToolkitWiFi_Client *twfc);

        void acceptNewClients();

        void checkClientList();
        void handleUnknownRequest(ToolkitWiFi_Client *twfc);
        void handleGetRequest(ToolkitWiFi_Client *twfc, const char *path);
        void handlePostRequest(ToolkitWiFi_Client *twfc, const char *buffer,
            size_t size);

        void websocket_handleLiveChanges(const char *name, const char *value);
        void websocket_echoToOthers(ToolkitWiFi_Client *twfc,
            const char *name, const char *value);
        boolean websocket_handleIncoming(ToolkitWiFi_Client *twfc,
            const char *buffer, size_t size);

        void handleWebSocketMessage(ToolkitWiFi_Client *twfc);
        void handleOutgoingMP3Stream(ToolkitWiFi_Client *twfc);

        void markMP3StreamDataSent();
};

#endif

//
// END OF ToolkitWiFi.h
