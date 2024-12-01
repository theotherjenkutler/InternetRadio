//
// ToolkitWiFi_Client.h

#ifndef ToolkitWiFi_Client_H
#define ToolkitWiFi_Client_H

#include <Arduino.h>
#include <WiFi.h>

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

        enum {
            MAX_CLIENTS     = 30
        };

        WiFiClient *client;
        uint32_t type;
        uint32_t millis_last_used;
        void (*closed)();   // call this when the client closes

        ToolkitWiFi_Client();
        ~ToolkitWiFi_Client();

        void closeClient();
        boolean didClientTimeout();
        void setClientTimedClose();

        static ToolkitWiFi_Client *getAnEmptyClient();
        static void acceptNewClient(WiFiClient client);
        static void checkClientList(void(*func)(ToolkitWiFi_Client*));

        static uint32_t _num_clients;
        static ToolkitWiFi_Client _client_list[MAX_CLIENTS];
};


#endif

//
// END OF ToolkitWiFi_Client.h
