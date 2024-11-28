//----------------------------------------------------------------------------------
//
// icy_stream.cpp - 2019 / 2024
//

#include "ToolkitWiFi.h"
#include "icy_request_header.h"
#include "../ToolkitSPIFFS/ToolkitSettings.h"

boolean icy_start_stream(WiFiClient *client)
{
	static char request[512];  // reusable preallocated buffer
    icy_make_request(request,
        SettingItem::findString("remote_icecast_url"),        //prefs_getIcyURL(),
        SettingItem::findUInt("remote_icecast_port",8000),    // prefs_getIcyPort(),
        SettingItem::findString("remote_icecast_user"),       //prefs_getIcyUser(),
        SettingItem::findString("remote_icecast_password"),   //prefs_getIcyPass(),
        SettingItem::findString("remote_icecast_mountpoint")); //prefs_getIcyMount());
    Serial.println(request);

    int32_t request_length = strlen(request);
    client->write(request, request_length);

    // wait for icy reply
    uint16_t timeout = 20;
    size_t avail = 0;
    while (timeout && (0==(avail=client->available()))) {
        delay(100);
        Serial.print("~");
        timeout--;
    }

    if (avail) {
        char reply[1024];
        if (avail > 1023) { avail=1023; }
        client->readBytes(reply,avail);
        reply[avail] = 0;
        Serial.printf("\nREPLY\n%s\n", reply);
    } else {
        Serial.println("Error reading reply from ICY server!");
        return false;
    }

    return true;
}

//
// END OF icy_stream.cpp
