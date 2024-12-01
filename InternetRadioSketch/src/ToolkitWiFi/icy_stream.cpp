//----------------------------------------------------------------------------------
//
// icy_stream.cpp - 2019 / 2024
//

#include <WiFi.h>
#include "../ToolkitFiles/ToolkitFiles.h"

//----------------------------------------------------------------------------------------------
//
//	icy_request_header
//	February 2019 / 2024
//

//
// Base64 Encoding

static const uint8_t base64_table[65] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void encode3as4(uint8_t *b64, const uint8_t *text)
{
     *b64++ = base64_table[text[0] >> 2];
     *b64++ = base64_table[((text[0] & 0x03) << 4) | (text[1] >> 4)];
     *b64++ = base64_table[((text[1] & 0x0f) << 2) | (text[2] >> 6)];
     *b64++ = base64_table[text[2] & 0x3f];
}

static void encode2as4(uint8_t *b64, const uint8_t *text)
{
     *b64++ = base64_table[text[0] >> 2];
     *b64++ = base64_table[((text[0] & 0x03) << 4) | (text[1] >> 4)];
     *b64++ = base64_table[((text[1] & 0x0f) << 2)];
     *b64++ = '=';
}

static void encode1as4(uint8_t *b64, const uint8_t *text)
{
     *b64++ = base64_table[text[0] >> 2];
     *b64++ = base64_table[((text[0] & 0x03) << 4)];
     *b64++ = '=';
     *b64++ = '=';
}

static void encodeAsBase64(char *b64, const char *text)
{
	int32_t len = strlen(text);
     while (len >= 3) {
     	encode3as4((uint8_t *) b64, (const uint8_t *) text);
          b64 += 4;
          text += 3;
          len -= 3;
     }
     if (2 == len) {
     	encode2as4((uint8_t *) b64, (const uint8_t *) text);
          b64 += 4;
     }
     else if (1 == len) {
     	encode1as4((uint8_t *) b64, (const uint8_t *) text);
          b64 += 4;
     }
	*b64 = '\0';
}

//
// BUILD A LOGIN REQUEST and HEADERS

//     > PUT /stream.mp3 HTTP/1.1
//     > Host: example.com:8000
//     > Authorization: Basic c291cmNlOmhhY2ttZQ==
//     > User-Agent: curl/7.51.0
//     > Accept: */*
//     > Transfer-Encoding: chunked
//     > Content-Type: audio/mpeg
//     > Ice-Public: 1
//     > Ice-Name: Teststream
//     > Ice-Description: This is just a simple test stream
//     > Ice-URL: http://example.org
//     > Ice-Genre: Rock
//     > Expect: 100-continue
//     >

// should typically fit in a 512 byte buffer

static void icy_make_request(char *buffer, const char *ip, uint32_t port,
	char *username, char *password, char *mountpoint)
{
	const char *requestA = "PUT /";
	const char *requestB = " HTTP/1.1";
    const char *host = "HOST: ";	// ip:port
    const char *auth = "Authorization: Basic "; // base64 of username:password
    const char *accept = "Accept: */*";
    const char *trans = "Transfer-Encoding: chunked";
    const char *type = "Content-Type: audio/mpeg";
    const char *icy_public = "Ice-Public: 1";
    const char *icy_name = "Ice-Name: Toolkit Stream";
    const char *icy_descr = "Ice-Description: Toolkit stream description.";
    const char *icy_url = "Ice-URL: http://wavefarm.org";
    const char *icy_genre = "Ice-Genre: Toolkit Art";
    const char *expect = "Expect: 100-continue";

    char authbuffer[64];
    strcpy(authbuffer, username);
    strcat(authbuffer, ":");
    strcat(authbuffer, password);
    char b64[64];
    encodeAsBase64(b64, authbuffer);

	sprintf(buffer, "%s%s%s\n%s%s:%u\n%s%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n\n",
     	requestA, mountpoint, requestB,
        host, ip, port,
        auth, b64,
        accept, trans, type,
        icy_public, icy_name, icy_descr, icy_url, icy_genre,
        expect);
}

//----------------------------------------------------------------------------------------------
//
// icy_start_stream
//

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
