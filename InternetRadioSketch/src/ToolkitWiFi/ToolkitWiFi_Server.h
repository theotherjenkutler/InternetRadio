//
// ToolkitWiFi_Server.h

//
// This is the local connection, "captive portal" and
// HTTP+WS server code.
// The server can be accessed via the local router IP
// and via the WiFi access point.
//

#ifndef ToolkitWiFi_Server_H
#define ToolkitWiFi_Server_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>

#include "ToolkitWiFi_Client.h"

//
// The server object

class ToolkitWiFi_Server
{
    public:
        ToolkitWiFi_Server();
        ~ToolkitWiFi_Server();

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

        // timeout for connecting to the local WiFi router
        static uint16_t begin(uint16_t timeout_in_seconds=60);

        // main server run function
        static void run();

        // in case there is no index.html on the flash drive
        // you can set a default block of text
        static void setDefaultIndexPage(const char *buffer, size_t size);

        // function that the server uses to get the next available
        // mp3 data buffer
        static void setMP3DataStreamFunction(uint8_t*(*func)(size_t*));

        // connect to icy and start streaming
        static boolean startIcecastBroadcast();
        static boolean isIcecastBroadcastStillConnected();

        // function to forward incoming WS volume control messages
        // to the VLSI chip
        static void setWSLiveChangesFunction(void(*func)(const char*,const char*));
        static void handleWSLiveChanges(const char *name, const char *value);

};

#endif

//
// END OF ToolkitWiFi_Server.h
